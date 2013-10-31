/*
 *  Linux/m68k bootinfo tool
 *
 *  (C) Copyright 2013 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.
 */

#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bsd/string.h>

#ifndef USE_M68K_HEADERS
#include <asm/bootinfo.h>
#include <asm/bootinfo-amiga.h>
#include <asm/bootinfo-apollo.h>
#include <asm/bootinfo-atari.h>
#include <asm/bootinfo-hp300.h>
#include <asm/bootinfo-mac.h>
#include <asm/bootinfo-vme.h>
#else
#include "m68k-headers/asm/bootinfo.h"
#include "m68k-headers/asm/bootinfo-amiga.h"
#include "m68k-headers/asm/bootinfo-apollo.h"
#include "m68k-headers/asm/bootinfo-atari.h"
#include "m68k-headers/asm/bootinfo-hp300.h"
#include "m68k-headers/asm/bootinfo-mac.h"
#include "m68k-headers/asm/bootinfo-vme.h"
#endif


#define BOOTINFO_FILE	"/proc/bootinfo"

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))


static const char *program_name;

static const char *opt_bootinfo = BOOTINFO_FILE;

enum type {
	TYPE_UNKNOWN,
	TYPE_U8,		/* 8-bit byte */
	TYPE_BE16,		/* 16-bit big endian */
	TYPE_BE32,		/* 32-bit big endian */
	TYPE_STRING,		/* zero-terminated string */
	TYPE_MEM_INFO,		/* struct mem_info */
	TYPE_AMIGA_CONFIG_DEV,	/* Amiga struct ConfigDev */
	TYPE_VME_BOARDINFO,	/* VME board information */
};

static int type_sizes[] = {
	[TYPE_UNKNOWN]		= 0,	/* unknown */
	[TYPE_U8]		= 1,
	[TYPE_BE16]		= 2,
	[TYPE_BE32]		= 4,
	[TYPE_STRING]		= -1,	/* variable length */
	[TYPE_MEM_INFO]		= 8,
	[TYPE_AMIGA_CONFIG_DEV] = 68,
	[TYPE_VME_BOARDINFO]	= 32,
};

struct record {
	uint16_t tag;
	uint16_t size;	/* size of data at end of struct */
	enum type type;
	char name[32];
	const char *desc;	/* description for data value */
	char data[0];
};

struct map {
	uint32_t key;
	const char *val;
};

static const struct map cputypes[] = {
#if 0 // Not yet defined in any public version
	{ CPU_68000, "68000" },
	{ CPU_68010, "68010" },
#endif
	{ CPU_68020, "68020" },
	{ CPU_68030, "68030" },
	{ CPU_68040, "68040" },
	{ CPU_68060, "68060" },
	{ CPU_COLDFIRE, "COLDFIRE" },
	{ 0, NULL },
};

static const struct map fputypes[] = {
	{ 0, "NONE" },
	{ FPU_68881, "68881" },
	{ FPU_68882, "68882" },
	{ FPU_68040, "68040" },
	{ FPU_68060, "68060" },
	{ FPU_SUNFPA, "SUNFPA" },
	{ FPU_COLDFIRE, "COLDFIRE" },
	{ 0, NULL },
};

static const struct map mmutypes[] = {
	{ 0, "NONE" },
	{ MMU_68851, "68851" },
	{ MMU_68030, "68030" },
	{ MMU_68040, "68040" },
	{ MMU_68060, "68060" },
	{ MMU_SUN3, "SUN3" },
	{ MMU_APOLLO, "APOLLO" },
	{ MMU_COLDFIRE, "COLDFIRE" },
	{ 0, NULL },
};

struct record_def {
	uint16_t tag;
	const char *name;
	enum type type;
	const struct map *map;
};

static const struct record_def m68k_records[] = {
	{ BI_MACHTYPE,		"machtype",	TYPE_BE32 },
	{ BI_CPUTYPE,		"cputype",	TYPE_BE32,	cputypes },
	{ BI_FPUTYPE,		"fputype",	TYPE_BE32,	fputypes },
	{ BI_MMUTYPE,		"mmutype",	TYPE_BE32,	mmutypes },
	{ BI_MEMCHUNK,		"memchunk",	TYPE_MEM_INFO },
	{ BI_RAMDISK,		"ramdisk",	TYPE_MEM_INFO },
	{ BI_COMMAND_LINE,	"command_line",	TYPE_STRING },
	{ BI_LAST,		"last",		TYPE_UNKNOWN },
};

static const struct map amiga_models[] = {
	{ AMI_UNKNOWN, "UNKNOWN" },
	{ AMI_500, "A500" },
	{ AMI_500PLUS, "A500+" },
	{ AMI_600, "A600" },
	{ AMI_1000, "A1000" },
	{ AMI_1200, "A1200" },
	{ AMI_2000, "A2000" },
	{ AMI_2500, "A2500" },
	{ AMI_3000, "A3000" },
	{ AMI_3000T, "A3000T" },
	{ AMI_3000PLUS, "A3000+" },
	{ AMI_4000, "A4000" },
	{ AMI_4000T, "A4000T" },
	{ AMI_CDTV, "CDTV" },
	{ AMI_CD32, "CD32" },
	{ AMI_DRACO, "DRACO" },
	{ 0, NULL },
};

static const struct map amiga_chipsets[] = {
	{ CS_STONEAGE, "STONEAGE" },
	{ CS_OCS, "OCS" },
	{ CS_ECS, "ECS" },
	{ CS_AGA, "AGA" },
	{ 0, NULL },
};

static const struct record_def amiga_records[] = {
	{ BI_AMIGA_MODEL,	"model",	TYPE_BE32,	amiga_models },
	{ BI_AMIGA_AUTOCON,	"autocon",	TYPE_AMIGA_CONFIG_DEV },
	{ BI_AMIGA_CHIP_SIZE,	"chip_size",	TYPE_BE32 },
	{ BI_AMIGA_VBLANK,	"vblank",	TYPE_U8 },
	{ BI_AMIGA_PSFREQ,	"psfreq",	TYPE_U8 },
	{ BI_AMIGA_ECLOCK,	"eclock",	TYPE_BE32 },
	{ BI_AMIGA_CHIPSET,	"chipset",	TYPE_BE32,	amiga_chipsets },
	{ BI_AMIGA_SERPER,	"serper",	TYPE_BE16 },
	{ BI_LAST,		"last",		TYPE_UNKNOWN },
};

static const struct map apollo_models[] = {
	{ APOLLO_UNKNOWN, "UNKNOWN" },
	{ APOLLO_DN3000, "DN3000" },
	{ APOLLO_DN3010, "DN3010" },
	{ APOLLO_DN3500, "DN3500" },
	{ APOLLO_DN4000, "DN4000" },
	{ APOLLO_DN4500, "DN4500" },
	{ 0, NULL },
};

static const struct record_def apollo_records[] = {
	{ BI_APOLLO_MODEL,	"model",	TYPE_BE32,	apollo_models },
	{ BI_LAST,		"last",		TYPE_UNKNOWN },
};

static const struct map atari_mch_cookies[] = {
	{ ATARI_MCH_ST << 16, "ST" },
	{ ATARI_MCH_STE << 16, "STE" },
	{ ATARI_MCH_TT << 16, "TT" },
	{ ATARI_MCH_FALCON << 16, "FALCON" },
	{ 0, NULL },
};

static const struct map atari_mch_types[] = {
	{ ATARI_MACH_NORMAL, "NORMAL" },
	{ ATARI_MACH_MEDUSA, "MEDUSA" },
	{ ATARI_MACH_HADES, "HADES" },
	{ ATARI_MACH_AB40, "AB40" },
	{ 0, NULL },
};

static const struct record_def atari_records[] = {
	{ BI_ATARI_MCH_COOKIE,	"mch_cookie",	TYPE_BE32,	atari_mch_cookies },
	{ BI_ATARI_MCH_TYPE,	"mch_type",	TYPE_BE32,	atari_mch_types },
	{ BI_LAST,		"last",		TYPE_UNKNOWN },
};

static const struct map hp300_models[] = {
	{ HP_320, "HP9000/320" },
	{ HP_330, "HP9000/330" },
	{ HP_340, "HP9000/340" },
	{ HP_345, "HP9000/345" },
	{ HP_350, "HP9000/350" },
	{ HP_360, "HP9000/360" },
	{ HP_370, "HP9000/370" },
	{ HP_375, "HP9000/375" },
	{ HP_380, "HP9000/380" },
	{ HP_385, "HP9000/385" },
	{ HP_400, "HP9000/400" },
	{ HP_425T, "HP9000/425T" },
	{ HP_425S, "HP9000/425S" },
	{ HP_425E, "HP9000/425E" },
	{ HP_433T, "HP9000/433T" },
	{ HP_433S, "HP9000/433S" },
	{ 0, NULL },
};

static const struct record_def hp300_records[] = {
	{ BI_HP300_MODEL,	"model",	TYPE_BE32,	hp300_models },
	{ BI_HP300_UART_SCODE,	"uart_scode",	TYPE_BE32 },
	{ BI_HP300_UART_ADDR,	"uart_addr",	TYPE_BE32 },
	{ BI_LAST,		"last",		TYPE_UNKNOWN },
};

static const struct map mac_models[] = {
	{ MAC_MODEL_II, "Mac II" },
	{ MAC_MODEL_IIX, "Mac IIX" },
	{ MAC_MODEL_IICX, "Mac IICX" },
	{ MAC_MODEL_SE30, "Mac SE30" },
	{ MAC_MODEL_IICI, "Mac IICI" },
	{ MAC_MODEL_IIFX, "Mac IIFX" },
	{ MAC_MODEL_IISI, "Mac IISI" },
	{ MAC_MODEL_LC, "Mac LC" },
	{ MAC_MODEL_Q900, "Mac Q900" },
	{ MAC_MODEL_PB170, "Mac PB170" },
	{ MAC_MODEL_Q700, "Mac Q700" },
	{ MAC_MODEL_CLII, "Mac CLII" },
	{ MAC_MODEL_PB140, "Mac PB140" },
	{ MAC_MODEL_Q950, "Mac Q950" },
	{ MAC_MODEL_LCIII, "Mac LCIII" },
	{ MAC_MODEL_PB210, "Mac PB210" },
	{ MAC_MODEL_C650, "Mac C650" },
	{ MAC_MODEL_PB230, "Mac PB230" },
	{ MAC_MODEL_PB180, "Mac PB180" },
	{ MAC_MODEL_PB160, "Mac PB160" },
	{ MAC_MODEL_Q800, "Mac Q800" },
	{ MAC_MODEL_Q650, "Mac Q650" },
	{ MAC_MODEL_LCII, "Mac LCII" },
	{ MAC_MODEL_PB250, "Mac PB250" },
	{ MAC_MODEL_IIVI, "Mac IIVI" },
	{ MAC_MODEL_P600, "Mac P600" },
	{ MAC_MODEL_IIVX, "Mac IIVX" },
	{ MAC_MODEL_CCL, "Mac CCL" },
	{ MAC_MODEL_PB165C, "Mac PB165C" },
	{ MAC_MODEL_C610, "Mac C610" },
	{ MAC_MODEL_Q610, "Mac Q610" },
	{ MAC_MODEL_PB145, "Mac PB145" },
	{ MAC_MODEL_P520, "Mac P520" },
	{ MAC_MODEL_C660, "Mac C660" },
	{ MAC_MODEL_P460, "Mac P460" },
	{ MAC_MODEL_PB180C, "Mac PB180C" },
	{ MAC_MODEL_PB520, "Mac PB520" },
	{ MAC_MODEL_PB270C, "Mac PB270C" },
	{ MAC_MODEL_Q840, "Mac Q840" },
	{ MAC_MODEL_P550, "Mac P550" },
	{ MAC_MODEL_CCLII, "Mac CCLII" },
	{ MAC_MODEL_PB165, "Mac PB165" },
	{ MAC_MODEL_PB190, "Mac PB190" },
	{ MAC_MODEL_TV, "Mac TV" },
	{ MAC_MODEL_P475, "Mac P475" },
	{ MAC_MODEL_P475F, "Mac P475F" },
	{ MAC_MODEL_P575, "Mac P575" },
	{ MAC_MODEL_Q605, "Mac Q605" },
	{ MAC_MODEL_Q605_ACC, "Mac Q605_ACC" },
	{ MAC_MODEL_Q630, "Mac Q630" },
	{ MAC_MODEL_P588, "Mac P588" },
	{ MAC_MODEL_PB280, "Mac PB280" },
	{ MAC_MODEL_PB280C, "Mac PB280C" },
	{ MAC_MODEL_PB150, "Mac PB150" },
	{ 0, NULL },
};

static const struct record_def mac_records[] = {
	{ BI_MAC_MODEL,		"model",	TYPE_BE32,	mac_models },
	{ BI_MAC_VADDR,		"vaddr",	TYPE_BE32 },
	{ BI_MAC_VDEPTH,	"vdepth",	TYPE_BE32 },
	{ BI_MAC_VROW,		"vrow",		TYPE_BE32 },
	{ BI_MAC_VDIM,		"vdim",		TYPE_BE32 },
	{ BI_MAC_VLOGICAL,	"vlogical",	TYPE_BE32 },
	{ BI_MAC_SCCBASE,	"sccbase",	TYPE_BE32 },
	{ BI_MAC_BTIME,		"btime",	TYPE_BE32 },
	{ BI_MAC_GMTBIAS,	"gmtbias",	TYPE_BE32 },
	{ BI_MAC_MEMSIZE,	"memsize",	TYPE_BE32 },
	{ BI_MAC_CPUID,		"cpuid",	TYPE_BE32 },
	{ BI_MAC_ROMBASE,	"rombase",	TYPE_BE32 },
	{ BI_MAC_VIA1BASE,	"via1base",	TYPE_BE32 },
	{ BI_MAC_VIA2BASE,	"via2base",	TYPE_BE32 },
	{ BI_MAC_VIA2TYPE,	"via2type",	TYPE_BE32 },
	{ BI_MAC_ADBTYPE,	"adbtype",	TYPE_BE32 },
	{ BI_MAC_ASCBASE,	"ascbase",	TYPE_BE32 },
	{ BI_MAC_SCSI5380,	"scsi5380",	TYPE_BE32 },
	{ BI_MAC_SCSIDMA,	"scsidma",	TYPE_BE32 },
	{ BI_MAC_SCSI5396,	"scsi5396",	TYPE_BE32 },
	{ BI_MAC_IDETYPE,	"idetype",	TYPE_BE32 },
	{ BI_MAC_IDEBASE,	"idebase",	TYPE_BE32 },
	{ BI_MAC_NUBUS,		"nubus",	TYPE_BE32 },
	{ BI_MAC_SLOTMASK,	"slotmask",	TYPE_BE32 },
	{ BI_MAC_SCCTYPE,	"scctype",	TYPE_BE32 },
	{ BI_MAC_ETHTYPE,	"ethtype",	TYPE_BE32 },
	{ BI_MAC_ETHBASE,	"ethbase",	TYPE_BE32 },
	{ BI_MAC_PMU,		"pmu",		TYPE_BE32 },
	{ BI_MAC_IOP_SWIM,	"iop_swim",	TYPE_BE32 },
	{ BI_MAC_IOP_ADB,	"iop_adb",	TYPE_BE32 },
	{ BI_LAST,		"last",		TYPE_UNKNOWN },
};

static const struct map vme_types[] = {
	{ VME_TYPE_TP34V, "TP34V" },
	{ VME_TYPE_MVME147, "MVME147" },
	{ VME_TYPE_MVME162, "MVME162" },
	{ VME_TYPE_MVME166, "MVME166" },
	{ VME_TYPE_MVME167, "MVME167" },
	{ VME_TYPE_MVME172, "MVME172" },
	{ VME_TYPE_MVME177, "MVME177" },
	{ VME_TYPE_BVME4000, "BVME4000" },
	{ VME_TYPE_BVME6000, "BVME6000" },
	{ 0, NULL },
};

static const struct record_def vme_records[] = {
	{ BI_VME_TYPE,		"type",		TYPE_BE32,	vme_types },
	{ BI_VME_BRDINFO,	"brdinfo",	TYPE_VME_BOARDINFO },
	{ BI_LAST,		"last",		TYPE_UNKNOWN },
};

static const struct machine {
	uint32_t type;
	const char *name;
	const struct record_def *records;
} machtypes[] = {
	{ MACH_AMIGA,		"amiga",	amiga_records },
	{ MACH_ATARI,		"atari",	atari_records },
	{ MACH_MAC,		"mac",		mac_records },
	{ MACH_APOLLO,		"apollo",	apollo_records },
	{ MACH_SUN3,		"sun3",		NULL },
	{ MACH_MVME147,		"mvme147",	NULL },
	{ MACH_MVME16x,		"mvme16X",	NULL },
	{ MACH_BVME6000,	"bvme6000",	NULL },
	{ MACH_HP300,		"hp300",	hp300_records },
	{ MACH_Q40,		"q40",		NULL },
	{ MACH_SUN3X,		"sun3x",	NULL },
	{ MACH_M54XX,		"m54xx",	NULL },
};


static uint32_t mach_type;
static const char *mach_name = "0";
static const struct record_def *mach_records;


static int read_bytes(int file, void *buf, size_t n)
{
	ssize_t nread = read(file, buf, n);
	if (nread < 0) {
		perror("Cannot read bootinfo");
		return nread;
	}

	if (nread < n) {
		fprintf(stderr, "Unexpected end of file\n");
		return -1;
	}

	return 0;
}

static uint16_t get_be16(const void *buf)
{
	const uint8_t *p = buf;
	return p[0] << 8 | p[1];
}

static uint32_t get_be32(const void *buf)
{
	const uint8_t *p = buf;
	return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

static int parse_record(struct record *record)
{
	size_t i;
	const struct record_def *records;
	const struct map *map;

	if (record->tag == BI_MACHTYPE) {
		mach_type = get_be32(record->data);
		for (i = 0; i < ARRAY_SIZE(machtypes); i++)
			if (mach_type == machtypes[i].type) {
				mach_name = machtypes[i].name;
				mach_records = machtypes[i].records;
				record->desc = mach_name;
				break;
			}
	}

	if (record->tag & 0x8000)
		records = mach_records;
	else
		records = m68k_records;

	if (!records)
		return 0;

	for (i = 0; records[i].tag != BI_LAST; i++)
		if (record->tag == records[i].tag) {
			strlcat(record->name, records[i].name,
				sizeof(record->name));
			record->type = records[i].type;
			for (map = records[i].map; map && map->val; map++)
				if (map->key == get_be32(record->data)) {
					record->desc = map->val;
					break;
				}
			return 1;
		}

	return 0;
}

static struct record *read_next_record(int file)
{
	char buf[2];
	int error;
	unsigned int tag, rawsize, size;
	struct record *record;

	error = read_bytes(file, buf, sizeof(buf));
	if (error)
		return NULL;

	tag = get_be16(buf);
	if (tag == BI_LAST)
		return NULL;

	error = read_bytes(file, buf, sizeof(buf));
	if (error)
		return NULL;

	rawsize = get_be16(buf);
	if (rawsize < 4 || rawsize % 4) {
		fprintf(stderr, "Invalid size %u for tag 0x%04x\n", rawsize,
			tag);
		exit(-1);
	}

	size = rawsize - 4;
	record = malloc(sizeof(*record) + size);
	if (!record) {
		perror("No memory for record");
		exit(-1);
	}

	record->tag = tag;
	record->size = size;
	record->type = TYPE_UNKNOWN;

	if (!(tag & 0x8000)) {
		record->name[0] = '\0';
	} else if (mach_name) {
		strlcpy(record->name, mach_name, sizeof(record->name));
		strlcat(record->name, ".", sizeof(record->name));
	} else {
		record->name[0] = '\0';
	}

	record->desc = NULL;

	if (size) {
		error = read_bytes(file, record->data, size);
		if (error) {
			free(record);
			return NULL;
		}
	}

	if (!parse_record(record))
		snprintf(record->name, sizeof(record->name), "0x%04x",
			 record->tag);

	if (record->type == TYPE_STRING) {
		if (!memchr(record->data, '\0', size)) {
			fprintf(stderr, "Unterminated string for tag %s\n",
				record->name);
			exit(-1);
		}
	} else if (size < type_sizes[record->type]) {
		fprintf(stderr, "Unexpected size %u for tag %s\n", rawsize,
			record->name);
		exit(-1);
	}

	return record;
}

static void usage(void) __attribute__ ((noreturn));

static void usage(void)
{
	fprintf(stderr,
		"\n"
		"%s: [options]\n\n"
		"Valid options are:\n"
		"    -f, --file file  Specify bootinfo file\n"
		"                     (default: %s)\n"
		"    -h, --help       Display this usage information\n"
		"\n",
		program_name, opt_bootinfo);
	exit(1);
}


int main(int argc, char *argv[])
{
	const char *slash = strrchr(argv[0], '/');
	program_name = slash ? slash + 1 : argv[0];
	int file;
	struct record *record;

	while (argc > 1) {
		if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
			usage();
		else if (!strcmp(argv[1], "-f") ||
			 !strcmp(argv[1], "--file")) {
			if (argc <= 2)
				usage();
			opt_bootinfo = argv[2];
			argv += 2;
			argc -= 2;
		} else
			usage();
	}

	file = open(opt_bootinfo, O_RDONLY);
	if (file < 0) {
		perror("Cannot open bootinfo for reading");
		exit(-1);
	}

	while ((record = read_next_record(file))) {
		switch (record->type) {
		case TYPE_UNKNOWN:
			printf("%s\n", record->name);
			break;
		case TYPE_U8:
			printf("%s = %u\n", record->name, record->data[0]);
			break;
		case TYPE_BE16:
			printf("%s = %u\n", record->name,
			       get_be16(record->data));
			break;
		case TYPE_BE32:
			if (record->desc)
				printf("%s = %s\n", record->name,
				       record->desc);
			else
				printf("%s = 0x%08x\n", record->name,
				       get_be32(record->data));
			break;
		case TYPE_STRING:
			printf("%s = \"%s\"\n", record->name, record->data);
			break;
		case TYPE_MEM_INFO:
			printf("%s of 0x%08x bytes at 0x%08x\n", record->name,
			       get_be32(record->data + 4),
			       get_be32(record->data));
			break;
		case TYPE_AMIGA_CONFIG_DEV:
			printf("%s board 0x%04x:0x%02x at 0x%08x\n",
			       record->name, get_be16(record->data + 20),
			       record->data[17], get_be32(record->data + 32));
			break;
		case TYPE_VME_BOARDINFO:
			printf("%s\n", record->name);
			break;
		}
		free(record);
	}

	close(file);

	exit(0);
}

