# Usage Examples
## cipher.sh 
'./cipher.sh (id_num:integer) (encrypted_name:hex string)'

./cipher.sh 28 9df8b9929d87f5a7b4d5c42818eb2acb
id num: 28
phoebe

## decript_blob.sh
'./decript_blob.sh (id_num:integer)'

./decript_blob.sh 12
921624
57600+1 records in
57600+1 records out
921608 bytes (922 kB, 900 KiB) copied, 0.338744 s, 2.7 MB/s

lg@LgFaceRecProject:~/work/jhahn/aes_cipher$ ll
total 5440
drwxrwxr-x 2 lg lg   4096 Jun 22 12:44 ./
drwxrwxr-x 4 lg lg   4096 Jun 22 12:38 ../
-rw-rw-r-- 1 lg lg 921624 Jun 22 12:44 blob12.bin
-rw-rw-r-- 1 lg lg 921600 Jun 22 12:44 blob12.dec
-rw-rw-r-- 1 lg lg 921600 Jun 22 12:44 blob12.mod

