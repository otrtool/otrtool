# (adapted from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html)
#
# 'make depend' uses makedepend to automatically generate dependencies 
#               (dependencies are added to end of Makefile)
# 'make'        build executable file 'otrtool'
# 'make clean'  removes all .o and executable files
#

DVERSION = v0.3
VERSION := $(shell git describe --long 2>/dev/null || echo "$(DVERSION)")

CC = gcc
CFLAGS = -Wall -g -DVERSION='"$(VERSION)"'
LIBS = -lmcrypt -lssl -lcurl

SRCS = src/main.c
MAIN = otrtool

OBJS = $(SRCS:.c=.o)

.PHONY: depend clean doc

all:    $(MAIN) doc
	@echo Done.

doc:
	gzip -c doc/otrtool.1 > otrtool.1.gz
	@echo Manpage was gzipped successfully

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) -o $(MAIN) $(OBJS) $(LIBS)
	@echo Build successful

# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) -c $<  -o $@

clean:
	$(RM) $(OBJS) $(MAIN) $(MAIN).1.gz

depend: $(SRCS)
	makedepend $^

# DO NOT DELETE THIS LINE -- make depend needs it

src/main.o: /usr/include/stdlib.h /usr/include/features.h
src/main.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
src/main.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-64.h
src/main.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
src/main.o: /usr/include/endian.h /usr/include/bits/endian.h
src/main.o: /usr/include/bits/byteswap.h /usr/include/sys/types.h
src/main.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
src/main.o: /usr/include/time.h /usr/include/sys/select.h
src/main.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
src/main.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
src/main.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
src/main.o: /usr/include/stdio.h /usr/include/libio.h
src/main.o: /usr/include/_G_config.h /usr/include/wchar.h
src/main.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
src/main.o: /usr/include/string.h /usr/include/xlocale.h
src/main.o: /usr/include/strings.h /usr/include/unistd.h
src/main.o: /usr/include/bits/posix_opt.h /usr/include/bits/environments.h
src/main.o: /usr/include/bits/confname.h /usr/include/getopt.h
src/main.o: /usr/include/sys/stat.h /usr/include/bits/stat.h
src/main.o: /usr/include/fcntl.h /usr/include/bits/fcntl.h
src/main.o: /usr/include/errno.h /usr/include/bits/errno.h
src/main.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
src/main.o: /usr/include/asm-generic/errno.h
src/main.o: /usr/include/asm-generic/errno-base.h /usr/include/sys/syscall.h
src/main.o: /usr/include/asm/unistd.h /usr/include/asm/unistd_64.h
src/main.o: /usr/include/bits/syscall.h /usr/include/mcrypt.h
src/main.o: /usr/include/mutils/mcrypt.h /usr/include/openssl/md5.h
src/main.o: /usr/include/openssl/e_os2.h /usr/include/openssl/opensslconf.h
src/main.o: /usr/include/curl/curl.h /usr/include/curl/curlver.h
src/main.o: /usr/include/curl/curlbuild.h /usr/include/sys/socket.h
src/main.o: /usr/include/sys/uio.h /usr/include/bits/uio.h
src/main.o: /usr/include/bits/socket.h /usr/include/bits/sockaddr.h
src/main.o: /usr/include/asm/socket.h /usr/include/asm-generic/socket.h
src/main.o: /usr/include/asm/sockios.h /usr/include/asm-generic/sockios.h
src/main.o: /usr/include/curl/curlrules.h /usr/include/limits.h
src/main.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
src/main.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
src/main.o: /usr/include/sys/time.h /usr/include/curl/easy.h
src/main.o: /usr/include/curl/multi.h /usr/include/curl/curl.h
src/main.o: /usr/include/curl/types.h /usr/include/curl/easy.h
