# General
MAIN = otrtool
DVERSION = v1.1.0
VERSION := $(shell git describe --tags --long --dirty 2>/dev/null || echo "$(DVERSION)")

# Directories
PREFIX    = /usr/local
MANPREFIX = $(PREFIX)/share/man

# Programs and compilers
SHELL = /bin/sh
CC    = gcc

# Compiler flags
CFLAGS = -O3 -Wall -Wextra -g -DVERSION='"$(VERSION)"'
LDFLAGS = -lmcrypt -lcurl

# large file support
CFLAGS += $(shell getconf LFS_CFLAGS)
LDFLAGS += $(shell getconf LFS_LDFLAGS)
