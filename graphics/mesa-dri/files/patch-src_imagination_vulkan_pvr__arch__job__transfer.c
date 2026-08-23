--- src/imagination/vulkan/pvr_arch_job_transfer.c.orig	2026-08-22 23:56:30 UTC
+++ src/imagination/vulkan/pvr_arch_job_transfer.c
@@ -2981,6 +2981,33 @@ static VkResult pvr_3d_copy_blit_core(struct pvr_trans
       if (pvr_pick_component_needed(&state->custom_mapping))
          pvr_dma_texel_unwind(state, sh_reg_layout, dma_space);
 
+      /* Dump the texture state actually handed to the TQ blit shader.
+       *
+       * The TQ shader is the only internal shader that issues a texture
+       * sample (smp.2d.nncoords) and then waits on it (wdf drc0). If that
+       * sample never returns, the shader blocks forever, which is exactly the
+       * GUILTY_LOCKUP *timeout* every TQ_3D job dies with. So the question is
+       * whether the image state we DMA into the shared registers is valid.
+       *
+       * The shader reads image state at sh0..3 and sampler state at sh8..11,
+       * so those are the words that matter.
+       */
+      {
+         char _w[512];
+         unsigned _o = 0;
+         unsigned _n = MIN2(tex_state_dma_size_dw, 20U);
+
+         for (unsigned _i = 0; _i < _n && _o < sizeof(_w) - 12; _i++)
+            _o += sprintf(_w + _o, "%08x ", dma_space[_i]);
+
+         mesa_logw("PVR texstate: dw=%u dst_sh=%u src_addr=0x%llx",
+                   tex_state_dma_size_dw,
+                   state->common_ptr,
+                   (unsigned long long)transfer_cmd->sources[0]
+                      .surface.dev_addr.addr);
+         mesa_logw("PVR texstate: %s", _w);
+      }
+
       pvr_pds_encode_dma_burst(unitex_prog.texture_dma_control,
                                unitex_prog.texture_dma_address,
                                state->common_ptr,
