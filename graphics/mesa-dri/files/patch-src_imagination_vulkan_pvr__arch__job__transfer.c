--- src/imagination/vulkan/pvr_arch_job_transfer.c.orig	2025-07-10 00:00:00 UTC
+++ src/imagination/vulkan/pvr_arch_job_transfer.c
@@ -5713,6 +5713,27 @@
                                         prep_data,
                                         pass_idx,
                                         finished_out);
+         if (result != VK_SUCCESS)
+            return result;
+
+         /* FAST_SCALE (copy_blit_core) never builds an ISP control stream
+          * and defaults isp_bgobjvals.mask=false ("No background enabled"),
+          * which disables all ISP pixel output per pvr_3d_clip_blit's own
+          * comment. That path avoids this by flipping the mask bit and
+          * building a real stream via pvr_isp_ctrl_stream() -- do the same
+          * here.
+          */
+         {
+            uint32_t mask_bit;
+
+            pvr_csb_pack (&mask_bit, CR_ISP_BGOBJVALS, reg) {
+               reg.mask = true;
+            }
+            state->regs.isp_bgobjvals |= mask_bit;
+
+            result =
+               pvr_isp_ctrl_stream(dev_info, ctx, active_cmd, prep_data);
+         }
       }
 
       return result;
@@ -5736,11 +5757,30 @@
       }
    }
 
-   return pvr_3d_copy_blit_core(ctx,
-                                active_cmd,
-                                prep_data,
-                                pass_idx,
-                                finished_out);
+   result = pvr_3d_copy_blit_core(ctx,
+                                  active_cmd,
+                                  prep_data,
+                                  pass_idx,
+                                  finished_out);
+   if (result != VK_SUCCESS)
+      return result;
+
+   /* FAST_SCALE (copy_blit_core) never builds an ISP control stream and
+    * defaults isp_bgobjvals.mask=false ("No background enabled"), which
+    * disables all ISP pixel output per pvr_3d_clip_blit's own comment.
+    * That path avoids this by flipping the mask bit and building a real
+    * stream via pvr_isp_ctrl_stream() -- do the same here.
+    */
+   {
+      uint32_t mask_bit;
+
+      pvr_csb_pack (&mask_bit, CR_ISP_BGOBJVALS, reg) {
+         reg.mask = true;
+      }
+      state->regs.isp_bgobjvals |= mask_bit;
+   }
+
+   return pvr_isp_ctrl_stream(dev_info, ctx, active_cmd, prep_data);
 }
 
 /* TODO: This should be generated in csbgen. */
