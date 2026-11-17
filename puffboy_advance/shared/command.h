/**
 * I am pretty done trying to question thing for now the endianess has gotten
 * me to twisted..
 *
 * what i have landed on that appears to work:
 * sending - never swap bytes - send as is
 * receiving - call ntohl
 *   on the wii - this is a no-op
 *   on the gba - this is a swap
 * makes zero sense to me at this point but I need to move on with my life
 * then it is important that you swap the attributes themselves before they are
 * used. for example if there is a uint32_t in the response struct you need to
 * bswap32 it before you use it.
 */
#ifndef _COMMAND_H
#define _COMMAND_H

#if defined(__powerpc__)
	#include <arpa/inet.h>
	#include "../wii/pba.h"
	#include "../../wiishared/lib/gcport_ioctl.h"

	#define BSWAP16(n)	bswap16(n)
	#define BSWAP32(n)	bswap32(n)
	#define BSWAP64(n)	bswap64(n)
#else
	#include <cstring>
	#define BSWAP16(n)	__builtin_bswap16(n)
	#define BSWAP32(n)	__builtin_bswap32(n)
	#define BSWAP64(n)	__builtin_bswap64(n)

	#define htonl(n) 	BSWAP32(n)
	#define ntohl(n) 	BSWAP32(n)
	#define htons(n) 	BSWAP16(n)
	#define ntohs(n) 	BSWAP16(n)
#endif

#define SSWAP16(s, n)	((s)->n = BSWAP16((s)->n))
#define SSWAP32(s, n)	((s)->n = BSWAP32((s)->n))
#define SSWAP64(s, n)	((s)->n = BSWAP64((s)->n))

#define CMD_READY	111

#define CMD_STATVFS	0x0010
#define CMD_READDIR	0x0011
#define CMD_LOOKUP	0x0012
#define CMD_GETATTR	0x0013
#define CMD_CREATE	0x0014
#define CMD_WRITE	0x0015
#define CMD_READ	0x0016
#define CMD_MKDIR	0x0017
#define CMD_RM		0x0018
#define CMD_RECLAIM	0x0019
#define CMD_SETSIZE	0x001A

#define BLOCKSIZE	128
#define WORD_CNT(n) 	((sizeof(n)+3)/4)
#define SEQ_NUM(n) 	((n % 254) + 1)

struct statvfs_resp {
	uint32_t	f_bsize;	/* fs block size */
	uint32_t	f_frsize;	/* fundamental fs block size */
	uint32_t	f_blocks;	/* number of blocks in fs */
	uint32_t	f_bfree;	/* free blocks avail */
	uint32_t	f_bavail;	/* free blocks avail to non root */
	uint32_t	f_bresvd;	/* blocks reserved for root */
	uint32_t	f_files;	/* total file nodes in fs */
	uint32_t	f_ffree;	/* free file nodes in fs */
	uint32_t	f_favail;	/* free file nodes avail to non-root */
	uint32_t	f_fresvd;	/* file nodes reserved for root */
};

struct readdir_req {
	uint32_t	parent_fileid;
	uint32_t	n;
};

struct readdir_resp {
	uint32_t	exists;
	uint32_t	va_type;
	uint32_t 	va_fileid;
	char 		name[32];
};

struct lookup_req {
	uint32_t	parent_fileid;
	char		name[32];
};

struct lookup_resp {
	uint32_t	exists;
	uint32_t	va_fileid;
	uint32_t	va_type;
	uint64_t	va_size;
	uint64_t	va_rdev;
};

struct getattr_req {
	uint32_t	fileid;
};

struct getattr_resp {
	uint32_t	exists;
	uint32_t	va_type;
	uint32_t	va_mode;
	uint32_t	va_nlink;
	uint32_t	va_uid;
	uint32_t	va_gid;
	uint64_t	va_gen;
	uint64_t	va_fsid;
	uint32_t	va_fileid;
	uint64_t	va_size;
	uint32_t	va_blocksize;
	uint64_t	va_flags;
	uint64_t	va_rdev;
	uint64_t	va_bytes;
	uint64_t	va_filerev;
	uint64_t	va_vaflags;
	uint64_t	va_spare;
};

/* using create for mkdir as well for now */
struct create_req {
	uint32_t	parent_fileid;
	char		name[32];
};

/* using create for mkdir as well for now */
struct create_resp {
	uint32_t	exists;
	uint32_t	va_type;
	uint32_t	va_mode;
	uint32_t	va_nlink;
	uint32_t	va_uid;
	uint32_t	va_gid;
	uint64_t	va_gen;
	uint64_t	va_fsid;
	uint32_t	va_fileid;
	uint64_t	va_size;
	uint32_t	va_blocksize;
	uint64_t	va_flags;
	uint64_t	va_rdev;
	uint64_t	va_bytes;
	uint64_t	va_filerev;
	uint64_t	va_vaflags;
	uint64_t	va_spare;
};

struct write_req {
	uint32_t	fileid;
	uint32_t	io_append;
	uint64_t	offset;
	uint64_t	resid;
};

struct write_resp {
	uint32_t	exists;
	uint32_t	err;
};

struct write_buf_req {
	uint32_t	buflen;
	uint8_t		buf[BLOCKSIZE];
};

struct write_buf_resp {
	uint32_t	err;
};

struct read_req {
	uint32_t	fileid;
	uint32_t	copylen;
	uint64_t	offset;
};

struct read_resp {
	uint32_t	err;
	uint32_t	copylen;
	uint8_t		buf[BLOCKSIZE];
};

struct rm_req {
	uint32_t parent_fileid;
	char 		name[32];
};

struct rm_resp {
	uint32_t err;
	uint32_t va_fileid;
	uint32_t va_nlink;
};

struct reclaim_req {
	uint32_t va_fileid;
};

struct reclaim_resp {
	uint32_t err;
	uint32_t va_fileid;
	uint32_t va_nlink;
};

struct setattr_req {
	uint32_t va_fileid;
	uint64_t va_size;
};

struct setattr_resp {
	uint32_t err;
	uint32_t va_fileid;
	uint64_t va_bytes;
	uint64_t va_size;
};

static inline void
send_request(void *ctx, void *req, size_t sz)
{
	size_t i;
	uint8_t *b;

#if defined(__powerpc__)
	struct pba_context *jbctx = ctx;
	uint8_t out[4];
	uint32_t cur;

	b = (uint8_t *)req;
	i = 0;
	while (i < sz) {
		memset(out, 0, 4);
		out[0] = b[i];
		if ((i+1) < sz) out[1] = b[i+1];
		if ((i+2) < sz) out[2] = b[i+2];
		if ((i+3) < sz) out[3] = b[i+3];
		cur = *(uint32_t *)out;
		gba_write(jbctx->fd, cur, &jbctx->status, jbctx->delay);
		i += 4;
	}
#else
	uint8_t out_arr[sz + 4];
	size_t j;

	memset(out_arr, 0, sz + 4);
	out_arr[0] = CMD_READY;

	b = (uint8_t *)req;
	i = 0;
	j = 4;
	while (i < sz) {
		out_arr[j] = b[i];
		if ((i+1) < sz) out_arr[j+1] = b[i+1];
		if ((i+2) < sz) out_arr[j+2] = b[i+2];
		if ((i+3) < sz) out_arr[j+3] = b[i+3];
		i += 4;
		j += 4;
	}

	((LinkCube *)ctx)->sendArr(out_arr, sz + 4);
#endif
}

static inline void
receive_response(void *ctx, void *resp, size_t sz)
{
	size_t i;
	uint32_t in;
	uint8_t *b;

	b = (uint8_t *)resp;

#if defined(__powerpc__)
	struct pba_context *jbctx = ctx;

	while (1) {
		in = BSWAP32(ntohl(gba_read(jbctx->fd, &jbctx->status,
			     jbctx->delay)));
		if (in == CMD_READY) break;
	}

	i = 0;
	while (i < sz) {
		in = BSWAP32(ntohl(gba_read(jbctx->fd, &jbctx->status,
					    jbctx->delay)));
		b[i] = (in) & 0xFF;
		if ((i+1) < sz) b[i+1] = (in >> 8) & 0xFF;
		if ((i+2) < sz) b[i+2] = (in >> 16) & 0xFF;
		if ((i+3) < sz) b[i+3] = (in >> 24) & 0xFF;
		i += 4;
	}
#else
	i = 0;
	while (i < sz) {
		LinkCube *linkCube = (LinkCube *)ctx;
		while (!linkCube->canRead());
		in = ntohl(linkCube->read());
		b[i] = (in >> 24) & 0xFF;
		if ((i+1) < sz) b[i+1] = (in >> 16) & 0xFF;
		if ((i+2) < sz) b[i+2] = (in >> 8) & 0xFF;
		if ((i+3) < sz) b[i+3] = in & 0xFF;
		i += 4;
	}
#endif

	memcpy(resp, b, sz);
}

#endif
