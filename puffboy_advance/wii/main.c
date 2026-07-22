#include <err.h>
#include <errno.h>
#include <paths.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct gba_node {
	uint32_t id;
	unsigned char is_dir;
};

/*
 * Looking at other examples the first thing you want to do is create the root
 * node.
 */
static int
puffboy_domount(struct puffs_usermount *pu)
{
	struct puffs_node *root;
	printf("in domount\n");

	root = puffs_pn_new(pu, NULL);
	root->pn_va.va_type = VDIR;
	root->pn_va.va_mode = 0755;
	if (!root) {
		err(1, "failed to create root node");
	}

	puffs_setroot(pu, root);
	return 0;
}

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
static int
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

static void *
addrcmp(struct puffs_usermount *pu, struct puffs_node *pn, void *arg)
{

	struct gba_node *pn_gn = (struct gba_node *) pn->pn_data;
	struct gba_node *arg_gn = (struct gba_node *) arg;

	if (pn_gn->id == arg_gn->id) return pn;
	return NULL;
}

static int
puffboy_node_lookup(struct puffs_usermount *pu, puffs_cookie_t opc,
		    struct puffs_newinfo *pni,
		    const struct puffs_cn *pcn)
{
	struct puffs_node *pn;

	printf("in node_lookup for %s\n", pcn->pcn_name);

	/* we are not concerning ourselves with parent directories */
	if (PCNISDOTDOT(pcn)) {
		printf("node_lookup PCN is DOTDOT\n");
		return ENOENT;
	}

	if ((pn = puffs_pn_nodewalk(pu, addrcmp, opc)) == NULL) {
		printf("node_lookup pn is NULL\n");
		return ESTALE;
	}

	printf("node found!\n");
	printf("fileid %ld\n", (long int)pn->pn_va.va_fileid);
	printf("is VREG (%d)? %d\n", pn->pn_va.va_type, pn->pn_va.va_type == VREG);
	puffs_newinfo_setcookie(pni, pn);
	puffs_newinfo_setvtype(pni, pn->pn_va.va_type);
	puffs_newinfo_setsize(pni, (voff_t)pn->pn_va.va_size);
	puffs_newinfo_setrdev(pni, pn->pn_va.va_rdev);
  	return 0;
}

int
main(int argc, char *argv[])
{
	uint32_t pflags;
	int mntflags;
	struct puffs_usermount *pu;
	struct puffs_ops *pops;
	struct gba_node root_node;

	root_node.id = 1;
	root_node.is_dir = 1;

	setprogname(argv[0]);

	pflags = 0;
	mntflags = 0;

	/*
	 * according to puffs (3) all vfs functions are required and only
	 * puffs_node_lookup is required
	 *
	 * i am not sure what the value is of using SETFSNOP vs just not
	 * actually setting it... it see some examples using it so i will as
	 * well until i can figure it out
	 */
	PUFFSOP_INIT(pops);
	PUFFSOP_SET(pops, puffboy, fs, statvfs);
	PUFFSOP_SETFSNOP(pops, sync);

	PUFFSOP_SET(pops, puffboy, node, lookup);

	pu = puffs_init(pops, _PATH_PUFFS, "puffboy", NULL, pflags);
	if (pu == NULL) {
		err(1, "puffs_init failed");
	}

	if (puffboy_domount(pu) != 0) {
		err(1, "puffboy_domount failed");
	}

	if (puffs_mount(pu, argv[1], mntflags, &root_node) == -1) {
		err(1, "mount failed");
	}

	if (puffs_mainloop(pu) == -1) {
		err(1, "mainloop failed");
	}

	return 0;
}
