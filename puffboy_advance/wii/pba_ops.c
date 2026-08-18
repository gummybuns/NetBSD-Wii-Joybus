#include <sys/time.h>

#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <util.h>

#include "pba.h"
#include "../../wiishared/lib/gcport_ioctl.h"

/*
 * The cookies used by the implementation follow what was done in pgfs
 * but i dont think that pgfs actually works (or I would be surprised if it did
 * puffs now assumes that the cookie is the puffs_node. We are using the id
 * itself.
 *
 * You need to tell puffs that this by calling puffs_set_cmap(). This function
 * will create a new puffs_node or return an existing one by the id
 *
 * Maybe it would be better to use a puffs_node as the cookie as expected, I
 * just havent figured out when I should be creating these nodes if all of the
 * data lives on the gameboy
 */
struct puffs_node *
pba_cmap(struct puffs_usermount *pu, puffs_cookie_t cookie)
{
	uint32_t id;
	struct entry *ent;
	struct pba_context *ctx;

	ctx = puffs_getspecific(pu);
	id = cookie_to_fileid(cookie);
	printf("IN PBA_CMAP looking for %d\n", id);
	SLIST_FOREACH(ent, &ctx->head, entries) {
		if (id == ent->id) {
			printf("entry found!\n");
			return ent->pn;
		}
	}

	printf("entry not found.. creating\n");
	ent = entry_init(pu, id);
	return ent->pn;
}

struct entry *
entry_init(struct puffs_usermount *pu, uint32_t id)
{
	struct entry *ent;
	struct pba_context *ctx;

	ctx = puffs_getspecific(pu);

	ent = malloc(sizeof(struct entry));
	ent->pn = puffs_pn_new(pu, NULL);
	ent->id = id;
	SLIST_INSERT_HEAD(&(ctx->head), ent, entries);
	return ent;
}


uint32_t
cookie_to_fileid(puffs_cookie_t cookie)
{

	return (uint32_t)(uintptr_t)cookie;
}

puffs_cookie_t
fileid_to_cookie(uint32_t id)
{
	puffs_cookie_t cookie = (puffs_cookie_t)(uintptr_t)id;
	return cookie;
}

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

void wait_for(int fd, uint32_t val, long delay, long timeout)
{
	uint32_t v, status;
	long count = 0;
	for (;;) {
		v = gba_read(fd, &status, DELAY);
		if (v == val) break;
		if (count > timeout) {
			errx(1, "gba failed to get ready\n");
		}
		count++;
	}
}
void
wait_clear(int fd, long delay, long timeout)
{
	wait_for(fd, 0, delay, timeout);
}
