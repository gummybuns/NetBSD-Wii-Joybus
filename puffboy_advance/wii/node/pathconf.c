#include <errno.h>
#include <puffs.h>
#include <unistd.h>

#include "../pba.h"

int
puffboy_node_pathconf(struct puffs_usermount *pu, puffs_cookie_t opc,
	int name, register_t *retval)
{

	switch (name) {
	case _PC_LINK_MAX:
		*retval = LINK_MAX;
		return 0;
	case _PC_NAME_MAX:
		*retval = NAME_MAX;
		return 0;
	case _PC_PATH_MAX:
		*retval = PATH_MAX;
		return 0;
	case _PC_PIPE_BUF:
		*retval = PIPE_BUF;
		return 0;
	case _PC_CHOWN_RESTRICTED:
		*retval = 1;
		return 0;
	case _PC_NO_TRUNC:
		*retval = 1;
		return 0;
	case _PC_SYNC_IO:
		*retval = 1;
		return 0;
	case _PC_FILESIZEBITS:
		*retval = 43; /* this one goes to 11 */
		return 0;
	case _PC_SYMLINK_MAX:
		*retval = MAXPATHLEN;
		return 0;
	case _PC_2_SYMLINKS:
		*retval = 1;
		return 0;
	default:
		return EINVAL;
	}
}
