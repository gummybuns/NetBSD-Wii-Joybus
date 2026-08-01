#include <sys/time.h>

#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <util.h>

#include "pba.h"

void
puffboy_baseattrs(struct vattr *vap, enum vtype type, ino_t id)
{
	struct timeval tv;
	struct timespec ts;

	gettimeofday(&tv, NULL);
	TIMEVAL_TO_TIMESPEC(&tv, &ts);

	vap->va_type = type;
	if (type == VDIR) {
		vap->va_mode = 0777;
		vap->va_nlink = 1;	/* n + 1 after adding dent */
	} else {
		vap->va_mode = 0666;
		vap->va_nlink = 0;	/* n + 1 */
	}
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fileid = id;
	vap->va_size = 0;
	vap->va_blocksize = getpagesize();
	vap->va_gen = (unsigned long)random();
	vap->va_flags = 0;
	vap->va_rdev = (dev_t)PUFFS_VNOVAL;
	vap->va_bytes = 0;
	vap->va_filerev = 1;
	vap->va_vaflags = 0;

	vap->va_atime = vap->va_mtime = vap->va_ctime = vap->va_birthtime = ts;
}
