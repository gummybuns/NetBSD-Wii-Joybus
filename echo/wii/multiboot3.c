#include <sys/ioctl.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define SI_TRANS_DELAY 	50

struct rom {
	const char 	*path;
	unsigned char 	*buf;
	long		size;
};

struct gba_write {
	uint32_t *status;
	uint8_t *in;
	uint32_t out;
	long delay;
};

struct gba_read {
	uint32_t *status;
	uint32_t *in;
	long delay;
};

struct gba_multiboot {
        long    size;
        void    *rom;
};


#define GBA_SEND     	_IOWR(0, 1, struct gba_send)
#define GBA_MULTIBOOT	_IOWR(0, 2, struct gba_multiboot)
#define GBA_WRITE       _IOWR(0, 3, struct gba_write)
#define GBA_READ        _IOWR(0, 4, struct gba_read)

static const char * gba_device = "/dev/gba0";
static const char * gba_file = "gba.mb.gba";

int fd;
uint8_t gba_out[5];
uint8_t gba_in[4];
uint32_t status;

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

static void
wait_clear(long delay)
{
	uint32_t v;
	struct gba_read gbr;
	for (;;) {
		gbr.status = &status;
		gbr.in = &v;
		gbr.delay = SI_TRANS_DELAY;
		ioctl(fd, GBA_READ, &gbr);
		//status = gbr.status;
		if (v == 0) break;
	}
}

int
main(int argc, char *argv[])
{
	int res, count, i;
	struct rom rom;
	struct gba_multiboot gbm;
	struct gba_write gbw;
	struct gba_read gbr;
	char msg[127];
	char inmsg[127];
	uint16_t msg_len;
	uint8_t write_in;
	uint32_t read_in;

	gbw.delay = SI_TRANS_DELAY;
	gbr.delay = SI_TRANS_DELAY;
	gbr.status = &status;
	gbw.status = &status;

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
		printf("\nENTER A MESSAGE:\n");
		memset(msg, 0, sizeof(msg));
		memset(inmsg, 0, sizeof(inmsg));
		fgets(msg, sizeof(msg), stdin);
		msg_len = strlen(msg);
		uint32_t sendsize = ((msg_len+3)&~3);	

		wait_clear(SI_TRANS_DELAY);

		for (int i = 0; i < sendsize; i += 4) {
			char *p = msg + i;
			gbw.out = bswap32(*(uint32_t *)p);
			gbw.in = &write_in;
			ioctl(fd, GBA_WRITE, &gbw);
		}
		printf("\nSENT:\n%s", msg);

		printf("\nRECEIVED:\n");
		i = 0;
		int count = 0;
		while (i < sendsize) {
			count++;
			gbr.in = &read_in;
			ioctl(fd, GBA_READ, &gbr);
			if (read_in != 0) {
				char *p = inmsg + i;
				((uint32_t *)p)[0] = bswap32(read_in);
				i += 4;
			}
		}

		printf("%s\n", inmsg);
	}
cleanup:
	free(rom.buf);
done:
	close(fd);
	return res;
}
