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

#define	__BIT(__n)							      \
	(((__UINTMAX_TYPE__)(__n) >= __CHAR_BIT__ * sizeof(__UINTMAX_TYPE__)) \
	    ? 0								      \
	    : ((__UINTMAX_TYPE__)1 <<					      \
		(__UINTMAX_TYPE__)((__n) &				      \
		    (__CHAR_BIT__ * sizeof(__UINTMAX_TYPE__) - 1))))

/* __MASK(n): first n bits all set, where __MASK(4) == 0b1111. */
#define	__MASK(__n)	(__BIT(__n) - 1)

/* Macros for min/max. */
#define	__MIN(a,b)	((/*CONSTCOND*/(a)<=(b))?(a):(b))
#define	__MAX(a,b)	((/*CONSTCOND*/(a)>(b))?(a):(b))

/* __BITS(m, n): bits m through n, m < n. */
#define	__BITS(__m, __n)	\
	((__BIT(__MAX((__m), (__n)) + 1) - 1) ^ (__BIT(__MIN((__m), (__n))) - 1))

/* find least significant bit that is set */
#define	__LOWEST_SET_BIT(__mask) ((((__mask) - 1) & (__mask)) ^ (__mask))

#define	__PRIuBIT	PRIuMAX
#define	__PRIuBITS	__PRIuBIT

#define	__PRIxBIT	PRIxMAX
#define	__PRIxBITS	__PRIxBIT

#define	__SHIFTOUT(__x, __mask)	(((__x) & (__mask)) / __LOWEST_SET_BIT(__mask))
#define	__SHIFTIN(__x, __mask) ((__x) * __LOWEST_SET_BIT(__mask))
#define	__SHIFTOUT_MASK(__mask) __SHIFTOUT((__mask), (__mask))

#endif

#define SSWAP16(s, n)	((s)->n = BSWAP16((s)->n))
#define SSWAP32(s, n)	((s)->n = BSWAP32((s)->n))
#define SSWAP64(s, n)	((s)->n = BSWAP64((s)->n))

#define CMD_READDIR	0x0011
#define CMD_LOOKUP	0x0012
#define CMD_GETATTR	0x0013
#define CMD_CREATE	0x0014
#define CMD_WRITE	0x0015
#define CMD_READ	0x0016

#define BLOCKSIZE	128
#define WORD_CNT(n) 	((sizeof(n)+3)/4)
#define SEQ_NUM(n) 	((n % 254) + 1)

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

static void
do_send(void *ctx, uint32_t pkt)
{
#if defined(__powerpc__)
	struct pba_context *jbctx = ctx;
	gba_write(jbctx->fd, pkt, &jbctx->status, jbctx->delay);
#else
	((LinkCube *)ctx)->send(pkt);
#endif
}

static inline void
send_request(void *ctx, void *req, size_t sz)
{
	size_t i;
	uint8_t b[sz];
	uint8_t out[4];
	uint32_t remainder, cur, pkt;

	// why do i need a memcpy? cant i just cast as uint8_t
	i = 0;
	remainder = 0;
	memcpy(b, req, sz);
	while (i < sz) {
		memset(out, 0, 4);
		out[0] = b[i];
		if ((i+1) < sz) out[1] = b[i+1];
		if ((i+2) < sz) out[2] = b[i+2];
		if ((i+3) < sz) out[3] = b[i+3];

		cur = *(uint32_t *)out;
		pkt = (1 << 31) | remainder | (cur & __BITS(0, 30));
		do_send(ctx, pkt);
		remainder = (cur & __BIT(31)) >> 1;
		i += 4;
	}

	pkt = (1 << 31) | remainder;
	do_send(ctx, pkt);
}


static uint32_t
do_receive(void *ctx)
{
#if defined(__powerpc__)
		struct pba_context *jbctx = ctx;
		uint32_t recv;
		while (1) {
			recv = BSWAP32(ntohl(gba_read(jbctx->fd, &jbctx->status, jbctx->delay)));
			if (recv != 0) break;
		}
		return recv;
#else
		LinkCube *linkCube = (LinkCube *)ctx;
		while (!linkCube->canRead());
		return ntohl(linkCube->read());
#endif
}

static inline void
receive_response(void *ctx, void *resp, size_t sz)
{
	size_t i;
	uint32_t in, recv;
	uint8_t b[sz];

	i = 0;
	recv = do_receive(ctx);
	in = recv & __BITS(0, 30);

	while (i < sz) {
		recv = do_receive(ctx);
		in |= ((recv & __BIT(31)) << 1);
#if defined(__powerpc__)
		b[i] = (in) & 0xFF;
		if ((i+1) < sz) b[i+1] = (in >> 8) & 0xFF;
		if ((i+2) < sz) b[i+2] = (in >> 16) & 0xFF;
		if ((i+3) < sz) b[i+3] = (in >> 24) & 0xFF;
# else
		b[i] = (in >> 24) & 0xFF;
		if ((i+1) < sz) b[i+1] = (in >> 16) & 0xFF;
		if ((i+2) < sz) b[i+2] = (in >> 8) & 0xFF;
		if ((i+3) < sz) b[i+3] = in & 0xFF;
#endif

		in = recv & __BITS(0, 30);
		i += 4;
	}
	memcpy(resp, b, sz);
}
#endif
