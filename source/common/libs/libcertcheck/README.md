# Note for libcertcheck

Actually, we didn't push the libcertcheck library code to here, git, but we provided all the source code and build config related to libcertcheck to the red team (team-5).

The first purpose of separating the libcertcheck code from the main code was to obfuscate the part instead of hardcoding the ROOT key related to the AES cipher in libcertcheck so that a lot of effort is required through binary reverse analysis to find the key.
(There was also a hash value for checking the integrity of the ROOT certificate.)

The second purpose was to be able to build independently so that static analysis could be performed through the coverity used in lg. (But, we cannot do that, some reason...)

The third is that the part related to the selection of the initial vector was intentionally not specified in detail in the document, but the part can be seen by looking at the code, so it was hoped that the trick would not be disclosed to the public domain.

It is stated that team-5 understood this part, and team-5 received and analyzed the code related to libcertcheck.
