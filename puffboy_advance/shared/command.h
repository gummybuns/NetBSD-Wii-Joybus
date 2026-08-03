#ifndef _COMMAND_H
#define _COMMAND_H

#define CMD_NTH_ENTRY 0x0001
#define WORD_CNT(n) ((sizeof(n)+3)/4)

struct nth_entry_request {
	uint32_t	parent_fileid;
	uint32_t	n;
};

struct nth_entry_response {
	unsigned char	exists;
	uint32_t 	va_fileid;
	uint32_t	va_type;
	char 		name[32];
};
#endif
