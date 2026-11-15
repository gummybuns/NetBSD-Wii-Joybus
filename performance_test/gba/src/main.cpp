#include "./LinkCube.hpp"

#include "../../../gbashared/_lib/common.h"
#include "../../../gbashared/_lib/interrupt.h"

#include <string.h>

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
	u32 data[1025];
	while (true) {
		while (linkCube->canRead()) {
			recv = linkCube->read();
			if (recv > 0) {
				data[0] = 1111;

				for (int i = 1; i < 1025; i++) {
					data[i] = i;
				}

				linkCube->sendArr(data, sizeof(u32) * 1025);
			}
		}

		//VBlankIntrWait();
	}

	return 0;
}
