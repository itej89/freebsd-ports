--- src/imagination/vulkan/pvr_arch_job_transfer.c.orig
+++ src/imagination/vulkan/pvr_arch_job_transfer.c
@@ -3032,6 +3032,19 @@
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
@@ -4569,8 +4582,21 @@
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
@@ -5535,6 +5561,35 @@
       transfer_cmd->dst.mem_layout = PVR_MEMLAYOUT_TWIDDLED;
    }
 
+   /*
+    * The blob emits its copy blits with PBE memlayout LINEAR; we emit
+    * TWIDDLE_2D (docs/18). That may be legitimate - a compositor blits into
+    * GPU images, while the blob's test wrote a plain linear buffer - so
+    * report what the destination actually looks like, and allow forcing
+    * LINEAR to test whether the twiddled description is what scrambles the
+    * output into rectangular blocks.
+    */
+   {
+      static int logged;
+
+      if (logged < 6) {
+         logged++;
+         mesa_logw("PVRDST: mem_layout=%d w=%u h=%u stride=%u addr=0x%llx",
+                   (int)transfer_cmd->dst.mem_layout,
+                   transfer_cmd->dst.width,
+                   transfer_cmd->dst.height,
+                   transfer_cmd->dst.stride,
+                   (unsigned long long)transfer_cmd->dst.dev_addr.addr);
+      }
+
+      if (transfer_cmd->dst.mem_layout == PVR_MEMLAYOUT_TWIDDLED &&
+          getenv("PVR_FORCE_LINEAR_DST")) {
+         transfer_cmd->dst.mem_layout = PVR_MEMLAYOUT_LINEAR;
+         if (logged <= 6)
+            mesa_logw("PVRDST: forced LINEAR");
+      }
+   }
+
    if (transfer_cmd->dst.mem_layout == PVR_MEMLAYOUT_TWIDDLED) {
       transfer_cmd->dst.width =
          MIN2((uint32_t)custom_mapping->max_clip_size, transfer_cmd->dst.width);
