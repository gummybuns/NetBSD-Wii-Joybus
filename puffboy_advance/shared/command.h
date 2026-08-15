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

#define WORD_CNT(n) 	((sizeof(n)+3)/4)
#define SEQ_NUM(n) 	((n % 254) + 1)

/* set and swap functions */
#define SSWAP16(s, n)	((s)->n = BSWAP16((s)->n))
#define SSWAP32(s, n)	((s)->n = BSWAP32((s)->n))
#define SSWAP64(s, n)	((s)->n = BSWAP64((s)->n))


struct packet {
	uint8_t seq;
	uint8_t cmd;
	uint16_t data;
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

static struct packet                                                                      
to_packet(uint32_t val)                                                            
{                                                                                  
        struct packet pk;                                                          
        pk.seq = (val >> 24) & 0xFF;                                               
        pk.cmd = (val >> 16) & 0xFF;                                               
        pk.data = val & 0xFFFF;                                                    
        return pk;                                                                 
}

static inline void
send_request(void *ctx, void *req, size_t sz)
{
	size_t i;
	uint32_t out;
	struct packet pk;
	uint8_t buf[sz];

	memcpy(buf, req, sz);
	for (i = 0; i < sz / 2; i++) {
		pk.seq = SEQ_NUM(i);
		pk.cmd = 1;
		pk.data = ((uint16_t *)buf)[i];
		memcpy(&out, &pk, sizeof(uint32_t));
#if defined(__powerpc__)
		struct pba_context *jbctx = ctx;
		gba_write(jbctx->fd, out, &jbctx->status, jbctx->delay);
# else
		((LinkCube *)ctx)->send(out);
#endif
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
	while (i < sz / 2) {
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
		if (pk.seq == 0 || pk.cmd == 0) continue;
		((uint16_t *)buf)[i] = ntohs(pk.data);
		i++;
	}
	memcpy(resp, buf, sz);
}
