#include <sys/ioctl.h>

#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "../../wiishared/lib/multiboot.h"
#include "../../wiishared/lib/gcport_ioctl.h"

#define DEFAULT_PATH 	"../gba/gba.mb.gba"
#define DELAY		50
#define READY_TIMEOUT	100000
#define MSG_TIMEOUT	5000

uint32_t status;
int fd;

static const char *shortopts = "d:g::";
static struct option longopts[] = {
	{	"device",	required_argument,	NULL,	'd'	},
	{	"gba-file",	optional_argument,	NULL,	'g'	},
};

static void
usage(void)
{
	fputs("echo\t[-d device] [-g game]\n", stderr);
	exit(1);
}

static void
wait_clear(long delay, long timeout)
{
	uint32_t v;
	long count = 0;
	for (;;) {
		v = gba_read(fd, &status, DELAY);
		if (v == 0) break;
		if (count > timeout) {
			errx(1, "gba failed to get ready\n");
		}
		count++;
	}
}

int
main(int argc, char *argv[])
{
	int res, count, ch, i;
	struct rom rom;
	char msg[127];
	char inmsg[127];
	char *p;
	const char *device, *gba_file;
	uint16_t msg_len;
	uint8_t write_in;
	uint32_t read_in;

	gba_file = NULL;

	while (
	    (ch = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (ch) {
		case 'd':
			device = optarg;
			break;
		case 'g':
			if (optarg == NULL && argv[optind] != NULL &&
			    argv[optind][0] != '-') {
				gba_file = argv[optind];
				++optind;
			} else {
				gba_file = optarg == NULL
				    ? DEFAULT_PATH : optarg;
			}
			break;
		default:
			usage();
		}
	}

	fd = open(device, O_RDWR);
	if (fd == -1) {
		errx(1, "can't open device");
	}

	if (gba_file != NULL) {
		rom.path = gba_file;
		if ((res = read_rom(&rom)) != 0) {
			errx(1, "failed to read rom");
		}

		if ((res = multiboot(fd, &rom)) != 0) {
			errx(1, "failed to multiboot");
		}
	}

	printf("waiting for gameboy...\n");
	wait_clear(DELAY, READY_TIMEOUT);
	
	for(;;) {
		printf("================\n");
		printf("\nENTER A MESSAGE:\n");
		memset(msg, 0, sizeof(msg));
		memset(inmsg, 0, sizeof(inmsg));
		fgets(msg, sizeof(msg), stdin);
		msg_len = strlen(msg);
		uint32_t sendsize = ((msg_len+3)&~3);	

		wait_clear(DELAY, MSG_TIMEOUT);

		for (i = 0; i < sendsize; i += 4) {
			p = msg + i;
			gba_write(fd, bswap32(*(uint32_t *)p), &status, DELAY);
		}
		printf("\nSENT:\n%s", msg);

		printf("\nRECEIVED:\n");
		i = 0;
		int count = 0;
		while (i < sendsize) {
			count++;
			read_in = gba_read(fd, &status, DELAY);
			if (read_in != 0) {
				p = inmsg + i;
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
