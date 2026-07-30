#include <sys/queue.h>

#include <puffs.h>
#include <stdio.h>
#include <errno.h>

#include "../pba.h"

static struct gba_node * get_nth_entry(struct gba_node *, int);

int
puffboy_node_readdir(struct puffs_usermount *pu, puffs_cookie_t opc,
		     struct dirent *dent, off_t *readoff, size_t *reslen,
		     const struct puffs_cred *pcr, int *eofflag, off_t *cookies,
		     size_t *ncookies)
{
	struct puffs_node *pn, *pn_nth;
	struct gba_node *gn;

	printf("in readdir\n");
	pn = opc;

	/* my understanding is that ncookies is always initialized to 0 */
	*ncookies = 0;

	/* dont perform for non directories */
	if (pn->pn_va.va_type != VDIR) {
		printf("in node_readdir pn is not a VDIR\n");
		return ENOTDIR;
	}

again:
	if (*readoff == DENT_DOT || *readoff == DENT_DOTDOT) {
		printf("is DENT_DOT / DENT_DOTDOT\n");
		puffs_gendotdent(&dent, pn->pn_va.va_fileid, (int)*readoff, reslen);
		(*readoff)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *readoff);
		printf("finished DENT_DOT logic. running again\n");
		goto again;
	}

	for (;;) {
		printf("getting %dth entry\n", (int)DENT_ADJ(*readoff));
		if (pn->pn_data == NULL) {
			printf("PN_DATA IS NULL\n");
		}
		gn = get_nth_entry(pn->pn_data, (int)DENT_ADJ(*readoff));
		if (!gn) {
			*eofflag = 1;
			break;
		}

		pn_nth = gn->pn;
		if (!puffs_nextdent(&dent, gn->name, pn_nth->pn_va.va_fileid, (uint8_t)puffs_vtype2dt(pn_nth->pn_va.va_type), reslen)) {
			break;
		}

		(*readoff)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *readoff);
	}

	return 0;
}

static struct gba_node *
get_nth_entry(struct gba_node *gn, int n)
{
	struct gba_node *entry;
	int i;

	i = 0;
	SLIST_FOREACH(entry, &gn->head, entries) {
		printf("in loop for %d\n", i);
		if (i == n) {
			return entry;
		}
		i++;
	}

	return NULL;
}

