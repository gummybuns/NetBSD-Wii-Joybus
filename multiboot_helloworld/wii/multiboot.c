#include <sys/ioctl.h>

#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "../../wiishared/lib/multiboot.h"


#define DEFAULT_PATH "../gba/gba.mb.gba"

static const char *shortopts = "d:g::";
static struct option longopts[] = {
	{	"device",	required_argument,	NULL,	'd'	},
	{	"gba-file",	optional_argument,	NULL,	'g'	},
};

static void
usage(void)
{
	fputs("multiboot\t[-d device] [-g game]\n", stderr);
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *device, *gba_file;
	struct rom rom;
	int fd, ch;

	gba_file = DEFAULT_PATH;

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

	rom.path = gba_file;
	if (read_rom(&rom) != 0) {
		errx(1, "failed to read rom");
	}

	return multiboot(fd, &rom);
}
