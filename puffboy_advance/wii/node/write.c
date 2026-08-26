#include <puffs.h>
#include <errno.h>
#include <stdio.h>

#include "../pba.h"
#include "../../shared/command.h"
#include "../../../wiishared/lib/gcport_ioctl.h"

static void prepare_gba_write(struct pba_context *, struct write_req *,
			      struct write_resp *);
static void do_gba_write(struct pba_context *, struct write_buf_req *,
			 struct write_buf_resp *);

int
puffboy_node_write(struct puffs_usermount *pu, void *opc, uint8_t *buf,
		   off_t offset, size_t *resid, const struct puffs_cred *pcr,
		   int ioflag)
{
	struct pba_context *ctx;
	struct write_req wreq;
	struct write_resp wresp;
	struct write_buf_req breq;
	struct write_buf_resp bresp;
	size_t copylen;
	uint8_t *src;

	printf("IN NODE WRITE\n");
	ctx = puffs_getspecific(pu);
	wreq.fileid = cookie_to_fileid(opc);
	wreq.resid = *resid;
	wreq.offset = offset;
	wreq.io_append = (ioflag & PUFFS_IO_APPEND) > 0;
	prepare_gba_write(ctx, &wreq, &wresp);
	printf("finished prepare...\n");
	if (!wresp.exists) {
		printf("file doesnt exist so ENOENT\n");
		return ENOENT;
	}

	if (wresp.err) {
		printf("there is an error %d\n", wresp.err);
		return wresp.err;
	}

	printf("entering loop\n");
	src = buf;
	while (*resid > 0) {
		// TODO - constantize the 128 to a blocksize
		copylen = MIN(*resid, 128);
		memset(breq.buf, 0, 128);
		//printf("copylen is %d\n", copylen);
		memcpy(breq.buf, src, copylen);
		//printf("Writing:\n%s\n", breq.buf);
		breq.buflen = copylen;
		do_gba_write(ctx, &breq, &bresp);
		if (bresp.err > 0) {
			return bresp.err;
		}
		src += copylen;
		*resid -= copylen;
	}
	printf("write done\n");
	return 0;
}

static void
prepare_gba_write(struct pba_context *ctx, struct write_req *req,
	  struct write_resp * resp)
{
	wait_clear(ctx->fd, DELAY, MSG_TIMEOUT);
	gba_write(ctx->fd, htonl(CMD_WRITE), &ctx->status, ctx->delay);
	printf("send CMD_WRITE\n");
	send_request(ctx, req, sizeof(struct write_req));
	printf("sent request\n");
	receive_response(ctx, resp, sizeof(struct write_resp));

	SSWAP32(resp, exists);
	SSWAP32(resp, err);
}

static void
do_gba_write(struct pba_context *ctx, struct write_buf_req *req,
	     struct write_buf_resp *resp)
{
	send_request(ctx, req, sizeof(struct write_buf_req));
	receive_response(ctx, resp, sizeof(struct write_buf_resp));
	SSWAP32(resp, err);
}
