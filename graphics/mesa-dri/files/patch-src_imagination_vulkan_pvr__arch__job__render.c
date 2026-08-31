--- src/imagination/vulkan/pvr_arch_job_render.c.orig	2026-08-23 01:28:16 UTC
+++ src/imagination/vulkan/pvr_arch_job_render.c
@@ -24,6 +24,7 @@
 #include <assert.h>
 #include <stdbool.h>
 #include <stdint.h>
+#include <stdlib.h>
 #include <vulkan/vulkan.h>
 
 #include "hwdef/rogue_hw_defs.h"
@@ -609,6 +610,30 @@ static void pvr_rt_dataset_ws_create_info_init(
 
    create_info->rgn_header_size =
       pvr_rt_get_isp_region_size(device, mtile_info);
+
+   /* Dump the render-target geometry.
+    *
+    * Renders of up to 12 tiles (32x32) come out exactly right; 13 or more
+    * fail, sharply and repeatably, regardless of shape or pixel area. A
+    * watchdog would not produce a boundary that crisp, so something is sized
+    * for 12 tiles. These are the numbers that decide the sizing, printed for
+    * a passing and a failing render so they can be compared directly rather
+    * than derived by hand.
+    */
+   mesa_logw("PVR rt: tiles=%ux%u=%u mtiles=%ux%u mtile1=%ux%u "
+             "tiles_per_mtile=%ux%u tile_max=%ux%u rgn_hdr_size=%llu",
+             mtile_info->num_tiles_x,
+             mtile_info->num_tiles_y,
+             mtile_info->num_tiles_x * mtile_info->num_tiles_y,
+             mtile_info->mtiles_x,
+             mtile_info->mtiles_y,
+             mtile_info->mtile_x1,
+             mtile_info->mtile_y1,
+             mtile_info->tiles_per_mtile_x,
+             mtile_info->tiles_per_mtile_y,
+             mtile_info->x_tile_max,
+             mtile_info->y_tile_max,
+             (unsigned long long)create_info->rgn_header_size);
 }
 
 VkResult pvr_arch_render_target_dataset_create(
@@ -653,13 +678,50 @@ VkResult pvr_arch_render_target_dataset_create(
     * the hardware. See the documentation of ROGUE_FREE_LIST_MAX_SIZE for more
     * details.
     */
-   result = pvr_free_list_create(device,
-                                 runtime_info->min_free_list_size,
-                                 runtime_info->min_free_list_size,
-                                 0 /* grow_size */,
-                                 0 /* grow_threshold */,
-                                 rt_dataset->global_free_list,
-                                 &rt_dataset->local_free_list);
+   /* The local (per-render-target) free list is pinned to the hardware
+    * minimum and given grow_size = 0, so it can never grow. The kernel
+    * reports that minimum as 25 PM pages on roguexe parts (50 otherwise),
+    * which at 4 KB pages is only 100 KB of parameter buffer.
+    *
+    * Renders come out exactly right at up to 12 region headers and fail at 13
+    * or more. If each header's tile list needs a couple of PM pages, 25 pages
+    * lands almost exactly on that boundary - so this non-growable list is the
+    * best candidate for the limit. The global free list has already been
+    * ruled out: sweeping it from 2 MB to 128 MB did not move the threshold at
+    * all, but the global list *can* grow, and this one cannot.
+    *
+    *   PVR_LOCAL_FREE_LIST_KB=<n>  override the local free list size
+    *   PVR_LOCAL_FREE_LIST_GROW=1  additionally allow it to grow
+    */
+   {
+      uint64_t local_size = runtime_info->min_free_list_size;
+      uint32_t local_grow = 0;
+      uint32_t local_thresh = 0;
+      const char *e;
+
+      e = getenv("PVR_LOCAL_FREE_LIST_KB");
+      if (e)
+         local_size = (uint64_t)atoi(e) * 1024U;
+
+      if (getenv("PVR_LOCAL_FREE_LIST_GROW")) {
+         local_grow = 1U * 1024U * 1024U;
+         local_thresh = 50U;
+      }
+
+      mesa_logw("PVR localfl: min=%llu using=%llu grow=%u thresh=%u",
+                (unsigned long long)runtime_info->min_free_list_size,
+                (unsigned long long)local_size,
+                local_grow,
+                local_thresh);
+
+      result = pvr_free_list_create(device,
+                                    local_size,
+                                    local_grow ? (local_size * 64) : local_size,
+                                    local_grow,
+                                    local_thresh,
+                                    rt_dataset->global_free_list,
+                                    &rt_dataset->local_free_list);
+   }
    if (result != VK_SUCCESS)
       goto err_vk_free_rt_dataset;
 
