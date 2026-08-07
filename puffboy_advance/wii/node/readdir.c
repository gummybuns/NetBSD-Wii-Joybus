#include <sys/queue.h>

#include <puffs.h>
#include <stdio.h>
#include <errno.h>

#include "../pba.h"
#include "../../shared/command.h"
#include "../../../wiishared/lib/gcport_ioctl.h"

static struct gba_node * get_nth_entry(int, struct gba_node *, int);

int
puffboy_node_readdir(struct puffs_usermount *pu, puffs_cookie_t opc,
		     struct dirent *dent, off_t *readoff, size_t *reslen,
		     const struct puffs_cred *pcr, int *eofflag, off_t *cookies,
		     size_t *ncookies)
{
	struct puffs_node *pn;
	struct pba_context *ctx;
	struct gba_node *gn;

	printf("in readdir\n");
	ctx = puffs_getspecific(pu);
	pn = opc;

	/* my understanding is that ncookies is always initialized to 0 */
	*ncookies = 0;

	/* dont perform for non directories */
	if (pn->pn_va.va_type != VDIR) {
		printf("in node_readdir pn is not a VDIR\n");
		return ENOTDIR;
	}

again:
	if (*readoff == DENT_DOT || *readoff == DENT_DOTDOT) {
		//printf("is DENT_DOT / DENT_DOTDOT\n");
		puffs_gendotdent(&dent, pn->pn_va.va_fileid, (int)*readoff, reslen);
		(*readoff)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *readoff);
		//printf("finished DENT_DOT logic. running again\n");
		goto again;
	}

	for (;;) {
		printf("getting %dth entry\n", (int)DENT_ADJ(*readoff));
		if (pn->pn_data == NULL) {
			printf("PN_DATA IS NULL\n");
		}
		printf("CALLING GET_NTH_ENTRY\n");
		gn = get_nth_entry(ctx->fd, pn->pn_data, (int)DENT_ADJ(*readoff));
		if (!gn) {
			*eofflag = 1;
			return 0;
			//break;
		}

		// i dont need pn_nth since the gba_node _should_ have everything
		// i need. or at least the gba response includes it so i have to
		// change the gba_node to reflect
		/*
		pn_nth = gn->pn;
		if (!puffs_nextdent(&dent, gn->name, gn->va_fileid, (uint8_t)puffs_vtype2dt(gn->va_type), reslen)) {
			break;
		}

		(*readoff)++;
		PUFFS_STORE_DCOOKIE(cookies, ncookies, *readoff);
		*/
	}

	return 0;
}

static struct packet                                                                      
to_packet(uint32_t val)                                                            
{                                                                                  
        struct packet pk;                                                          
        pk.seq = (val >> 24) & 0xFF;                                               
        pk.cmd = (val >> 16) & 0xFF;                                               
        pk.data = val & 0xFFFF;                                                    
        return pk;                                                                 
}


static void
send_request(int fd, void *req, size_t sz)
{
	size_t i;
	uint32_t status, out;
	struct packet pk;
	uint8_t buf[sz];

	memcpy(buf, req, sz);
	for (i = 0; i < sz / 2; i++) {
		pk.seq = SEQ_NUM(i);
		pk.cmd = 1;
		pk.data = ((uint16_t *)buf)[i];
		memcpy(&out, &pk, sizeof(uint32_t));
		printf("sending 0x%08X\n", out);
		gba_write(fd, bswap32(out), &status, DELAY);
	}
}

static struct gba_node *
get_nth_entry(int fd, struct gba_node *parent, int n)
{
	struct nth_entry_request req;
	struct nth_entry_response res;
	uint32_t res_buf[sizeof(struct nth_entry_response)];
	uint32_t status, in, in2;
	struct packet pk, pk2;
	size_t i;

	req.parent_fileid = parent->id;
	req.n = n;
	gba_write(fd, bswap32(CMD_NTH_ENTRY), &status, DELAY);
	wait_clear(fd, DELAY, MSG_TIMEOUT);
	send_request(fd, &req, sizeof(struct nth_entry_request));

	i = 0;
	while (i < WORD_CNT(struct nth_entry_response)) {
		for (;;) {
			in = bswap32(gba_read(fd, &status, DELAY));
			pk = to_packet(in);
			if (pk.seq == 6 && pk.cmd == 7) {
				printf("RECEIVED 0x%08X\n", in);
				break;
			}
		}
		for (;;) {
			in2 = bswap32(gba_read(fd, &status, DELAY));
			pk2 = to_packet(in2);
			if (pk2.seq == 6 && pk2.cmd == 7) {
				printf("RECEIVED 0x%08X\n", in2);
				break;
			}
		}
		res_buf[i] = bswap16(pk2.data) | (bswap16(pk.data) << 16);
		i++;
	}

	memcpy(&res, res_buf, sizeof(struct nth_entry_response));
	res.va_fileid = bswap32(res.va_fileid);
	printf("GOT A RESPONSE!\n");
	printf("exists: %d 0x%08X\n", res.exists, res.exists);
	printf("fileid: %d 0x%08X\n", res.va_fileid, res.va_fileid);
	printf("type: %d 0x%08xX\n", res.va_type, res.va_type);
	printf("name: %s\n", res.name);
	errx(1, "done");
	return NULL;
}

/*
static struct gba_node *
get_nth_entry(struct gba_node *gn, int n)
{
	struct gba_node *entry;
	int i;

	i = 0;
	SLIST_FOREACH(entry, &gn->head, entries) {
		printf("looking for %d, currently %d\n", n, i);
		printf("current entry has name %s\n", entry->name);
		if (i == n) {
			return entry;
		}
		i++;
	}

	printf("entry %d could not be found\n", n);
	return NULL;
}
*/
