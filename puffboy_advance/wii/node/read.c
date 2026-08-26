#include <puffs.h>
#include <errno.h>
#include <stdio.h>

#include "../pba.h"
#include "../../shared/command.h"
#include "../../../wiishared/lib/gcport_ioctl.h"

static void do_gba_read(struct pba_context *, struct read_req *,
		        struct read_resp *);
int
puffboy_node_read(struct puffs_usermount *pu, void *opc, uint8_t *buf,
		  off_t offset, size_t *resid, const struct puffs_cred *pcr,
		  int ioflag)
{
	struct pba_context *ctx;
	struct read_req req;
	struct read_resp resp;
	uint8_t *dest;

	printf("IN READ - resid: %d\n", *resid);
	ctx = puffs_getspecific(pu);

	req.fileid = cookie_to_fileid(opc);
	req.offset = offset;
	dest = buf;
	/*
	 * submit the offset and how many bytes to copy
	 * get back the bytes and how many were actually copied
	 * keep going to resid is zero or the resp copylen is zero (eof)
	 */
	while (*resid > 0) {
		req.copylen = MIN(*resid, BLOCKSIZE);
		//printf("req.copylen: %d\n", req.copylen);
		//printf("req.offset: %lld\n", req.offset);
		do_gba_read(ctx, &req, &resp);

		if (resp.err) {
			return resp.err;
		}

		if (!resp.copylen) {
			break;
		}

		memcpy(dest, resp.buf, resp.copylen);
		req.offset += resp.copylen;
		dest += resp.copylen;
		*resid -= resp.copylen;
	}

	printf("read done\n");
	return 0;
}

static void
do_gba_read(struct pba_context *ctx, struct read_req *req,
	    struct read_resp *resp)
{
	wait_clear(ctx->fd, DELAY, MSG_TIMEOUT);
	gba_write(ctx->fd, htonl(CMD_READ), &ctx->status, ctx->delay);
	send_request(ctx, req, sizeof(struct read_req));
	receive_response(ctx, resp, sizeof(struct read_resp));

	SSWAP32(resp, err);
	SSWAP32(resp, copylen);
}
