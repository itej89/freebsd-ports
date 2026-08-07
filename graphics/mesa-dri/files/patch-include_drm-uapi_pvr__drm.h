--- include/drm-uapi/pvr_drm.h.orig	2025-07-10 00:00:00 UTC
+++ include/drm-uapi/pvr_drm.h
@@ -6,7 +6,13 @@

 #include "drm.h"

+#ifdef __linux__
 #include <linux/const.h>
+#else
+#ifndef _BITULL
+#define _BITULL(x) (1ULL << (x))
+#endif
+#endif
 #include <linux/types.h>

 #if defined(__cplusplus)
