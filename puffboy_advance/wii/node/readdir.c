#include <puffs.h>
#include <stdio.h>
#include <errno.h>

#include "../pba.h"
#include "../../shared/command.h"
#include "../../../wiishared/lib/gcport_ioctl.h"

static void gba_readdir(int, struct readdir_req *, struct readdir_resp *);

int
puffboy_node_readdir(struct puffs_usermount *pu, puffs_cookie_t opc,
		     struct dirent *dent, off_t *readoff, size_t *reslen,
		     const struct puffs_cred *pcr, int *eofflag, off_t *cookies,
		     size_t *ncookies)
{
	struct pba_context *ctx;
	struct readdir_req req;
	struct readdir_resp resp;
	uint32_t id;

	printf("in readdir\n");
	ctx = puffs_getspecific(pu);
	id = cookie_to_fileid(opc);
	printf("id is %d\n", id);
	resp.exists = 0;

	*ncookies = 0;

again:
	if (*readoff == DENT_DOT || *readoff == DENT_DOTDOT) {
		// TODO - using 1 but should make a constant
		puffs_gendotdent(&dent, 1, (int)*readoff, reslen);
		(*readoff)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *readoff);
		goto again;
	}

	for (;;) {
		req.parent_fileid = id;
		req.n = (int)DENT_ADJ(*readoff);
		gba_readdir(ctx->fd, &req, &resp);
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
gba_readdir(int fd, struct readdir_req *req, struct readdir_resp *resp)
{
	struct joybus_ctx ctx;

	ctx.fd = fd;
	ctx.delay = DELAY;

	wait_clear(fd, DELAY, MSG_TIMEOUT);
	gba_write(ctx.fd, htonl(CMD_READDIR), &ctx.status, ctx.delay);
	send_request(&ctx, req, sizeof(struct readdir_req));
	receive_response(&ctx, resp, sizeof(struct readdir_resp));

	resp->exists = bswap32(resp->exists);
	resp->va_fileid = bswap32(resp->va_fileid);
	resp->va_type = bswap32(resp->va_type);
}
