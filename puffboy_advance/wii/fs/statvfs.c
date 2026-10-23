#include  <puffs.h>
#include <stdio.h>
#include <unistd.h>

#include "../pba.h"
#include "../../shared/command.h"
#include "../../../wiishared/lib/gcport_ioctl.h"

static void gba_statvfs(struct pba_context *, struct statvfs_resp *);

int
puffboy_fs_statvfs(struct puffs_usermount *pu, struct puffs_statvfs *sbp)
{
	printf("in statvfs\n");

	struct pba_context *ctx;
	struct statvfs_resp resp;

	ctx = puffs_getspecific(pu);
	gba_statvfs(ctx, &resp);

	printf("=====================\n");
	printf("bsize %d\n", resp.f_bsize);
	printf("frsize %d\n", resp.f_frsize);
	printf("blocks %d\n", resp.f_blocks);
	printf("bfree %d\n", resp.f_bfree);
	printf("bavail %d\n", resp.f_bavail);
	printf("bresvd %d\n", resp.f_bresvd);
	printf("files %d\n", resp.f_files);
	printf("ffree %d\n", resp.f_ffree);
	printf("favail %d\n", resp.f_favail);
	printf("fresvd %d\n", resp.f_fresvd);
	printf("=====================\n");
	sbp->f_bsize = resp.f_bsize;
	sbp->f_frsize = resp.f_frsize;
	sbp->f_blocks = resp.f_blocks;
	sbp->f_bfree = resp.f_bfree;
	sbp->f_bavail = resp.f_bavail;
	sbp->f_bresvd = resp.f_bresvd;

	sbp->f_files = resp.f_files;
	sbp->f_ffree = resp.f_ffree;
	sbp->f_favail = resp.f_favail;
	sbp->f_fresvd = resp.f_fresvd;

	return 0;
}

static void
gba_statvfs(struct pba_context * ctx, struct statvfs_resp *resp)
{
	wait_clear(ctx->fd, DELAY, MSG_TIMEOUT);
	gba_write(ctx->fd, htonl(CMD_STATVFS), &ctx->status, ctx->delay);
	receive_response(ctx, resp, sizeof(struct statvfs_resp));

	SSWAP32(resp, f_bsize);
	SSWAP32(resp, f_frsize);
	SSWAP32(resp, f_blocks);
	SSWAP32(resp, f_bfree);
	SSWAP32(resp, f_bavail);
	SSWAP32(resp, f_bresvd);
	SSWAP32(resp, f_files);
	SSWAP32(resp, f_ffree);
	SSWAP32(resp, f_favail);
	SSWAP32(resp, f_fresvd);
}
