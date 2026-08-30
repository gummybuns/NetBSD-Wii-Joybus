# $NetBSD: Makefile,v 1.2 2021/05/08 14:11:37 cjep Exp $
#
PROG=	performance_test
SRCS+=	main.c

CPPFLAGS+=	-D_KERNTYPES -g
DPADD+=		${LIBPUFFS}
LDADD+=		-lc -lutil

WARNS=	4

format:
	clang-format -i ./*.c

.include <bsd.prog.mk>
