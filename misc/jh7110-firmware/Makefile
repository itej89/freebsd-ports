PORTNAME=	jh7110-firmware
# A date version rather than the commit: DISTVERSION mangles a hash into
# something like g6.d94.a3.a132. The exact source is pinned by distinfo, and
# the commit is recorded in DISTNAME below.
PORTVERSION=	20260922
CATEGORIES=	misc
MASTER_SITES=	# staged into DISTFILES_CACHE by scripts/ports/mkdistfile.sh
DISTNAME=	jh7110-firmware-db6d94a3a132

MAINTAINER=	itej89@gmail.com
COMMENT=	Vendor firmware blobs for the StarFive JH7110 SoC
WWW=		https://github.com/itej89/jh7110-firmware

# Imagination and StarFive binaries. Redistribution is restricted, which is
# why the source repository is private and why this package must not be
# mirrored or put on installation media.
LICENSE=	IMG-SF-PROP
LICENSE_NAME=	Imagination/StarFive proprietary
LICENSE_TEXT=	Vendor firmware. Redistribution is restricted; see the vendor terms.
LICENSE_PERMS=	no-dist-mirror no-dist-sell no-pkg-mirror no-pkg-sell auto-accept

RESTRICTED=	vendor firmware, redistribution restricted

ONLY_FOR_ARCHS=	riscv64
ONLY_FOR_ARCHS_REASON=	firmware for the StarFive JH7110, a riscv64 SoC

NO_BUILD=	yes
NO_ARCH=	yes

PLIST_FILES=	/boot/firmware/rgx.fw.36.50.54.182 \
		/boot/firmware/rgx.sh.36.50.54.182 \
		/boot/firmware/wave511_dec_fw.bin

# firmware(9) opens /boot/firmware/<name> directly, so the flat name is what
# matters rather than the layout inside the distfile. The MANIFEST names both
# for every blob; only the "firmware" kind is installed here. The "flash" kind
# -- SPL and U-Boot -- is deliberately not installed: writing those is a
# separate, riskier operation and cannot be done from FreeBSD at all while the
# QSPI read path is broken (IMG-13).
FWDIR=		/boot/firmware

do-install:
	@${MKDIR} ${STAGEDIR}${FWDIR}
	@cd ${WRKSRC} && ${AWK} -F"\t" \
	    '!/^#/ && $$1 == "firmware" && $$5 == "yes" { print $$2, $$3 }' \
	    MANIFEST | while read src dst; do \
		${INSTALL_DATA} ${WRKSRC}/$${src} ${STAGEDIR}${FWDIR}/$${dst}; \
	    done

.include <bsd.port.mk>
