#!/bin/sh

SRC_DIR=sample
DST_DIR=target

# check and create Destination DIR
if [ -d ${DST_DIR} ]; then
  rm -rf ${DST_DIR}
fi
mkdir ${DST_DIR}

# Use same size & header for all blobs
set_file_size=921600
HDR_FILE=file_header
IDX=38

FILE_SIZE_HEADER='00100e000000000000100e0000000000'
echo -n ${FILE_SIZE_HEADER} | xxd -r -p > ${HDR_FILE}

# loop start
# loop for sample file
for file in ${SRC_DIR}/*
do
    if [ -f $file ]; then

        SRC_FILE=${file}
        TMP_FILE=${SRC_FILE}.adj
        ENC_FILE=${SRC_FILE}.enc
        DST_FILE=${SRC_FILE}.bin

        cp -fpRL ${SRC_FILE} ${TMP_FILE}

        # check file size, and adjust to 
        file_size=$(stat -c%s ${TMP_FILE})

        if [ ${file_size} -lt ${set_file_size} ]; then
            diff_size=`expr ${set_file_size} - ${file_size}`
            echo "truncate +${diff_size}"
            truncate -s +${diff_size} ${TMP_FILE}
        fi
        if [ ${file_size} -gt ${set_file_size} ]; then
            diff_size=`expr ${file_size} - ${set_file_size}`
            echo "truncate -${diff_size}"
            truncate -s -${diff_size} ${TMP_FILE}
        fi

        # encrypt blob file
        AES_ROOT_KEY=$(hexdump -e '16/1 "%02x"' /var/shinpark/secret.key)
        #echo ${AES_ROOT_KEY}
        IV_VALUE='00000000000000000000000000000000'
        #echo ${IV_VALUE}

        openssl enc -aes-128-cbc -e -in ${TMP_FILE} -out ${ENC_FILE}\
            -K '123456789ABCDEF03456789ABCDEF012'\
            -iv '00000000000000000000000000000000'\
            -nosalt -nopad
        # openssl enc -aes-128-cbc -e -in ${TMP_FILE} -out ${ENC_FILE}\
        # 	-K ${AES_ROOT_KEY}\
        # 	-iv ${IV_VALUE}\
        # 	-nosalt -nopad

        # add header + encrypted data
        dd if=${HDR_FILE} >> ${DST_FILE}
        dd if=${ENC_FILE} >> ${DST_FILE}
        truncate -s +8 ${DST_FILE}

        #insert blob data
        # FACE_DB_PATH=/home/lg/team6/tartan/LgFaceRecDemoTCP_Jetson_NanoV2/tartan_faces.db
        # FACE_DB_PATH=/usr/local/tartan/tartan_faces.db
        FACE_DB_PATH=./tartan_faces.db
        SQL_STRING="insert into faces (names_id, face) values ('${IDX}', (readfile('${DST_FILE}')));"
        sqlite3 ${FACE_DB_PATH} "${SQL_STRING}"
        echo ${IDX}
        IDX=`expr ${IDX} + 1`
        SQL_STRING2="insert into names (name) values('3ed0b77fde1534148a1c6afe01debcbf');"
        sqlite3 ${FACE_DB_PATH} "${SQL_STRING2}"

        mv ${TMP_FILE} ${DST_DIR}
        mv ${ENC_FILE} ${DST_DIR}
        mv ${DST_FILE} ${DST_DIR}
    fi
done
rm ${HDR_FILE}

