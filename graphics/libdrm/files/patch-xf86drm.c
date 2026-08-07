--- xf86drm.c.orig	2026-08-07 12:09:59.074244791 -0500
+++ xf86drm.c	2026-08-07 12:14:35.088275107 -0500
@@ -3639,8 +3639,37 @@
             return DRM_BUS_VIRTIO;
      }
     return subsystem_type;
-#elif defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__)
+#elif defined(__OpenBSD__) || defined(__DragonFly__)
     return DRM_BUS_PCI;
+#elif defined(__FreeBSD__)
+    {
+        char dname[SPECNAMELEN];
+        char sysctl_name[16];
+        char sysctl_val[256];
+        size_t sysctl_len;
+        int id, type;
+        unsigned int rdev;
+
+        rdev = makedev(maj, min);
+        if (!devname_r(rdev, S_IFCHR, dname, sizeof(dname)))
+            return DRM_BUS_PCI;
+        if (sscanf(dname, "drm/%d", &id) != 1)
+            return DRM_BUS_PCI;
+        type = drmGetMinorType(maj, min);
+        if (type == DRM_NODE_RENDER)
+            id -= 128;
+        if (id < 0)
+            return DRM_BUS_PCI;
+        if (snprintf(sysctl_name, sizeof(sysctl_name), "hw.dri.%d.busid",
+            id) <= 0)
+            return DRM_BUS_PCI;
+        sysctl_len = sizeof(sysctl_val);
+        if (sysctlbyname(sysctl_name, sysctl_val, &sysctl_len, NULL, 0))
+            return DRM_BUS_PCI;
+        if (strncmp(sysctl_val, "platform:", 9) == 0)
+            return DRM_BUS_PLATFORM;
+        return DRM_BUS_PCI;
+    }
 #else
 #warning "Missing implementation of drmParseSubsystemType"
     return -EINVAL;
@@ -4319,6 +4348,35 @@
     free(name);
 
     return 0;
+#elif defined(__FreeBSD__)
+    {
+        char dname[SPECNAMELEN];
+        char sysctl_name[16];
+        char sysctl_val[256];
+        size_t sysctl_len;
+        int id;
+        unsigned int rdev;
+
+        rdev = makedev(maj, min);
+        if (!devname_r(rdev, S_IFCHR, dname, sizeof(dname)))
+            return -ENOENT;
+        if (sscanf(dname, "drm/%d", &id) != 1)
+            return -ENOENT;
+        if (id >= 128)
+            id -= 128;
+        if (snprintf(sysctl_name, sizeof(sysctl_name), "hw.dri.%d.busid",
+            id) <= 0)
+            return -ENOENT;
+        sysctl_len = sizeof(sysctl_val);
+        if (sysctlbyname(sysctl_name, sysctl_val, &sysctl_len, NULL, 0))
+            return -ENOENT;
+        if (strncmp(sysctl_val, "platform:", 9) == 0) {
+            strncpy(fullname, sysctl_val + 9, DRM_PLATFORM_DEVICE_NAME_LEN);
+            fullname[DRM_PLATFORM_DEVICE_NAME_LEN - 1] = '\0';
+            return 0;
+        }
+        return -ENOENT;
+    }
 #else
 #warning "Missing implementation of drmParseOFBusInfo"
     return -EINVAL;
@@ -4379,6 +4437,37 @@
 
     free(*compatible);
     return err;
+#elif defined(__FreeBSD__)
+    {
+        char dname[SPECNAMELEN];
+        char sysctl_name[16];
+        char sysctl_val[256];
+        size_t sysctl_len;
+        int id;
+        unsigned int rdev;
+
+        rdev = makedev(maj, min);
+        if (!devname_r(rdev, S_IFCHR, dname, sizeof(dname)))
+            return -ENOENT;
+        if (sscanf(dname, "drm/%d", &id) != 1)
+            return -ENOENT;
+        if (id >= 128)
+            id -= 128;
+        if (snprintf(sysctl_name, sizeof(sysctl_name), "hw.dri.%d.busid",
+            id) <= 0)
+            return -ENOENT;
+        sysctl_len = sizeof(sysctl_val);
+        if (sysctlbyname(sysctl_name, sysctl_val, &sysctl_len, NULL, 0))
+            return -ENOENT;
+        if (strncmp(sysctl_val, "platform:", 9) == 0) {
+            *compatible = calloc(2, sizeof(char *));
+            if (!*compatible)
+                return -ENOMEM;
+            (*compatible)[0] = strdup(sysctl_val + 9);
+            return 0;
+        }
+        return -ENOENT;
+    }
 #else
 #warning "Missing implementation of drmParseOFDeviceInfo"
     return -EINVAL;
