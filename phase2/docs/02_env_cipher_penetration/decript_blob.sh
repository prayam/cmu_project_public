#!/bin/sh

AES_ROOT_KEY=$(hexdump -e '16/1 "%02x"' /var/shinpark/secret.key)
#echo ${AES_ROOT_KEY}
IV_VALUE='00000000000000000000000000000000'
#echo ${IV_VALUE}

#extract blob data
FACE_DB_PATH=/home/lg/team6/tartan/LgFaceRecDemoTCP_Jetson_NanoV2/tartan_faces.db
SQL_STRING="select writefile('blob.bin', face) from faces where id="${1}
sqlite3 ${FACE_DB_PATH} "${SQL_STRING}"

mv blob.bin blob${1}.bin
# just eliminate first 16byte, it's for size variables
dd bs=16 skip=1 if=blob${1}.bin of=blob${1}.mod

# remove last 8 byte, why is this attached?? I don't know
truncate -s -8 blob${1}.mod

# or for test, you can attach 8byte 00s... It's required for AES block length matching
#truncate -s +8 ${2}.mod

# openssl enc -aes-128-cbc -d -in blob${1}.mod -out blob${1}.dec\
# 	-K '123456789ABCDEF03456789ABCDEF012'\
# 	-iv '00000000000000000000000000000000'\
# 	-nosalt -nopad
openssl enc -aes-128-cbc -d -in name${1} -out name${1}.dec\
	-K ${AES_ROOT_KEY}\
	-iv ${IV_VALUE}\
	-nosalt -nopad
