#ifndef _MULTIBOOT_H
#define _MULTIBOOT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "./gcport_ioctl.h"

#define MB_DELAY 	50

struct rom {
	const char 	*path;
	unsigned char 	*buf;
	long		size;
};

static int read_rom(struct rom *);
static unsigned int calckey(unsigned int);
static unsigned int docrc(uint32_t, uint32_t);
static int multiboot(int, struct rom *);


/*
 * Given a rom, open by its path and read the raw bytes
 */
static int
read_rom(struct rom *r)
{
	FILE *file;
	long exact_size;

	file = fopen(r->path, "rb");
	if (file == NULL) {
		return 1;
	}

	fseek(file, 0, SEEK_END);
	exact_size = ftell(file);
	rewind(file);
	r->size = ((exact_size+7) & ~7);
	printf("ROM EXACT SIZE: %d\n", exact_size);
	printf("ROM ROUNDED SIZE: %d\n", r->size);

	r->buf = calloc(1, sizeof(unsigned char) * r->size);
	if (r->buf == NULL) {
		return 1;
	}

	fread(r->buf, 1, r->size, file);
	fclose(file);
	return 0;
}

static unsigned int
calckey(unsigned int size)
{
	unsigned int ret = 0;
	size=(size-0x200) >> 3;
	int res1 = (size&0x3F80) << 1;
	res1 |= (size&0x4000) << 2;
	res1 |= (size&0x7F);
	res1 |= 0x380000;
	int res2 = res1;
	res1 = res2 >> 0x10;
	int res3 = res2 >> 8;
	res3 += res1;
	res3 += res2;
	res3 <<= 24;
	res3 |= res2;
	res3 |= 0x80808080;

	if((res3&0x200) == 0) {
		ret |= (((res3)&0xFF)^0x4B)<<24;
		ret |= (((res3>>8)&0xFF)^0x61)<<16;
		ret |= (((res3>>16)&0xFF)^0x77)<<8;
		ret |= (((res3>>24)&0xFF)^0x61);
	} else {
		ret |= (((res3)&0xFF)^0x73)<<24;
		ret |= (((res3>>8)&0xFF)^0x65)<<16;
		ret |= (((res3>>16)&0xFF)^0x64)<<8;
		ret |= (((res3>>24)&0xFF)^0x6F);
	}
	return ret;
}

static unsigned int
docrc(uint32_t crc, uint32_t val)
{
	int i;
	for(i = 0; i < 0x20; i++)
	{
		if((crc^val)&1)
		{
			crc>>=1;
			crc^=0xa1c1;
		}
		else
			crc>>=1;
		val>>=1;
	}
	return crc;
}

static int multiboot(int fd, struct rom *r)
{
	uint32_t enc, sessionkeyraw, sessionkey, res, status;
	int count, i;
	unsigned int fcrc, ourkey, sendsize;
	unsigned char *rom = r->buf;
	long size = r->size;

	count = 0;
	for (;;) {
		cmd_reset(fd, &status, MB_DELAY);
		res = cmd_identify(fd, &status, MB_DELAY);
		if (res & 0x00001000) {
			break;
		}
		count++;
	}

	sendsize = ((size+7)&~7);
	ourkey = calckey(sendsize);

	sessionkeyraw = gba_read(fd, &status, MB_DELAY);
	sessionkey = bswap32(sessionkeyraw^0x7365646F);
	gba_write(fd, ourkey, &status, MB_DELAY);
	fcrc = 0x15A0;

	printf("multiboot: sending header\n");
	for (i = 0; i < 0xC0; i += 4) {
		gba_write(fd, *(uint32_t*)(rom+i), &status, MB_DELAY);
	}

	printf("multiboot: sending rom\n");
	for (i = 0xC0; i < sendsize; i += 4) {
		enc = ((rom[i+3]<<24)|(rom[i+2]<<16)|(rom[i+1]<<8)|(rom[i]));
		fcrc = docrc(fcrc, enc);
		sessionkey = (sessionkey*0x6177614B)+1;
		enc^=sessionkey;
		enc^=((~(i+(0x20<<20)))+1);
		enc^=0x20796220;
		gba_write(fd, bswap32(enc), &status, MB_DELAY);
	}
	printf("multiboot: finished sending rom\n");

	fcrc |= (sendsize<<16);
	sessionkey = (sessionkey*0x6177614B)+1;
	fcrc^=sessionkey;
	fcrc^=((~(i+(0x20<<20)))+1);
	fcrc^=0x20796220;

	gba_write(fd, bswap32(fcrc), &status, MB_DELAY);
	gba_read(fd, &status, MB_DELAY);
	return 0;

}
#endif
