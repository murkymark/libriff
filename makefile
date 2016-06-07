#GNU gcc makefile
#Call "make" to build executeable
#Call "make lib" to build static library

CC=gcc
CFLAGS=

AR=ar -rcs


.PHONY: all
all:
	$(CC) -o example.exe example.c riff.c

.PHONY: lib
lib: riff.o
	$(AR) libriff.a $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
