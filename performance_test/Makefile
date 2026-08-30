# $NetBSD: Makefile,v 1.2 2021/05/08 14:11:37 cjep Exp $
#
PROG=	pba
SRCS+=	main.c pba_ops.c
SRCS+=	fs/statvfs.c
SRCS+=	node/read.c node/create.c node/write.c
SRCS+=	node/lookup.c node/readdir.c node/pathconf.c node/getattr.c

CPPFLAGS+=	-D_KERNTYPES -g
DPADD+=		${LIBPUFFS}
LDADD+=		-lpuffs -lc -lutil

WARNS=	4

format:
	clang-format -i ./*.c

.include <bsd.prog.mk>
