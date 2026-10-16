#include <errno.h>
#include <puffs.h>
#include <stdio.h>

#include "../pba.h"

int
puffboy_node_inactive(struct puffs_usermount *pu, void *opc)
{
	struct puffs_node *pn;

	pn = pba_cmap(pu, opc);
	if (!pn) {
		printf("COULDNT FIND NODE WITH ID %d\n", (int)opc);
		return ENOENT;
	}

	/*
	 * we use PUFFS_KFLAG_IAONDEMAND so we need to manually inform
	 * of the inactive state when nlink becomes 0 otherwise reclaim
	 * never frees up the blocks
	 */
	if (pn->pn_va.va_nlink == 0) {
		puffs_setback(puffs_cc_getcc(pu), PUFFS_SETBACK_NOREF_N2);
	}

	return 0;
}
