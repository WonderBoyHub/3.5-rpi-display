#ifndef MODERN_DRM_INTERFACE_H
#define MODERN_DRM_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef __cplusplus
extern "C" {
#endif

// Modern DRM/KMS interface for Pi displays
typedef struct {
    int drm_fd;
    drmModeRes *resources;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeCrtc *crtc;
    drmModeModeInfo mode;
    uint32_t crtc_id;
    uint32_t connector_id;
    uint32_t encoder_id;
    
    // GBM for buffer management
    struct gbm_device *gbm_device;
    struct gbm_surface *gbm_surface;
    
    // EGL for GPU acceleration
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    EGLConfig egl_config;
    
    // Buffer management
    struct gbm_bo *previous_bo;
    uint32_t previous_fb;
    
    // Performance tracking
    struct {
        uint64_t frame_count;
        uint64_t vblank_count;
        double last_fps;
        uint64_t last_frame_time;
    } perf;
    
    // Configuration
    bool vsync_enabled;
    bool gpu_acceleration;
    bool huge_pages_enabled;
    int refresh_rate;
    
    // Hardware detection
    char gpu_name[256];
    char display_name[256];
    bool is_pi5;
    bool has_v3d;
    bool has_vc4;
    
} drm_context_t;

// DRM/KMS interface functions
int drm_init(drm_context_t *drm_ctx, const char *device_path);
int drm_setup_display(drm_context_t *drm_ctx, int width, int height, int refresh_rate);
int drm_create_framebuffer(drm_context_t *drm_ctx, uint32_t width, uint32_t height, uint32_t *fb_id);
int drm_present_buffer(drm_context_t *drm_ctx, uint32_t fb_id);
int drm_wait_vblank(drm_context_t *drm_ctx);
void drm_destroy(drm_context_t *drm_ctx);

// GPU acceleration functions
int drm_init_gpu_acceleration(drm_context_t *drm_ctx);
int drm_render_with_gpu(drm_context_t *drm_ctx, void *render_data);
void drm_destroy_gpu_acceleration(drm_context_t *drm_ctx);

// Hardware detection
int drm_detect_hardware(drm_context_t *drm_ctx);
bool drm_is_pi5_or_newer(drm_context_t *drm_ctx);
bool drm_has_v3d_support(drm_context_t *drm_ctx);
const char* drm_get_gpu_info(drm_context_t *drm_ctx);

// Performance optimization
int drm_enable_huge_pages(drm_context_t *drm_ctx);
int drm_optimize_memory_layout(drm_context_t *drm_ctx);
double drm_get_fps(drm_context_t *drm_ctx);

// Wayland EGL integration
int drm_init_wayland_egl(drm_context_t *drm_ctx);
int drm_create_wayland_surface(drm_context_t *drm_ctx, void *wl_display, void *wl_surface);

// Multi-display support
typedef struct {
    drm_context_t displays[4];  // Support up to 4 displays
    int num_displays;
    int primary_display;
} multi_display_context_t;

int drm_init_multi_display(multi_display_context_t *ctx);
int drm_add_display(multi_display_context_t *ctx, const char *device_path);
int drm_set_primary_display(multi_display_context_t *ctx, int display_index);
void drm_destroy_multi_display(multi_display_context_t *ctx);

// Error handling
typedef enum {
    DRM_OK = 0,
    DRM_ERROR_INIT = -1,
    DRM_ERROR_NO_DEVICE = -2,
    DRM_ERROR_NO_DISPLAY = -3,
    DRM_ERROR_GPU_INIT = -4,
    DRM_ERROR_MEMORY = -5,
    DRM_ERROR_HARDWARE = -6,
    DRM_ERROR_PERMISSION = -7,
    DRM_ERROR_NOT_SUPPORTED = -8
} drm_error_t;

const char* drm_get_error_string(drm_error_t error);

#ifdef __cplusplus
}
#endif

#endif // MODERN_DRM_INTERFACE_H 