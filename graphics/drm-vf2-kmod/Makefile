PORTNAME=	drm-vf2-kmod
PORTVERSION=	20260922
CATEGORIES=	graphics

MAINTAINER=	itej89@gmail.com
COMMENT=	DRM core and verisilicon display driver for the StarFive VisionFive 2
WWW=		https://github.com/itej89/drm-kmod

LICENSE=	MIT

ONLY_FOR_ARCHS=	riscv64
ONLY_FOR_ARCHS_REASON=	riscv64 support and the verisilicon display driver for the JH7110

USES=		kmod uidfix

USE_GITHUB=	yes
GH_ACCOUNT=	itej89
GH_PROJECT=	drm-kmod
# Pinned by commit, not by branch: a branch would silently build different
# source as it moved. distinfo records the hash of exactly these bytes.
GH_TAGNAME=	691da90dc6526bce9a10c5d0a9104df4f8290067

# Deliberately NOT the OSS pvr driver. It claims the same GPU device as
# pvrsrvkm, the loser binds nothing, and they cannot be swapped at runtime --
# unloading pvr panics this kernel. graphics/pvrsrvkm-kmod provides the GPU;
# this port provides only the DRM core and the display controller.
MAKE_ARGS=	KMODS="dmabuf ttm drm verisilicon"

OPTIONS_DEFINE=		PVR_OSS
PVR_OSS_DESC=		Also build the open-source PowerVR driver (conflicts with pvrsrvkm)
PVR_OSS_MAKE_ARGS=	KMODS="dmabuf ttm drm verisilicon pvr"

.include <bsd.port.mk>
