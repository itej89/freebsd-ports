--- src/imagination/vulkan/pvr_arch_job_transfer.c.orig	2025-07-10 00:00:00 UTC
+++ src/imagination/vulkan/pvr_arch_job_transfer.c
@@ -5595,7 +5595,78 @@
 
    return vk_error(ctx->device, VK_ERROR_FORMAT_NOT_SUPPORTED);
 }
+
+static VkResult
+pvr_3d_copy_blit_core_with_isp_stream(struct pvr_transfer_ctx *ctx,
+                                      struct pvr_transfer_cmd *active_cmd,
+                                      struct pvr_transfer_prep_data *prep_data,
+                                      uint32_t pass_idx,
+                                      bool *finished_out)
+{
+   const struct pvr_device_info *const dev_info =
+      &ctx->device->pdevice->dev_info;
+   struct pvr_transfer_3d_state *const state = &prep_data->state;
+   struct pvr_transfer_cmd bg_cmd = { 0U };
+   uint32_t texel_unwind_src = state->custom_mapping.texel_unwind_src;
+   uint32_t mask_bit;
+   VkResult result;
 
+   /* Fix attempt 4: pvr_3d_clip_blit() (the working FAST_2D path) does two
+    * separate copy_blit_core-shaped things, not one: (1) a synthetic
+    * pass-through background sub-pass (dst blended onto itself) that
+    * populates state->regs.pds_ and tex_state_ shader-offset fields for the
+    * ISP background object, then (2) sets isp_bgobjvals.mask and calls
+    * pvr_isp_ctrl_stream() on the REAL content, reusing that background
+    * state. Fix attempt 3 skipped step (1) entirely and called
+    * pvr_isp_ctrl_stream() with shader state describing the real content
+    * instead of a background config -- this replicates the full sequence.
+    */
+   bg_cmd.scissor = active_cmd->scissor;
+   bg_cmd.cmd_buffer = active_cmd->cmd_buffer;
+   bg_cmd.flags = active_cmd->flags;
+   bg_cmd.flags &=
+      ~(PVR_TRANSFER_CMD_FLAGS_FAST2D | PVR_TRANSFER_CMD_FLAGS_FILL |
+        PVR_TRANSFER_CMD_FLAGS_DSMERGE | PVR_TRANSFER_CMD_FLAGS_PICKD);
+   bg_cmd.source_count = state->custom_mapping.pass_count > 0U ? 0 : 1;
+   if (bg_cmd.source_count > 0) {
+      struct pvr_transfer_cmd_source *src = &bg_cmd.sources[0];
+
+      src->mappings[0U].src_rect = active_cmd->scissor;
+      src->mappings[0U].dst_rect = active_cmd->scissor;
+      src->resolve_op = PVR_RESOLVE_BLEND;
+      src->surface = active_cmd->dst;
+   }
+
+   state->filter[0] = PVR_FILTER_DONTCARE;
+   bg_cmd.dst = active_cmd->dst;
+   state->custom_mapping.texel_unwind_src =
+      state->custom_mapping.texel_unwind_dst;
+
+   result =
+      pvr_3d_copy_blit_core(ctx, &bg_cmd, prep_data, pass_idx, finished_out);
+   if (result != VK_SUCCESS)
+      return result;
+
+   state->dont_force_pbe = true;
+   state->custom_mapping.texel_unwind_src = texel_unwind_src;
+
+   pvr_csb_pack (&mask_bit, CR_ISP_BGOBJVALS, reg) {
+      reg.mask = true;
+   }
+   state->regs.isp_bgobjvals |= mask_bit;
+
+   pvr_transfer_set_filter(active_cmd, state);
+
+   result = pvr_isp_ctrl_stream(dev_info, ctx, active_cmd, prep_data);
+
+   mesa_loge("PVR_DEBUG v4: isp_ctrl_stream result=%d mtile_base=0x%016llx bgobjvals=0x%08x",
+             (int)result,
+             (unsigned long long)state->regs.isp_mtile_base,
+             state->regs.isp_bgobjvals);
+
+   return result;
+}
+
 static VkResult pvr_3d_copy_blit(struct pvr_transfer_ctx *ctx,
                                  struct pvr_transfer_cmd *transfer_cmd,
                                  struct pvr_transfer_prep_data *prep_data,
@@ -5708,11 +5779,11 @@
 
          active_cmd->scissor = mappings[0U].dst_rect;
 
-         result = pvr_3d_copy_blit_core(ctx,
-                                        active_cmd,
-                                        prep_data,
-                                        pass_idx,
-                                        finished_out);
+         result = pvr_3d_copy_blit_core_with_isp_stream(ctx,
+                                                        active_cmd,
+                                                        prep_data,
+                                                        pass_idx,
+                                                        finished_out);
       }
 
       return result;
@@ -5736,11 +5807,11 @@
       }
    }
 
-   return pvr_3d_copy_blit_core(ctx,
-                                active_cmd,
-                                prep_data,
-                                pass_idx,
-                                finished_out);
+   return pvr_3d_copy_blit_core_with_isp_stream(ctx,
+                                                active_cmd,
+                                                prep_data,
+                                                pass_idx,
+                                                finished_out);
 }
 
 /* TODO: This should be generated in csbgen. */
