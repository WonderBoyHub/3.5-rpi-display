#ifndef EFFICIENT_RPI_DISPLAY_H
#define EFFICIENT_RPI_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Display dimensions
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 480
#define DISPLAY_BPP    16

// Color definitions (RGB565)
#define COLOR_BLACK    0x0000
#define COLOR_WHITE    0xFFFF
#define COLOR_RED      0xF800
#define COLOR_GREEN    0x07E0
#define COLOR_BLUE     0x001F
#define COLOR_YELLOW   0xFFE0
#define COLOR_CYAN     0x07FF
#define COLOR_MAGENTA  0xF81F

// Display rotation
typedef enum {
    ROTATE_0   = 0,
    ROTATE_90  = 1,
    ROTATE_180 = 2,
    ROTATE_270 = 3
} display_rotation_t;

// Display configuration
typedef struct {
    uint32_t spi_speed;
    uint8_t spi_mode;
    display_rotation_t rotation;
    bool enable_dma;
    bool enable_double_buffer;
    uint32_t refresh_rate;
} display_config_t;

// Display handle (opaque)
typedef struct rpi_display_ctx* display_handle_t;

// Touch point structure
typedef struct {
    int16_t x;
    int16_t y;
    bool pressed;
    uint32_t timestamp;
} touch_point_t;

// Touch configuration
typedef struct {
    int16_t cal_x_min;
    int16_t cal_x_max;
    int16_t cal_y_min;
    int16_t cal_y_max;
    bool swap_xy;
    bool invert_x;
    bool invert_y;
} touch_config_t;

// Display API
display_handle_t rpi_display_init(const display_config_t* config);
void rpi_display_destroy(display_handle_t display);
int rpi_display_set_rotation(display_handle_t display, display_rotation_t rotation);
int rpi_display_get_width(display_handle_t display);
int rpi_display_get_height(display_handle_t display);

// Drawing functions
int rpi_display_clear(display_handle_t display, uint16_t color);
int rpi_display_set_pixel(display_handle_t display, int x, int y, uint16_t color);
uint16_t rpi_display_get_pixel(display_handle_t display, int x, int y);
int rpi_display_fill_rect(display_handle_t display, int x, int y, int width, int height, uint16_t color);
int rpi_display_draw_line(display_handle_t display, int x0, int y0, int x1, int y1, uint16_t color);
int rpi_display_draw_circle(display_handle_t display, int x, int y, int radius, uint16_t color);
int rpi_display_draw_text(display_handle_t display, int x, int y, const char* text, uint16_t color);

// Buffer operations
int rpi_display_copy_buffer(display_handle_t display, const uint16_t* buffer, int x, int y, int width, int height);
int rpi_display_refresh(display_handle_t display);
int rpi_display_refresh_rect(display_handle_t display, int x, int y, int width, int height);

// Touch API
int rpi_touch_init(display_handle_t display, const touch_config_t* config);
void rpi_touch_destroy(display_handle_t display);
touch_point_t rpi_touch_read(display_handle_t display);
bool rpi_touch_is_pressed(display_handle_t display);
int rpi_touch_calibrate(display_handle_t display);
int rpi_touch_set_config(display_handle_t display, const touch_config_t* config);

// Utility functions
uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b);
void rgb565_to_rgb(uint16_t color, uint8_t* r, uint8_t* g, uint8_t* b);

// Error codes
#define RPI_DISPLAY_OK              0
#define RPI_DISPLAY_ERROR_INIT     -1
#define RPI_DISPLAY_ERROR_SPI      -2
#define RPI_DISPLAY_ERROR_GPIO     -3
#define RPI_DISPLAY_ERROR_MEMORY   -4
#define RPI_DISPLAY_ERROR_INVALID  -5
#define RPI_DISPLAY_ERROR_TIMEOUT  -6

#ifdef __cplusplus
}
#endif

#endif // EFFICIENT_RPI_DISPLAY_H 