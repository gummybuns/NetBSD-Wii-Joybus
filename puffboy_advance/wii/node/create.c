#include <puffs.h>
#include <errno.h>
#include <stdio.h>

#include "../pba.h"
#include "../../shared/command.h"
#include "../../../wiishared/lib/gcport_ioctl.h"

static void gba_create(struct pba_context *, struct create_req *,
		       struct create_resp *);

int puffboy_node_create(struct puffs_usermount *pu, void *opc,
		        struct puffs_newinfo *pni, const struct puffs_cn *pcn,
			const struct vattr *va)
{
	struct pba_context *ctx;
	struct create_req req;
	struct create_resp resp;
	struct entry *ent;
	struct puffs_node *pn;
	struct vattr *pn_va;

	printf("i am in create\n");
	if (!(va->va_type == VREG || va->va_type == VSOCK)) {
		return ENODEV;
	}

	ctx = puffs_getspecific(pu);
	req.parent_fileid = cookie_to_fileid(opc);
	memcpy(req.name, pcn->pcn_name, sizeof(req.name));
	gba_create(ctx, &req, &resp);

	if (!resp.exists) {
		/* TODO - prolly better error handling */
		printf("failed to create node\n");
		return ENODEV;
	}

	ent = entry_init(pu, resp.va_fileid);

	pn = ent->pn;
	pn_va = &pn->pn_va;
	pn_va->va_uid = resp.va_uid;
	pn_va->va_gid = resp.va_gid;
	pn_va->va_fileid = resp.va_fileid;
	pn_va->va_size = resp.va_size;
	pn_va->va_blocksize = resp.va_blocksize;
	pn_va->va_gen = resp.va_gen;
	pn_va->va_flags = resp.va_flags;
	pn_va->va_rdev = (dev_t)resp.va_rdev;
	pn_va->va_bytes = resp.va_bytes;
	pn_va->va_filerev = resp.va_filerev;
	pn_va->va_vaflags = resp.va_vaflags;
	puffs_newinfo_setcookie(pni, fileid_to_cookie(resp.va_fileid));

	printf("finished create\n");
	return 0;
}

static void
gba_create(struct pba_context *ctx, struct create_req *req,
	   struct create_resp *resp)
{
	wait_clear(ctx->fd, DELAY, MSG_TIMEOUT);
	gba_write(ctx->fd, htonl(CMD_CREATE), &ctx->status, ctx->delay);
	send_request(ctx, req, sizeof(struct create_req));
	receive_response(ctx, resp, sizeof(struct create_resp));

	SSWAP32(resp, exists);
	SSWAP32(resp, va_fileid);
}
