#include <sys/ioctl.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "../../wiishared/ioctl.h"

#define SI_TRANS_DELAY 	50
#define GBA_READ	0x14
#define GBA_WRITE	0x15

struct rom {
	const char 	*path;
	unsigned char 	*buf;
	long		size;
};

static const char * gba_device = "/dev/gba0";
static const char * gba_file = "gba.mb.gba";

int fd;
uint8_t gba_out[5];
uint8_t gba_in[4];

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
gba_write(uint32_t val, long delay)
{
	struct gba_send gbs;
	uint8_t out[5];
	uint8_t in[1];
	uint8_t *p;

	p = out + 1;
	out[0] = GBA_WRITE;
	((uint32_t *)p)[0] = bswap32(val);
	gbs.in = in;
	gbs.out = out;
	gbs.insize = 1;
	gbs.outsize = 5;
	usleep(delay);
	ioctl(fd, GBA_SEND, &gbs);
	return (uint32_t)(in[0]);
}

static uint32_t
gba_read(long delay)
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
	usleep(delay);
	ioctl(fd, GBA_SEND, &gbs);

	/* first route bytes are the value. last byte is the status */
	return bswap32(*(uint32_t *)in);
}

static void
wait_clear(long delay)
{
	uint32_t v;
	for (;;) {
		v = gba_read(delay);
		if (v == 0) break;
	}
}

int
main(int argc, char *argv[])
{
	int res, count, i;
	struct rom rom;
	struct gba_multiboot gbm;
	struct gba_send gbs;
	char msg[127];
	char inmsg[127];
	uint8_t out[128];
	uint16_t msg_len;
	uint32_t in[1];

	fd = open(gba_device, O_RDWR);
	if (fd == -1) {
		perror("cant open device");
		return 1;
	}

	rom.path = gba_file;
	if (read_rom(&rom) != 0) {
		perror("failed to read rom");
		goto done;
	}
	printf("ROM SIZE: %d BYTES\n", rom.size);

	gbm.size = rom.size;
	gbm.rom = rom.buf;
	res = ioctl(fd, GBA_MULTIBOOT, &gbm);
	printf("RES IS %d\n", res);
	if (res != 0) {
		goto done;
	}

	wait_clear(SI_TRANS_DELAY);
	
	for(;;) {
		printf("================\n");
		printf("ENTER A MESSAGE:\n");
		memset(msg, 0, sizeof(msg));
		memset(inmsg, 0, sizeof(inmsg));
		fgets(msg, sizeof(msg), stdin);
		msg_len = strlen(msg);
		uint32_t sendsize = ((msg_len+7)&~7);	

		for (int i = 0; i < sendsize; i += 4) {
			char *p = msg + i;
			gba_write(*(uint32_t *)p, SI_TRANS_DELAY);
		}
		printf("SENT:\n%s", msg);

		wait_clear(SI_TRANS_DELAY);

		for (int i = 0; i < sendsize; i += 4) {
			char *p = inmsg + i;
			((uint32_t *)p)[0] = gba_read(SI_TRANS_DELAY);
		}
		printf("RECEIVED:\n");
		printf("%s\n", inmsg);
	}
cleanup:
	free(rom.buf);
done:
	close(fd);
	return res;
}
