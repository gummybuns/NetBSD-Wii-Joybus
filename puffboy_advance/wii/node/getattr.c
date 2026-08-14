#include <puffs.h>
#include <stdio.h>
#include <errno.h>

#include "../pba.h"
#include "../../shared/command.h"
#include "../../../wiishared/lib/gcport_ioctl.h"

static void gba_getattr(struct pba_context *, struct getattr_req *,
		        struct getattr_resp *);

int
puffboy_node_getattr(struct puffs_usermount *pu, puffs_cookie_t opc,
		     struct vattr *vap, const struct puffs_cred *pcr)
{
	struct pba_context *ctx;
	struct getattr_req req;
	struct getattr_resp resp;

	printf("in getattr %p\n", opc);
	ctx = puffs_getspecific(pu);
	req.fileid = cookie_to_fileid(opc);
	resp.exists = 0;

	gba_getattr(ctx, &req, &resp);
	if (!resp.exists) {
		printf("cant be found\n");
		return ESTALE;
	}

	vap->va_type = resp.va_type;
	vap->va_mode = resp.va_mode;
	vap->va_nlink = resp.va_nlink;
	vap->va_uid = resp.va_uid;
	vap->va_gid = resp.va_gid;
	vap->va_fsid = resp.va_fsid;
	vap->va_fileid = resp.va_fileid;
	vap->va_size = resp.va_size;
	vap->va_blocksize = resp.va_blocksize;
	vap->va_flags = resp.va_flags;
	vap->va_rdev = resp.va_rdev;
	vap->va_bytes = resp.va_bytes;
	vap->va_filerev = resp.va_filerev;
	vap->va_vaflags = resp.va_vaflags;
	vap->va_spare = resp.va_spare;
	return 0;
}

static void
gba_getattr(struct pba_context *ctx, struct getattr_req *req, struct getattr_resp *resp)
{
	wait_clear(ctx->fd, DELAY, MSG_TIMEOUT);
	gba_write(ctx->fd, htonl(CMD_GETATTR), &ctx->status, ctx->delay);
	send_request(ctx, req, sizeof(struct getattr_req));
	receive_response(ctx, resp, sizeof(struct getattr_resp));

	resp->exists = bswap32(resp->exists);
	resp->va_type = bswap32(resp->va_type);
	resp->va_mode = bswap32(resp->va_mode);
	resp->va_nlink = bswap32(resp->va_nlink);
	resp->va_uid = bswap32(resp->va_uid);
	resp->va_gid = bswap32(resp->va_gid);
	resp->va_gen = bswap64(resp->va_gen);
	resp->va_fsid = bswap64(resp->va_fsid);
	resp->va_fileid = bswap32(resp->va_fileid);
	resp->va_size = bswap64(resp->va_size);
	resp->va_size = bswap32(resp->va_blocksize);
	resp->va_flags = bswap64(resp->va_flags);
	resp->va_rdev = bswap64(resp->va_rdev);
	resp->va_bytes = bswap64(resp->va_bytes);
	resp->va_filerev = bswap64(resp->va_filerev);
	resp->va_vaflags = bswap64(resp->va_vaflags);
	resp->va_spare = bswap64(resp->va_spare);
}
