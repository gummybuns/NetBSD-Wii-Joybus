#include <sys/ioctl.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define SI_TRANS_DELAY 	50
#define GBA_WRITE 0x15
#define GBA_READ 0x14

int fd;
uint32_t status;

struct rom {
	const char 	*path;
	unsigned char 	*buf;
	long		size;
};

struct gba_send {
       uint32_t        insize;         /* number of bytes for in buffer */
       uint32_t        outsize;        /* number of bytes for out buffer */
       uint32_t        *status;         /* status from sisr */
       void            *in;            /* buffer to store response */
       void            *out;           /* buffer to send out to ext device */
       long		delay;
};


#define GBA_SEND     	_IOWR(0, 1, struct gba_send)

static const char * gba_device = "/dev/gba0";
static const char * gba_file = "../gba/gba.mb.gba";

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

static uint32_t
gba_write32(uint32_t val, long delay)
{
       struct gba_send gbs;
       uint8_t out[5];
       uint8_t in[1];
       uint8_t *p;

       p = out + 1;
       out[0] = GBA_WRITE;
       ((uint32_t *)p)[0] = val;
       gbs.in = in;
       gbs.out = out;
       gbs.insize = 1;
       gbs.outsize = 5;
       gbs.delay = delay;
       gbs.status = &status;
       ioctl(fd, GBA_SEND, &gbs);
       return (uint32_t)(in[0]);
}

static uint32_t
gba_read32(long delay)
{
       struct gba_send gbs;
       uint8_t out[1];
       uint8_t in[5];
       uint8_t *p;

       out[0] = GBA_READ;
       gbs.in = in;
       gbs.out = out;
       gbs.insize = 5;
       gbs.outsize = 1;
       gbs.delay = delay;
       gbs.status = &status;
       ioctl(fd, GBA_SEND, &gbs);

       /* first four bytes are the value. last byte is the status */
       return *(uint32_t *)in;
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

static inline uint32_t
jb_reset()
{
	struct gba_send gbs;
	uint32_t out[1];
	uint32_t in[1];
	int err;
	out[0] = 0xFF000000;

	gbs.outsize = 1;
	gbs.insize = 3;
	gbs.in = in;
	gbs.out = out;
	gbs.delay = SI_TRANS_DELAY;
	gbs.status = &status;

	err = ioctl(fd, GBA_SEND, &gbs);
	printf("reset: 0x%08X - err %d\n", in[0], err);
	return in[0];
}

static inline uint32_t
jb_identify()
{
	struct gba_send gbs;
	uint32_t out[1];
	uint32_t in[1];
	out[0] = 0x00000000;

	gbs.outsize = 1;
	gbs.insize = 3;
	gbs.in = in;
	gbs.out = out;
	gbs.delay = SI_TRANS_DELAY;
	gbs.status = &status;

	ioctl(fd, GBA_SEND, &gbs);
	printf("status: 0x%08X\n", in[0]);
	return in[0];
}

static int
do_multiboot(struct rom* r)
{
	uint32_t enc, sessionkeyraw, sessionkey, res;
	int count, i;
	unsigned int fcrc, ourkey, sendsize;
	unsigned char *rom = r->buf;
	long size = r->size;

	count = 0;
	for (;;) {
		if (count >= 1000) {
			printf("multiboot: initialize failure\n");
			return 1;
		}

		jb_reset();
		res = jb_identify();
		if (res & 0x00001000) {
			break;
		}
		count++;
	}

	sendsize = ((size+7)&~7);
	ourkey = calckey(sendsize);

	sessionkeyraw = gba_read32(SI_TRANS_DELAY);
	sessionkey = bswap32(sessionkeyraw^0x7365646F);
	gba_write32(ourkey, SI_TRANS_DELAY);
	fcrc = 0x15A0;

	printf("multiboot: sending header\n");
	for (i = 0; i < 0xC0; i += 4) {
		gba_write32(*(uint32_t*)(rom+i), SI_TRANS_DELAY);
	}

	printf("multiboot: sending rom\n");
	for (i = 0xC0; i < sendsize; i += 4) {
		enc = ((rom[i+3]<<24)|(rom[i+2]<<16)|(rom[i+1]<<8)|(rom[i]));
		fcrc = docrc(fcrc, enc);
		sessionkey = (sessionkey*0x6177614B)+1;
		enc^=sessionkey;
		enc^=((~(i+(0x20<<20)))+1);
		enc^=0x20796220;
		gba_write32(bswap32(enc), SI_TRANS_DELAY);
	}
	printf("multiboot: finished sending rom\n");

	fcrc |= (sendsize<<16);
	sessionkey = (sessionkey*0x6177614B)+1;
	fcrc^=sessionkey;
	fcrc^=((~(i+(0x20<<20)))+1);
	fcrc^=0x20796220;

	gba_write32(bswap32(fcrc), SI_TRANS_DELAY);
	gba_read32(SI_TRANS_DELAY);
	return 0;
}


int
main(int argc, char *argv[])
{
	struct rom rom;
	fd = open(gba_device, O_RDWR);
	if (fd == -1) {
		perror("cant open device");
		return 1;
	}

	rom.path = gba_file;
	if (read_rom(&rom) != 0) {
		perror("failed to read rom");
		return -1;
	}

	return do_multiboot(&rom);
}
