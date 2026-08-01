#include <sys/queue.h>

#include <puffs.h>

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

void puffboy_baseattrs(struct vattr *, enum vtype, ino_t);

PUFFSOP_PROTOS(puffboy);
