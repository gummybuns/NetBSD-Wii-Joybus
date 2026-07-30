#include <sys/queue.h>

#include <err.h>
#include <errno.h>
#include <paths.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <util.h>

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

#define TEST_FILE_NAME "hello.txt"

/*
 * Looking at other examples the first thing you want to do is create the root
 * node.
 */
static int
puffboy_domount(struct puffs_usermount *pu, struct gba_node *gn)
{
	struct puffs_node *root;
	printf("in domount\n");

	root = puffs_pn_new(pu, gn);
	root->pn_va.va_type = VDIR;
	root->pn_va.va_mode = 0755;
	if (!root) {
		err(1, "failed to create root node");
	}

	gn->pn = root;
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

	struct puffs_node *arg_pn = (struct puffs_node *) arg;
	struct gba_node *pn_gn = (struct gba_node *) pn->pn_data;
	struct gba_node *arg_gn = arg_pn->pn_data;

	printf("addrcmp: comparing %d - %d\n", pn_gn->id, arg_gn->id);
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

static struct gba_node *
get_nth_entry(struct gba_node *gn, int n)
{
	struct gba_node *entry;
	int i;

	i = 0;
	SLIST_FOREACH(entry, &gn->head, entries) {
		printf("in loop for %d\n", i);
		if (i == n) {
			return entry;
		}
		i++;
	}

	return NULL;
}

static int
puffboy_node_readdir(struct puffs_usermount *pu, puffs_cookie_t opc,
		     struct dirent *dent, off_t *readoff, size_t *reslen,
		     const struct puffs_cred *pcr, int *eofflag, off_t *cookies,
		     size_t *ncookies)
{
	struct puffs_node *pn, *pn_nth;
	struct gba_node *gn;

	printf("in readdir\n");
	pn = opc;

	/* my understanding is that ncookies is always initialized to 0 */
	*ncookies = 0;

	/* dont perform for non directories */
	if (pn->pn_va.va_type != VDIR) {
		printf("in node_readdir pn is not a VDIR\n");
		return ENOTDIR;
	}

again:
	if (*readoff == DENT_DOT || *readoff == DENT_DOTDOT) {
		printf("is DENT_DOT / DENT_DOTDOT\n");
		puffs_gendotdent(&dent, pn->pn_va.va_fileid, (int)*readoff, reslen);
		(*readoff)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *readoff);
		printf("finished DENT_DOT logic. running again\n");
		goto again;
	}

	for (;;) {
		printf("getting %dth entry\n", (int)DENT_ADJ(*readoff));
		if (pn->pn_data == NULL) {
			printf("PN_DATA IS NULL\n");
		}
		gn = get_nth_entry(pn->pn_data, (int)DENT_ADJ(*readoff));
		if (!gn) {
			*eofflag = 1;
			break;
		}

		pn_nth = gn->pn;
		if (!puffs_nextdent(&dent, gn->name, pn_nth->pn_va.va_fileid, (uint8_t)puffs_vtype2dt(pn_nth->pn_va.va_type), reslen)) {
			break;
		}

		(*readoff)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *readoff);
	}

	return 0;
}

static int
puffboy_node_getattr(struct puffs_usermount *pu, puffs_cookie_t opc,
		     struct vattr *vap, const struct puffs_cred *pcr)
{
	struct puffs_node *pn;
	struct vattr pn_va;

	printf("In getattr\n");

	pn = opc;

	if (pn == NULL) {
		printf("getattr - pn is NULL\n");
		return ESTALE;
	}

	pn_va = pn->pn_va;

	vap->va_type = pn_va.va_type;
	vap->va_mode = pn_va.va_mode;
	vap->va_nlink = pn_va.va_nlink;
	vap->va_uid = pn_va.va_uid;
	vap->va_gid = pn_va.va_gid;
	vap->va_fsid = pn_va.va_fsid;
	vap->va_fileid = pn_va.va_fileid;
	vap->va_size = pn_va.va_size;
	vap->va_blocksize = pn_va.va_blocksize;
	vap->va_gen = pn_va.va_gen;
	vap->va_flags = pn_va.va_flags;
	vap->va_rdev = pn_va.va_rdev;
	vap->va_bytes = pn_va.va_bytes;
	vap->va_filerev = pn_va.va_filerev;
	vap->va_vaflags = pn_va.va_vaflags;
	vap->va_spare = pn_va.va_spare;
	vap->va_atime.tv_sec = pn_va.va_atime.tv_sec;
	vap->va_atime.tv_nsec = pn_va.va_atime.tv_nsec;
	vap->va_mtime.tv_sec = pn_va.va_mtime.tv_sec;
	vap->va_mtime.tv_nsec = pn_va.va_mtime.tv_nsec;
	vap->va_ctime.tv_sec = pn_va.va_ctime.tv_sec;
	vap->va_ctime.tv_nsec = pn_va.va_ctime.tv_nsec;

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

	setprogname(argv[0]);

	root_node.id = 1;
	root_node.is_dir = 1;
	SLIST_INIT(&root_node.head);

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
	PUFFSOP_SET(pops, puffboy, node, readdir);
	PUFFSOP_SET(pops, puffboy, node, getattr);

	/*
	 * TODO next is to do readdir which should get me basic `ls`
	 * there is an example in NetBSD-src/tests/fs/puffs/h_dtfs
	 * and the helloworld one i did is also decent. i am at a point now
	 * i think what i want to do is have every gba_node have a simple queue
	 * as part of it?
	 * and then i can iterate over each item in the queue somehow...
	 */

	pu = puffs_init(pops, _PATH_PUFFS, "puffboy", NULL, pflags);
	if (pu == NULL) {
		err(1, "puffs_init failed");
	}

	/*
	 * Basic readdir implementation. The root node has some hard coded
	 * entries.
	 * TODO - i need to set the pn attributes
	 */
	struct gba_node file_node;
	file_node.id = 2;
	file_node.is_dir = 0;
	file_node.name = estrndup(TEST_FILE_NAME, strlen(TEST_FILE_NAME));
	file_node.namelen = strlen(file_node.name);
	file_node.pu = pu;
	file_node.pn = puffs_pn_new(pu, &file_node);
	file_node.pn->pn_va.va_type = VREG;
	file_node.pn->pn_va.va_mode = 0755;
	SLIST_INSERT_HEAD(&(root_node.head), &file_node, entries);

	if (puffboy_domount(pu, &root_node) != 0) {
		err(1, "puffboy_domount failed");
	}

	if (puffs_mount(pu, argv[1], mntflags, puffs_getroot(pu)) == -1) {
		err(1, "mount failed");
	}

	if (puffs_mainloop(pu) == -1) {
		err(1, "mainloop failed");
	}

	return 0;
}
