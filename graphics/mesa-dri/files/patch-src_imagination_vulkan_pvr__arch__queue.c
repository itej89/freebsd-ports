--- src/imagination/vulkan/pvr_arch_queue.c.orig	2026-08-10 15:37:54.472209621 -0500
+++ src/imagination/vulkan/pvr_arch_queue.c	2026-08-14 09:25:40.699477148 -0500
@@ -38,9 +38,12 @@
 #include <unistd.h>
 #include <vulkan/vulkan.h>
 
+#include "pvr_bo.h"
+#include "pvr_border.h"
 #include "pvr_cmd_buffer.h"
 #include "pvr_device.h"
 #include "pvr_entrypoints.h"
+#include "pvr_job_common.h"
 #include "pvr_job_compute.h"
 #include "pvr_job_context.h"
 #include "pvr_job_render.h"
@@ -48,6 +51,7 @@
 #include "pvr_limits.h"
 #include "pvr_macros.h"
 #include "pvr_physical_device.h"
+#include "pvr_rt_dataset.h"
 #include "pvr_pipeline.h"
 
 #include "util/macros.h"
@@ -65,6 +69,234 @@
 static VkResult pvr_driver_queue_submit(struct vk_queue *queue,
                                         struct vk_queue_submit *submit);
 
+static void
+pvr_transfer_geom_state_init(struct pvr_queue *queue,
+                             struct vk_sync *wait,
+                             struct pvr_winsys_geometry_state *state)
+{
+   struct pvr_device *device = queue->device;
+   const struct pvr_device_info *dev_info = &device->pdevice->dev_info;
+   uint32_t *stream_ptr = (uint32_t *)state->fw_stream;
+   uint32_t *stream_len_ptr = stream_ptr;
+
+   stream_ptr += pvr_cmd_length(KMD_STREAM_HDR);
+
+   pvr_csb_pack ((uint64_t *)stream_ptr, CR_VDM_CTRL_STREAM_BASE, value) {
+      value.addr = queue->transfer_geom_terminate_bo->dev_addr;
+   }
+   stream_ptr += pvr_cmd_length(CR_VDM_CTRL_STREAM_BASE);
+
+   pvr_csb_pack ((uint64_t *)stream_ptr,
+                 CR_TPU_BORDER_COLOUR_TABLE_VDM, value) {
+      value.border_colour_table_address =
+         device->border_color_table->table->vma->dev_addr;
+   }
+   stream_ptr += pvr_cmd_length(CR_TPU_BORDER_COLOUR_TABLE_VDM);
+
+   pvr_csb_pack (stream_ptr, CR_PPP_CTRL, value) {
+      value.wclampen = true;
+      value.fixed_point_format = 1;
+   }
+   stream_ptr += pvr_cmd_length(CR_PPP_CTRL);
+
+   pvr_csb_pack (stream_ptr, CR_TE_PSG, value) {
+      value.completeonterminate = true;
+      value.region_stride =
+         queue->transfer_rt_dataset->rgn_headers_stride /
+         ROGUE_CR_TE_PSG_REGION_STRIDE_UNIT_SIZE;
+      value.forcenewstate = PVR_HAS_QUIRK(dev_info, 52942);
+   }
+   stream_ptr += pvr_cmd_length(CR_TE_PSG);
+
+   pvr_csb_pack (stream_ptr, VDMCTRL_PDS_STATE0, value) {
+      value.usc_common_size = 0;
+   }
+   stream_ptr += pvr_cmd_length(VDMCTRL_PDS_STATE0);
+
+   pvr_csb_pack (stream_ptr, KMD_STREAM_VIEW_IDX, value) {
+      value.idx = 0;
+   }
+   stream_ptr += pvr_cmd_length(KMD_STREAM_VIEW_IDX);
+
+   state->fw_stream_len = (uint8_t *)stream_ptr - (uint8_t *)state->fw_stream;
+   assert(state->fw_stream_len <= ARRAY_SIZE(state->fw_stream));
+
+   pvr_csb_pack ((uint64_t *)stream_len_ptr, KMD_STREAM_HDR, value) {
+      value.length = state->fw_stream_len;
+   }
+
+   state->flags = (struct pvr_winsys_geometry_state_flags){
+      .is_first_geometry = true,
+      .is_last_geometry = true,
+      .use_single_core = false,
+   };
+   state->wait = wait;
+}
+
+static void
+pvr_transfer_frag_state_init(struct pvr_queue *queue,
+                             const struct pvr_winsys_transfer_regs *regs,
+                             struct pvr_winsys_fragment_state *state)
+{
+   struct pvr_device *device = queue->device;
+   const struct pvr_physical_device *pdevice = device->pdevice;
+   const struct pvr_device_runtime_info *dev_runtime_info =
+      &pdevice->dev_runtime_info;
+   const struct pvr_device_info *dev_info = &pdevice->dev_info;
+   uint32_t *stream_ptr = (uint32_t *)state->fw_stream;
+   uint32_t *stream_len_ptr = stream_ptr;
+   uint32_t pixel_ctl, isp_ctl;
+
+#define DWORDS_PER_U64 (sizeof(uint64_t) / sizeof(uint32_t))
+
+   stream_ptr += pvr_cmd_length(KMD_STREAM_HDR);
+
+   pvr_arch_setup_tiles_in_flight(dev_info, dev_runtime_info,
+                                  ROGUE_CR_ISP_AA_MODE_TYPE_AA_NONE,
+                                  1, false, 0, &isp_ctl, &pixel_ctl);
+
+   pvr_csb_pack ((uint64_t *)stream_ptr, CR_ISP_SCISSOR_BASE, value) {
+      value.addr = PVR_DEV_ADDR_INVALID;
+   }
+   stream_ptr += pvr_cmd_length(CR_ISP_SCISSOR_BASE);
+
+   pvr_csb_pack ((uint64_t *)stream_ptr, CR_ISP_DBIAS_BASE, value) {
+      value.addr = PVR_DEV_ADDR_INVALID;
+   }
+   stream_ptr += pvr_cmd_length(CR_ISP_DBIAS_BASE);
+
+   pvr_csb_pack ((uint64_t *)stream_ptr, CR_ISP_OCLQRY_BASE, value) {
+      value.addr = PVR_DEV_ADDR_INVALID;
+   }
+   stream_ptr += pvr_cmd_length(CR_ISP_OCLQRY_BASE);
+
+   memset(stream_ptr, 0, PVR_DW_TO_BYTES(pvr_cmd_length(CR_ISP_ZLSCTL)));
+   stream_ptr += pvr_cmd_length(CR_ISP_ZLSCTL);
+
+   memset(stream_ptr, 0, PVR_DW_TO_BYTES(pvr_cmd_length(CR_ISP_ZLOAD_BASE)));
+   stream_ptr += pvr_cmd_length(CR_ISP_ZLOAD_BASE);
+
+   memset(stream_ptr, 0,
+          PVR_DW_TO_BYTES(pvr_cmd_length(CR_ISP_STENCIL_LOAD_BASE)));
+   stream_ptr += pvr_cmd_length(CR_ISP_STENCIL_LOAD_BASE);
+
+   if (PVR_HAS_FEATURE(dev_info, requires_fb_cdc_zls_setup)) {
+      pvr_csb_pack ((uint64_t *)stream_ptr, CR_FB_CDC_ZLS, value) {
+         value.fbdc_depth_fmt = ROGUE_TEXSTATE_FORMAT_F32;
+         value.fbdc_stencil_fmt = ROGUE_TEXSTATE_FORMAT_U8;
+      }
+      stream_ptr += pvr_cmd_length(CR_FB_CDC_ZLS);
+   }
+
+   {
+      const uint32_t frag_pbe_total =
+         PVR_MAX_COLOR_ATTACHMENTS * ROGUE_NUM_PBESTATE_REG_WORDS;
+      uint64_t *pbe_dst = (uint64_t *)stream_ptr;
+
+      memset(pbe_dst, 0, frag_pbe_total * sizeof(uint64_t));
+
+      for (uint32_t rt = 0; rt < PVR_TRANSFER_MAX_RENDER_TARGETS; rt++) {
+         const uint64_t *src =
+            &regs->pbe_wordx_mrty[rt * ROGUE_NUM_PBESTATE_REG_WORDS_FOR_TRANSFER];
+         uint64_t *dst = &pbe_dst[rt * ROGUE_NUM_PBESTATE_REG_WORDS];
+
+         dst[0] = src[0];
+         dst[1] = src[1];
+         dst[2] = 0;
+      }
+
+      stream_ptr += frag_pbe_total * DWORDS_PER_U64;
+   }
+
+   pvr_csb_pack ((uint64_t *)stream_ptr,
+                 CR_TPU_BORDER_COLOUR_TABLE_PDM, value) {
+      value.border_colour_table_address =
+         device->border_color_table->table->vma->dev_addr;
+   }
+   stream_ptr += pvr_cmd_length(CR_TPU_BORDER_COLOUR_TABLE_PDM);
+
+   {
+      uint64_t pds_bgnd[ROGUE_NUM_CR_PDS_BGRND_WORDS];
+      pds_bgnd[0] = regs->pds_bgnd0_base;
+      pds_bgnd[1] = regs->pds_bgnd1_base;
+      pds_bgnd[2] = regs->pds_bgnd3_sizeinfo;
+      memcpy(stream_ptr, pds_bgnd, sizeof(pds_bgnd));
+      stream_ptr += ROGUE_NUM_CR_PDS_BGRND_WORDS * DWORDS_PER_U64;
+      memcpy(stream_ptr, pds_bgnd, sizeof(pds_bgnd));
+      stream_ptr += ROGUE_NUM_CR_PDS_BGRND_WORDS * DWORDS_PER_U64;
+   }
+
+   memset(stream_ptr, 0,
+          ROGUE_KMD_STREAM_USC_CLEAR_REGISTER_COUNT *
+             PVR_DW_TO_BYTES(pvr_cmd_length(CR_USC_CLEAR_REGISTER)));
+   stream_ptr += ROGUE_KMD_STREAM_USC_CLEAR_REGISTER_COUNT *
+                 pvr_cmd_length(CR_USC_CLEAR_REGISTER);
+
+   *stream_ptr = pixel_ctl;
+   stream_ptr += pvr_cmd_length(CR_USC_PIXEL_OUTPUT_CTRL);
+
+   pvr_csb_pack (stream_ptr, CR_ISP_BGOBJDEPTH, value) {
+      value.value = fui(1.0f);
+   }
+   stream_ptr += pvr_cmd_length(CR_ISP_BGOBJDEPTH);
+
+   *stream_ptr = regs->isp_bgobjvals;
+   stream_ptr += pvr_cmd_length(CR_ISP_BGOBJVALS);
+
+   *stream_ptr = regs->isp_aa;
+   stream_ptr += pvr_cmd_length(CR_ISP_AA);
+
+   *stream_ptr = isp_ctl;
+   stream_ptr += pvr_cmd_length(CR_ISP_CTL);
+
+   *stream_ptr = regs->event_pixel_pds_info;
+   stream_ptr += pvr_cmd_length(CR_EVENT_PIXEL_PDS_INFO);
+
+   if (PVR_HAS_FEATURE(dev_info, cluster_grouping)) {
+      *stream_ptr = 0;
+      stream_ptr += pvr_cmd_length(KMD_STREAM_PIXEL_PHANTOM);
+   }
+
+   *stream_ptr = 0;
+   stream_ptr += pvr_cmd_length(KMD_STREAM_VIEW_IDX);
+
+   *stream_ptr = regs->event_pixel_pds_data;
+   stream_ptr += pvr_cmd_length(CR_EVENT_PIXEL_PDS_DATA);
+
+   if (PVR_HAS_FEATURE(dev_info, gpu_multicore_support)) {
+      *stream_ptr = 0;
+      stream_ptr++;
+   }
+
+   if (PVR_HAS_FEATURE(dev_info, zls_subtile)) {
+      *stream_ptr = 0;
+      stream_ptr += pvr_cmd_length(CR_ISP_ZLS_PIXELS);
+   }
+
+   *stream_ptr = 0;
+   stream_ptr++;
+
+   *stream_ptr = 0;
+   stream_ptr++;
+
+   if (PVR_HAS_FEATURE(dev_info, gpu_multicore_support)) {
+      *stream_ptr = 0;
+      stream_ptr++;
+   }
+
+   state->fw_stream_len = (uint8_t *)stream_ptr - (uint8_t *)state->fw_stream;
+   assert(state->fw_stream_len <= ARRAY_SIZE(state->fw_stream));
+
+   pvr_csb_pack ((uint64_t *)stream_len_ptr, KMD_STREAM_HDR, value) {
+      value.length = state->fw_stream_len;
+   }
+
+#undef DWORDS_PER_U64
+
+   state->flags = (struct pvr_winsys_fragment_state_flags){ 0 };
+   state->wait = NULL;
+}
+
 static VkResult pvr_queue_init(struct pvr_device *device,
                                struct pvr_queue *queue,
                                const VkDeviceQueueCreateInfo *pCreateInfo,
@@ -113,6 +345,28 @@
    if (result != VK_SUCCESS)
       goto err_query_ctx_destroy;
 
+   {
+      uint32_t *terminate_map;
+      result = pvr_bo_suballoc(&device->suballoc_general,
+                               sizeof(uint32_t),
+                               ROGUE_CR_VDM_CTRL_STREAM_BASE_ADDR_ALIGNMENT,
+                               false,
+                               &queue->transfer_geom_terminate_bo);
+      if (result != VK_SUCCESS)
+         goto err_gfx_ctx_destroy;
+      terminate_map = pvr_bo_suballoc_get_map_addr(
+         queue->transfer_geom_terminate_bo);
+      pvr_csb_pack (terminate_map, VDMCTRL_STREAM_TERMINATE, word) {
+         (void)word;
+      }
+   }
+
+   result = pvr_arch_render_target_dataset_create(device,
+                                                  64, 64, 1, 1,
+                                                  &queue->transfer_rt_dataset);
+   if (result != VK_SUCCESS)
+      goto err_free_terminate_bo;
+
    queue->device = device;
    queue->gfx_ctx = gfx_ctx;
    queue->compute_ctx = compute_ctx;
@@ -123,6 +377,13 @@
 
    return VK_SUCCESS;
 
+err_free_terminate_bo:
+   pvr_bo_suballoc_free(queue->transfer_geom_terminate_bo);
+   queue->transfer_geom_terminate_bo = NULL;
+
+err_gfx_ctx_destroy:
+   pvr_arch_render_ctx_destroy(gfx_ctx);
+
 err_query_ctx_destroy:
    pvr_arch_compute_ctx_destroy(query_ctx);
 
@@ -187,6 +448,11 @@
          vk_sync_destroy(&queue->device->vk, queue->last_job_signal_sync[i]);
    }
 
+   if (queue->transfer_rt_dataset)
+      pvr_render_target_dataset_destroy(queue->transfer_rt_dataset);
+   if (queue->transfer_geom_terminate_bo)
+      pvr_bo_suballoc_free(queue->transfer_geom_terminate_bo);
+
    pvr_arch_render_ctx_destroy(queue->gfx_ctx);
    pvr_arch_compute_ctx_destroy(queue->query_ctx);
    pvr_arch_compute_ctx_destroy(queue->compute_ctx);
@@ -394,6 +660,8 @@
                                           struct pvr_queue *queue,
                                           struct pvr_sub_cmd_transfer *sub_cmd)
 {
+   struct pvr_winsys_render_submit_info submit_info;
+   struct pvr_winsys_transfer_regs transfer_regs;
    struct vk_sync *sync;
    VkResult result;
 
@@ -405,19 +673,59 @@
    if (result != VK_SUCCESS)
       return result;
 
+   memset(&transfer_regs, 0, sizeof(transfer_regs));
+
+   mesa_logw("PVR transfer: building PDS programs via transfer path");
+
    result = pvr_arch_transfer_job_submit(
       queue->transfer_ctx,
       sub_cmd,
       queue->next_job_wait_sync[PVR_JOB_TYPE_TRANSFER],
+      NULL,
+      &transfer_regs);
+   if (result != VK_SUCCESS) {
+      vk_sync_destroy(&device->vk, sync);
+      return result;
+   }
+
+   mesa_logw("PVR transfer: submitting via 3D render pipeline (pds_bgnd0=0x%llx)",
+             (unsigned long long)transfer_regs.pds_bgnd0_base);
+
+   memset(&submit_info, 0, sizeof(submit_info));
+   submit_info.rt_dataset = queue->transfer_rt_dataset->ws_rt_dataset;
+   submit_info.rt_data_idx = 0;
+   submit_info.frame_num = p_atomic_inc_return(
+      &device->global_queue_present_count);
+   submit_info.job_num = p_atomic_inc_return(
+      &device->global_cmd_buffer_submit_count);
+   submit_info.has_fragment_job = true;
+
+   pvr_transfer_geom_state_init(
+      queue,
+      queue->next_job_wait_sync[PVR_JOB_TYPE_TRANSFER],
+      &submit_info.geometry);
+
+   pvr_transfer_frag_state_init(queue, &transfer_regs, &submit_info.fragment);
+   submit_info.fragment_pr = submit_info.fragment;
+
+   result = device->ws->ops->render_submit(
+      queue->gfx_ctx->ws_ctx,
+      &submit_info,
+      &device->pdevice->dev_info,
+      NULL,
       sync);
    if (result != VK_SUCCESS) {
       vk_sync_destroy(&device->vk, sync);
       return result;
    }
 
-   pvr_update_job_syncs(device, queue, sync, PVR_JOB_TYPE_TRANSFER);
+   vk_sync_wait(&device->vk, sync, 0U, VK_SYNC_WAIT_COMPLETE, UINT64_MAX);
 
-   return result;
+   mesa_logw("PVR transfer: render submit + sync wait done");
+
+   pvr_update_job_syncs(device, queue, sync, PVR_JOB_TYPE_FRAG);
+
+   return VK_SUCCESS;
 }
 
 static VkResult pvr_process_query_cmd(struct pvr_device *device,
