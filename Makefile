# (adapted from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html)
#
# 'make depend'  uses makedepend to automatically generate dependencies 
#                (dependencies are added to end of Makefile)
# 'make'         alias for 'make otrtool && make doc'
# 'make doc'     gzips manpage
# 'make otrtool' build executable file 'otrtool'
# 'make clean'   removes all .o and executable files
#

DVERSION = v0.8
VERSION := $(shell git describe --long --dirty 2>/dev/null || echo "$(DVERSION)")

CC = gcc
CFLAGS = -Wall -g -DVERSION='"$(VERSION)"'
LIBS = -lmcrypt -lcurl

SRCS = src/md5.c src/main.c
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
	makedepend -w70 $^

# DO NOT DELETE THIS LINE -- make depend needs it
