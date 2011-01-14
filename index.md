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

Packages for your distribution of choice
----------------------------------------

If you miss a package, contact me!

**This projects needs package maintainers (=> you?)**
It would be nice to have a working specfile for the rpm-packages (suse, fedora, etc..)

See [the otrtool-project at suse build services](https://build.opensuse.org/package/show?package=otrtool&project=home%3Apyropeter%3Aotrtool).


### Arch Linux
* [AUR/otrtool](http://aur.archlinux.org/packages.php?ID=41577)
* [AUR/otrtool-git](http://aur.archlinux.org/packages.php?ID=40775)

### Gentoo
* [lem](http://github.com/lem/) wrote [an ebuild](http://bugs.gentoo.org/attachment.cgi?id=251059), but [the submit process is taking a long time](http://bugs.gentoo.org/show_bug.cgi?id=341059).

### Debian Lenny
* Stable:
  [i386](http://download.opensuse.org/repositories/home:/pyropeter:/otrtool/Debian_5.0/i386/otrtool_1.0.0_i386.deb),
  [amd64](http://download.opensuse.org/repositories/home:/pyropeter:/otrtool/Debian_5.0/amd64/otrtool_1.0.0_amd64.deb)

### Debian Sid
Sid is not (yet?) supported by the suse build service.
The Lenny package could work, try it.

### Ubuntu 10.04: Lucid Lynx
* Stable:
  [i386](http://download.opensuse.org/repositories/home:/pyropeter:/otrtool/xUbuntu_10.04/i386/otrtool_1.0.0_i386.deb),
  [amd64](http://download.opensuse.org/repositories/home:/pyropeter:/otrtool/xUbuntu_10.04/amd64/otrtool_1.0.0_amd64.deb)

Changelog
---------

### 1.1 `development`
* No changes yet

### 1.0 `stable`
* Offers to overwrite destination file
* Piping in otrkeys using `tail -f` now works
* Nicer error messages ;-)

### 0.9
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
