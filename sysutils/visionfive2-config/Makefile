PORTNAME=	visionfive2-config
PORTVERSION=	20260922.1
PORTREVISION=	1
CATEGORIES=	sysutils

MAINTAINER=	itej89@gmail.com
COMMENT=	Board configuration for FreeBSD on the StarFive VisionFive 2
WWW=		https://github.com/itej89/freebsd-src

LICENSE=	BSD2CLAUSE

ONLY_FOR_ARCHS=	riscv64

# Headers only: vf2-setmode includes <libdrm/drm.h> and <libdrm/drm_mode.h>
# but links against no library -- see the note on do-build below.
BUILD_DEPENDS=	${LOCALBASE}/include/libdrm/drm.h:graphics/libdrm
ONLY_FOR_ARCHS_REASON=	configuration for the StarFive JH7110, a riscv64 SoC

# The vf2_display service kldloads dmabuf, ttm, drm, verisilicon, pvrsrvkm and
# wave5. Only wave5 is in the base kernel; the other five ship in packages, so
# without this the service installs, runs, warns five times and leaves the
# board with no display and no GPU. graphics/pvrsrvkm-kmod pulls in
# graphics/drm-vf2-kmod and misc/jh7110-firmware in turn, so naming it alone
# brings the whole set.
RUN_DEPENDS=	/boot/modules/pvrsrvkm.ko:graphics/pvrsrvkm-kmod

# No upstream: everything ships in files/.
DISTFILES=	#
EXTRACT_ONLY=	#
NO_WRKSUBDIR=	yes

USE_RC_SUBR=	vf2_display

SUB_FILES=	pkg-message

PLIST_FILES=	sbin/vf2-setmode \
		"@sample /boot/loader.conf.d/visionfive2.conf.sample"

# vf2-setmode talks to /dev/dri with raw ioctls and links against nothing: the
# libdrm package here ships the kernel ABI headers but not xf86drm.h, and a
# modeset this small does not justify linking the library. It does still need
# libdrm's headers to compile, which is what BUILD_DEPENDS above is for.
#
# That dependency was missing until 2026-09-22 and the port built anyway,
# because the poudriere jail had a whole build environment left inside it and
# the headers were simply lying there. Cleaning the jail turned this into an
# honest failure.
do-build:
	${CC} ${CFLAGS} -I${LOCALBASE}/include \
	    -o ${WRKDIR}/vf2-setmode ${FILESDIR}/vf2-setmode.c

do-install:
	${INSTALL_PROGRAM} ${WRKDIR}/vf2-setmode ${STAGEDIR}${PREFIX}/sbin/
	@${MKDIR} ${STAGEDIR}/boot/loader.conf.d
	${INSTALL_DATA} ${FILESDIR}/visionfive2.conf \
	    ${STAGEDIR}/boot/loader.conf.d/visionfive2.conf.sample

.include <bsd.port.mk>
