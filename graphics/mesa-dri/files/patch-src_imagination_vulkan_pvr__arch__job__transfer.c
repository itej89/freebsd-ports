--- src/imagination/vulkan/pvr_arch_job_transfer.c.orig	2025-07-10 00:00:00 UTC
+++ src/imagination/vulkan/pvr_arch_job_transfer.c
@@ -5994,12 +5994,15 @@
          prep_data->state = prev_prep_data->state;
       }

-      if (transfer_cmd->flags & PVR_TRANSFER_CMD_FLAGS_FAST2D) {
-         result =
-            pvr_3d_clip_blit(ctx, transfer_cmd, prep_data, pass, &finished);
-      } else {
-         result =
-            pvr_3d_copy_blit(ctx, transfer_cmd, prep_data, pass, &finished);
+      {
+         /* Force FAST2D path: pvr_3d_copy_blit uses FAST_SCALE mode but
+          * never builds an ISP control stream, leaving isp_mtile_base=0.
+          * The ISP faults on the NULL address, causing a FW context reset
+          * that wipes PDS_EXEC_BASE and cascades into further faults.
+          */
+         transfer_cmd->flags |= PVR_TRANSFER_CMD_FLAGS_FAST2D;
+         result =
+            pvr_3d_clip_blit(ctx, transfer_cmd, prep_data, pass, &finished);
       }
       if (result != VK_SUCCESS)
          return result;
@@ -6030,7 +6033,8 @@
 VkResult pvr_arch_transfer_job_submit(struct pvr_transfer_ctx *ctx,
                                       struct pvr_sub_cmd_transfer *sub_cmd,
                                       struct vk_sync *wait_sync,
-                                      struct vk_sync *signal_sync)
+                                      struct vk_sync *signal_sync,
+                                      struct pvr_winsys_transfer_regs *regs_out)
 {
    list_for_each_entry_safe (struct pvr_transfer_cmd,
                              transfer_cmd,
