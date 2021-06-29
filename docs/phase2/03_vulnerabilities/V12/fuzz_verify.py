import socket
import sys
import os

file_count = 0

def test_packet(files):
    global file_count

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('192.168.0.228', 50000)) # connect to the server

        start_idx = file_count

        for i in range(len(files) - start_idx):
            idx = i + start_idx 
            full_path = os.path.join('./fuzz_packet', files[idx])
            file_count += 1

            if os.path.isfile(full_path):
                f = open(full_path, 'rb')
                data = f.read()
                f.close()

                print(full_path) # print file contents
                print(data)
                print('enter to send data above')
                input() # wait user input
                s.sendall(data) # sent the contents of the file to server
    except Exception as err:
        print(err)

        if err.errno == 111:
            return 0

        return -1
    finally:
        s.close()

    return 0

if __name__ == "__main__":
    if os.path.exists('./fuzz_packet'):
        arr = os.listdir('./fuzz_packet')
        arr = sorted(arr, reverse=True)

        while True:
            if test_packet(arr) == 0: # all files are sent to the server, or server port is closed.
                break
    else:
        print('no ./fuzz_packet path')
