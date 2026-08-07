--- src/imagination/vulkan/pvr_physical_device.c.orig	2025-07-10 00:00:00 UTC
+++ src/imagination/vulkan/pvr_physical_device.c
@@ -12,7 +12,11 @@
 #include "pvr_winsys.h"
 #include "util/macros.h"

+#ifdef __linux__
 #include <sys/sysmacros.h>
+#else
+#include <sys/types.h>
+#endif

 static bool pvr_physical_device_get_properties(
    struct pvr_physical_device *pdevice,
