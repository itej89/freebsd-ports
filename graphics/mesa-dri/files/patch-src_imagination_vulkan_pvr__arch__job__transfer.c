--- src/imagination/vulkan/pvr_arch_job_transfer.c.orig
+++ src/imagination/vulkan/pvr_arch_job_transfer.c
@@ -742,6 +742,35 @@
    *width_out = surface->width;
    *stride_out = surface->stride;
    *dev_addr_out = surface->dev_addr;
+
+   /*
+    * The blob emits copy blits with PBE memlayout LINEAR; we emit TWIDDLE_2D
+    * (docs/18, from pbe_wordx_mrty bit 32). That may be legitimate - a
+    * compositor blits into GPU images while the blob's test wrote a linear
+    * buffer - so report what the destination really is. If stride equals
+    * width * bpp/8 the buffer is linear and describing it as twiddled is a
+    * bug; otherwise TWIDDLE_2D is correct and this lead is dead.
+    *
+    * PVR_FORCE_LINEAR_DST=1 forces LINEAR on the destination only, so the
+    * hypothesis can be A/B tested visually without another rebuild.
+    */
+   {
+      static int logged;
+
+      if (!is_input && logged < 6) {
+         logged++;
+         mesa_logw("PVRDST: layout=%d w=%u h=%u stride=%u bpp=%u addr=0x%llx",
+                   (int)*mem_layout_out, *width_out, *height_out, *stride_out,
+                   bpp, (unsigned long long)dev_addr_out->addr);
+      }
+
+      if (!is_input && *mem_layout_out == PVR_MEMLAYOUT_TWIDDLED &&
+          getenv("PVR_FORCE_LINEAR_DST")) {
+         *mem_layout_out = PVR_MEMLAYOUT_LINEAR;
+         if (logged <= 6)
+            mesa_logw("PVRDST: forced LINEAR");
+      }
+   }
 
    if (surface->mem_layout != PVR_MEMLAYOUT_LINEAR &&
        !pvr_is_surface_aligned(*dev_addr_out, is_input, bpp)) {
@@ -3032,6 +3061,19 @@
                                       &reg.dir_type);
       if (result != VK_SUCCESS)
          return result;
+   }
+
+   /* BXE-4-32 fix: the FAST_SCALE path never set isp_rgn, leaving it zero
+    * from the memset of prep_data. The firmware feeds this field straight to
+    * GPU register 0x0F28, and the working proprietary driver programs
+    * 0x1F000000 there. See the matching change in pvr_isp_ctrl_stream().
+    */
+   if (PVR_HAS_FEATURE(dev_info, simple_internal_parameter_format_v2) &&
+       PVR_HAS_FEATURE(dev_info, ipf_creq_pf)) {
+      pvr_csb_pack (&regs->isp_rgn, CR_ISP_RGN_SIPF, isp_rgn) {
+         isp_rgn.cs_size_ipf_creq_pf =
+            ROGUE_CR_ISP_RGN_SIPF_CS_SIZE_IPF_CREQ_PF_MAX;
+      }
    }
 
    /* Set up pixel event handling. */
@@ -4569,8 +4611,21 @@
       pvr_csb_pack (&regs->isp_rgn, CR_ISP_RGN_SIPF, isp_rgn) {
          /* Bit 0 in CR_ISP_RGN.cs_size_ipf_creq_pf is used to indicate the
           * presence of a link.
+          *
+          * BXE-4-32 fix: this field must be 0x1F, not just the link bit.
+          * csbgen's own cr.xml carries a FIXME saying it "should have a
+          * default value of 0x1F"; the field is bits[24:28] with MAX=31.
+          * Confirmed three ways on this hardware:
+          *   - the open firmware's TQ_3D worker writes cmd+0x6c (isp_rgn)
+          *     straight to GPU register 0x0F28;
+          *   - a live /dev/mem read of that register under the WORKING
+          *     proprietary driver shows 0x1F000000 (= 0x1F << 24);
+          *   - we were submitting 0, and 100% of TQ_3D transfer jobs hung
+          *     with GUILTY_LOCKUP regardless of ISP render mode.
           */
-         isp_rgn.cs_size_ipf_creq_pf = was_linked;
+         isp_rgn.cs_size_ipf_creq_pf =
+            ROGUE_CR_ISP_RGN_SIPF_CS_SIZE_IPF_CREQ_PF_MAX;
+         (void)was_linked;
       }
    } else {
       /* clang-format off */
@@ -5534,6 +5589,7 @@
          transfer_cmd->dst.stride + custom_mapping->texel_unwind_dst;
       transfer_cmd->dst.mem_layout = PVR_MEMLAYOUT_TWIDDLED;
    }
+   }
 
    if (transfer_cmd->dst.mem_layout == PVR_MEMLAYOUT_TWIDDLED) {
       transfer_cmd->dst.width =
