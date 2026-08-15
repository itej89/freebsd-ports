--- src/imagination/vulkan/pvr_arch_job_transfer.c.orig	2025-07-10 00:00:00 UTC
+++ src/imagination/vulkan/pvr_arch_job_transfer.c
@@ -2806,6 +2806,207 @@
    return VK_SUCCESS;
 }

+/**
+ * Build a minimal ISP control stream for FAST_SCALE (copy_blit_core) path.
+ *
+ * pvr_3d_clip_blit builds its control stream via pvr_isp_ctrl_stream(), but
+ * pvr_3d_copy_blit_core never did -- leaving isp_mtile_base=0 and causing ISP
+ * faults. This function builds the same stream structure using the shader
+ * state that copy_blit_core already set up.
+ */
+static VkResult
+pvr_3d_copy_blit_build_isp_stream(struct pvr_transfer_ctx *ctx,
+                                  struct pvr_transfer_cmd *transfer_cmd,
+                                  struct pvr_transfer_prep_data *prep_data)
+{
+   const struct pvr_device_info *dev_info = &ctx->device->pdevice->dev_info;
+   struct pvr_transfer_3d_state *state = &prep_data->state;
+   struct pvr_winsys_transfer_regs *regs = &state->regs;
+   struct pvr_suballoc_bo *pvr_cs_bo;
+   pvr_dev_addr_t stream_base_vaddr;
+   uint32_t region_arrays_size;
+   uint32_t prim_blk_size;
+   uint32_t total_size;
+   uint32_t *cs_ptr;
+   uint32_t *blk_ptr;
+   VkResult result;
+
+   bool fill_blit = (transfer_cmd->flags & PVR_TRANSFER_CMD_FLAGS_FILL) != 0U;
+   struct pvr_transfer_cmd_source *src = NULL;
+   struct pvr_rect_mapping *mappings = NULL;
+   uint32_t num_mappings;
+
+   if (fill_blit) {
+      src = NULL;
+      num_mappings = 1U;
+   } else if (transfer_cmd->source_count > 0U) {
+      src = &transfer_cmd->sources[0];
+      num_mappings = src->mapping_count;
+      if (num_mappings == 0U)
+         num_mappings = 1U;
+      mappings = src->mappings;
+   } else {
+      src = NULL;
+      num_mappings = 0U;
+   }
+
+   if (num_mappings == 0U) {
+      return VK_SUCCESS;
+   }
+
+   prim_blk_size = pvr_isp_primitive_block_size(dev_info,
+                                                fill_blit ? NULL : src,
+                                                num_mappings);
+
+   region_arrays_size = ROGUE_IPF_CONTROL_STREAM_SIZE_DWORDS * sizeof(uint32_t);
+   total_size = region_arrays_size + prim_blk_size;
+
+   result = pvr_arch_cmd_buffer_alloc_mem(transfer_cmd->cmd_buffer,
+                                          ctx->device->heaps.transfer_frag_heap,
+                                          total_size,
+                                          &pvr_cs_bo);
+   if (result != VK_SUCCESS)
+      return result;
+
+   stream_base_vaddr =
+      PVR_DEV_ADDR(pvr_cs_bo->dev_addr.addr -
+                   ctx->device->heaps.transfer_frag_heap->base_addr.addr);
+
+   cs_ptr = pvr_bo_suballoc_get_map_addr(pvr_cs_bo);
+   memset(cs_ptr, 0, total_size);
+   blk_ptr = cs_ptr + region_arrays_size / sizeof(uint32_t);
+
+   {
+      pvr_dev_addr_t prim_blk_addr = stream_base_vaddr;
+      uint32_t stream_start_offset = 0U;
+
+      if (fill_blit) {
+         struct pvr_rect_mapping fill_mapping = {
+            .dst_rect = transfer_cmd->scissor,
+         };
+
+         result = pvr_isp_primitive_block(dev_info,
+                                          ctx,
+                                          transfer_cmd,
+                                          prep_data,
+                                          NULL,
+                                          state->custom_filter,
+                                          &fill_mapping,
+                                          1U,
+                                          0U,
+                                          false,
+                                          &stream_start_offset,
+                                          &blk_ptr);
+      } else {
+         result = pvr_isp_primitive_block(dev_info,
+                                          ctx,
+                                          transfer_cmd,
+                                          prep_data,
+                                          src,
+                                          state->custom_filter,
+                                          mappings,
+                                          num_mappings,
+                                          0U,
+                                          false,
+                                          &stream_start_offset,
+                                          &blk_ptr);
+      }
+      if (result != VK_SUCCESS)
+         return result;
+
+      prim_blk_addr.addr += region_arrays_size + stream_start_offset;
+
+      if (PVR_HAS_FEATURE(dev_info, simple_internal_parameter_format_v2)) {
+         uint8_t *cs_byte_ptr = (uint8_t *)cs_ptr;
+         uint32_t tmp;
+
+         pvr_csb_pack (&tmp, IPF_PRIMITIVE_HEADER_SIPF2, prim_header) {
+            prim_header.cs_prim_base_size = 1;
+            prim_header.cs_mask_num_bytes = 1;
+            prim_header.cs_valid_tile0 = true;
+         }
+         cs_byte_ptr =
+            pvr_isp_ctrl_stream_sipf_write_aligned(cs_byte_ptr, tmp, 1);
+
+         pvr_csb_pack (&tmp, IPF_PRIMITIVE_BASE_SIPF2, word) {
+            word.cs_prim_base = prim_blk_addr;
+         }
+         cs_byte_ptr =
+            pvr_isp_ctrl_stream_sipf_write_aligned(cs_byte_ptr, tmp, 4);
+
+         pvr_csb_pack (&tmp,
+                       IPF_BYTE_BASED_MASK_ONE_BYTE_WORD_0_SIPF2,
+                       mask) {
+            switch (num_mappings) {
+            case 4:
+               mask.cs_mask_one_byte_tile0_7 = true;
+               mask.cs_mask_one_byte_tile0_6 = true;
+               FALLTHROUGH;
+            case 3:
+               mask.cs_mask_one_byte_tile0_5 = true;
+               mask.cs_mask_one_byte_tile0_4 = true;
+               FALLTHROUGH;
+            case 2:
+               mask.cs_mask_one_byte_tile0_3 = true;
+               mask.cs_mask_one_byte_tile0_2 = true;
+               FALLTHROUGH;
+            case 1:
+               mask.cs_mask_one_byte_tile0_1 = true;
+               mask.cs_mask_one_byte_tile0_0 = true;
+               break;
+            default:
+               break;
+            }
+         }
+         cs_byte_ptr =
+            pvr_isp_ctrl_stream_sipf_write_aligned(cs_byte_ptr, tmp, 1);
+
+         pvr_csb_pack (&tmp, IPF_CONTROL_STREAM_TERMINATE_SIPF2, term);
+         cs_byte_ptr =
+            pvr_isp_ctrl_stream_sipf_write_aligned(cs_byte_ptr, tmp, 1);
+      } else {
+         pvr_csb_pack (cs_ptr, IPF_PRIMITIVE_FORMAT, word) {
+            word.cs_type = ROGUE_IPF_CS_TYPE_PRIM;
+            word.cs_isp_state_read = true;
+            word.cs_isp_state_size = 2U;
+            word.cs_prim_total = 2U * num_mappings - 1U;
+            word.cs_mask_fmt = ROGUE_IPF_CS_MASK_FMT_FULL;
+            word.cs_prim_base_pres = true;
+         }
+         cs_ptr += pvr_cmd_length(IPF_PRIMITIVE_FORMAT);
+
+         pvr_csb_pack (cs_ptr, IPF_PRIMITIVE_BASE, word) {
+            word.cs_prim_base = prim_blk_addr;
+         }
+         cs_ptr += pvr_cmd_length(IPF_PRIMITIVE_BASE);
+
+         pvr_csb_pack (cs_ptr, IPF_CONTROL_STREAM, word) {
+            word.cs_type = ROGUE_IPF_CS_TYPE_TERM;
+         }
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
 static VkResult pvr_3d_copy_blit_core(struct pvr_transfer_ctx *ctx,
                                       struct pvr_transfer_cmd *transfer_cmd,
                                       struct pvr_transfer_prep_data *prep_data,
@@ -3053,6 +3254,10 @@
    if ((pass_idx + 1U) < state->custom_mapping.pass_count)
       *finished_out = false;

+   result = pvr_3d_copy_blit_build_isp_stream(ctx, transfer_cmd, prep_data);
+   if (result != VK_SUCCESS)
+      return result;
+
    return VK_SUCCESS;
 }

