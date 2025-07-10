#ifndef XPT2046_TOUCH_H
#define XPT2046_TOUCH_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "efficient_rpi_display.h"

// XPT2046 Commands
#define XPT2046_START_BIT   0x80
#define XPT2046_X_MEASURE   0x50  // Measure X position
#define XPT2046_Y_MEASURE   0x10  // Measure Y position
#define XPT2046_Z1_MEASURE  0x30  // Measure Z1 (pressure)
#define XPT2046_Z2_MEASURE  0x40  // Measure Z2 (pressure)
#define XPT2046_TEMP0       0x00  // Temperature 0
#define XPT2046_TEMP1       0x70  // Temperature 1
#define XPT2046_VBAT        0x20  // Battery voltage
#define XPT2046_VAUX        0x60  // Auxiliary voltage

// Touch GPIO pins
#define GPIO_TOUCH_CS   7   // Touch chip select
#define GPIO_TOUCH_IRQ  17  // Touch interrupt pin

// Touch settings
#define TOUCH_SPI_DEVICE     "/dev/spidev0.1"
#define TOUCH_SPI_MODE       SPI_MODE_0
#define TOUCH_SPI_SPEED      2000000  // 2MHz for touch controller
#define TOUCH_SAMPLE_COUNT   5        // Number of samples for averaging
#define TOUCH_PRESSURE_THRESHOLD 400  // Minimum pressure for valid touch
#define TOUCH_DEBOUNCE_TIME  50       // Debounce time in milliseconds

// Calibration constants (default values)
#define TOUCH_CAL_X_MIN     200
#define TOUCH_CAL_X_MAX     3900
#define TOUCH_CAL_Y_MIN     200
#define TOUCH_CAL_Y_MAX     3900

// Touch context structure
typedef struct {
    // SPI interface
    int spi_fd;
    struct spi_ioc_transfer spi_tr;
    
    // GPIO interface
    int gpio_fd_cs;
    int gpio_fd_irq;
    
    // Touch state
    bool touch_pressed;
    int16_t raw_x;
    int16_t raw_y;
    int16_t pressure;
    int16_t screen_x;
    int16_t screen_y;
    uint32_t touch_timestamp;
    
    // Calibration data
    touch_config_t calibration;
    
    // Threading
    pthread_t touch_thread;
    pthread_mutex_t touch_mutex;
    bool thread_running;
    
    // Interrupt handling
    int epoll_fd;
    bool interrupt_enabled;
    
    // Filtering
    int16_t filter_x[TOUCH_SAMPLE_COUNT];
    int16_t filter_y[TOUCH_SAMPLE_COUNT];
    int filter_index;
    bool filter_initialized;
    
    // Performance tracking
    uint32_t touch_count;
    uint64_t last_touch_time;
    
} xpt2046_ctx_t;

// Function prototypes
int xpt2046_init(xpt2046_ctx_t* ctx, const touch_config_t* config);
void xpt2046_destroy(xpt2046_ctx_t* ctx);
int xpt2046_start_interrupt_thread(xpt2046_ctx_t* ctx);
void xpt2046_stop_interrupt_thread(xpt2046_ctx_t* ctx);
touch_point_t xpt2046_read_touch(xpt2046_ctx_t* ctx);
bool xpt2046_is_touched(xpt2046_ctx_t* ctx);
int xpt2046_calibrate(xpt2046_ctx_t* ctx);

// Low-level touch functions
int xpt2046_read_raw_x(xpt2046_ctx_t* ctx);
int xpt2046_read_raw_y(xpt2046_ctx_t* ctx);
int xpt2046_read_pressure(xpt2046_ctx_t* ctx);
int xpt2046_read_channel(xpt2046_ctx_t* ctx, uint8_t channel);

// Calibration functions
void xpt2046_apply_calibration(xpt2046_ctx_t* ctx, int16_t raw_x, int16_t raw_y, int16_t* screen_x, int16_t* screen_y);
void xpt2046_set_calibration(xpt2046_ctx_t* ctx, const touch_config_t* config);

// Filtering functions
void xpt2046_filter_touch(xpt2046_ctx_t* ctx, int16_t raw_x, int16_t raw_y, int16_t* filtered_x, int16_t* filtered_y);
void xpt2046_reset_filter(xpt2046_ctx_t* ctx);

// Interrupt handling
void* xpt2046_interrupt_thread(void* arg);
int xpt2046_setup_interrupt(xpt2046_ctx_t* ctx);
void xpt2046_cleanup_interrupt(xpt2046_ctx_t* ctx);

// SPI helpers for touch
int touch_spi_init(xpt2046_ctx_t* ctx);
void touch_spi_destroy(xpt2046_ctx_t* ctx);
int touch_spi_transfer(xpt2046_ctx_t* ctx, const uint8_t* tx_data, uint8_t* rx_data, uint32_t length);

#endif // XPT2046_TOUCH_H 