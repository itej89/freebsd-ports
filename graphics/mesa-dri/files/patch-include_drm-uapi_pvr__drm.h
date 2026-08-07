--- include/drm-uapi/pvr_drm.h.orig	2025-07-10 00:00:00 UTC
+++ include/drm-uapi/pvr_drm.h
@@ -6,8 +6,18 @@

 #include "drm.h"

+#ifdef __linux__
 #include <linux/const.h>
 #include <linux/types.h>
+#else
+#include <stdint.h>
+#ifndef _BITULL
+#define _BITULL(x) (1ULL << (x))
+#endif
+typedef uint32_t __u32;
+typedef uint64_t __u64;
+typedef int32_t __s32;
+#endif

 #if defined(__cplusplus)
 extern "C" {
