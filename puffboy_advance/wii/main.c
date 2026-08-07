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
 * GBA Communication
 *
 * I can only send 4 bytes over at a time
 * But before I send the actual payload over I need to send a single uint32_t
 * that tells the gameboy what type of command to receive.
 *
 * i think what we want is to have something like
 *
 * 1. Gameboy is ready to receive a command
 * 2. Wii sends cmd request.
 * 3. Wii waits for the cmd to echo back.
 * 4. Gameboy receives the request
 * 5. Gameboy mallocs space for the exact payload based on the type of request
 * 6. Gameboy sends the exact request back
 * 7. Gameboy sends the response
 * 8. Wii sees the cmd echod back
 * 9. Wii reads data back until nothing comes back
 */



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
	struct pba_context ctx;

	setprogname(argv[0]);

	ctx.fd = open("/dev/gcport0", O_RDWR);
	if (ctx.fd == -1) {
		errx(1, "can't open gcport device");
	}
	wait_clear(ctx.fd, DELAY, READY_TIMEOUT);

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
	PUFFSOP_SET(pops, puffboy, node, pathconf);
	PUFFSOP_SET(pops, puffs_genfs, node, getattr);

	pu = puffs_init(pops, _PATH_PUFFS, "puffboy", &ctx, pflags);
	if (pu == NULL) {
		err(1, "puffs_init failed");
	}

	// TODO - change the 2 for a global counter
	// i think at this point it is time to start working on the gba side
	// of things. i think readdir and node lookup can be the exact same.
	// what changes now is how the file_node(s) are generated. there should
	// be some initial handshake before any mounting logic. at that point
	// the gba should tell the program all of the file nodes that exist and
	// their meta data. then any writes / deletes are written back to the
	// gameboy, and all reads can be requested by some id
	/*
	struct gba_node file_node;
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
	*/

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
