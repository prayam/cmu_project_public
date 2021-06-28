# Usage Examples
## name_decrypt.sh : decrypt user name
> Usage : name_decrypt.sh (id_num:integer)
```
$ ./name_decrypt.sh 28
phoebe  (id_num:28)
```

## blob_decrypt.sh : decrypt img data and convert to jpg
> Usage : blob_decrypt.sh (id_num:integer)
```
$ ./blob_decrypt.sh 12
921624
57600+1 records in
57600+1 records out
921608 bytes (922 kB, 900 KiB) copied, 0.346125 s, 2.7 MB/s

$ ll
total 2628
drwxrwxr-x 4 lg lg    4096 Jun 23 05:03 ./
drwxrwxr-x 4 lg lg    4096 Jun 22 12:38 ../
-rw-rw-r-- 1 lg lg  100416 Jun 23 05:03 blob12.jpg
```

## loop_test.sh : loop db from index 1 to index 37, and execute name_decrypt.sh & blob_decrypt.sh
> Usage : loop_test.sh
```
$ ./loop_test.sh
```
