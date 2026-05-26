#include "../../../gbashared/lib/LinkCube.hpp"

#include "../../../gbashared/_lib/common.h"
#include "../../../gbashared/_lib/interrupt.h"
#include <string.h>

bool a = true, b = true, l = true;

LinkCube* linkCube = new LinkCube();

void init() {
 	Common::initTTE();

  	interrupt_init();
  	interrupt_add(INTR_VBLANK, []() {});
  	interrupt_add(INTR_SERIAL, LINK_CUBE_ISR_SERIAL);
}


int main()
{
	init();

	linkCube->activate();

	u32 recv;
	char msg[128];
	char to_print[128];
	bool print_ready = false;
	bool reset = false;
	u32 vblanks = 0;
	int i, j;

	tte_write("Waiting for messages\n");
	i = 0;
	while (true) {
		if (print_ready) {
			tte_write(to_print);
			print_ready = false;
		}

		while (linkCube->canRead()) {
			recv = linkCube->read();
			for (j = 0; j < 4; j++) {
				msg[i+j] = (u8)(recv >> (32 - (j+1)*8) & 0xFF);
				if (msg[i+j] == '\0') {
					snprintf(to_print, sizeof(to_print), "%s\n", msg);
					print_ready = true;
					/*
					 * kinda hacky but we add 4 at the end
					 * of the loop so to reset msg we do -4
					 */
					i = -4;
					break;
				}
			}
			linkCube->send(recv);
			i += 4;
		}

		// Clear
		if (Common::didPress(KEY_B, b)) {
			tte_erase_screen();
			tte_set_pos(0, 0);
			tte_write("Waiting for messages\n");
		}


		// Reset warning
		if (linkCube->didReset()) {
			reset = true;
			vblanks = 0;
		}
		if (reset) {
			vblanks++;
			if (vblanks > 60) reset = false;
		}

		VBlankIntrWait();
	}

	return 0;
}
