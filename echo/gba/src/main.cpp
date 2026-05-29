#include "../../../gbashared/lib/LinkCube.hpp"

#include "../../../gbashared/_lib/common.h"
#include "../../../gbashared/_lib/interrupt.h"
#include <string.h>

extern "C" {
	#include <maxmod.h>
	#include "soundbank.h"
	#include "soundbank_bin.h"
}

bool a = true, b = true, l = true;

LinkCube* linkCube = new LinkCube();

void init() {
 	Common::initTTE();

  	interrupt_init();
  	interrupt_add(INTR_VBLANK, mmVBlank);
  	interrupt_add(INTR_SERIAL, LINK_CUBE_ISR_SERIAL);
}


int main()
{
	init();

	linkCube->activate();
	mmInitDefault( (mm_addr)soundbank_bin, 8 );

	u32 recv;
	int i;

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
			linkCube->send(recv);
			for (i = 0; i < 4; i++) {
				u8 c = (u8)(recv >> (32 - (i+1)*8) & 0xFF);
				if (c == '\n') {
					/* i dont really know how to get
					 * tte_putc to newline on its own.
					 * i guess there is some magic in
					 * tte_write to handle the cursor
					 */
					tte_write("\n");
					mmEffectEx(&ding);
					break;
				} else {
					tte_putc(c);
				}
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
