#ifndef _GCPORT_IOCTL_H
#define _GCPORT_IOCTL_H

#include <fcntl.h>

/* The command itself is only 1 byte, however the response to these commands
 * may not touch the subsequent bytes in the buf. By explicitly making the
 * command 4 bytes we ensure that the response has zeros instead of garbage
 */
#define CMD_IDENTIFY	0x00000000
#define CMD_RESET	0xFF000000

#define GBA_WRITE 	0x15
#define GBA_READ 	0x14

struct si_payload {
	uint32_t	insize;		/* number of bytes for in buffer */
	uint32_t	outsize;	/* number of bytes for out buffer */
	uint32_t	*status;	/* status from sisr */
	void		*in;		/* buffer to store response */
	void		*out;		/* buffer to send out to ext device */
	long		delay;		/* delay the transactoin (microsec) */
};


#define SI_SEND     	_IOWR(0, 1, struct si_payload)

static inline uint32_t
gba_write(int fd, uint32_t val, uint32_t *status, long delay)
{
       struct si_payload payload;
       uint8_t out[5];
       uint8_t in[1];
       uint8_t *p;

       p = out + 1;
       out[0] = GBA_WRITE;
       ((uint32_t *)p)[0] = val;
       payload.in = in;
       payload.out = out;
       payload.insize = 1;
       payload.outsize = 5;
       payload.delay = delay;
       payload.status = status;
       ioctl(fd, SI_SEND, &payload);
       return (uint32_t)(in[0]);
}

static inline uint32_t
gba_read(int fd, uint32_t *status, long delay)
{
       struct si_payload payload;
       uint8_t out[1];
       uint8_t in[5];

       out[0] = GBA_READ;
       payload.in = in;
       payload.out = out;
       payload.insize = 5;
       payload.outsize = 1;
       payload.delay = delay;
       payload.status = status;
       ioctl(fd, SI_SEND, &payload);

       /* first four bytes are the value. last byte is the status */
       return *(uint32_t *)in;
}

static inline uint32_t
cmd_identify(int fd, uint32_t *status, long delay)
{
	struct si_payload payload;
	uint32_t out[1];
	uint32_t in[1];
	out[0] = CMD_IDENTIFY;

	payload.outsize = 1;
	payload.insize = 3;
	payload.in = in;
	payload.out = out;
	payload.delay = delay;
	payload.status = status;

	ioctl(fd, SI_SEND, &payload);
	return in[0];
}

static inline uint32_t
cmd_reset(int fd, uint32_t *status, long delay)
{
	struct si_payload payload;
	uint32_t out[1];
	uint32_t in[1];
	int err;
	out[0] = CMD_RESET;

	payload.outsize = 1;
	payload.insize = 3;
	payload.in = in;
	payload.out = out;
	payload.delay = delay;
	payload.status = status;

	err = ioctl(fd, SI_SEND, &payload);
	return in[0];
}

#endif
