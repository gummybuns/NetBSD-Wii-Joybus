#include "../../../gbashared/lib/LinkCube.hpp"

#include "../../../gbashared/_lib/common.h"
#include "../../../gbashared/_lib/interrupt.h"

#include "../../shared/command.h"

#include <string.h>

#define ENTRIES_MAX 20

#define BLOCK_MAX 1024
#define PBA_BLOCKSHIFT (7) // CHANGE THIS with BLOCKSIZE 2^x = BLOCKSIZE
#define PBA_BLOCKSIZE  (1<<PBA_BLOCKSHIFT)
#define ROUNDUP(a,b) ((a) & ((b)-1))
#define BLOCKNUM(a,b) (((a) & ~((1<<(b))-1)) >> (b))
#define BLOCKOFF(a,b) ((a) & ((b)-1))
#define BLOCKLEFT(a,b) ((b) - BLOCKOFF(a,b))
#define EBLOCKNOTFOUND 	1
#define ENOBLOCKSFREE	2


/* taken from NetBSD-src/sys/sys/vnode.h */
enum vtype      { VNON, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO, VBAD };

struct gba_file {
	size_t numblocks;
	size_t datalen;
};

/* i hope there is a way to make these properties smaller. do i need a u64
 * for some of these??
 */
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
	struct gba_file	file;
};

struct gba_block {
	uint32_t va_fileid;
	uint32_t idx;
	uint8_t data[BLOCKSIZE];
};

struct packet to_packet(uint32_t);
uint32_t to_u32(struct packet);
uint32_t merge_packets(struct packet, struct packet);
static struct entry *entry_init(enum vtype);
static void entry_append(struct entry *, struct entry *);
uint32_t entry_setsize(struct entry *ent,  size_t sz);
static void entry_remove(struct entry *, struct entry *);
static void entry_reclaim(struct entry *);

bool a = true, b = true, l = true;
int ID = 1;
struct entry ENTRIES[ENTRIES_MAX];
EWRAM_BSS struct gba_block GBA_BLOCKS[BLOCK_MAX];

LinkCube* linkCube = new LinkCube();

void init() {
 	Common::initTTE();

  	interrupt_init();
  	interrupt_add(INTR_SERIAL, LINK_CUBE_ISR_SERIAL);
	//interrupt_add(INTR_VBLANK, []() {});
	interrupt_disable(INTR_VBLANK);
}

static inline struct gba_block *
BLOCKFETCH(uint32_t id, uint32_t idx)
{
	int i;
	for (i = 0; i < BLOCK_MAX; i++) {
		if (GBA_BLOCKS[i].va_fileid == id && GBA_BLOCKS[i].idx == idx) {
			return &GBA_BLOCKS[i];
		}
	}

	return NULL;
}

static inline struct gba_block *
BLOCKFREE()
{
	int i;
	for (i = 0; i < BLOCK_MAX; i++) {
		if (GBA_BLOCKS[i].va_fileid == 0) {
			return &GBA_BLOCKS[i];
		}
	}
	return NULL;
}


static struct entry *
find_by_id(u32 id)
{
	int i;
	struct entry *cur;
	for (i = 0; i < ENTRIES_MAX; i++) {
		cur = &ENTRIES[i];
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
	while (ent) {
		if (strcmp(ent->name, req.name) == 0) {
			match = ent;
			break;
		}
		ent = ent->next;
	}
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
handle_create_request(enum vtype vt)
{
	struct create_req req;
	struct create_resp resp;
	struct entry *parent, *ent;

	parent = NULL;
	ent = NULL;
	resp.exists = 0;

	receive_response(linkCube, &req, sizeof(struct create_req));
	SSWAP32(&req, parent_fileid);
	parent = find_by_id(req.parent_fileid);

	if (!parent) {
		goto send_response;	
	}

	ent = entry_init(vt);
	if (!ent) {
		goto send_response;
	}

	entry_append(parent, ent);
	snprintf(ent->name, sizeof(ent->name), req.name);
	resp.exists = 1;
send_response:
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
	send_request(linkCube, &resp, sizeof(struct create_resp));
	return 0;
}

uint32_t
entry_setsize(struct entry *ent, size_t newsize)
{
	struct gba_file *file;
	struct gba_block *block;
	size_t newblocks, i;
	int needalloc, shrinks;

	file = &ent->file;
	needalloc = newsize > ROUNDUP(file->datalen, PBA_BLOCKSIZE);
	shrinks = newsize < ent->va_size;

	if (needalloc || shrinks) {
		newblocks = BLOCKNUM(newsize, PBA_BLOCKSHIFT) + 1;

		if (shrinks) {
			for (i = newblocks; i < file->numblocks; i++) {
				block = BLOCKFETCH(ent->va_fileid, i);
				if (!block) {
					return EBLOCKNOTFOUND;
				}
				block->va_fileid = 0;
				block->idx = 0;
				/*
				 * since we memset to 0 whenever the block is
				 * taken next i dont think we really need this
				 * but it feels good to just clean up
				 */
				memset(block->data, 0, BLOCKSIZE);
			}
		}

		if (!shrinks) {
			for (i = file->numblocks; i < newblocks; i++) {
				block = BLOCKFREE();
				if (!block) {
					return ENOBLOCKSFREE;
				}
				block->va_fileid = ent->va_fileid;
				block->idx = i;
				memset(block->data, 0, BLOCKSIZE);
			}
		}

		file->datalen = newsize;
		file->numblocks = newblocks;
	}

	ent->va_size = newsize;
	ent->va_bytes = BLOCKNUM(newsize,PBA_BLOCKSHIFT)>>PBA_BLOCKSHIFT;

	return 0;
}

static int
handle_read_request()
{
	struct read_req req;
	struct read_resp resp;
	struct entry *ent;
	struct gba_file *file;
	struct gba_block *block;
	uint8_t *src;

	resp.err = 0;
	resp.copylen = 0;
	receive_response(linkCube, &req, sizeof(struct read_req));
	SSWAP32(&req, fileid);
	SSWAP32(&req, copylen);
	SSWAP64(&req, offset);

	ent = find_by_id(req.fileid);
	if (ent == NULL) {
		resp.err = 1;
		goto send_read_resp;
	}

	file = &ent->file;
	if (req.offset > file->datalen) {
		resp.err = 2;
		goto send_read_resp;
	}

	resp.copylen = MIN(file->datalen - req.offset, PBA_BLOCKSIZE);
	block = BLOCKFETCH(ent->va_fileid, BLOCKNUM(req.offset, PBA_BLOCKSHIFT));
	src = block->data + BLOCKOFF(req.offset, PBA_BLOCKSIZE);
	memcpy(resp.buf, src, resp.copylen);
send_read_resp:
	send_request(linkCube, &resp, sizeof(struct read_resp));
	return 0;
}

static int
handle_setsize_request()
{
	struct setattr_req req;
	struct setattr_resp resp;
	struct entry *ent;

	resp.err = 0;

	receive_response(linkCube, &req, sizeof(struct setattr_req));
	SSWAP32(&req, va_fileid);
	SSWAP64(&req, va_size);

	ent = find_by_id(req.va_fileid);
	if (!ent) {
		resp.err = ENOENT;
		goto send_response;
	}
	entry_setsize(ent, req.va_size);
	resp.va_fileid = ent->va_fileid;
	resp.va_size = ent->va_size;
	resp.va_bytes = ent->va_bytes;
send_response:
	send_request(linkCube, &resp, sizeof(struct setattr_resp));
	return 0;
}

static int
handle_write_request()
{
	struct write_req wreq;
	struct write_resp wresp;
	struct write_buf_req breq;
	struct write_buf_resp bresp;
	struct entry *ent;
	struct gba_block *block;
	uint8_t *dest, *src;
	size_t copylen;
	int i;

	receive_response(linkCube, &wreq, sizeof(struct write_req));
	SSWAP32(&wreq, fileid);
	SSWAP32(&wreq, io_append);
	SSWAP64(&wreq, offset);
	SSWAP64(&wreq, resid);
	
	ent = find_by_id(wreq.fileid);
	if (ent == NULL) {
		wresp.exists = 0;
		wresp.err = 1;
		goto send_write_resp;
	}
	wresp.exists = 1;
	wresp.err = 0;

	if (wreq.offset + wreq.resid > ent->va_size) {
		wresp.err = entry_setsize(ent,  wreq.offset + wreq.resid);
	}

send_write_resp:
	send_request(linkCube, &wresp, sizeof(struct write_resp));
	if (wresp.err > 0) return 0;

	// TODO it would be better to not have an infinite while loop
	// instead the write_buf_request should be an entirely separate cmd
	// with its own handler. then the wii can choose what to send and the
	// offset
	uint32_t x;
	while (wreq.resid > 0) {
		receive_response(linkCube, &breq, sizeof(struct write_buf_req));
		SSWAP32(&breq, buflen);
		x = breq.buflen;
		src = breq.buf;

		while (x > 0) {
			copylen = MIN(wreq.resid, BLOCKLEFT(wreq.offset, PBA_BLOCKSIZE));
			i = BLOCKNUM(wreq.offset, PBA_BLOCKSHIFT);
			block = BLOCKFETCH(ent->va_fileid, i);
			dest = block->data + BLOCKOFF(wreq.offset, PBA_BLOCKSIZE);
			memcpy(dest, src, copylen);
			wreq.offset += copylen;
			wreq.resid -= copylen;
			x -= copylen;
			src += copylen;
		}

		send_request(linkCube, &bresp, sizeof(struct write_buf_resp));
	}

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

static int
handle_reclaim_request()
{
	struct reclaim_req req;
	struct reclaim_resp resp;
	struct entry *ent;

	resp.err = 0;
	resp.va_fileid = 0;

	receive_response(linkCube, &req, sizeof(struct reclaim_req));
	SSWAP32(&req, va_fileid);

	ent = find_by_id(req.va_fileid);
	if (!ent) {
		resp.err = ENOENT;
		goto send_response;
	}

	resp.va_fileid = ent->va_fileid;
	entry_reclaim(ent);
	resp.va_nlink = ent->va_nlink;
send_response:
	send_request(linkCube, &resp, sizeof(struct reclaim_resp));
	return 0;
}

static int
handle_rm_request()
{
	struct rm_req req;
	struct rm_resp resp;
	struct entry *parent, *res, *ent;

	parent = NULL;
	res = NULL;
	resp.err = 0;
	resp.va_fileid = 0;

	receive_response(linkCube, &req, sizeof(struct rm_req));
	SSWAP32(&req, parent_fileid);

	parent = find_by_id(req.parent_fileid);
	if (!parent || !parent->child) {
		resp.err = ENOENT;
		goto send_response;
	}

	if (parent->va_type != VDIR) {
		resp.err = ENOTDIR;
		goto send_response;
	}

	ent = parent->child;
	while (ent) {
		if (strcmp(ent->name, req.name) == 0) {
			res = ent;
			break;
		}
		ent = ent->next;
	}

	if (res == NULL) {
		resp.err = ENOENT;
		goto send_response;
	}

	resp.va_fileid = res->va_fileid;
	entry_remove(parent, res);
	resp.va_nlink = res->va_nlink;
send_response:
	send_request(linkCube, &resp, sizeof(struct rm_resp));
	return 0;
}

static void
entry_reclaim(struct entry *ent)
{
	int i;
	struct gba_block *block;

	/* seems kinda hacky but i can leave all of the values as-is
	 * besides the va_fileid. setting it to 0 is how we identify
	 * an available entry. and the next time it is used all values
	 * get overwritten in entry_init()
	 */
	for (i = 0; i < ENTRIES_MAX; i++) {
		if (ENTRIES[i].va_fileid == ent->va_fileid) {
			ENTRIES[i].va_fileid = 0;
		}
	}

	for (i = 0; i < BLOCK_MAX; i++) {
		if (GBA_BLOCKS[i].va_fileid == ent->va_fileid) {
			block = &GBA_BLOCKS[i];
			block->va_fileid = 0;
			block->idx = 0;
			memset(block->data, 0, BLOCKSIZE);
		}
	}
}

static void
entry_remove(struct entry *parent, struct entry *ent)
{
	struct entry *prev;

	prev = parent->child;
	while (prev) {
		if (prev->next && prev->next->va_fileid == ent->va_fileid) {
			prev->next = ent->next;
			break;
		}
		prev = prev->next;
	}

	ent->va_nlink--;
}

static struct entry *
entry_init(enum vtype type)
{
	int i;
	struct entry *ent;

	ent = NULL;
	for (i = 0; i < ENTRIES_MAX; i++) {
		if (ENTRIES[i].va_fileid == 0) {
			ent = &ENTRIES[i];
			break;
		}
	}

	if (!ent) {
		return NULL;
	}

	ent->va_fileid = ID++;
	ent->va_type = type;
	ent->next = NULL;
	ent->child = NULL;
	ent->file.numblocks = 0;
	ent->file.datalen = 0;
	/*
	 * va_nlink should be 2 for directory, 1 for file
	 * entry_append (which adds the file to the dir) will increment
	 */
	if (type == VDIR) {
		ent->va_mode = 0777;
		ent->va_nlink = 1;
	} else {
		ent->va_mode = 0666;
		ent->va_nlink = 0;
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

	return ent;
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

	ent->va_nlink++;
}

int main()
{
	init();
	linkCube->activate();
	u32 recv;
	struct entry *root, *test;

	for (int i = 0; i < ENTRIES_MAX; i++) {
		ENTRIES[i].va_fileid = 0;
	}
	for (int i = 0; i < BLOCK_MAX; i++) {
		GBA_BLOCKS[i].va_fileid = 0;
	}

	root = entry_init(VDIR);
	test = entry_init(VREG);
	entry_append(root, test);
	snprintf(test->name, sizeof(test->name), "abcdefghijklmnopqrstuvwxyz12345");
	tte_write("puffboy is ready\n");
	while (true) {
		while (linkCube->canRead()) {
			recv = BSWAP32(linkCube->read());
			switch (recv) {
			case CMD_READDIR:
				handle_readdir_request();
				break;
			case CMD_LOOKUP:
				handle_lookup_request();
				break;
			case CMD_GETATTR:
				handle_getattr_request();
				break;
			case CMD_CREATE:
				handle_create_request(VREG);
				break;
			case CMD_WRITE:
				handle_write_request();
				break;
			case CMD_READ:
				handle_read_request();
				break;
			case CMD_MKDIR:
				handle_create_request(VDIR);
				break;
			case CMD_RM:
				handle_rm_request();
				break;
			case CMD_RECLAIM:
				handle_reclaim_request();
				break;
			case CMD_SETSIZE:
				handle_setsize_request();
				break;
			default:
				break;
			}
		}

		// Clear
		/*
		if (Common::didPress(KEY_B, b)) {
			tte_erase_screen();
			tte_set_pos(0, 0);
			//tte_write("Waiting for messages\n");
		}
		VBlankIntrWait();
		*/
	}
	return 0;
}
