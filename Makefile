# file Makefile
CC= gcc
RM= rm -vf
CFLAGS= -Wall -g
.PHONY: all clean

all : ext2_ls ext2_mkdir
shared: shared.c shared.h
		$(CC) $(CFLAGS) -c shared.c
ext2_ls : shared
		$(CC) $(CFLAGS) ext2_ls.c shared.o -o ext2_ls
ext2_mkdir : shared
		$(CC) $(CFLAGS) ext2_mkdir.c shared.o -o ext2_mkdir


clean :
		$(RM) *.o ext2_ls ext2_cp ext2_mkdir *~
## eof Makefile
