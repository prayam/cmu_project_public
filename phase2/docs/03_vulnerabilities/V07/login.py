# -*- coding: utf-8 -*-
import protocolLogin_pb2
import sys
import socket

# login protobuf
login = protocolLogin_pb2.LoginMsg()
login.user_id = "admin"
login.password = "lg"

msg = login.SerializeToString()

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('192.168.0.228', 50000))

test = b'\x53\x42\x31\x54\x1b\x00\x00\x00\xc7\x8c\x07\x3c\xe8\x03\x00\x00'
s.sendall(test + msg)

#result = test+msg
#print(result)
#print(type(result))

#s.sendall(b'\x53\x42\x31\x54\x1b\x00\x00\x00\xc7\x8c\x07\x3c\xe8\x03\x00\x00\x0a\x05\x61\x64\x6d\x69\x6e\x12\x02\x6c\x67')


#f = open("bbb", "wb")
#f.write(login.SerializeToString())
#f.close()
