--- src/imagination/vulkan/pvr_usc.c.orig	2026-08-23 00:19:01 UTC
+++ src/imagination/vulkan/pvr_usc.c
@@ -10,6 +10,7 @@
  * \brief USC internal shader generation.
  */
 
+#include <stdlib.h>
 #include "hwdef/rogue_hw_utils.h"
 #include "nir/nir.h"
 #include "nir/nir_builder.h"
@@ -937,6 +938,28 @@ pco_shader *pvr_uscgen_tq(pco_ctx *ctx,
 
    loaded_data =
       pvr_uscgen_tq_frag_load(&b, 0, coords, shader_props, sh_reg_layout);
+
+   /* Diagnostic: PVR_TQ_NO_SAMPLE=1 replaces the sampled texel with a
+    * constant.
+    *
+    * The TQ shader is the only internal shader that issues a texture sample
+    * and then waits on it (smp.2d.nncoords followed by wdf drc0), and every
+    * TQ_3D job dies with a GUILTY_LOCKUP, which is a *timeout* rather than a
+    * fault at an address. If that sample never returns, the wait blocks
+    * forever and produces exactly that signature.
+    *
+    * Replacing the result makes the sample dead code, so DCE drops it and the
+    * shader keeps its shape otherwise. If the faults stop, the sample is the
+    * hang; if they continue, the sample is innocent and the problem is in the
+    * surrounding job setup. This is a diagnostic, never a fix - the blit
+    * copies nothing with it enabled.
+    */
+   if (getenv("PVR_TQ_NO_SAMPLE")) {
+      loaded_data = nir_imm_zero(&b,
+                                 loaded_data->num_components,
+                                 loaded_data->bit_size);
+      mesa_logw("PVR tq: SAMPLE BYPASSED (diagnostic)");
+   }
 
    loaded_data =
       pvr_uscgen_tq_frag_conv(&b, loaded_data, layer_props->pbe_format);
