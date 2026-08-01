#include <sys/queue.h>

#include <err.h>
#include <errno.h>
#include <paths.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <util.h>

#include "pba.h"

#define ROOT_NAME "root"
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
	root_node.name = estrndup(ROOT_NAME, strlen(ROOT_NAME));
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
	PUFFSOP_SET(pops, puffs_genfs, node, getattr);
	// TODO - need to implement pathconf to get ls -l to work

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
	// TODO - change the 2 for a global counter
	file_node.id = 2;
	file_node.is_dir = 0;
	file_node.name = estrndup(TEST_FILE_NAME, strlen(TEST_FILE_NAME));
	file_node.namelen = strlen(file_node.name);
	file_node.pu = pu;
	file_node.pn = puffs_pn_new(pu, &file_node);
	file_node.pn->pn_va.va_type = VREG;
	file_node.pn->pn_va.va_mode = 0755;
	puffboy_baseattrs(&file_node.pn->pn_va, VREG, file_node.id);
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
