#include <sys/ioctl.h>

#include <err.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "../../wiishared/lib/multiboot.h"


#define DEFAULT_PATH "../gba/gba_mb.gba"

struct thread_data {
	int fd;
	struct rom *rom;
};

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

static void*
multiboot_worker(void *arg)
{
	struct thread_data *data = (struct thread_data *)arg;
	multiboot(data->fd, data->rom);
}

int
main(int argc, char *argv[])
{
	const char *gba_file;
	pthread_t threads[4];
	struct thread_data t_data[4];
	char *devices[4];
	int fd[4];
	int device_count = 0;
	struct rom rom;
	int ch;

	gba_file = DEFAULT_PATH;

	while (
	    (ch = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (ch) {
		case 'd':
			if (device_count < 4) {
				devices[device_count] = optarg;
				device_count++;
			} else {
				errx(1, "too many devices\n");
			}
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

	rom.path = gba_file;
	if (read_rom(&rom) != 0) {
		errx(1, "failed to read rom");
	}
	for (int i = 0; i < device_count; i++) {
		fd[i] = open(devices[i], O_RDWR);
		if (fd[i] == -1) {
			errx(1, "cant open device %s", devices[i]);
		}
	}

	for (int i = 0; i < device_count; i++) {
		int err;
		t_data[i].fd = fd[i];
		t_data[i].rom = &rom;
		err = pthread_create(&threads[i], NULL, multiboot_worker, &t_data[i]);
		if (err != 0) {
			errx(1, "failed to create thread\n");
		}
	}

	for (int i = 0; i < device_count; i++) {
		pthread_join(threads[i], NULL);
	}

	return 0;
}
