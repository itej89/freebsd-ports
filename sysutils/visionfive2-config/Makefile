PORTNAME=	visionfive2-config
PORTVERSION=	20260922.1
CATEGORIES=	sysutils

MAINTAINER=	itej89@gmail.com
COMMENT=	Board configuration for FreeBSD on the StarFive VisionFive 2
WWW=		https://github.com/itej89/freebsd-src

LICENSE=	BSD2CLAUSE

ONLY_FOR_ARCHS=	riscv64
ONLY_FOR_ARCHS_REASON=	configuration for the StarFive JH7110, a riscv64 SoC

# No upstream: everything ships in files/.
DISTFILES=	#
EXTRACT_ONLY=	#
NO_WRKSUBDIR=	yes

USE_RC_SUBR=	vf2_display

SUB_FILES=	pkg-message

PLIST_FILES=	sbin/vf2-setmode \
		"@sample /boot/loader.conf.d/visionfive2.conf.sample"

# vf2-setmode talks to /dev/dri with raw ioctls rather than libdrm: the libdrm
# package here ships the kernel ABI headers but not xf86drm.h, and a modeset
# this small does not justify the dependency.
do-build:
	${CC} ${CFLAGS} -I${LOCALBASE}/include \
	    -o ${WRKDIR}/vf2-setmode ${FILESDIR}/vf2-setmode.c

do-install:
	${INSTALL_PROGRAM} ${WRKDIR}/vf2-setmode ${STAGEDIR}${PREFIX}/sbin/
	@${MKDIR} ${STAGEDIR}/boot/loader.conf.d
	${INSTALL_DATA} ${FILESDIR}/visionfive2.conf \
	    ${STAGEDIR}/boot/loader.conf.d/visionfive2.conf.sample

.include <bsd.port.mk>
