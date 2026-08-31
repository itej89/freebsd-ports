--- src/imagination/vulkan/pvr_arch_device.c.orig	2026-08-23 02:12:26 UTC
+++ src/imagination/vulkan/pvr_arch_device.c
@@ -778,13 +778,55 @@ VkResult PVR_PER_ARCH(create_device)(struct pvr_physic
       initial_free_list_size = PVR_SECONDARY_DEVICE_FREE_LIST_INITAL_SIZE;
    }
 
-   result = pvr_free_list_create(device,
-                                 initial_free_list_size,
-                                 PVR_GLOBAL_FREE_LIST_MAX_SIZE,
-                                 PVR_GLOBAL_FREE_LIST_GROW_SIZE,
-                                 PVR_GLOBAL_FREE_LIST_GROW_THRESHOLD,
-                                 NULL /* parent_free_list */,
-                                 &device->global_free_list);
+   /* Diagnostic: let the free-list geometry be set at run time.
+    *
+    * Renders of up to 12 region headers come out exactly right and 13 or more
+    * fail, sharply, even though the region-header buffer itself is allocated
+    * with 48-64 headers. So something else caps the render, and the parameter
+    * buffer backed by this free list is a candidate.
+    *
+    * Making these settable by environment variable means the hypothesis can
+    * be swept in seconds instead of one 20-minute emulated rebuild per value:
+    *
+    *   PVR_FREE_LIST_MB=<n>        initial size, default 2
+    *   PVR_FREE_LIST_GROW_MB=<n>   grow size, default 1
+    *   PVR_FREE_LIST_THRESHOLD=<n> grow threshold percent, default 13
+    *
+    * If the failure threshold moves with the initial size, the free list is
+    * the limiting resource. If it does not move at any size, the free list is
+    * ruled out.
+    */
+   {
+      const char *e;
+      uint32_t grow_size = PVR_GLOBAL_FREE_LIST_GROW_SIZE;
+      uint32_t threshold = PVR_GLOBAL_FREE_LIST_GROW_THRESHOLD;
+
+      e = getenv("PVR_FREE_LIST_MB");
+      if (e)
+         initial_free_list_size = (uint32_t)atoi(e) * 1024U * 1024U;
+
+      e = getenv("PVR_FREE_LIST_GROW_MB");
+      if (e)
+         grow_size = (uint32_t)atoi(e) * 1024U * 1024U;
+
+      e = getenv("PVR_FREE_LIST_THRESHOLD");
+      if (e)
+         threshold = (uint32_t)atoi(e);
+
+      mesa_logw("PVR freelist: initial=%u grow=%u threshold=%u max=%u",
+                initial_free_list_size,
+                grow_size,
+                threshold,
+                PVR_GLOBAL_FREE_LIST_MAX_SIZE);
+
+      result = pvr_free_list_create(device,
+                                    initial_free_list_size,
+                                    PVR_GLOBAL_FREE_LIST_MAX_SIZE,
+                                    grow_size,
+                                    threshold,
+                                    NULL /* parent_free_list */,
+                                    &device->global_free_list);
+   }
    if (result != VK_SUCCESS)
       goto err_dec_device_count;
 
