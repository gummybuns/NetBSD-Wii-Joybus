#include <sys/queue.h>

#include <puffs.h>
#include <stdio.h>
#include <errno.h>

#include "../pba.h"

static struct gba_node * get_by_name(struct gba_node *, char *);

/*
 * opc is the cookie that references the _directory_ not the node we actually
 * want to find. so i need to revisit what pn_nodewalk actually does. there
 * is probably a better way to do it but i can just strcmp i guess since every
 * entry in a directory has a unique name
 */
int
puffboy_node_lookup(struct puffs_usermount *pu, puffs_cookie_t opc,
		    struct puffs_newinfo *pni,
		    const struct puffs_cn *pcn)
{
	struct puffs_node *pn;
	struct gba_node *dir_gn, *entry;

	dir_gn = ((struct puffs_node *)opc)->pn_data;

	printf("in node_lookup for %s\n", pcn->pcn_name);

	/* we are not concerning ourselves with parent directories */
	if (PCNISDOTDOT(pcn)) {
		printf("I AM IN HERE!!!!\n");
		printf("node_lookup PCN is DOTDOT\n");
		return ENOENT;
	}

	if ((entry = get_by_name(dir_gn, pcn->pcn_name)) == NULL) {
		printf("node_lookup is NULL\n");
		return ESTALE;
	}

	printf("node found!\n");
	pn = entry->pn;
	printf("fileid %ld\n", (long int)pn->pn_va.va_fileid);
	printf("is VREG (%d)? %d\n", pn->pn_va.va_type, pn->pn_va.va_type == VREG);
	puffs_newinfo_setcookie(pni, pn);
	puffs_newinfo_setvtype(pni, pn->pn_va.va_type);
	puffs_newinfo_setsize(pni, (voff_t)pn->pn_va.va_size);
	puffs_newinfo_setrdev(pni, pn->pn_va.va_rdev);
  	return 0;
}

static struct gba_node *
get_by_name(struct gba_node *gn, char *name)
{
	struct gba_node *entry;

	SLIST_FOREACH(entry, &gn->head, entries) {
		printf("looking for %s, currently %s\n", name, entry->name);
		if (strcmp(name, entry->name) == 0) {
			return entry;
		}
	}

	return NULL;
}
