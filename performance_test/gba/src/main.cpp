#include "./LinkCube.hpp"

#include "../../../gbashared/_lib/common.h"
#include "../../../gbashared/_lib/interrupt.h"

#include <string.h>

#define ENTRIES_MAX 20

LinkCube* linkCube = new LinkCube();

void init() {
 	Common::initTTE();

  	interrupt_init();
  	interrupt_add(INTR_SERIAL, LINK_CUBE_ISR_SERIAL);
	//interrupt_add(INTR_VBLANK, []() {});
	interrupt_disable(INTR_VBLANK);
}

int main()
{
	init();
	linkCube->activate();
	u32 recv;
	tte_write("Ready\n");
	while (true) {
		while (linkCube->canRead()) {
			recv = linkCube->read();
			if (recv > 0) {
				for (int i = 0; i < 1024; i++) {
					linkCube->send(i);
				}
			}
		}

		//VBlankIntrWait();
	}

	return 0;
}
