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
	req.parent_fileid = ntohl(req.parent_fileid);
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
	struct req_getattr req;
	struct resp_getattr resp;
	struct entry *ent;

	receive_response(linkCube, &req, sizeof(struct req_getattr));
	req.fileid = ntohl(req.fileid);

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
	send_request(linkCube, &resp, sizeof(struct resp_getattr));
	return 0;
}

static int
handle_nth_entry_request()
{
	u32 recv;
	int i,j;
	struct nth_entry_request req;
	struct nth_entry_response resp;
	struct entry *parent, *res;
	struct packet pk, pk2;

	parent = NULL;
	res = NULL;
	resp.exists = 0;

	receive_response(linkCube, &req, sizeof(struct nth_entry_request));
	req.parent_fileid = ntohl(req.parent_fileid);
	req.n = ntohl(req.n);

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
	send_request(linkCube, (struct nth_entry_response *) &resp, sizeof(struct nth_entry_response));
	return 0;
}

static void mytest()
{
	return;
	u32 recv;
	int i,j;
	u32 req_pkt[WORD_CNT(struct nth_entry_request) * 2];
	u32 req_buf[WORD_CNT(struct nth_entry_request)];
	u32 resp_buf[WORD_CNT(struct nth_entry_response)];
	u32 resp_pkt[WORD_CNT(struct nth_entry_response) * 2];
	u32 in, in2;
	char msg[100];
	struct nth_entry_request req;
	struct nth_entry_response resp;
	struct entry *parent, *res;
	struct packet pk, pk2;

	//linkCube->send(CMD_NTH_ENTRY);
	parent = NULL;
	res = NULL;
	resp.exists = 0;

	i = 0;
	req_pkt[0] = 0x01010000;
	req_pkt[1] = 0x02010001;
	req_pkt[2] = 0x03010000;
	req_pkt[3] = 0x04010000;

	i = 0;
	j = 0;
	while (i < WORD_CNT(struct nth_entry_request)) {
		j = 2*i;
		pk = to_packet(req_pkt[j]);
		pk2 = to_packet(req_pkt[j+1]);
		req_buf[i] = merge_packets(pk, pk2);
		snprintf(msg, sizeof(msg), "in:0x%08X | 0x%08X - 0x%08X\n", pk.data, pk2.data, req_buf[i]);
		tte_write(msg);
		i++;
	}
	memcpy(&req, req_buf, sizeof(struct nth_entry_request));
	req.parent_fileid = req.parent_fileid;
	req.n = req.n;
	tte_write("==============\n");
	snprintf(msg, sizeof(msg), "req.parent 0x%08X / req.n 0x%08x\n", req.parent_fileid, req.n);
	tte_write(msg);
	snprintf(msg, sizeof(msg), "req.parent %d / req.n %d\n", req.parent_fileid, req.n);
	tte_write(msg);
	tte_write("==============\n");
	/*
	u32 resp_buf[WORD_CNT(struct nth_entry_response)];
	u32 resp_pkt[WORD_CNT(struct nth_entry_response) * 2];
	char msg[100];
	int i, j;
	struct nth_entry_response resp;
	struct packet pk;

	resp.exists = 1;
	resp.va_fileid = 1;
	resp.va_type = VREG;
	snprintf(resp.name, sizeof(resp.name), "hello.txt");

	memcpy(resp_buf, &resp, sizeof(struct nth_entry_response));
	snprintf(msg, sizeof(msg), "va_type: %d\n", resp.va_type);
	tte_write(msg);
	snprintf(msg, sizeof(msg), "name: %s\n", resp.name);
	tte_write(msg);
	i = 0;
	j = 0;
	while (i < WORD_CNT(struct nth_entry_response)) {
		j = 2 * i;
		pk.seq = 1;
		pk.cmd = 1;
		pk.data = (u16)(resp_buf[i] & 0xFFFF);
		resp_pkt[j] = to_u32(pk);
		//snprintf(msg, sizeof(msg), "j: 0x%08x\n", resp_pkt[j]);
		//tte_write(msg);
		pk.data = (u16)(resp_buf[i] >> 16);
		resp_pkt[j+1] = to_u32(pk);
		//snprintf(msg, sizeof(msg), "j+1: 0x%08x\n", resp_pkt[j+1]);
		//tte_write(msg);
		i++;
	}
	for (i = 0; i < (int)WORD_CNT(struct nth_entry_response)*2; i++) {
		snprintf(msg, sizeof(msg), "sending 0x%08X\n", resp_pkt[i]);
		tte_write(msg);
		//linkCube->send(resp_pkt[i]);
	}
	*/
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
	mytest();
	while (true) {
		while (linkCube->canRead()) {
			recv = ntohl(linkCube->read());
			switch (recv) {
			case CMD_NTH_ENTRY:
				handle_nth_entry_request();
				mmEffectEx(&ding);
				break;
			case CMD_GETATTR:
				handle_getattr_request();
				mmEffectEx(&ding);
				break;
			case CMD_LOOKUP:
				handle_lookup_request();
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
