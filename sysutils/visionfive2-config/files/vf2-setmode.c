/*
 * Minimal KMS modeset, raw ioctls only.
 *
 * HDMI audio needs a live TMDS clock: the transmitter regenerates the audio
 * clock against the video mode it is actually sending, so with no mode set
 * there is nothing to embed audio into. This board boots headless, so
 * something has to set one.
 *
 * Deliberately avoids libdrm -- the package here ships the kernel ABI headers
 * but not xf86drm.h -- and holds the fd until killed, since dropping DRM
 * master would blank the output again.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>

#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>

static int fd;

static int
rq(unsigned long req, void *arg)
{
	int r;

	do {
		r = ioctl(fd, req, arg);
	} while (r < 0 && (errno == EINTR || errno == EAGAIN));
	return (r);
}

int
main(void)
{
	struct drm_mode_card_res res;
	uint64_t conn_ids[32], crtc_ids[32], enc_ids[32], fb_ids[32];
	struct drm_mode_get_connector conn;
	struct drm_mode_modeinfo modes[64];
	struct drm_mode_create_dumb creq;
	struct drm_mode_map_dumb mreq;
	struct drm_mode_fb_cmd fbcmd;
	struct drm_mode_crtc crtc;
	unsigned int i;
	void *p;

	fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("open /dev/dri/card0");
		return (1);
	}

	memset(&res, 0, sizeof(res));
	if (rq(DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
		perror("getresources");
		return (1);
	}
	res.connector_id_ptr = (uint64_t)(uintptr_t)conn_ids;
	res.crtc_id_ptr = (uint64_t)(uintptr_t)crtc_ids;
	res.encoder_id_ptr = (uint64_t)(uintptr_t)enc_ids;
	res.fb_id_ptr = (uint64_t)(uintptr_t)fb_ids;
	if (res.count_connectors > 32)
		res.count_connectors = 32;
	if (res.count_crtcs > 32)
		res.count_crtcs = 32;
	if (rq(DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
		perror("getresources 2");
		return (1);
	}
	if (res.count_connectors == 0 || res.count_crtcs == 0) {
		fprintf(stderr, "no connectors or crtcs\n");
		return (1);
	}

	/* First connected output with at least one mode. */
	for (i = 0; i < res.count_connectors; i++) {
		memset(&conn, 0, sizeof(conn));
		conn.connector_id = ((uint32_t *)conn_ids)[i];
		if (rq(DRM_IOCTL_MODE_GETCONNECTOR, &conn) < 0)
			continue;
		if (conn.connection != 1 || conn.count_modes == 0)
			continue;

		conn.modes_ptr = (uint64_t)(uintptr_t)modes;
		conn.count_props = 0;
		conn.count_encoders = 0;
		if (conn.count_modes > 64)
			conn.count_modes = 64;
		if (rq(DRM_IOCTL_MODE_GETCONNECTOR, &conn) < 0)
			continue;
		if (conn.count_modes > 0)
			break;
	}
	if (i == res.count_connectors) {
		fprintf(stderr, "no connected output with modes\n");
		return (1);
	}

	printf("connector %u: %s %ux%u %u Hz, pixel clock %u kHz\n",
	    conn.connector_id, modes[0].name, modes[0].hdisplay,
	    modes[0].vdisplay, modes[0].vrefresh, modes[0].clock);

	memset(&creq, 0, sizeof(creq));
	creq.width = modes[0].hdisplay;
	creq.height = modes[0].vdisplay;
	creq.bpp = 32;
	if (rq(DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
		perror("create_dumb");
		return (1);
	}

	memset(&fbcmd, 0, sizeof(fbcmd));
	fbcmd.width = creq.width;
	fbcmd.height = creq.height;
	fbcmd.pitch = creq.pitch;
	fbcmd.bpp = 32;
	fbcmd.depth = 24;
	fbcmd.handle = creq.handle;
	if (rq(DRM_IOCTL_MODE_ADDFB, &fbcmd) < 0) {
		perror("addfb");
		return (1);
	}

	memset(&mreq, 0, sizeof(mreq));
	mreq.handle = creq.handle;
	if (rq(DRM_IOCTL_MODE_MAP_DUMB, &mreq) == 0) {
		p = mmap(NULL, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED,
		    fd, mreq.offset);
		if (p != MAP_FAILED) {
			/* Dark grey: visibly "on" without being a bright flash. */
			memset(p, 0x20, creq.size);
			munmap(p, creq.size);
		}
	}

	memset(&crtc, 0, sizeof(crtc));
	crtc.crtc_id = ((uint32_t *)crtc_ids)[0];
	crtc.fb_id = fbcmd.fb_id;
	crtc.set_connectors_ptr = (uint64_t)(uintptr_t)&conn.connector_id;
	crtc.count_connectors = 1;
	crtc.mode = modes[0];
	crtc.mode_valid = 1;
	if (rq(DRM_IOCTL_MODE_SETCRTC, &crtc) < 0) {
		perror("setcrtc");
		return (1);
	}

	/*
	 * Give up DRM master but keep the fd open.
	 *
	 * Holding master would stop a compositor ever becoming one, so this
	 * would be a boot-time service that quietly breaks the desktop.
	 * Dropping it leaves the CRTC programmed -- which is all HDMI audio
	 * needs, since the transmitter derives its audio clock from the TMDS
	 * clock -- while letting anything else take over later.
	 *
	 * The fd stays open on purpose: closing the last one lets the driver
	 * tear the mode down again.
	 */
	if (rq(DRM_IOCTL_DROP_MASTER, NULL) < 0)
		perror("drop master (continuing)");

	printf("mode set on crtc %u; master dropped, holding fd\n",
	    crtc.crtc_id);
	fflush(stdout);
	for (;;)
		pause();
	return (0);
}
