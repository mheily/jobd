# $FreeBSD$

PORTNAME=	relaunchd
PORTVERSION=	0.4.1
DISTVERSIONPREFIX=v
CATEGORIES=	sysutils

MAINTAINER=	mark@heily.com
COMMENT=	Service management daemon similar to Darwin's launchd(8)

LICENSE=	ISCL

USE_GITHUB=	YES
GH_ACCOUNT=	mheily

LIB_DEPENDS=	libucl.so:${PORTSDIR}/textproc/libucl

.include <bsd.port.mk>
