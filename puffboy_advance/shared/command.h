#ifndef _COMMAND_H
#define _COMMAND_H

#define CMD_NTH_ENTRY 0x0001
#define WORD_CNT(n) ((sizeof(n)+3)/4)

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
	char		exists;
	char		va_type;
	uint32_t 	va_fileid;
	char 		name[31];
};
#endif
