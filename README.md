# cmu_project

## folder
* docs/ : project documents
    * phase1/ : phase1 documents
    * phase2/ : phase2 documents
* source/ : project code
    * client/ : client code
    * server/ : server code
    * common/ : common code for server & client
    * checksec.sh : script for checking secure options in ELF binary

---
## terms
* server : target device with camera (i.e. jetson nano)
* client : pc program which has some interface to control target 

## commit message rule
<pre>
<code>
[feature] title

description
</code>
</pre>

example
<pre>
<code>
[docs|server|client] Fix crash when resolve fail

when resolving failure is happened, the crash is happened because the
</code>
</pre>
