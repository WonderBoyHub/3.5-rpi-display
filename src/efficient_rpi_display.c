#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include "efficient_rpi_display.h"
#include "display_context.h"
#include "ili9486l_driver.h"
#include "xpt2046_touch.h"

// Font data for text rendering (8x8 bitmap font)
static const uint8_t font_8x8[128][8] = {
    // ASCII 32-127 (basic printable characters)
    // This is a simplified font - in a real implementation, you'd include a complete font
    [32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    [33] = {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // !
    [34] = {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // "
    [35] = {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // #
    [36] = {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // $
    [37] = {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // %
    [38] = {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // &
    [39] = {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // '
    [40] = {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, // (
    [41] = {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, // )
    [42] = {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // *
    [43] = {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, // +
    [44] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x06, 0x00}, // ,
    [45] = {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, // -
    [46] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // .
    [47] = {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // /
    [48] = {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, // 0
    [49] = {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, // 1
    [50] = {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, // 2
    [51] = {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, // 3
    [52] = {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, // 4
    [53] = {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, // 5
    [54] = {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, // 6
    [55] = {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, // 7
    [56] = {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, // 8
    [57] = {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, // 9
    [58] = {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // :
    [59] = {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x06, 0x00}, // ;
    [60] = {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, // <
    [61] = {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, // =
    [62] = {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // >
    [63] = {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, // ?
    [64] = {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, // @
    [65] = {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, // A
    [66] = {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, // B
    [67] = {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, // C
    [68] = {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, // D
    [69] = {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, // E
    [70] = {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, // F
    [71] = {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, // G
    [72] = {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, // H
    [73] = {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // I
    [74] = {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, // J
    [75] = {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, // K
    [76] = {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, // L
    [77] = {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, // M
    [78] = {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // N
    [79] = {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // O
    [80] = {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, // P
    [81] = {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, // Q
    [82] = {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, // R
    [83] = {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, // S
    [84] = {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // T
    [85] = {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, // U
    [86] = {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // V
    [87] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // W
    [88] = {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, // X
    [89] = {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, // Y
    [90] = {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, // Z
    // Add more characters as needed...
};

// Internal helper functions
static void draw_character(display_handle_t display, int x, int y, char c, uint16_t color);
static void swap_buffers(rpi_display_ctx_t* ctx);

// Display API implementation
display_handle_t rpi_display_init(const display_config_t* config) {
    rpi_display_ctx_t* ctx = malloc(sizeof(rpi_display_ctx_t));
    if (!ctx) {
        return NULL;
    }
    
    memset(ctx, 0, sizeof(*ctx));
    
    // Initialize default configuration if none provided
    if (config) {
        memcpy(&ctx->config, config, sizeof(display_config_t));
    } else {
        // Default configuration
        ctx->config.spi_speed = 80000000;
        ctx->config.spi_mode = 0;
        ctx->config.rotation = ROTATE_0;
        ctx->config.enable_dma = true;
        ctx->config.enable_double_buffer = true;
        ctx->config.refresh_rate = 60;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&ctx->context_mutex, NULL) != 0) {
        free(ctx);
        return NULL;
    }
    
    // Initialize display driver
    if (ili9486l_init(&ctx->display, &ctx->config) != RPI_DISPLAY_OK) {
        pthread_mutex_destroy(&ctx->context_mutex);
        free(ctx);
        return NULL;
    }
    
    // Initialize touch driver
    touch_config_t touch_config = {
        .cal_x_min = TOUCH_CAL_X_MIN,
        .cal_x_max = TOUCH_CAL_X_MAX,
        .cal_y_min = TOUCH_CAL_Y_MIN,
        .cal_y_max = TOUCH_CAL_Y_MAX,
        .swap_xy = false,
        .invert_x = false,
        .invert_y = false
    };
    
    if (xpt2046_init(&ctx->touch, &touch_config) == RPI_DISPLAY_OK) {
        ctx->touch_enabled = true;
        xpt2046_start_interrupt_thread(&ctx->touch);
    } else {
        printf("Warning: Touch initialization failed, display-only mode\n");
        ctx->touch_enabled = false;
    }
    
    ctx->initialized = true;
    return (display_handle_t)ctx;
}

void rpi_display_destroy(display_handle_t display) {
    if (!display) return;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    if (ctx->initialized) {
        // Destroy touch driver
        if (ctx->touch_enabled) {
            xpt2046_destroy(&ctx->touch);
        }
        
        // Destroy display driver
        ili9486l_destroy(&ctx->display);
        
        // Destroy mutex
        pthread_mutex_destroy(&ctx->context_mutex);
        
        ctx->initialized = false;
    }
    
    free(ctx);
}

int rpi_display_set_rotation(display_handle_t display, display_rotation_t rotation) {
    if (!display) return RPI_DISPLAY_ERROR_INVALID;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    pthread_mutex_lock(&ctx->context_mutex);
    int result = ili9486l_set_rotation(&ctx->display, rotation);
    ctx->config.rotation = rotation;
    pthread_mutex_unlock(&ctx->context_mutex);
    
    return result;
}

int rpi_display_get_width(display_handle_t display) {
    if (!display) return 0;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    return ctx->display.width;
}

int rpi_display_get_height(display_handle_t display) {
    if (!display) return 0;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    return ctx->display.height;
}

// Drawing functions
int rpi_display_clear(display_handle_t display, uint16_t color) {
    if (!display) return RPI_DISPLAY_ERROR_INVALID;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    pthread_mutex_lock(&ctx->context_mutex);
    
    uint16_t* buffer = ctx->display.double_buffer_enabled ? 
                      ctx->display.backbuffer : ctx->display.framebuffer;
    
    uint32_t pixel_count = ctx->display.width * ctx->display.height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        buffer[i] = color;
    }
    
    // Mark entire screen as dirty
    mark_dirty_rect(&ctx->display, 0, 0, ctx->display.width, ctx->display.height);
    
    pthread_mutex_unlock(&ctx->context_mutex);
    
    return RPI_DISPLAY_OK;
}

int rpi_display_set_pixel(display_handle_t display, int x, int y, uint16_t color) {
    if (!display) return RPI_DISPLAY_ERROR_INVALID;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    if (x < 0 || x >= ctx->display.width || y < 0 || y >= ctx->display.height) {
        return RPI_DISPLAY_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&ctx->context_mutex);
    
    uint16_t* buffer = ctx->display.double_buffer_enabled ? 
                      ctx->display.backbuffer : ctx->display.framebuffer;
    
    buffer[y * ctx->display.width + x] = color;
    
    // Mark pixel as dirty
    mark_dirty_rect(&ctx->display, x, y, 1, 1);
    
    pthread_mutex_unlock(&ctx->context_mutex);
    
    return RPI_DISPLAY_OK;
}

uint16_t rpi_display_get_pixel(display_handle_t display, int x, int y) {
    if (!display) return 0;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    if (x < 0 || x >= ctx->display.width || y < 0 || y >= ctx->display.height) {
        return 0;
    }
    
    pthread_mutex_lock(&ctx->context_mutex);
    
    uint16_t* buffer = ctx->display.double_buffer_enabled ? 
                      ctx->display.backbuffer : ctx->display.framebuffer;
    
    uint16_t pixel = buffer[y * ctx->display.width + x];
    
    pthread_mutex_unlock(&ctx->context_mutex);
    
    return pixel;
}

int rpi_display_fill_rect(display_handle_t display, int x, int y, int width, int height, uint16_t color) {
    if (!display) return RPI_DISPLAY_ERROR_INVALID;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    // Clip rectangle to display bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > ctx->display.width) width = ctx->display.width - x;
    if (y + height > ctx->display.height) height = ctx->display.height - y;
    
    if (width <= 0 || height <= 0) return RPI_DISPLAY_OK;
    
    pthread_mutex_lock(&ctx->context_mutex);
    
    uint16_t* buffer = ctx->display.double_buffer_enabled ? 
                      ctx->display.backbuffer : ctx->display.framebuffer;
    
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            buffer[(y + row) * ctx->display.width + (x + col)] = color;
        }
    }
    
    // Mark rectangle as dirty
    mark_dirty_rect(&ctx->display, x, y, width, height);
    
    pthread_mutex_unlock(&ctx->context_mutex);
    
    return RPI_DISPLAY_OK;
}

int rpi_display_draw_line(display_handle_t display, int x0, int y0, int x1, int y1, uint16_t color) {
    if (!display) return RPI_DISPLAY_ERROR_INVALID;
    
    // Bresenham's line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    int x = x0, y = y0;
    
    while (true) {
        rpi_display_set_pixel(display, x, y, color);
        
        if (x == x1 && y == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    return RPI_DISPLAY_OK;
}

int rpi_display_draw_circle(display_handle_t display, int x, int y, int radius, uint16_t color) {
    if (!display) return RPI_DISPLAY_ERROR_INVALID;
    
    // Midpoint circle algorithm
    int xx = 0;
    int yy = radius;
    int d = 3 - 2 * radius;
    
    while (yy >= xx) {
        // Draw 8 symmetric points
        rpi_display_set_pixel(display, x + xx, y + yy, color);
        rpi_display_set_pixel(display, x - xx, y + yy, color);
        rpi_display_set_pixel(display, x + xx, y - yy, color);
        rpi_display_set_pixel(display, x - xx, y - yy, color);
        rpi_display_set_pixel(display, x + yy, y + xx, color);
        rpi_display_set_pixel(display, x - yy, y + xx, color);
        rpi_display_set_pixel(display, x + yy, y - xx, color);
        rpi_display_set_pixel(display, x - yy, y - xx, color);
        
        xx++;
        if (d > 0) {
            yy--;
            d = d + 4 * (xx - yy) + 10;
        } else {
            d = d + 4 * xx + 6;
        }
    }
    
    return RPI_DISPLAY_OK;
}

int rpi_display_draw_text(display_handle_t display, int x, int y, const char* text, uint16_t color) {
    if (!display || !text) return RPI_DISPLAY_ERROR_INVALID;
    
    int start_x = x;
    
    while (*text) {
        if (*text == '\n') {
            x = start_x;
            y += 8;
        } else {
            draw_character(display, x, y, *text, color);
            x += 8;
        }
        text++;
    }
    
    return RPI_DISPLAY_OK;
}

int rpi_display_copy_buffer(display_handle_t display, const uint16_t* buffer, int x, int y, int width, int height) {
    if (!display || !buffer) return RPI_DISPLAY_ERROR_INVALID;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    // Clip rectangle to display bounds
    if (x < 0) { width += x; buffer += -x; x = 0; }
    if (y < 0) { height += y; buffer += -y * width; y = 0; }
    if (x + width > ctx->display.width) width = ctx->display.width - x;
    if (y + height > ctx->display.height) height = ctx->display.height - y;
    
    if (width <= 0 || height <= 0) return RPI_DISPLAY_OK;
    
    pthread_mutex_lock(&ctx->context_mutex);
    
    uint16_t* target_buffer = ctx->display.double_buffer_enabled ? 
                             ctx->display.backbuffer : ctx->display.framebuffer;
    
    for (int row = 0; row < height; row++) {
        memcpy(&target_buffer[(y + row) * ctx->display.width + x],
               &buffer[row * width], width * sizeof(uint16_t));
    }
    
    // Mark rectangle as dirty
    mark_dirty_rect(&ctx->display, x, y, width, height);
    
    pthread_mutex_unlock(&ctx->context_mutex);
    
    return RPI_DISPLAY_OK;
}

int rpi_display_refresh(display_handle_t display) {
    if (!display) return RPI_DISPLAY_ERROR_INVALID;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    pthread_mutex_lock(&ctx->context_mutex);
    
    // Swap buffers if double buffering is enabled
    if (ctx->display.double_buffer_enabled) {
        swap_buffers(ctx);
    }
    
    int result = ili9486l_refresh_display(&ctx->display);
    
    pthread_mutex_unlock(&ctx->context_mutex);
    
    return result;
}

int rpi_display_refresh_rect(display_handle_t display, int x, int y, int width, int height) {
    if (!display) return RPI_DISPLAY_ERROR_INVALID;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    pthread_mutex_lock(&ctx->context_mutex);
    
    int result = ili9486l_refresh_rect(&ctx->display, x, y, width, height);
    
    pthread_mutex_unlock(&ctx->context_mutex);
    
    return result;
}

// Touch API implementation
int rpi_touch_init(display_handle_t display, const touch_config_t* config) {
    if (!display) return RPI_DISPLAY_ERROR_INVALID;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    if (ctx->touch_enabled) {
        return RPI_DISPLAY_OK; // Already initialized
    }
    
    if (xpt2046_init(&ctx->touch, config) == RPI_DISPLAY_OK) {
        ctx->touch_enabled = true;
        xpt2046_start_interrupt_thread(&ctx->touch);
        return RPI_DISPLAY_OK;
    }
    
    return RPI_DISPLAY_ERROR_INIT;
}

void rpi_touch_destroy(display_handle_t display) {
    if (!display) return;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    if (ctx->touch_enabled) {
        xpt2046_destroy(&ctx->touch);
        ctx->touch_enabled = false;
    }
}

touch_point_t rpi_touch_read(display_handle_t display) {
    touch_point_t point = {0};
    
    if (!display) return point;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    if (ctx->touch_enabled) {
        point = xpt2046_read_touch(&ctx->touch);
    }
    
    return point;
}

bool rpi_touch_is_pressed(display_handle_t display) {
    if (!display) return false;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    if (ctx->touch_enabled) {
        return xpt2046_is_touched(&ctx->touch);
    }
    
    return false;
}

int rpi_touch_calibrate(display_handle_t display) {
    if (!display) return RPI_DISPLAY_ERROR_INVALID;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    if (ctx->touch_enabled) {
        return xpt2046_calibrate(&ctx->touch);
    }
    
    return RPI_DISPLAY_ERROR_INIT;
}

int rpi_touch_set_config(display_handle_t display, const touch_config_t* config) {
    if (!display || !config) return RPI_DISPLAY_ERROR_INVALID;
    
    rpi_display_ctx_t* ctx = (rpi_display_ctx_t*)display;
    
    if (ctx->touch_enabled) {
        xpt2046_set_calibration(&ctx->touch, config);
        return RPI_DISPLAY_OK;
    }
    
    return RPI_DISPLAY_ERROR_INIT;
}

// Utility functions
uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void rgb565_to_rgb(uint16_t color, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = (color >> 11) << 3;
    *g = ((color >> 5) & 0x3F) << 2;
    *b = (color & 0x1F) << 3;
}

// Internal helper functions
static void draw_character(display_handle_t display, int x, int y, char c, uint16_t color) {
    if (c < 32 || c > 127) c = 32; // Default to space for unsupported characters
    
    const uint8_t* char_data = font_8x8[(int)c];
    
    for (int row = 0; row < 8; row++) {
        uint8_t line = char_data[row];
        for (int col = 0; col < 8; col++) {
            if (line & (0x80 >> col)) {
                rpi_display_set_pixel(display, x + col, y + row, color);
            }
        }
    }
}

static void swap_buffers(rpi_display_ctx_t* ctx) {
    uint16_t* temp = ctx->display.framebuffer;
    ctx->display.framebuffer = ctx->display.backbuffer;
    ctx->display.backbuffer = temp;
} 