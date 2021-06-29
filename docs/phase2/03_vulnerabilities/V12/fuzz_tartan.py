import socket
import os
import sys
import random
import shutil
import collections
from time import sleep
from scapy.all import *

head_len = 0
fuzzed_packet_count = 0

def get_packet(tf):
    '''
    ' tartan message structure
    ' 
    '   0                   1                   2                   3
    '   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    '  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    '  |                        preamble (="SB1T")                     |
    '  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    '  |    size (whole packet or protocol message) (little endian)    |
    '  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    '  |                           timestamp                           |
    '  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    '  |      message type (1000, 1002~1007, 9999) (little endian)     |
    '  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    '  |                        protocol meesage                       |
    '  |                             ....                              |
    '  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    '''
    global head_len

    random.seed()
    payload_len = random.randrange(1, 100)
    msgtype = random.randrange(998, 1010).to_bytes(4, 'little')
    
    # head_length can be valid (=whole packet size) or invalid (= only protocol message size)
    if tf: # fuzz [preamble, length(1~100), timestamp, message type(998~1010), protocol message]
        p = fuzz(Raw(RandBin(size = 4)))/ \
                Raw(load=(payload_len + head_len).to_bytes(4, 'little'))/ \
                fuzz(Raw(RandBin(size = 4)))/ \
                Raw(load=msgtype)/ \
                fuzz(Raw(RandBin(size=payload_len)))
    else: # fuzz [length(1~100), timestamp, message type(998~1010), protocol message]
        p = fuzz(Raw(load="SB1T"))/ \
                Raw(load=(payload_len + head_len).to_bytes(4, 'little'))/ \
                fuzz(Raw(RandBin(size = 4)))/ \
                Raw(load=msgtype)/ \
                fuzz(Raw(RandBin(size=payload_len)))

    return p.copy() # return deep copy of the fuzzed packet

def test_tcp_fuzz():
    global fuzzed_packet_count
    fuzzed_packets = collections.deque(maxlen=1000) # in order to store the last 1000 fuzzed packets

    try:
        sleep(3)
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('192.168.0.228', 50000)) # conenct to the server
        ss = StreamSocket(s)

        while True:
            p = get_packet(random.choice([True, False])) # get a fuzzed packet
            fuzzed_packets.append(p) # keep the fuzzed packet to store it
            fuzzed_packet_count += 1
            print('[pkt_{0:010d}]'.format(fuzzed_packet_count))
            hexdump(p) # print fuzzed packet
            print()
            ss.send(p) # send the fuzzed packet to the server
            sleep(0.05)
    except Exception as err:
        print('error: ', err)

        if err.errno == 111: # found the server crash because the server doesn't open the connection port
            return -1

        if os.path.exists('./fuzz_packet'): # delete path including the fuzzed packets to reproduce the crash.
            shutil.rmtree('./fuzz_packet')
     
        os.mkdir('./fuzz_packet') # make path to store the fuzzed packets

        # save the last fuzzed packets. name ofrmat is pkt_[10 digit with left padding 0]            
        fuzzed_packets.reverse()
        for i in range(len(fuzzed_packets)):
            pkt_num = fuzzed_packet_count - i
            f = open('./fuzz_packet/pkt_' + str(pkt_num).zfill(10), 'wb')
            f.write(bytes(fuzzed_packets[i]))
            f.close()

    finally:
        s.close()

    return 0;

if __name__ == "__main__":
    if len(sys.argv) == 2:
        head_len = int(sys.argv[1])

    if os.path.exists('./fuzz_packet'): # delete path including the fuzzed packets to reproduce the crash.
        shutil.rmtree('./fuzz_packet')

    while 0 == test_tcp_fuzz(): # do fuzz
        print("fuzz again")
