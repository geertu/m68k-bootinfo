
CC = $(CROSS_COMPILE)gcc

CFLAGS = -Wall -Werror -O3 -fomit-frame-pointer -fno-strict-aliasing

.PHONY:		all clean


all:		m68k-bootinfo

m68k-bootinfo:	m68k-bootinfo.c
		$(CC) $(CFLAGS) -o m68k-bootinfo m68k-bootinfo.c -lbsd

clean:
		$(RM) m68k-bootinfo

