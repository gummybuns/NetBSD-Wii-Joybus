#include <puffs.h>
#include <stdio.h>
#include <errno.h>

#include "../pba.h"

static void * addrcmp(struct puffs_usermount *, struct puffs_node *, void *);

int
puffboy_node_lookup(struct puffs_usermount *pu, puffs_cookie_t opc,
		    struct puffs_newinfo *pni,
		    const struct puffs_cn *pcn)
{
	struct puffs_node *pn;

	printf("in node_lookup for %s\n", pcn->pcn_name);

	/* we are not concerning ourselves with parent directories */
	if (PCNISDOTDOT(pcn)) {
		printf("node_lookup PCN is DOTDOT\n");
		return ENOENT;
	}

	if ((pn = puffs_pn_nodewalk(pu, addrcmp, opc)) == NULL) {
		printf("node_lookup pn is NULL\n");
		return ESTALE;
	}

	printf("node found!\n");
	printf("fileid %ld\n", (long int)pn->pn_va.va_fileid);
	printf("is VREG (%d)? %d\n", pn->pn_va.va_type, pn->pn_va.va_type == VREG);
	puffs_newinfo_setcookie(pni, pn);
	puffs_newinfo_setvtype(pni, pn->pn_va.va_type);
	puffs_newinfo_setsize(pni, (voff_t)pn->pn_va.va_size);
	puffs_newinfo_setrdev(pni, pn->pn_va.va_rdev);
  	return 0;
}

static void *
addrcmp(struct puffs_usermount *pu, struct puffs_node *pn, void *arg)
{

	struct puffs_node *arg_pn = (struct puffs_node *) arg;
	struct gba_node *pn_gn = (struct gba_node *) pn->pn_data;
	struct gba_node *arg_gn = arg_pn->pn_data;

	printf("addrcmp: comparing %d - %d\n", pn_gn->id, arg_gn->id);
	if (pn_gn->id == arg_gn->id) return pn;
	return NULL;
}
