#include <errno.h>
#include <puffs.h>
#include <stdio.h>

#include "../pba.h"
#include "../../shared/command.h"
#include "../../../wiishared/lib/gcport_ioctl.h"

static void gba_reclaim(struct pba_context *, struct reclaim_req *,
			struct reclaim_resp *);
/*
 * so this is where i am actually supposed to delete the blocks and
 * stuff. i think remove is just supposed to remove the entry from the
 * directory. and reclaim is where we actually delete the blocks and
 * stuff
 *
 * why didnt node_reclaim2 work? i have no idea...
 */
int
puffboy_node_reclaim(struct puffs_usermount *pu, void *opc)
{
	struct pba_context *ctx;
	struct reclaim_req req;
	struct reclaim_resp resp;
	struct puffs_node *pn;

	pn = pba_cmap(pu, opc);
	if (!pn) {
		printf("COULDNT FIND NODE WITH ID %d\n", (int)opc);
		return ENOENT;
	}

	ctx = puffs_getspecific(pu);
	req.va_fileid = cookie_to_fileid(opc);

	gba_reclaim(ctx, &req, &resp);
	if (resp.err) {
		/* need better error translation most likely */
		printf("rm returned err %d\n", resp.err);
		return resp.err;
	}


	pn->pn_va.va_nlink = resp.va_nlink;
	if (pn->pn_va.va_nlink == 0) {
		puffs_pn_put(pn);
	}

	return 0;
}

static void
gba_reclaim(struct pba_context *ctx, struct reclaim_req *req,
	    struct reclaim_resp *resp)
{
	wait_clear(ctx->fd, DELAY, MSG_TIMEOUT);
	gba_write(ctx->fd, htonl(CMD_RECLAIM), &ctx->status, ctx->delay);
	send_request(ctx, req, sizeof(struct reclaim_req));
	receive_response(ctx, resp, sizeof(struct reclaim_resp));

	SSWAP32(resp, err);
	SSWAP32(resp, va_fileid);
	SSWAP32(resp, va_nlink);
}
