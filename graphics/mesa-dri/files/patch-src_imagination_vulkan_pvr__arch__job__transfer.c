--- src/imagination/vulkan/pvr_arch_job_transfer.c.orig	2025-07-10 00:00:00 UTC
+++ src/imagination/vulkan/pvr_arch_job_transfer.c	2025-07-10 00:00:00 UTC
@@ -2806,6 +2806,11 @@
    return VK_SUCCESS;
 }
 
+static VkResult
+pvr_3d_copy_blit_build_isp_stream(struct pvr_transfer_ctx *ctx,
+                                  struct pvr_transfer_cmd *transfer_cmd,
+                                  struct pvr_transfer_prep_data *prep_data);
+
 static VkResult pvr_3d_copy_blit_core(struct pvr_transfer_ctx *ctx,
                                       struct pvr_transfer_cmd *transfer_cmd,
                                       struct pvr_transfer_prep_data *prep_data,
@@ -3053,6 +3058,10 @@
    if ((pass_idx + 1U) < state->custom_mapping.pass_count)
       *finished_out = false;
 
+   result = pvr_3d_copy_blit_build_isp_stream(ctx, transfer_cmd, prep_data);
+   if (result != VK_SUCCESS)
+      return result;
+
    return VK_SUCCESS;
 }
 
@@ -4035,6 +4044,60 @@
    return stream + size;
 }
 
+static VkResult
+pvr_3d_copy_blit_build_isp_stream(struct pvr_transfer_ctx *ctx,
+                                  struct pvr_transfer_cmd *transfer_cmd,
+                                  struct pvr_transfer_prep_data *prep_data)
+{
+   const struct pvr_device_info *dev_info = &ctx->device->pdevice->dev_info;
+   struct pvr_transfer_3d_state *state = &prep_data->state;
+   struct pvr_winsys_transfer_regs *regs = &state->regs;
+   struct pvr_suballoc_bo *pvr_cs_bo;
+   uint32_t region_arrays_size;
+   uint32_t *cs_ptr;
+   VkResult result;
+
+   region_arrays_size = ROGUE_IPF_CONTROL_STREAM_SIZE_DWORDS * sizeof(uint32_t);
+
+   result = pvr_arch_cmd_buffer_alloc_mem(transfer_cmd->cmd_buffer,
+                                          ctx->device->heaps.transfer_frag_heap,
+                                          region_arrays_size,
+                                          &pvr_cs_bo);
+   if (result != VK_SUCCESS)
+      return result;
+
+   cs_ptr = pvr_bo_suballoc_get_map_addr(pvr_cs_bo);
+   memset(cs_ptr, 0, region_arrays_size);
+
+   if (PVR_HAS_FEATURE(dev_info, simple_internal_parameter_format_v2)) {
+      uint8_t *cs_byte_ptr = (uint8_t *)cs_ptr;
+      uint32_t tmp;
+
+      pvr_csb_pack (&tmp, IPF_CONTROL_STREAM_TERMINATE_SIPF2, term);
+      cs_byte_ptr =
+         pvr_isp_ctrl_stream_sipf_write_aligned(cs_byte_ptr, tmp, 1);
+   } else {
+      pvr_csb_pack (cs_ptr, IPF_CONTROL_STREAM, word) {
+         word.cs_type = ROGUE_IPF_CS_TYPE_TERM;
+      }
+   }
+
+   pvr_csb_pack (&regs->isp_mtile_base, CR_ISP_MTILE_BASE, reg) {
+      reg.addr =
+         PVR_DEV_ADDR(pvr_cs_bo->dev_addr.addr -
+                      ctx->device->heaps.transfer_frag_heap->base_addr.addr);
+   }
+
+   if (PVR_HAS_FEATURE(dev_info, simple_internal_parameter_format_v2) &&
+       PVR_HAS_FEATURE(dev_info, ipf_creq_pf)) {
+      pvr_csb_pack (&regs->isp_rgn, CR_ISP_RGN_SIPF, isp_rgn) {
+         isp_rgn.cs_size_ipf_creq_pf = 0;
+      }
+   }
+
+   return VK_SUCCESS;
+}
+
 /**
  * Writes ISP ctrl stream.
  *
