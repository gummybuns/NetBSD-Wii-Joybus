#ifndef _PBA_H_
#define _PBA_H_

#include <sys/queue.h>

#include <puffs.h>

#define DELAY		50
#define READY_TIMEOUT	100000
#define MSG_TIMEOUT	5000

struct entry {
	uint32_t id;
	struct puffs_node *pn;
	SLIST_ENTRY(entry) entries;
};

struct pba_context {
	int fd;		/* for gba ioctl */
	SLIST_HEAD(, entry) head;
};

struct gba_node {
	uint32_t id;
	unsigned char is_dir;
	struct puffs_node *pn;
	struct puffs_usermount *pu;
	char *name;
	size_t namelen;
	SLIST_ENTRY(gba_node) entries;
	SLIST_HEAD(, gba_node) head;
};

struct puffs_node *pba_cmap(struct puffs_usermount *, puffs_cookie_t);
puffs_cookie_t fileid_to_cookie(uint32_t);
uint32_t cookie_to_fileid(puffs_cookie_t);
void puffboy_baseattrs(struct vattr *, enum vtype, ino_t);
void wait_for(int, uint32_t, long, long);
void wait_clear(int, long, long);


PUFFSOP_PROTOS(puffboy);
#endif
