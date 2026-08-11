#include <sys/queue.h>

#include <puffs.h>
#include <stdio.h>
#include <errno.h>

#include "../pba.h"
#include "../../shared/command.h"
#include "../../../wiishared/lib/gcport_ioctl.h"

static void get_nth_entry(int, struct nth_entry_request *, struct nth_entry_response *);

int
puffboy_node_readdir(struct puffs_usermount *pu, puffs_cookie_t opc,
		     struct dirent *dent, off_t *readoff, size_t *reslen,
		     const struct puffs_cred *pcr, int *eofflag, off_t *cookies,
		     size_t *ncookies)
{
	struct puffs_node *pn;
	struct pba_context *ctx;
	struct gba_node *gn;
	struct nth_entry_request req;
	struct nth_entry_response resp;

	printf("in readdir\n");
	ctx = puffs_getspecific(pu);
	pn = opc;
	resp.exists = 0;

	/* my understanding is that ncookies is always initialized to 0 */
	*ncookies = 0;

	/* dont perform for non directories */
	if (pn->pn_va.va_type != VDIR) {
		printf("in node_readdir pn is not a VDIR\n");
		return ENOTDIR;
	}

again:
	if (*readoff == DENT_DOT || *readoff == DENT_DOTDOT) {
		puffs_gendotdent(&dent, pn->pn_va.va_fileid, (int)*readoff, reslen);
		(*readoff)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *readoff);
		goto again;
	}

	for (;;) {
		if (pn->pn_data == NULL) {
			printf("PN_DATA IS NULL\n");
		}
		gn = pn->pn_data;
		req.parent_fileid = gn->id;
		req.n = (int)DENT_ADJ(*readoff);
		get_nth_entry(ctx->fd, &req, &resp);
		if (!resp.exists) {
			*eofflag = 1;
			break;
		}

		if (!puffs_nextdent(&dent, resp.name, resp.va_fileid,(uint8_t)puffs_vtype2dt(resp.va_type), reslen)) {
			break;
		}
		(*readoff)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *readoff);
	}

	return 0;
}

static void
get_nth_entry(int fd, struct nth_entry_request *req, struct nth_entry_response *resp)
{
	struct joybus_ctx ctx;

	ctx.fd = fd;
	ctx.delay = DELAY;

	wait_clear(fd, DELAY, MSG_TIMEOUT);
	gba_write(ctx.fd, htonl(CMD_NTH_ENTRY), &ctx.status, ctx.delay);
	send_request(&ctx, req, sizeof(struct nth_entry_request));
	receive_response(&ctx, resp, sizeof(struct nth_entry_response));

	resp->exists = bswap32(resp->exists);
	resp->va_fileid = bswap32(resp->va_fileid);
	resp->va_type = bswap32(resp->va_type);
}
