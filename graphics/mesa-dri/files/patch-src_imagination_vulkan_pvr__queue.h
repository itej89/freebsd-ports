--- src/imagination/vulkan/pvr_queue.h.orig	2026-08-10 15:37:54.473815443 -0500
+++ src/imagination/vulkan/pvr_queue.h	2026-08-10 15:38:04.003937781 -0500
@@ -25,6 +25,8 @@
 struct pvr_render_ctx;
 struct pvr_compute_ctx;
 struct pvr_transfer_ctx;
+struct pvr_rt_dataset;
+struct pvr_suballoc_bo;
 
 struct pvr_queue {
    struct vk_queue vk;
@@ -36,6 +38,9 @@
    struct pvr_compute_ctx *query_ctx;
    struct pvr_transfer_ctx *transfer_ctx;
 
+   struct pvr_rt_dataset *transfer_rt_dataset;
+   struct pvr_suballoc_bo *transfer_geom_terminate_bo;
+
    struct vk_sync *last_job_signal_sync[PVR_JOB_TYPE_MAX];
    struct vk_sync *next_job_wait_sync[PVR_JOB_TYPE_MAX];
 };
