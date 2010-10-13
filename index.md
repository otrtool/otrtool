---
title: Home
layout: wikistyle
---

otrtool
=======
otrtool aims to provide an open source tool to deal with otrkey-files from [OnlineTvRecorder.com](http://onlinetvrecoder.com). At the moment it is able to decrypt them, in the future a download manager and/or EPG could be added.

Packages
--------

### Arch Linux
<ul>
<li>[AUR/otrtool](http://aur.archlinux.org/packages.php?ID=41577)</li>
<li>[AUR/otrtool-git](http://aur.archlinux.org/packages.php?ID=40775)</li>
</ul>

*This projects needs package maintainers (=> you?)*

Dependencies
------------

<ul>
<li>libmcrypt</li>
<li>libcurl / curl</li>
</ul>

Building
--------

    $ make depend && make

After that, copy the following files to the places they belong:

<ul>
<li>`./otrtool` (the main binary)</li>
<li>`./otrtool.1.gz` (the gzip'ed manpage)</li>
<li>`./LICENSE` (the (rights waiver) license)</li>
</ul>

Links
-----

<ul>
<li>[github repository](http://github.com/pyropeter/otrtool)</li>
<li>[bugtracker](http://github.com/pyropeter/otrtool/issues)</li>
<ul>

### Related projects:

<ul>
<li>[lem's tk/perl GUI](http://github.com/Lem/otrtool-gui)</li>
<ul>

Contact
-------

    E-Mail:                 com.googlemail@abi1789
    IRC:                    PyroPeter at freenode

Reporting Bugs
--------------

For bug reports please run otrtool with the "-v"-option. Note that the output contains personal data like your email/password, and/or hashes of thiese.

History
-------

In April, 2010, eddy14 reverse-engineered the otrkey-file-format and OTRs Client-Server-protocol. The blog post describing his work (german):
    http://41yd.de/blog/2010/04/18/otrkey-breaker/
He also wrote a proove-of-concept tool to decrypt OTRKEY's. I rewrote it in proper C and without licensing issues.

License
-------

otrtool uses the CC0-license.
(This seems to place it into Public Domain in at least some countries, `disclaimer` but I am far from beeing a lawyer `/disclaimer`)

For the license text see the LICENSE file or [CC0 at creativecommons.org](http://creativecommons.org/publicdomain/zero/1.0/legalcode)
