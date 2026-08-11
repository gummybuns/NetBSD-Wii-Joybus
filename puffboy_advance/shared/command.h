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

#define CMD_NTH_ENTRY 0x0001
#define WORD_CNT(n) ((sizeof(n)+3)/4)
#define SEQ_NUM(n) ((n % 254) + 1)

struct packet {
	uint8_t seq;
	uint8_t cmd;
	uint16_t data;
};

struct nth_entry_request {
	uint32_t	parent_fileid;
	uint32_t	n;
};

struct nth_entry_response {
	uint32_t		exists;
	uint32_t		va_type;
	uint32_t 		va_fileid;
	char 			name[32];
};

#if defined(__powerpc__)
	#include <arpa/inet.h>
	#include "../../wiishared/lib/gcport_ioctl.h"

	struct joybus_ctx {
		int fd;
		uint32_t status;
		long delay;
	};
#else
	#include <cstring>
	#define htonl(n) __builtin_bswap32(n)
	#define ntohl(n) __builtin_bswap32(n)
	#define htons(n) __builtin_bswap16(n)
	#define ntohs(n) __builtin_bswap16(n)

	static inline void
	print_u32(uint32_t v)
	{
		char msg[100];
		uint8_t p1, p2, p3, p4;
		p1 = ((uint8_t *)&v)[0];
		p2 = ((uint8_t *)&v)[1];
		p3 = ((uint8_t *)&v)[2];
		p4 = ((uint8_t *)&v)[3];
		snprintf(msg, sizeof(msg), "%d - %02X %02X %02X %02X\n", v, p1, p2, p3, p4);
		tte_write(msg);
	}
#endif

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
		struct joybus_ctx *jbctx = ctx;
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
		struct joybus_ctx *jbctx = ctx;
		recv = ntohl(gba_read(jbctx->fd, &jbctx->status, 50));
# else
		LinkCube *linkCube = (LinkCube *)ctx;
		// TODO need to inform the wii about this or maybe i just retry?
		if (!linkCube->canRead()) break;
		recv = ntohl(linkCube->read());
#endif
		pk = to_packet(recv);
		if (pk.seq == 0 || pk.cmd == 0) continue;
		((uint16_t *)buf)[i] = ntohs(pk.data);
		i++;
	}

	memcpy(resp, buf, sz);
}
