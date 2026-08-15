--- src/imagination/vulkan/pvr_job_transfer.h.orig	2026-08-10 17:16:34.182524725 -0500
+++ src/imagination/vulkan/pvr_job_transfer.h	2026-08-10 17:16:54.722427940 -0500
@@ -29,6 +29,7 @@
 
 struct pvr_sub_cmd_transfer;
 struct pvr_transfer_ctx;
+struct pvr_winsys_transfer_regs;
 struct vk_sync;
 
 /**
@@ -46,7 +47,8 @@
 VkResult PVR_PER_ARCH(transfer_job_submit)(struct pvr_transfer_ctx *ctx,
                                            struct pvr_sub_cmd_transfer *sub_cmd,
                                            struct vk_sync *wait,
-                                           struct vk_sync *signal_sync);
+                                           struct vk_sync *signal_sync,
+                                           struct pvr_winsys_transfer_regs *regs_out);
 
 #define pvr_arch_transfer_job_submit PVR_PER_ARCH(transfer_job_submit)
 
