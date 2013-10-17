
CC = $(CROSS_COMPILE)gcc

CFLAGS += -Wall -Werror -O3 -fomit-frame-pointer -fno-strict-aliasing

.PHONY:		all clean


all:		m68k-bootinfo

ifneq ("$(M68K_HEADERS)", "")
DEPS += m68k-headers/asm/bootinfo.h
DEPS += m68k-headers/asm/bootinfo-amiga.h
DEPS += m68k-headers/asm/bootinfo-apollo.h
DEPS += m68k-headers/asm/bootinfo-atari.h
DEPS += m68k-headers/asm/bootinfo-hp300.h
DEPS += m68k-headers/asm/bootinfo-mac.h
DEPS += m68k-headers/asm/bootinfo-vme.h

CFLAGS += -DUSE_M68K_HEADERS

$(DEPS):
		ln -sf $(M68K_HEADERS) m68k-headers
endif

m68k-bootinfo:	m68k-bootinfo.c $(DEPS)
		$(CC) $(CFLAGS) -o m68k-bootinfo m68k-bootinfo.c -lbsd

clean:
		$(RM) m68k-bootinfo m68k-headers
