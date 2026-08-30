#include <sys/ioctl.h>

#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../../wiishared/lib/multiboot.h"
#include "../../wiishared/lib/gcport_ioctl.h"

#define DEFAULT_PATH 	"../gba/gba.mb.gba"
#define DEFAULT_DEV	"/dev/gcport0"
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
	int res, ch, i;
	struct rom rom;
	const char *device, *gba_file;
	uint32_t read_in;
	struct timespec before, after;

	device = DEFAULT_DEV;
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
		printf("Press enter to begin\n");
		ch = getchar();

		if (ch != '\n') {
			continue;
		}

		clock_gettime(CLOCK_MONOTONIC, &before);
		gba_write(fd, bswap32(1), &status, DELAY);
		i = 0;
		while (i < 1024) {
			read_in = gba_read(fd, &status, DELAY);
			if (read_in == 0) continue;
			if (read_in) i += 4;
		}
		clock_gettime(CLOCK_MONOTONIC, &after);
		long long elapsed_ms = (after.tv_sec - before.tv_sec) * 1000LL
		    + (after.tv_nsec - before.tv_nsec) / 1000000LL;
		printf("Elapsed time: %lld ms\n", elapsed_ms);
	}
	free(rom.buf);
	close(fd);
	return res;
}
