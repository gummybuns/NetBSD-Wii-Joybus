#include <sys/queue.h>

#include <puffs.h>
#include <stdio.h>
#include <errno.h>

#include "../pba.h"
#include "../../shared/command.h"

static void gba_lookup(int, struct lookup_req *, struct lookup_resp *);

int
puffboy_node_lookup(struct puffs_usermount *pu, puffs_cookie_t opc,
		    struct puffs_newinfo *pni,
		    const struct puffs_cn *pcn)
{
	struct pba_context *ctx;
	struct lookup_req req;
	struct lookup_resp resp;

	ctx = puffs_getspecific(pu);
	req.parent_fileid = cookie_to_fileid(opc);
	resp.exists = 0;

	printf("in node_lookup for %s\n", pcn->pcn_name);

	/* we are not concerning ourselves with parent directories */
	if (PCNISDOTDOT(pcn)) {
		printf("I AM IN HERE!!!!\n");
		printf("node_lookup PCN is DOTDOT\n");
		return ENOENT;
	}

	memcpy(req.name, pcn->pcn_name, sizeof(req.name));
	gba_lookup(ctx->fd, &req, &resp);
	if (!resp.exists) {
		printf("node_lookup is NULL\n");
		return ESTALE;
	}

	printf("node found!\n");
	puffs_newinfo_setcookie(pni, fileid_to_cookie(resp.va_fileid));
	puffs_newinfo_setvtype(pni, resp.va_type);
	puffs_newinfo_setsize(pni, (voff_t)resp.va_size);
	puffs_newinfo_setrdev(pni, resp.va_rdev);
  	return 0;
}

static void
gba_lookup(int fd, struct lookup_req *req, struct lookup_resp *resp)
{
	struct joybus_ctx ctx;

	ctx.fd = fd;
	ctx.delay = DELAY;

	wait_clear(fd, DELAY, MSG_TIMEOUT);
	gba_write(ctx.fd, htonl(CMD_LOOKUP), &ctx.status, ctx.delay);
	send_request(&ctx, req, sizeof(struct lookup_req));
	receive_response(&ctx, resp, sizeof(struct lookup_resp));

	resp->exists = bswap32(resp->exists);
	resp->va_type = bswap32(resp->va_type);
	resp->va_fileid = bswap32(resp->va_fileid);
}
