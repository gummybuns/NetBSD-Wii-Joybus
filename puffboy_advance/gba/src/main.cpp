#include "../../../gbashared/lib/LinkCube.hpp"

#include "../../../gbashared/_lib/common.h"
#include "../../../gbashared/_lib/interrupt.h"

#include "../../shared/command.h"

#include <string.h>

#define ENTRIES_MAX 20
#define BLOCKSIZE 1024

extern "C" {
	#include <maxmod.h>
	#include "soundbank.h"
	#include "soundbank_bin.h"
}
struct packet to_packet(uint32_t);
uint32_t to_u32(struct packet);
uint32_t merge_packets(struct packet, struct packet);

/* taken from NetBSD-src/sys/sys/vnode.h */
enum vtype      { VNON, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO, VBAD };

struct entry {
	u32		va_type; /* i think this is supposed to be u32 as well */
	u32		va_mode;
	u32		va_nlink;
	u32		va_uid;
	u32		va_gid;
	u64		va_fsid;
	u32		va_fileid; /* it is normally u64 but i chose u32 */
	u64		va_size;
	u32		va_blocksize;
	u64		va_gen;
	u64		va_flags;
	u64		va_rdev;
	u64		va_bytes;
	u64		va_filerev;
	u64		va_vaflags;
	u64		va_spare;
	char		name[32];
	struct entry	*next;
	struct entry	*child;
};

uint32_t
to_u32(struct packet pk)
{
	return (uint32_t)pk.data & 0xFFFF | (uint32_t)pk.cmd << 16 | (uint32_t)pk.seq << 24;
}

/*
 * Wii sends data over as packets. Each packet corresponds to half of
 * a word. Merge to get the full word
 */
uint32_t merge_packets(struct packet pk1, struct packet pk2)
{
	return ((uint32_t)pk2.data & 0xFFFF) | (((uint32_t)pk1.data) << 16);
}

bool a = true, b = true, l = true;
int ID = 1;
struct entry *ENTRIES[ENTRIES_MAX];

LinkCube* linkCube = new LinkCube();

void init() {
 	Common::initTTE();

  	interrupt_init();
  	interrupt_add(INTR_VBLANK, mmVBlank);
  	interrupt_add(INTR_SERIAL, LINK_CUBE_ISR_SERIAL);
}


static struct entry *
find_by_id(u32 id)
{
	int i;
	struct entry *cur;
	for (i = 0; i < ENTRIES_MAX; i++) {
		cur = ENTRIES[i];
		if (cur && cur->va_fileid == id) {
			return cur;
		}
	}

	return NULL;
}

static int
handle_lookup_request()
{
	struct lookup_req req;
	struct lookup_resp resp;
	struct entry *parent, *ent, *match;

	receive_response(linkCube, &req, sizeof(struct lookup_req));
	SSWAP32(&req, parent_fileid);
	match = NULL;

	parent = find_by_id(req.parent_fileid);
	if (!parent || !parent->child) {
		goto send_response;
	}

	ent = parent->child;
	do {
		if (strcmp(ent->name, req.name) == 0) {
			match = ent;
			break;
		}	
	} while (ent->next);
send_response:
	resp.exists = match != NULL;
	if (match) {
		resp.exists = 1;
		resp.va_fileid = match->va_fileid;
		resp.va_type = match->va_type;
		resp.va_size = match->va_size;
		resp.va_rdev = match->va_rdev;
	}
	send_request(linkCube, &resp, sizeof(struct lookup_resp));
	return 0;
}


static int
handle_getattr_request()
{
	struct getattr_req req;
	struct getattr_resp resp;
	struct entry *ent;

	receive_response(linkCube, &req, sizeof(struct getattr_req));
	SSWAP32(&req, fileid);

	ent = find_by_id(req.fileid);

	resp.exists = ent != NULL;
	if (resp.exists) {
		resp.va_type = ent->va_type;
		resp.va_mode = ent->va_mode;
		resp.va_nlink = ent->va_nlink;
		resp.va_uid = ent->va_uid;
		resp.va_gid = ent->va_gid;
		resp.va_gen = ent->va_gen;
		resp.va_fsid = ent->va_fsid;
		resp.va_fileid = ent->va_fileid;
		resp.va_size = ent->va_size;
		resp.va_flags = ent->va_flags;
		resp.va_rdev = ent->va_rdev;
		resp.va_bytes = ent->va_bytes;
		resp.va_filerev = ent->va_filerev;
		resp.va_vaflags = ent->va_vaflags;
		resp.va_spare = ent->va_spare;
	}
	send_request(linkCube, &resp, sizeof(struct getattr_resp));
	return 0;
}

static int
handle_readdir_request()
{
	int i;
	struct readdir_req req;
	struct readdir_resp resp;
	struct entry *parent, *res;

	parent = NULL;
	res = NULL;
	resp.exists = 0;

	receive_response(linkCube, &req, sizeof(struct readdir_req));
	SSWAP32(&req, parent_fileid);
	SSWAP32(&req, n);

	parent = find_by_id(req.parent_fileid);

	if (!parent) {
		goto send_response;	
	}

	i = 0;
	res = parent->child;
	while (res && i < (int)req.n) {
		res = res->next;
		i++;
	}
send_response:
	resp.exists = res != NULL;
	if (res != NULL) {
		resp.exists = 1;
		resp.va_fileid = res->va_fileid;
		resp.va_type = res->va_type;
		strcpy(resp.name, res->name);
	} else {
		resp.exists = 0;
		resp.va_fileid = 0;
		resp.va_type = 0;
	}
	send_request(linkCube, &resp, sizeof(struct readdir_resp));
	return 0;
}

static void
entry_init(struct entry *ent, enum vtype type)
{
	int i;

	ent->va_fileid = ID++;
	ent->va_type = type;
	ent->next = NULL;
	ent->child = NULL;
	if (type == VDIR) {
		ent->va_mode = 0777;
		ent->va_nlink = 1; /* n + 1 after adding dent */

	} else {
		ent->va_mode = 0666;
		ent->va_nlink = 0; /* n + 1 */
	}
	ent->va_uid = 0;
	ent->va_gid = 0;
	ent->va_size = 0;
	ent->va_blocksize = BLOCKSIZE;
	ent->va_gen = (u64)rand();
	ent->va_rdev = 0;
	ent->va_bytes = 0;
	ent->va_filerev = 1;
	ent->va_vaflags = 0;

	for (i = 0; i < ENTRIES_MAX; i++) {
		if (!ENTRIES[i]) {
			ENTRIES[i] = ent;
			break;
		}
	}
}

static void
entry_append(struct entry *parent, struct entry *ent)
{
	struct entry *n;

	if (parent->child == NULL) {
		parent->child = ent;
		return;
	}

	n = parent->child;
	while (n->next) {
		n = n->next;
	}
	n->next = ent;
}

int main()
{
	init();

	linkCube->activate();
	mmInitDefault( (mm_addr)soundbank_bin, 8 );

	u32 recv;
	struct entry root, test;

	for (int i = 0; i < ENTRIES_MAX; i++) {
		ENTRIES[i] = NULL;
	}
	entry_init(&root, VDIR);
	
	entry_init(&test, VREG);
	entry_append(&root, &test);
	snprintf(test.name, sizeof(test.name), "hello.txt");

	mm_sound_effect ding {
		{ SFX_DING },
		1024,
		0,
		255,
		0
	};

	tte_write("Waiting for messages\n");
	while (true) {
		while (linkCube->canRead()) {
			recv = BSWAP32(linkCube->read());
			switch (recv) {
			case CMD_READDIR:
				handle_readdir_request();
				mmEffectEx(&ding);
				break;
			case CMD_LOOKUP:
				handle_lookup_request();
				mmEffectEx(&ding);
				break;
			case CMD_GETATTR:
				handle_getattr_request();
				mmEffectEx(&ding);
				break;
			default:
				tte_write("unknown command");
			}
		}

		// Clear
		if (Common::didPress(KEY_B, b)) {
			tte_erase_screen();
			tte_set_pos(0, 0);
			tte_write("Waiting for messages\n");
		}

		VBlankIntrWait();
		mmFrame();
	}

	return 0;
}
