#include "../../../gbashared/lib/LinkCube.hpp"

#include "../../../gbashared/_lib/common.h"
#include "../../../gbashared/_lib/interrupt.h"

#include "../../shared/command.h"

#include <string.h>

#define ENTRIES_MAX 20
#define BLOCKSIZE 128
#define PBA_BLOCKSHIFT (12)
#define PBA_BLOCKSIZE  (1<<PBA_BLOCKSHIFT)
#define ROUNDUP(a,b) ((a) & ((b)-1))
#define BLOCKNUM(a,b) (((a) & ~((1<<(b))-1)) >> (b))
#define BLOCKOFF(a,b) ((a) & ((b)-1))
#define BLOCKLEFT(a,b) ((b) - BLOCKOFF(a,b))

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
	struct gba_file	*file;
};

struct gba_file {
	uint8_t **blocks;
	size_t numblocks;
	size_t datalen;
};

struct packet to_packet(uint32_t);
uint32_t to_u32(struct packet);
uint32_t merge_packets(struct packet, struct packet);
static struct entry *entry_init(enum vtype);
static void entry_append(struct entry *, struct entry *);
uint32_t entry_setsize(struct entry *ent,  size_t sz);

bool a = true, b = true, l = true;
int ID = 1;
struct entry ENTRIES[ENTRIES_MAX];

LinkCube* linkCube = new LinkCube();

void init() {
 	Common::initTTE();

  	interrupt_init();
  	interrupt_add(INTR_SERIAL, LINK_CUBE_ISR_SERIAL);
	interrupt_add(INTR_VBLANK, []() {});
	//interrupt_disable(INTR_VBLANK);
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
handle_create_request()
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

	ent = entry_init(VREG);
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
	size_t newblocks, i;
	int needalloc, shrinks;
	char msg[100];

	file = ent->file;
	needalloc = newsize > ROUNDUP(file->datalen, PBA_BLOCKSIZE);
	shrinks = newsize < ent->va_size;
	snprintf(msg, sizeof(msg), "newsize: %d / needalloc: %d / shrinks %d\n", newsize, needalloc, shrinks);
	tte_write(msg);

	if (needalloc || shrinks) {
		newblocks = BLOCKNUM(newsize, PBA_BLOCKSHIFT) + 1;

		if (shrinks) {
			for (i = newblocks; i < file->numblocks; i++) {
				free(file->blocks[i]);
			}
		}

		snprintf(msg, sizeof(msg), "newblocks: %d\n", newblocks);
		tte_write(msg);
		file->blocks = (uint8_t **)realloc(file->blocks, newblocks * sizeof(uint8_t *));

		if (!shrinks) {
			for (i = file->numblocks; i < newblocks; i++) {
				file->blocks[i] = (uint8_t *)malloc(PBA_BLOCKSIZE);
				memset(file->blocks[i], 0, PBA_BLOCKSIZE);
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
handle_write_request()
{
	struct write_req wreq;
	struct write_resp wresp;
	struct write_buf_req breq;
	struct write_buf_resp bresp;
	struct entry *ent;
	struct gba_file *file;
	uint8_t *src, *dest;
	size_t copylen;
	int i;
	char msg[150];

	receive_response(linkCube, &wreq, sizeof(struct write_req));
	SSWAP32(&wreq, fileid);
	SSWAP32(&wreq, io_append);
	SSWAP64(&wreq, offset);
	SSWAP64(&wreq, resid);
	snprintf(msg, sizeof(msg), "fileid: %ld / io_append: %ld / offset: %lld / resid: %lld\n", wreq.fileid, wreq.io_append, wreq.offset, wreq.resid);
	tte_write(msg);
	
	ent = find_by_id(wreq.fileid);
	if (ent == NULL) {
		tte_write("ENTRY NOT FOUND\n");
		wresp.exists = 0;
		wresp.err = 1;
		goto send_write_resp;
	}
	wresp.exists = 1;
	wresp.err = 0;
	tte_write("ENTRY FOUND - ");

	file = ent->file;
	if (wreq.io_append) {
		tte_write("APPENDING\n");
		wreq.offset += ent->va_size;
	}

	if (wreq.offset + wreq.resid > ent->va_size) {
		wresp.err = entry_setsize(ent,  wreq.offset + wreq.resid);
	}

send_write_resp:
	send_request(linkCube, &wresp, sizeof(struct write_resp));
	tte_write("FINISHED SENDING WRESP\n");
	if (wresp.err > 0) return 0;

	tte_write("ENTERING LOOP\n");
	while (wreq.resid > 0) {
		snprintf(msg, sizeof(msg),"resid is %lld\n", wreq.resid);
		tte_write(msg);
		copylen = MIN(wreq.resid, BLOCKLEFT(wreq.offset, PBA_BLOCKSIZE));
		i = BLOCKNUM(wreq.offset, PBA_BLOCKSHIFT);
		dest = file->blocks[i] + BLOCKOFF(wreq.offset, PBA_BLOCKSIZE);

		receive_response(linkCube, &breq, sizeof(struct write_buf_req));
		src = breq.buf;
		memcpy(dest, src, copylen);
		wreq.offset += copylen;
		dest += copylen;
		wreq.resid -= copylen;
		bresp.err = 0;
		send_request(linkCube, &bresp, sizeof(struct write_buf_resp));
	}

	tte_write("FINISHED LOOP\n");
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
	if (type == VDIR) {
		ent->va_mode = 0777;
		ent->va_nlink = 1; /* n + 1 after adding dent */
		ent->file = NULL;
	} else {
		ent->va_mode = 0666;
		ent->va_nlink = 0; /* n + 1 */
		ent->file = (struct gba_file *)malloc(sizeof(struct gba_file));
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
	root = entry_init(VDIR);
	if (root == NULL) {
		tte_write("root is NULL\n");
	}
	
	test = entry_init(VREG);
	if (test == NULL) {
		tte_write("test is NULL\n");
	}
	entry_append(root, test);
	snprintf(test->name, sizeof(test->name), "hello.txt");

	tte_write("Waiting for messages\n");
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
				handle_create_request();
				break;
			case CMD_WRITE:
				handle_write_request();
			default:
				break;
			}
		}

		VBlankIntrWait();
	}

	return 0;
}
