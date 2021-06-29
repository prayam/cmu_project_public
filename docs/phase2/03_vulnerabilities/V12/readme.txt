precondition
$ pip install --pre scapy[basic]

usage 
# head_length can be valid (=whole packet size) or invalid (= only protocol message size)
# fuzz [preamble, length(1~100), timestamp, message type(998~1010), protocol message]
$ python3 fuzz_tartan.py    #invalid head_length fuzz
$ python3 fuzz_tartan.py 16 #valid head_length fuzz

verify
$ python3 fuzz_verify.py