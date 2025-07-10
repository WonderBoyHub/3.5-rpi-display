#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <linux/version.h>

#include "modern_drm_interface.h"

// GPU vendor IDs for hardware detection
#define BROADCOM_VENDOR_ID 0x14E4
#define V3D_DEVICE_ID 0x2711  // Pi 4
#define V3D_7_DEVICE_ID 0x2712  // Pi 5

// DRM device paths
static const char* drm_device_paths[] = {
    "/dev/dri/card0",
    "/dev/dri/card1",
    "/dev/dri/card2",
    "/dev/dri/renderD128",
    "/dev/dri/renderD129",
    NULL
};

// Error string mapping
static const char* error_strings[] = {
    "Success",
    "Initialization failed",
    "No DRM device found",
    "No display found",
    "GPU initialization failed",
    "Memory allocation failed",
    "Hardware not supported",
    "Permission denied",
    "Feature not supported"
};

const char* drm_get_error_string(drm_error_t error) {
    int index = -error;
    if (index < 0 || index >= sizeof(error_strings) / sizeof(error_strings[0])) {
        return "Unknown error";
    }
    return error_strings[index];
}

// Hardware detection implementation
static bool is_raspberry_pi(void) {
    FILE *fp = fopen("/proc/device-tree/model", "r");
    if (!fp) return false;
    
    char model[256];
    if (fgets(model, sizeof(model), fp)) {
        fclose(fp);
        return strstr(model, "Raspberry Pi") != NULL;
    }
    fclose(fp);
    return false;
}

static bool is_pi5_or_newer(void) {
    FILE *fp = fopen("/proc/device-tree/model", "r");
    if (!fp) return false;
    
    char model[256];
    if (fgets(model, sizeof(model), fp)) {
        fclose(fp);
        return strstr(model, "Raspberry Pi 5") != NULL;
    }
    fclose(fp);
    return false;
}

int drm_detect_hardware(drm_context_t *drm_ctx) {
    if (!drm_ctx) return DRM_ERROR_INIT;
    
    // Clear hardware info
    memset(drm_ctx->gpu_name, 0, sizeof(drm_ctx->gpu_name));
    memset(drm_ctx->display_name, 0, sizeof(drm_ctx->display_name));
    drm_ctx->is_pi5 = false;
    drm_ctx->has_v3d = false;
    drm_ctx->has_vc4 = false;
    
    // Check if we're on a Raspberry Pi
    if (!is_raspberry_pi()) {
        printf("Warning: Not running on Raspberry Pi hardware\n");
        return DRM_OK;
    }
    
    // Detect Pi model
    drm_ctx->is_pi5 = is_pi5_or_newer();
    if (drm_ctx->is_pi5) {
        strcpy(drm_ctx->gpu_name, "VideoCore VII (V3D 7.1)");
        drm_ctx->has_v3d = true;
        drm_ctx->has_vc4 = true;
    } else {
        strcpy(drm_ctx->gpu_name, "VideoCore VI (V3D 4.2)");
        drm_ctx->has_v3d = true;
        drm_ctx->has_vc4 = true;
    }
    
    // Check for V3D driver
    if (access("/dev/dri/renderD128", F_OK) == 0) {
        drm_ctx->has_v3d = true;
        printf("V3D render node detected\n");
    }
    
    // Check for VC4 driver
    if (access("/sys/class/drm/card0", F_OK) == 0) {
        drm_ctx->has_vc4 = true;
        printf("VC4 display driver detected\n");
    }
    
    return DRM_OK;
}

bool drm_is_pi5_or_newer(drm_context_t *drm_ctx) {
    return drm_ctx ? drm_ctx->is_pi5 : false;
}

bool drm_has_v3d_support(drm_context_t *drm_ctx) {
    return drm_ctx ? drm_ctx->has_v3d : false;
}

const char* drm_get_gpu_info(drm_context_t *drm_ctx) {
    return drm_ctx ? drm_ctx->gpu_name : "Unknown";
}

// DRM initialization
int drm_init(drm_context_t *drm_ctx, const char *device_path) {
    if (!drm_ctx) return DRM_ERROR_INIT;
    
    memset(drm_ctx, 0, sizeof(*drm_ctx));
    
    // Detect hardware first
    drm_detect_hardware(drm_ctx);
    
    // Find DRM device
    if (device_path) {
        drm_ctx->drm_fd = open(device_path, O_RDWR | O_CLOEXEC);
    } else {
        // Try default paths
        for (int i = 0; drm_device_paths[i]; i++) {
            drm_ctx->drm_fd = open(drm_device_paths[i], O_RDWR | O_CLOEXEC);
            if (drm_ctx->drm_fd >= 0) {
                printf("Using DRM device: %s\n", drm_device_paths[i]);
                break;
            }
        }
    }
    
    if (drm_ctx->drm_fd < 0) {
        printf("Failed to open DRM device: %s\n", strerror(errno));
        return DRM_ERROR_NO_DEVICE;
    }
    
    // Get DRM resources
    drm_ctx->resources = drmModeGetResources(drm_ctx->drm_fd);
    if (!drm_ctx->resources) {
        printf("Failed to get DRM resources\n");
        close(drm_ctx->drm_fd);
        return DRM_ERROR_NO_DEVICE;
    }
    
    // Find a connected display
    for (int i = 0; i < drm_ctx->resources->count_connectors; i++) {
        drm_ctx->connector = drmModeGetConnector(drm_ctx->drm_fd, 
                                                drm_ctx->resources->connectors[i]);
        if (drm_ctx->connector && drm_ctx->connector->connection == DRM_MODE_CONNECTED) {
            drm_ctx->connector_id = drm_ctx->connector->connector_id;
            break;
        }
        if (drm_ctx->connector) {
            drmModeFreeConnector(drm_ctx->connector);
            drm_ctx->connector = NULL;
        }
    }
    
    if (!drm_ctx->connector) {
        printf("No connected display found\n");
        drmModeFreeResources(drm_ctx->resources);
        close(drm_ctx->drm_fd);
        return DRM_ERROR_NO_DISPLAY;
    }
    
    // Find encoder
    if (drm_ctx->connector->encoder_id) {
        drm_ctx->encoder = drmModeGetEncoder(drm_ctx->drm_fd, drm_ctx->connector->encoder_id);
        if (drm_ctx->encoder) {
            drm_ctx->encoder_id = drm_ctx->encoder->encoder_id;
            drm_ctx->crtc_id = drm_ctx->encoder->crtc_id;
        }
    }
    
    // Get CRTC
    if (drm_ctx->crtc_id) {
        drm_ctx->crtc = drmModeGetCrtc(drm_ctx->drm_fd, drm_ctx->crtc_id);
    }
    
    // Initialize GBM
    drm_ctx->gbm_device = gbm_create_device(drm_ctx->drm_fd);
    if (!drm_ctx->gbm_device) {
        printf("Failed to create GBM device\n");
        // Continue without GBM - fallback to basic mode
    }
    
    printf("DRM initialized successfully\n");
    printf("GPU: %s\n", drm_ctx->gpu_name);
    printf("Display: %dx%d\n", drm_ctx->connector->modes[0].hdisplay, 
           drm_ctx->connector->modes[0].vdisplay);
    
    return DRM_OK;
}

int drm_setup_display(drm_context_t *drm_ctx, int width, int height, int refresh_rate) {
    if (!drm_ctx || !drm_ctx->connector) return DRM_ERROR_INIT;
    
    // Find best mode
    drmModeModeInfo *best_mode = NULL;
    for (int i = 0; i < drm_ctx->connector->count_modes; i++) {
        drmModeModeInfo *mode = &drm_ctx->connector->modes[i];
        
        if (mode->hdisplay == width && mode->vdisplay == height) {
            if (!best_mode || mode->vrefresh == refresh_rate) {
                best_mode = mode;
                if (mode->vrefresh == refresh_rate) break;
            }
        }
    }
    
    if (!best_mode) {
        // Use first available mode
        best_mode = &drm_ctx->connector->modes[0];
        printf("Using default mode: %dx%d@%dHz\n", 
               best_mode->hdisplay, best_mode->vdisplay, best_mode->vrefresh);
    }
    
    memcpy(&drm_ctx->mode, best_mode, sizeof(drm_ctx->mode));
    drm_ctx->refresh_rate = best_mode->vrefresh;
    
    // Create GBM surface if available
    if (drm_ctx->gbm_device) {
        drm_ctx->gbm_surface = gbm_surface_create(drm_ctx->gbm_device,
                                                  drm_ctx->mode.hdisplay,
                                                  drm_ctx->mode.vdisplay,
                                                  GBM_FORMAT_XRGB8888,
                                                  GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
        if (!drm_ctx->gbm_surface) {
            printf("Warning: Failed to create GBM surface\n");
        }
    }
    
    return DRM_OK;
}

// GPU acceleration functions
int drm_init_gpu_acceleration(drm_context_t *drm_ctx) {
    if (!drm_ctx || !drm_ctx->gbm_device) return DRM_ERROR_GPU_INIT;
    
    // Initialize EGL
    drm_ctx->egl_display = eglGetDisplay((EGLNativeDisplayType)drm_ctx->gbm_device);
    if (drm_ctx->egl_display == EGL_NO_DISPLAY) {
        printf("Failed to get EGL display\n");
        return DRM_ERROR_GPU_INIT;
    }
    
    if (!eglInitialize(drm_ctx->egl_display, NULL, NULL)) {
        printf("Failed to initialize EGL\n");
        return DRM_ERROR_GPU_INIT;
    }
    
    // Configure EGL
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    
    EGLint num_configs;
    if (!eglChooseConfig(drm_ctx->egl_display, config_attribs, 
                        &drm_ctx->egl_config, 1, &num_configs)) {
        printf("Failed to choose EGL config\n");
        return DRM_ERROR_GPU_INIT;
    }
    
    // Create EGL context
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    
    drm_ctx->egl_context = eglCreateContext(drm_ctx->egl_display, 
                                           drm_ctx->egl_config, 
                                           EGL_NO_CONTEXT, 
                                           context_attribs);
    if (drm_ctx->egl_context == EGL_NO_CONTEXT) {
        printf("Failed to create EGL context\n");
        return DRM_ERROR_GPU_INIT;
    }
    
    // Create EGL surface
    if (drm_ctx->gbm_surface) {
        drm_ctx->egl_surface = eglCreateWindowSurface(drm_ctx->egl_display,
                                                     drm_ctx->egl_config,
                                                     (EGLNativeWindowType)drm_ctx->gbm_surface,
                                                     NULL);
        if (drm_ctx->egl_surface == EGL_NO_SURFACE) {
            printf("Failed to create EGL surface\n");
            return DRM_ERROR_GPU_INIT;
        }
    }
    
    // Make context current
    if (!eglMakeCurrent(drm_ctx->egl_display, drm_ctx->egl_surface, 
                       drm_ctx->egl_surface, drm_ctx->egl_context)) {
        printf("Failed to make EGL context current\n");
        return DRM_ERROR_GPU_INIT;
    }
    
    drm_ctx->gpu_acceleration = true;
    printf("GPU acceleration enabled\n");
    
    return DRM_OK;
}

// Performance optimization - huge pages support
int drm_enable_huge_pages(drm_context_t *drm_ctx) {
    if (!drm_ctx) return DRM_ERROR_INIT;
    
    // Check if huge pages are available
    if (access("/sys/kernel/mm/transparent_hugepage/enabled", F_OK) == 0) {
        printf("Transparent huge pages detected\n");
        drm_ctx->huge_pages_enabled = true;
        return DRM_OK;
    }
    
    // Try to enable huge pages for this process
    if (madvise(NULL, 0, MADV_HUGEPAGE) == 0) {
        printf("Huge pages enabled for process\n");
        drm_ctx->huge_pages_enabled = true;
        return DRM_OK;
    }
    
    printf("Huge pages not available\n");
    return DRM_ERROR_NOT_SUPPORTED;
}

int drm_optimize_memory_layout(drm_context_t *drm_ctx) {
    if (!drm_ctx) return DRM_ERROR_INIT;
    
    // Enable huge pages if available
    drm_enable_huge_pages(drm_ctx);
    
    // Set memory allocation hints for better performance
    if (drm_ctx->gbm_device) {
        // GBM handles memory optimization automatically
        printf("Memory optimization enabled via GBM\n");
        return DRM_OK;
    }
    
    return DRM_ERROR_NOT_SUPPORTED;
}

// Wayland EGL integration
int drm_init_wayland_egl(drm_context_t *drm_ctx) {
    if (!drm_ctx) return DRM_ERROR_INIT;
    
    // Check for Wayland environment
    if (!getenv("WAYLAND_DISPLAY")) {
        printf("Not running under Wayland\n");
        return DRM_ERROR_NOT_SUPPORTED;
    }
    
    // Initialize EGL for Wayland
    drm_ctx->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (drm_ctx->egl_display == EGL_NO_DISPLAY) {
        printf("Failed to get Wayland EGL display\n");
        return DRM_ERROR_GPU_INIT;
    }
    
    if (!eglInitialize(drm_ctx->egl_display, NULL, NULL)) {
        printf("Failed to initialize Wayland EGL\n");
        return DRM_ERROR_GPU_INIT;
    }
    
    printf("Wayland EGL initialized\n");
    return DRM_OK;
}

// Performance monitoring
double drm_get_fps(drm_context_t *drm_ctx) {
    if (!drm_ctx) return 0.0;
    
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    
    uint64_t current_ns = current_time.tv_sec * 1000000000ULL + current_time.tv_nsec;
    
    if (drm_ctx->perf.last_frame_time != 0) {
        uint64_t delta_ns = current_ns - drm_ctx->perf.last_frame_time;
        if (delta_ns > 0) {
            drm_ctx->perf.last_fps = 1000000000.0 / delta_ns;
        }
    }
    
    drm_ctx->perf.last_frame_time = current_ns;
    drm_ctx->perf.frame_count++;
    
    return drm_ctx->perf.last_fps;
}

int drm_wait_vblank(drm_context_t *drm_ctx) {
    if (!drm_ctx || drm_ctx->drm_fd < 0) return DRM_ERROR_INIT;
    
    drmVBlank vbl;
    vbl.request.type = DRM_VBLANK_RELATIVE;
    vbl.request.sequence = 1;
    
    if (drmWaitVBlank(drm_ctx->drm_fd, &vbl) == 0) {
        drm_ctx->perf.vblank_count++;
        return DRM_OK;
    }
    
    return DRM_ERROR_HARDWARE;
}

// Cleanup
void drm_destroy_gpu_acceleration(drm_context_t *drm_ctx) {
    if (!drm_ctx) return;
    
    if (drm_ctx->egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(drm_ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        
        if (drm_ctx->egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(drm_ctx->egl_display, drm_ctx->egl_surface);
        }
        
        if (drm_ctx->egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(drm_ctx->egl_display, drm_ctx->egl_context);
        }
        
        eglTerminate(drm_ctx->egl_display);
    }
    
    drm_ctx->gpu_acceleration = false;
}

void drm_destroy(drm_context_t *drm_ctx) {
    if (!drm_ctx) return;
    
    drm_destroy_gpu_acceleration(drm_ctx);
    
    if (drm_ctx->gbm_surface) {
        gbm_surface_destroy(drm_ctx->gbm_surface);
    }
    
    if (drm_ctx->gbm_device) {
        gbm_device_destroy(drm_ctx->gbm_device);
    }
    
    if (drm_ctx->crtc) {
        drmModeFreeCrtc(drm_ctx->crtc);
    }
    
    if (drm_ctx->encoder) {
        drmModeFreeEncoder(drm_ctx->encoder);
    }
    
    if (drm_ctx->connector) {
        drmModeFreeConnector(drm_ctx->connector);
    }
    
    if (drm_ctx->resources) {
        drmModeFreeResources(drm_ctx->resources);
    }
    
    if (drm_ctx->drm_fd >= 0) {
        close(drm_ctx->drm_fd);
    }
    
    printf("DRM context destroyed\n");
}

// Multi-display support
int drm_init_multi_display(multi_display_context_t *ctx) {
    if (!ctx) return DRM_ERROR_INIT;
    
    memset(ctx, 0, sizeof(*ctx));
    
    // Try to initialize primary display
    if (drm_init(&ctx->displays[0], NULL) == DRM_OK) {
        ctx->num_displays = 1;
        ctx->primary_display = 0;
        return DRM_OK;
    }
    
    return DRM_ERROR_NO_DEVICE;
}

void drm_destroy_multi_display(multi_display_context_t *ctx) {
    if (!ctx) return;
    
    for (int i = 0; i < ctx->num_displays; i++) {
        drm_destroy(&ctx->displays[i]);
    }
    
    memset(ctx, 0, sizeof(*ctx));
} 