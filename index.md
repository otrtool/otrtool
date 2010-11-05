---
title: Home
layout: wikistyle
---

otrtool
=======
otrtool aims to provide an open source tool to deal with otrkey-files from [OnlineTvRecorder.com](http://onlinetvrecoder.com/). At the moment it is able to decrypt them, in the future a download manager and/or EPG could be added.

Produces nice output!
---------------------

    $ ./otrtool -x Foo-Bar.otrkey
    OTR-Tool, v0.5-0-g1c8a465
    Enter your eMail-address: foo@bar.tld
    Enter your      password: god
    Trying to contact server...
    Server responded.
    Keyphrase: ACBD18DB4CC2F201037B51D194A71054FCCC4A4D8136F6524F2D51F2
    Decrypting...
    [========================================] 100%

How to install using packages
-----------------------------

### Arch Linux
* [AUR/otrtool](http://aur.archlinux.org/packages.php?ID=41577)
* [AUR/otrtool-git](http://aur.archlinux.org/packages.php?ID=40775)

### Gentoo
* [lem](http://github.com/lem/) wrote [an ebuild](http://bugs.gentoo.org/attachment.cgi?id=251059), but [the submit process is taking a long time](http://bugs.gentoo.org/show_bug.cgi?id=341059).

**This projects needs package maintainers (=> you?)**
Especially a deb-packet was requested twice.

Changelog
---------

### 1.0 `development`
* Offers to overwrite destination file
* Piping in otrkeys using `tail -f` now works
* Nicer error messages ;-)

### 0.9 `stable`
* `otrtool -h` slimmed down to "important" commands
* (various bugfixes)

How to install from source
--------------------------

### Get source

* stable: [tarball](http://github.com/pyropeter/otrtool/tarball/stable)
* development: [tarball](http://github.com/pyropeter/otrtool/tarball/master)
* git repository: `git clone git://github.com/pyropeter/otrtool.git`

### Install dependencies

The dependencies are:
* libmcrypt
* libcurl / curl

### Building

    $ make depend && make

After that, copy the following files to the places they belong:

* `./otrtool` (the main binary)
* `./otrtool.1.gz` (the gzip'ed manpage)
* `./LICENSE` (the (rights waiver) license)

Links
-----

* [github repository](http://github.com/pyropeter/otrtool)
* [bugtracker](http://github.com/pyropeter/otrtool/issues)

### Related projects:

* [lem's tk/perl GUI](http://github.com/Lem/otrtool-gui)

Contact
-------

    E-Mail:                 com.googlemail@abi1789
    IRC:                    PyroPeter at freenode

Reporting Bugs
--------------

For bug reports please run otrtool with the "-v"-option. Note that the output contains personal data like your email/password, and/or hashes of them.

History
-------

In April, 2010, eddy14 reverse-engineered the otrkey-file-format and OTRs Client-Server-protocol. [He described his work in a german blog post.](http://41yd.de/blog/2010/04/18/otrkey-breaker/) He also wrote a proof-of-concept tool to decrypt OTRKEYs. I rewrote it in proper C and without licensing issues.

License
-------

You are free to do **anything** with otrtool. For details, see the `LICENSE` file.
