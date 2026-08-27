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

#define CMD_READDIR	0x0011
#define CMD_LOOKUP	0x0012
#define CMD_GETATTR	0x0013
#define CMD_CREATE	0x0014
#define CMD_WRITE	0x0015
#define CMD_READ	0x0016

#define BLOCKSIZE	128
#define WORD_CNT(n) 	((sizeof(n)+3)/4)
#define SEQ_NUM(n) 	((n % 254) + 1)

/* set and swap functions */
#define SSWAP16(s, n)	((s)->n = BSWAP16((s)->n))
#define SSWAP32(s, n)	((s)->n = BSWAP32((s)->n))
#define SSWAP64(s, n)	((s)->n = BSWAP64((s)->n))


struct packet {
	uint8_t seq;
	uint8_t data[3];
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

struct create_req {
	uint32_t	parent_fileid;
	char		name[32];
};

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

static struct packet
to_packet(uint32_t val)
{
        struct packet pk;
        pk.seq = (val >> 24) & 0xFF;
	pk.data[0] = (val >> 16) & 0xFF;
	pk.data[1] = (val >> 8) & 0xFF;
	pk.data[2] = val & 0xFF;
        return pk;
}

static inline void
send_request(void *ctx, void *req, size_t sz)
{
	size_t i;
	uint32_t out;
	struct packet pk;
	uint8_t buf[sz];

	// why do i need a memcpy? cant i just cast as uint8_t
	i = 0;
	memcpy(buf, req, sz);
	while (i < sz) {
		memset(pk.data, 0, 3);
		pk.seq = SEQ_NUM(i);
		pk.data[0] = buf[i];
		if ((i+1) < sz) pk.data[1] = buf[i+1];
		if ((i+2) < sz) pk.data[2] = buf[i+2];
		memcpy(&out, &pk, sizeof(uint32_t));
#if defined(__powerpc__)
		struct pba_context *jbctx = ctx;
		gba_write(jbctx->fd, out, &jbctx->status, jbctx->delay);
# else
		((LinkCube *)ctx)->send(out);
#endif
		i += 3;
	}
}
#endif


static inline void
receive_response(void *ctx, void *resp, size_t sz)
{
	size_t i;
	uint32_t recv;
	struct packet pk;
	uint8_t buf[sz * 2];

	i = 0;
	while (i < sz) {
#if defined(__powerpc__)
		struct pba_context *jbctx = ctx;
		jbctx->status = 0;
		recv = ntohl(gba_read(jbctx->fd, &jbctx->status, jbctx->delay));
# else
		LinkCube *linkCube = (LinkCube *)ctx;
		// TODO need to inform the wii about this or maybe i just retry?
		if (!linkCube->canRead()) continue;
		recv = ntohl(linkCube->read());
#endif
		pk = to_packet(recv);
		if (pk.seq == 0) continue;
		buf[i] = pk.data[0];
		if ((i+1) < sz) buf[i+1] = pk.data[1];
		if ((i+2) < sz) buf[i+2] = pk.data[2];
		i += 3;
	}
	// can i avoid this memcpy as well? if i somehow cast resp as a uint8_t
	// buffer can i just write to it directly
	memcpy(resp, buf, sz);
}
