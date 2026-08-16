--- src/imagination/vulkan/pvr_arch_job_transfer.c.orig	2025-07-10 00:00:00 UTC
+++ src/imagination/vulkan/pvr_arch_job_transfer.c
@@ -909,6 +909,41 @@
        */
       state->width_in_tiles -= state->origin_x_in_tiles;
       state->height_in_tiles -= state->origin_y_in_tiles;
+
+      /*
+       * Fix attempt 7 (BXE-4-32 / JH7110): size the ISP macro-tile grid from
+       * the full destination surface rather than the clip rectangle.
+       *
+       * Evidence: the working proprietary driver was captured live on this
+       * exact silicon (DDK bridge ioctl 0xc0206440, RGXTQ bridge id 128
+       * function 3 = RGXKickTransfer) and submits
+       * ISP_MTILE_SIZE = 0x00500040 (x=80, y=64) with ISP_RENDER_ORIGIN = 0
+       * for BOTH a full-surface fill AND a 200x200 clipped blit -- i.e. it
+       * always describes the whole render target (1280x1024 / 16). Every
+       * other field we could decode matches ours byte-for-byte
+       * (usc_pixel_output_ctrl=0x1ff8, isp_ctl=0x23000, isp_aa=0,
+       * event_pixel_pds_info=0x202, isp_rgn=0).
+       *
+       * Ours derives the grid from the clip rect, which for a small blit
+       * (e.g. a terminal glyph) yields ISP_MTILE_SIZE = 0x00010001 -- a
+       * degenerate 1x1 macro-tile render. If the ISP control stream or
+       * primitive blocks then reference anything outside that single tile,
+       * the ISP has no valid tile to retire into and the job never
+       * terminates -- which matches the measured behaviour exactly: 100% of
+       * TQ_3D transfer jobs fault with reason=1 (GUILTY_LOCKUP) on dm=3,
+       * independently of FAST_SCALE vs FAST_2D.
+       */
+      {
+         uint32_t full_w = DIV_ROUND_UP(surface_params->width, tile_size_x);
+         uint32_t full_h = DIV_ROUND_UP(surface_params->height, tile_size_y);
+
+         if (full_w > 0U && full_h > 0U) {
+            state->origin_x_in_tiles = 0U;
+            state->origin_y_in_tiles = 0U;
+            state->width_in_tiles = full_w;
+            state->height_in_tiles = full_h;
+         }
+      }
    }
 
    render_params->source_start = PVR_PBE_STARTPOS_BIT0;
@@ -5994,12 +6029,15 @@
          prep_data->state = prev_prep_data->state;
       }
 
-      if (transfer_cmd->flags & PVR_TRANSFER_CMD_FLAGS_FAST2D) {
+      {
+         /* Force FAST2D path: pvr_3d_copy_blit uses FAST_SCALE mode but
+          * never builds an ISP control stream, leaving isp_mtile_base=0.
+          * The ISP faults on the NULL address, causing a FW context reset
+          * that wipes PDS_EXEC_BASE and cascades into further faults.
+          */
+         transfer_cmd->flags |= PVR_TRANSFER_CMD_FLAGS_FAST2D;
          result =
             pvr_3d_clip_blit(ctx, transfer_cmd, prep_data, pass, &finished);
-      } else {
-         result =
-            pvr_3d_copy_blit(ctx, transfer_cmd, prep_data, pass, &finished);
       }
       if (result != VK_SUCCESS)
          return result;
