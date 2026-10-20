#include <errno.h>
#include <puffs.h>
#include <stdio.h>

#include "../pba.h"
#include "../../shared/command.h"
#include "../../../wiishared/lib/gcport_ioctl.h"

static void gba_setsize(struct pba_context *ctx, struct setattr_req *req,
			struct setattr_resp *resp);

/*
 * only concerned with resize changes for now
 * see h_dtfs/dtfs_vnops.c in NetBSD-src for the other things to still do
 */
int
puffboy_node_setattr(struct puffs_usermount *pu, void *opc,
		     const struct vattr *va, const struct puffs_cred *pcr)
{
	struct pba_context *ctx;
	struct setattr_req req;
	struct setattr_resp resp;
	struct puffs_node *pn;

	pn = pba_cmap(pu, opc);
	if (!pn) {
		printf("COULDNT FIND NODE WITH ID %d\n", (int)opc);
		return ENOENT;
	}

	ctx = puffs_getspecific(pu);
	req.va_fileid = cookie_to_fileid(opc);

	/* originally wanted a single gba_setattr and for gba to decide
	 * what to do by PUFFS_VNOVAL. but that is -1 and i dont want
	 * to worry about signed stuff over the wire although its prolly
	 * easy i dont have the brain energy
	 */
	if (va->va_size != (u_quad_t)PUFFS_VNOVAL) {
		switch (pn->pn_va.va_type) {
		case VREG:
			req.va_size = va->va_size;
			gba_setsize(ctx, &req, &resp);
			if (resp.err) {
				printf("setattr returned err %d\n", resp.err);
				return resp.err;
			}
			pn->pn_va.va_size = resp.va_size;
			/* for some reason h_dtfs sets the va_bytes to the
			 * new size, even though the dtfs_setsize sets the
			 * va_bytes. what i noticed is the va_bytes always comes
			 * out to zero. so this is probably some sort of bug
			 * fix. i will keep the same for now although there
			 * is now a mismatch in the gba and the puff node...
			 */
			pn->pn_va.va_bytes = resp.va_size;
			break;
		case VBLK:
		case VCHR:
		case VFIFO:
			break;
		case VDIR:
			return EISDIR;
		default:
			return EOPNOTSUPP;
		}
	}

	return 0;
}

static void
gba_setsize(struct pba_context *ctx, struct setattr_req *req,
	    struct setattr_resp *resp)
{
	wait_clear(ctx->fd, DELAY, MSG_TIMEOUT);
	gba_write(ctx->fd, htonl(CMD_SETSIZE), &ctx->status, ctx->delay);
	send_request(ctx, req, sizeof(struct setattr_req));
	receive_response(ctx, resp, sizeof(struct setattr_resp));

	SSWAP32(resp, err);
	SSWAP32(resp, va_fileid);
	SSWAP64(resp, va_size);
	SSWAP64(resp, va_bytes);
}
