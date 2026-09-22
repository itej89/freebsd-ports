PORTNAME=	pvrsrvkm-kmod
PORTVERSION=	20260922
CATEGORIES=	graphics
MASTER_SITES=	# pvrsrvkm distfile is staged into DISTFILES_CACHE by \
		# scripts/ports/mkdistfile.sh; the repository is private and \
		# fetch(1) cannot speak ssh. See IMAGE-BUILD-PLAN.md.
MASTER_SITES+=	https://codeload.github.com/itej89/drm-kmod/tar.gz/${DRM_COMMIT}?dummy=/:drm

DISTFILES=	${PVR_DISTNAME}${EXTRACT_SUFX} \
		${DRM_DISTNAME}${EXTRACT_SUFX}:drm

MAINTAINER=	itej89@gmail.com
COMMENT=	Imagination PowerVR Rogue DDK 1.19 kernel driver for the JH7110
WWW=		https://github.com/itej89/pvrsrvkm-freebsd

# Imagination's DDK is dual MIT/GPLv2. The firmware it loads is separate and
# restricted; see misc/jh7110-firmware.
LICENSE=	MIT GPLv2
LICENSE_COMB=	dual

ONLY_FOR_ARCHS=	riscv64
ONLY_FOR_ARCHS_REASON=	the PowerVR GPU on the StarFive JH7110, a riscv64 SoC

# Pinned by commit. A branch would silently build different source as it
# moved; distinfo pins exactly these bytes.
PVR_COMMIT=	a52283a1a3dd
PVR_DISTNAME=	pvrsrvkm-freebsd-${PVR_COMMIT}
DRM_COMMIT=	691da90dc6526bce9a10c5d0a9104df4f8290067
DRM_DISTNAME=	itej89-drm-kmod-20260922-${DRM_COMMIT}_GH0

USES=		kmod uidfix
WRKSRC=		${WRKDIR}/${PVR_DISTNAME}

# The DDK compiles against drm-kmod's LinuxKPI headers -- linuxkpi/gplv2,
# linuxkpi/bsd, include and include/uapi -- and no drm-kmod package installs
# them, so the source is fetched alongside rather than depended on.
DRM_SRC=	${WRKDIR}/drm-kmod-${DRM_COMMIT}

RUN_DEPENDS=	${KMODDIR}/drm.ko:graphics/drm-vf2-kmod

# The firmware is a separate package because its redistribution is restricted.
RUN_DEPENDS+=	/boot/firmware/rgx.fw.36.50.54.182:misc/jh7110-firmware

PLIST_FILES=	${KMODDIR}/pvrsrvkm.ko

do-build:
	@cd ${WRKSRC} && ${SETENV} \
	    DRM=${DRM_SRC} \
	    SYSROOT= \
	    SYSDIR=${SRC_BASE}/sys \
	    B=${WRKDIR}/build \
	    WORK=${WRKDIR}/shimwork \
	    ${SH} freebsd/build-module.sh

do-install:
	@${MKDIR} ${STAGEDIR}${KMODDIR}
	${INSTALL_KLD} ${WRKDIR}/build/pvrsrvkm.ko ${STAGEDIR}${KMODDIR}/

.include <bsd.port.mk>
