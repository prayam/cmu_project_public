#!/bin/sh

AES_ROOT_KEY=$(hexdump -e '16/1 "%02x"' /var/shinpark/secret.key)
#echo ${AES_ROOT_KEY}
IV_VALUE='00000000000000000000000000000000'
#echo ${IV_VALUE}

#extract name data
FACE_DB_PATH=/home/lg/team6/tartan/LgFaceRecDemoTCP_Jetson_NanoV2/tartan_faces.db
SQL_STRING="select (name) from names where id="${1}
NAME_STRING=$(sqlite3 ${FACE_DB_PATH} "${SQL_STRING}")

echo -n ${NAME_STRING} | xxd -r -p > name${1}

# openssl enc -aes-128-cbc -d -in name${1} -out name${1}.dec\
# 	-K '123456789ABCDEF03456789ABCDEF012'\
# 	-iv '00000000000000000000000000000000'\
# 	-nosalt -nopad
openssl enc -aes-128-cbc -d -in name${1} -out name${1}.dec\
	-K ${AES_ROOT_KEY}\
	-iv ${IV_VALUE}\
	-nosalt -nopad

cat name${1}.dec
echo "  (id_num:"${1}")";

mv name${1}.dec name${1}
