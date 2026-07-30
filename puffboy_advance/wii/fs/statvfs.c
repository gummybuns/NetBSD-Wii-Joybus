#include  <puffs.h>
#include <stdio.h>
#include <unistd.h>

#include "../pba.h"

/*
 * my understanding is that statvfs is required and it gives high level details
 * about the file system.
 *
 * details taken from the man docs:
 * unsigned long  f_bsize;     file system block size
 * unsigned long  f_frsize;    fundamental file system block size
 * fsblkcnt_t     f_blocks;    number of blocks in file system,
 *                                      (in units of f_frsize)
 *
 * fsblkcnt_t     f_bfree;     free blocks avail in file system
 * fsblkcnt_t     f_bavail;    free blocks avail to non-root
 * fsblkcnt_t     f_bresvd;    blocks reserved for root
 *
 * fsfilcnt_t     f_files;     total file nodes in file system
 * fsfilcnt_t     f_ffree;     free file nodes in file system
 * fsfilcnt_t     f_favail;    free file nodes avail to non-root
 * fsfilcnt_t     f_fresvd;    file nodes reserved for root
 *
 * i will just make some stuff up for now to keep things going. i think this
 * will eventually have to be a command sent to the gameboy so you know how much
 * storage space is free and stuff
 */
int
puffboy_fs_statvfs(struct puffs_usermount *pu, struct puffs_statvfs *sbp)
{
	printf("in statvfs\n");
	sbp->f_bsize = _SC_PAGESIZE;
	sbp->f_frsize = _SC_PAGESIZE;
	sbp->f_blocks = 100;
	sbp->f_bfree = 100;
	sbp->f_bavail = 100;
	sbp->f_bresvd = 0;

	sbp->f_files = 100;
	sbp->f_ffree = 100;
	sbp->f_favail = 100;
	sbp->f_fresvd = 0;

	return 0;
}
