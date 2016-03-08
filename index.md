---
title: Home
layout: wikistyle
---

otrtool
=======
otrtool aims to provide an open source tool to deal with .otrkey files from [OnlineTvRecorder.com](http://onlinetvrecorder.com/).

Produces nice output!
---------------------

    $ otrtool -x somefile.avi.otrkey
    OTR-Tool, v1.2.0-0-gf94515e
    Enter your eMail-address: foo@bar.tld
    Enter your password:
    Trying to contact server...
    Server responded.
    Keyphrase: 6AD31FFC5F41EF31C5FF3A347D1AACA44E1A4CD37FD39CDD5F282B3E
    info: saved keyphrase to ~/.otrkey_cache
    Decrypting and verifying...
    [========================================] 100%
    OK checksums from header match

How to install from source
--------------------------

### Get source

* stable: [tarball](https://github.com/otrtool/otrtool/tarball/stable)
* development: [tarball](https://github.com/otrtool/otrtool/tarball/master)
* git repository: `git clone https://github.com/otrtool/otrtool.git`

### Install dependencies

The dependencies are:

* libmcrypt
* libcurl / curl

### Building

    $ make

After that, copy the following files to the places they belong:

* `./otrtool` (the main binary)
* `./otrtool.1.gz` (the gzip'ed manpage)
* `./LICENSE` (the (rights waiver) license)

Packages for your distribution of choice
----------------------------------------

If your package is not listed, please contact me!

### Arch Linux
* [AUR/otrtool](https://aur.archlinux.org/packages/otrtool/)
* [AUR/otrtool-git](https://aur.archlinux.org/packages/otrtool-git/)

### pkgsrc for NetBSD et al.
* [wip/otrtool](http://pkgsrc.se/wip/otrtool)

Changelog
---------

This list will only contain major and minor updates, no bugfix releases.

### 1.2.0
2016-03-01

* Multiple input files
* Option to unlink after decryption
* 'Verify only' mode of operation

### 1.1.0
2013-07-15

* Integrity check before and after decryption
* Improved decryption performance
* Keyphrase cache
* Output to a pipe is now supported

### 1.0
2010-11-05

* Offers to overwrite destination file
* Piping in otrkeys using `tail -f` now works
* Nicer error messages ;-)

### 0.9
2010-10-17

* `otrtool -h` slimmed down to "important" commands
* (various bugfixes)

Links
-----

* [Homepage](https://otrtool.github.io/otrtool/)
* [GitHub repository](https://github.com/otrtool/otrtool)
* [Bug tracker](https://github.com/otrtool/otrtool/issues)

### Related projects:

* [Lem's Perl/Tk GUI](https://github.com/Lem/otrtool-gui)

Contact
-------

    E-Mail:                 eshrdlu/AT/yandex/DOT/com

Reporting Bugs
--------------

For bug reports please run otrtool with the "-v"-option. Note that the output contains personal data like your email/password, and/or hashes of them.

History
-------

In April, 2010, eddy14 reverse-engineered the otrkey file format and OTR's client-server protocol. He described his work in a German [blog post](http://pyropeter.eu/41yd.de/blog/2010/04/18/otrkey-breaker/). He also wrote a proof-of-concept tool to decrypt OTRKEYs. PyroPeter rewrote it in C and without licensing issues.
In February, 2015, PyroPeter handed over maintenance to eshrdlu.

Copying
-------

otrtool uses the Creative Commons [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/) rights waiver / license, which basically means you are free to do *anything* with otrtool.
The full text is included in the `LICENSE` file within the source tree.
