--- src/imagination/vulkan/pvr_transfer_frag_store.c.orig	2026-08-22 22:33:12 UTC
+++ src/imagination/vulkan/pvr_transfer_frag_store.c
@@ -215,6 +215,39 @@ static VkResult pvr_transfer_frag_store_entry_data_com
 
    *num_usc_temps_out = pco_shader_data(tq)->common.temps;
 
+   /* Dump the generated transfer USC program so it can be compared against
+    * the reference programs the proprietary DDK ships in rgx.sh.
+    *
+    * Every TQ_3D transfer job on this part faults with a GUILTY_LOCKUP, which
+    * is a *timeout* - the GPU starts the job and never finishes it. A shader
+    * that never signals end-of-program produces exactly that signature, so
+    * the tail bytes matter as much as the size. In the DDK programs, 384 of
+    * 1611 end with 20 ff f3 ff ff ff ff ff and most of the rest with close
+    * variants.
+    *
+    * temps is directly comparable: it is the same quantity the DDK stores at
+    * record offset 0x04 and packs into the PDS DOUTU SRC0 word.
+    */
+   {
+      const uint8_t *const _bin = (const uint8_t *)pco_shader_binary_data(tq);
+      const unsigned _size = pco_shader_binary_size(tq);
+      const unsigned _n = MIN2(_size, 16U);
+      char _head[64] = { 0 };
+      char _tail[64] = { 0 };
+
+      for (unsigned _i = 0; _i < _n; _i++)
+         sprintf(_head + _i * 3, "%02x ", _bin[_i]);
+
+      for (unsigned _i = 0; _i < _n; _i++)
+         sprintf(_tail + _i * 3, "%02x ", _bin[_size - _n + _i]);
+
+      mesa_logw("PVR tq usc: size=%u temps=%u",
+                _size,
+                pco_shader_data(tq)->common.temps);
+      mesa_logw("PVR tq usc: head %s", _head);
+      mesa_logw("PVR tq usc: tail %s", _tail);
+   }
+
    result = pvr_gpu_upload_usc(device,
                                pco_shader_binary_data(tq),
                                pco_shader_binary_size(tq),
