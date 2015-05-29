OTRKEY file format
==================

bytes (dec) | content
------------|---------------------------
0-9         | magic string "OTRKEYFILE"
10-521      | encrypted header
522-...     | encrypted payload


Encryption
----------

The header is encrypted using Blowfish in ECB mode with a hard-coded key
(hardKey in main.c). The payload is encrypted in the same manner but with an
individual key. If the payload length is not a multiple of eight, the last
few bytes are stored unencrypted.

Note that both header and payload are encrypted with incorrect endianness,
so that you have to reverse byte order in 32-bit words before and after
decryption. (Or you can just use the "blowfish-compat" encryption in
libmcrypt.)


Header format
-------------
The header contains a concatenation of fields in the form "&NAME=VALUE".

name | value
-----|---------------------------------------------------
FN   | file name of the payload
FH   | hash of the payload
OH   | hash of the OTRKEY file (beginning from byte 522)
Sw   | unknown
SZ   | size of the OTRKEY file in bytes
H    | unknown
PD   | random padding to fill up to 512 bytes


### Hash values (FH, OH)

These fields contain a MD5 sum intermingled with some other 32 bits of
information. In a repeating pattern, the first two hex digits belong to
the MD5 sum and the third digit contains two bits of further information.

