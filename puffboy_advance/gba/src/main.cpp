#include "../../../gbashared/lib/LinkCube.hpp"

#include "../../../gbashared/_lib/common.h"
#include "../../../gbashared/_lib/interrupt.h"

#include "../../shared/command.h"

#include <string.h>

extern "C" {
	#include <maxmod.h>
	#include "soundbank.h"
	#include "soundbank_bin.h"
}

struct entry {
	uint32_t	va_fileid;
	uint32_t	va_type;
	char		name[32];
	struct entry	*next;
	struct entry	*child;
}

bool a = true, b = true, l = true;
int ID = 1;
struct entry *ENTRIES[20];


LinkCube* linkCube = new LinkCube();

void init() {
 	Common::initTTE();

  	interrupt_init();
  	interrupt_add(INTR_VBLANK, mmVBlank);
  	interrupt_add(INTR_SERIAL, LINK_CUBE_ISR_SERIAL);
}


static int
handle_nth_entry_request()
{
	u32 recv;
	int i, j;
	u8 req_buf[WORD_CNT(struct nth_entry_request)];
	u8 resp_buf[(WORD_CNT(struct nth_entry_response)];
	struct nth_entry_request req;
	struct nth_entry_response resp;
	struct entry *parent, *res;

	linkCube->send(recv);
	parent = NULL;
	res = NULL;
	resp.exists = 0;

	i = 0;
	while (linkCube->canRead() && i < WORD_CNT(struct nth_entry_request)) {
		recv = linkCube->read();
		req_buf[i] = recv;
		i++;
	}
	memcpy(&req, req_buf, sizeof(struct nth_entry_request));
	parent = find_by_id(req.parent_fileid);

	if (!parent) {
		goto send_response;	
	}

	i = 0;
	res = parent->child;
	while (res && i <= req.n) {
		res = res->next;
		i++;
	}
send_response:
	resp.exists = res != NULL;
	if (resp.exists) {
		resp.va_fileid = res->va_fileid;
		resp.va_type = res->va_type;
		resp.name = res->name;
	}
	memcpy(resp_buf, &resp, sizeof(struct nth_entry_response));
	for (i = 0; i < WORD_CNT(struct nth_entry_response); i++) {
		linkCube->send(resp_buf[i]);
	}

	return 0;
}

int main()
{
	init();

	linkCube->activate();
	mmInitDefault( (mm_addr)soundbank_bin, 8 );

	u32 recv;
	int i;
	struct entry root;
	struct nth_entry_request ner;

	root.va_fileid = ID++;
	root.va_type = VA_DIR;
	root.next = NULL;
	root.child = NULL;
	ENTRIES[0] = &root;

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
			recv = linkCube->read();
			switch (recv) {
			case CMD_NTH_ENTRY:
				tte_write("get_nth_entry");
				handle_nth_entry_request();
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
