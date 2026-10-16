#include <puffs.h>
#include <errno.h>
#include <stdio.h>

#include "../pba.h"
#include "../../shared/command.h"
#include "../../../wiishared/lib/gcport_ioctl.h"

static void gba_rm(struct pba_context *, struct rm_req *, struct rm_resp *);

int
puffboy_node_remove(struct puffs_usermount *pu, void *opc, void *targ,
		    const struct puffs_cn *pcn)
{
	struct pba_context *ctx;
	struct rm_req req;
	struct rm_resp resp;
	struct puffs_node *pn;

	pn = pba_cmap(pu, targ);

	if (!pn) {
		printf("COULDNT FIND node with id %d", (int)targ);
		return ENOENT;
	}

	ctx = puffs_getspecific(pu);
	req.parent_fileid = cookie_to_fileid(opc);
	memcpy(req.name, pcn->pcn_name, sizeof(req.name));

	gba_rm(ctx, &req, &resp);
	if (resp.err) {
		/* need better error translation most likely */
		printf("rm returned err %d\n", resp.err);
		return resp.err;
	}


	/*
	 * we use PUFFS_KFLAG_IAONDEMAND so we need to manually inform
	 * of the inactive state when nlink becomes 0 otherwise reclaim
	 * never frees up the blocks
	 */
	pn->pn_va.va_nlink = resp.va_nlink;
	if (pn->pn_va.va_nlink == 0) {
		puffs_setback(puffs_cc_getcc(pu), PUFFS_SETBACK_NOREF_N2);
	}

	return 0;
}

static void
gba_rm(struct pba_context *ctx, struct rm_req *req, struct rm_resp *resp)
{
	wait_clear(ctx->fd, DELAY, MSG_TIMEOUT);
	gba_write(ctx->fd, htonl(CMD_RM), &ctx->status, ctx->delay);
	send_request(ctx, req, sizeof(struct rm_req));
	receive_response(ctx, resp, sizeof(struct rm_resp));

	SSWAP32(resp, err);
	SSWAP32(resp, va_fileid);
	SSWAP32(resp, va_nlink);
}
