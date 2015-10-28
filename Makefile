# (adapted from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html)
#
# 'make depend'  uses makedepend to automatically generate dependencies 
#                (dependencies are added to end of Makefile)
# 'make'         alias for 'make otrtool && make doc'
# 'make doc'     gzips manpage
# 'make otrtool' build executable file 'otrtool'
# 'make clean'   removes all .o and executable files
#

include config.mk

SRCS = src/md5.c src/main.c
MAIN = otrtool

OBJS = $(SRCS:.c=.o)

.PHONY: depend clean install

all:    $(MAIN) otrtool.1.gz
	@echo Done.

otrtool.1.gz:
	gzip -c doc/otrtool.1 > otrtool.1.gz

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) -o $(MAIN) $(OBJS) $(LDFLAGS)
	@echo Build successful

# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(MAIN) $(MAIN).1.gz

install: $(MAIN) $(MAIN).1.gz
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	install -m 0755 $(MAIN) $(DESTDIR)$(PREFIX)/bin
	install -m 0644 $(MAIN).1.gz $(DESTDIR)$(MANPREFIX)/man1

depend: $(SRCS)
	makedepend -w70 -Y $^

# DO NOT DELETE THIS LINE -- make depend needs it

src/md5.o: src/md5.h
src/main.o: src/md5.h
