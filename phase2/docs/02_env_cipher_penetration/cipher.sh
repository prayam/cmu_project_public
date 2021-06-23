#hexdump -e '16/1 "%02x"' /var/shinpark/secret.key
#AES_ROOT_KEY=$?
#echo ${AES_ROOT_KEY}
#IV_VALUE='000000000000000000000000000000000'
#echo ${IV_VALUE}

echo -n ${2} | xxd -r -p > name${1}

openssl enc -aes-128-cbc -d -in name${1} -out name${1}.dec\
	-K '123456789ABCDEF03456789ABCDEF012'\
	-iv '00000000000000000000000000000000'\
	-nosalt -nopad

echo "id num: "${1};
cat name${1}.dec
echo;

