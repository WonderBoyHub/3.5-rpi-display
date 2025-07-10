#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <linux/spi/spidev.h>

#include "xpt2046_touch.h"
#include "ili9486l_driver.h"

// Static helper functions
static void delay_ms(int ms);
static uint64_t get_time_ns(void);
static int median_filter(int16_t* values, int count);

// Touch SPI helper functions
int touch_spi_init(xpt2046_ctx_t* ctx) {
    uint8_t mode = TOUCH_SPI_MODE;
    uint8_t bits = 8;
    uint32_t speed = TOUCH_SPI_SPEED;
    
    ctx->spi_fd = open(TOUCH_SPI_DEVICE, O_RDWR);
    if (ctx->spi_fd < 0) {
        perror("Failed to open touch SPI device");
        return -1;
    }
    
    // Set SPI mode
    if (ioctl(ctx->spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
        perror("Failed to set touch SPI mode");
        close(ctx->spi_fd);
        return -1;
    }
    
    // Set bits per word
    if (ioctl(ctx->spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        perror("Failed to set touch SPI bits per word");
        close(ctx->spi_fd);
        return -1;
    }
    
    // Set max speed
    if (ioctl(ctx->spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("Failed to set touch SPI speed");
        close(ctx->spi_fd);
        return -1;
    }
    
    return 0;
}

void touch_spi_destroy(xpt2046_ctx_t* ctx) {
    if (ctx->spi_fd >= 0) {
        close(ctx->spi_fd);
        ctx->spi_fd = -1;
    }
}

int touch_spi_transfer(xpt2046_ctx_t* ctx, const uint8_t* tx_data, uint8_t* rx_data, uint32_t length) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_data,
        .rx_buf = (unsigned long)rx_data,
        .len = length,
        .speed_hz = TOUCH_SPI_SPEED,
        .bits_per_word = 8,
        .delay_usecs = 0,
    };
    
    if (ioctl(ctx->spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("Touch SPI transfer failed");
        return -1;
    }
    
    return 0;
}

// Low-level touch functions
int xpt2046_read_channel(xpt2046_ctx_t* ctx, uint8_t channel) {
    uint8_t tx_data[3] = {XPT2046_START_BIT | channel, 0x00, 0x00};
    uint8_t rx_data[3] = {0, 0, 0};
    
    // Set touch CS low
    gpio_set_value(GPIO_TOUCH_CS, 0);
    
    // Transfer data
    if (touch_spi_transfer(ctx, tx_data, rx_data, 3) < 0) {
        gpio_set_value(GPIO_TOUCH_CS, 1);
        return -1;
    }
    
    // Set touch CS high
    gpio_set_value(GPIO_TOUCH_CS, 1);
    
    // Extract 12-bit value from response
    int result = ((rx_data[1] & 0x7F) << 5) | (rx_data[2] >> 3);
    return result;
}

int xpt2046_read_raw_x(xpt2046_ctx_t* ctx) {
    return xpt2046_read_channel(ctx, XPT2046_X_MEASURE);
}

int xpt2046_read_raw_y(xpt2046_ctx_t* ctx) {
    return xpt2046_read_channel(ctx, XPT2046_Y_MEASURE);
}

int xpt2046_read_pressure(xpt2046_ctx_t* ctx) {
    int z1 = xpt2046_read_channel(ctx, XPT2046_Z1_MEASURE);
    int z2 = xpt2046_read_channel(ctx, XPT2046_Z2_MEASURE);
    
    if (z1 == 0) return 0;
    
    // Calculate pressure using formula from datasheet
    int pressure = (z2 - z1) * 1000 / z1;
    return pressure;
}

// Filtering functions
void xpt2046_filter_touch(xpt2046_ctx_t* ctx, int16_t raw_x, int16_t raw_y, int16_t* filtered_x, int16_t* filtered_y) {
    // Add new values to circular buffer
    ctx->filter_x[ctx->filter_index] = raw_x;
    ctx->filter_y[ctx->filter_index] = raw_y;
    ctx->filter_index = (ctx->filter_index + 1) % TOUCH_SAMPLE_COUNT;
    
    if (!ctx->filter_initialized) {
        // Fill buffer with initial values
        for (int i = 0; i < TOUCH_SAMPLE_COUNT; i++) {
            ctx->filter_x[i] = raw_x;
            ctx->filter_y[i] = raw_y;
        }
        ctx->filter_initialized = true;
    }
    
    // Apply median filter to reduce noise
    int16_t temp_x[TOUCH_SAMPLE_COUNT];
    int16_t temp_y[TOUCH_SAMPLE_COUNT];
    
    memcpy(temp_x, ctx->filter_x, sizeof(temp_x));
    memcpy(temp_y, ctx->filter_y, sizeof(temp_y));
    
    *filtered_x = median_filter(temp_x, TOUCH_SAMPLE_COUNT);
    *filtered_y = median_filter(temp_y, TOUCH_SAMPLE_COUNT);
}

void xpt2046_reset_filter(xpt2046_ctx_t* ctx) {
    ctx->filter_initialized = false;
    ctx->filter_index = 0;
    memset(ctx->filter_x, 0, sizeof(ctx->filter_x));
    memset(ctx->filter_y, 0, sizeof(ctx->filter_y));
}

// Calibration functions
void xpt2046_apply_calibration(xpt2046_ctx_t* ctx, int16_t raw_x, int16_t raw_y, int16_t* screen_x, int16_t* screen_y) {
    // Apply calibration transformation
    int16_t cal_x = raw_x;
    int16_t cal_y = raw_y;
    
    // Apply coordinate transformations
    if (ctx->calibration.swap_xy) {
        int16_t temp = cal_x;
        cal_x = cal_y;
        cal_y = temp;
    }
    
    if (ctx->calibration.invert_x) {
        cal_x = 4095 - cal_x;
    }
    
    if (ctx->calibration.invert_y) {
        cal_y = 4095 - cal_y;
    }
    
    // Scale to screen coordinates
    *screen_x = (cal_x - ctx->calibration.cal_x_min) * DISPLAY_WIDTH / 
                (ctx->calibration.cal_x_max - ctx->calibration.cal_x_min);
    *screen_y = (cal_y - ctx->calibration.cal_y_min) * DISPLAY_HEIGHT / 
                (ctx->calibration.cal_y_max - ctx->calibration.cal_y_min);
    
    // Clamp to screen bounds
    if (*screen_x < 0) *screen_x = 0;
    if (*screen_x >= DISPLAY_WIDTH) *screen_x = DISPLAY_WIDTH - 1;
    if (*screen_y < 0) *screen_y = 0;
    if (*screen_y >= DISPLAY_HEIGHT) *screen_y = DISPLAY_HEIGHT - 1;
}

void xpt2046_set_calibration(xpt2046_ctx_t* ctx, const touch_config_t* config) {
    memcpy(&ctx->calibration, config, sizeof(touch_config_t));
}

// Interrupt handling
int xpt2046_setup_interrupt(xpt2046_ctx_t* ctx) {
    char path[64];
    int fd;
    
    // Set up interrupt pin
    if (gpio_export(GPIO_TOUCH_IRQ) < 0) {
        return -1;
    }
    
    if (gpio_set_direction(GPIO_TOUCH_IRQ, "in") < 0) {
        return -1;
    }
    
    // Set interrupt edge
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", GPIO_TOUCH_IRQ);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open interrupt edge");
        return -1;
    }
    
    if (write(fd, "falling", 7) < 0) {
        perror("Failed to set interrupt edge");
        close(fd);
        return -1;
    }
    close(fd);
    
    // Open interrupt value file
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", GPIO_TOUCH_IRQ);
    ctx->gpio_fd_irq = open(path, O_RDONLY);
    if (ctx->gpio_fd_irq < 0) {
        perror("Failed to open interrupt value");
        return -1;
    }
    
    // Set up epoll for interrupt handling
    ctx->epoll_fd = epoll_create1(0);
    if (ctx->epoll_fd < 0) {
        perror("Failed to create epoll");
        return -1;
    }
    
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = ctx->gpio_fd_irq;
    
    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->gpio_fd_irq, &ev) < 0) {
        perror("Failed to add interrupt to epoll");
        return -1;
    }
    
    ctx->interrupt_enabled = true;
    return 0;
}

void xpt2046_cleanup_interrupt(xpt2046_ctx_t* ctx) {
    if (ctx->epoll_fd >= 0) {
        close(ctx->epoll_fd);
        ctx->epoll_fd = -1;
    }
    
    if (ctx->gpio_fd_irq >= 0) {
        close(ctx->gpio_fd_irq);
        ctx->gpio_fd_irq = -1;
    }
    
    gpio_unexport(GPIO_TOUCH_IRQ);
    ctx->interrupt_enabled = false;
}

void* xpt2046_interrupt_thread(void* arg) {
    xpt2046_ctx_t* ctx = (xpt2046_ctx_t*)arg;
    struct epoll_event events[1];
    char buffer[64];
    
    while (ctx->thread_running) {
        int nfds = epoll_wait(ctx->epoll_fd, events, 1, 100); // 100ms timeout
        
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait failed");
            break;
        }
        
        if (nfds == 0) continue; // Timeout, check if thread should continue
        
        // Clear the interrupt by reading the value
        if (read(ctx->gpio_fd_irq, buffer, sizeof(buffer)) < 0) {
            perror("Failed to read interrupt value");
            continue;
        }
        
        // Check if touch is still pressed
        if (gpio_get_value(GPIO_TOUCH_IRQ) == 0) {
            // Touch is pressed, read coordinates
            pthread_mutex_lock(&ctx->touch_mutex);
            
            // Read multiple samples for better accuracy
            int16_t x_samples[TOUCH_SAMPLE_COUNT];
            int16_t y_samples[TOUCH_SAMPLE_COUNT];
            int valid_samples = 0;
            
            for (int i = 0; i < TOUCH_SAMPLE_COUNT; i++) {
                int x = xpt2046_read_raw_x(ctx);
                int y = xpt2046_read_raw_y(ctx);
                int pressure = xpt2046_read_pressure(ctx);
                
                if (x > 0 && y > 0 && pressure > TOUCH_PRESSURE_THRESHOLD) {
                    x_samples[valid_samples] = x;
                    y_samples[valid_samples] = y;
                    valid_samples++;
                }
                
                delay_ms(1); // Small delay between samples
            }
            
            if (valid_samples > 0) {
                // Use median of valid samples
                ctx->raw_x = median_filter(x_samples, valid_samples);
                ctx->raw_y = median_filter(y_samples, valid_samples);
                
                // Apply filtering
                int16_t filtered_x, filtered_y;
                xpt2046_filter_touch(ctx, ctx->raw_x, ctx->raw_y, &filtered_x, &filtered_y);
                
                // Apply calibration
                xpt2046_apply_calibration(ctx, filtered_x, filtered_y, &ctx->screen_x, &ctx->screen_y);
                
                ctx->touch_pressed = true;
                ctx->touch_timestamp = get_time_ns() / 1000000; // Convert to milliseconds
                ctx->touch_count++;
                ctx->last_touch_time = ctx->touch_timestamp;
            }
            
            pthread_mutex_unlock(&ctx->touch_mutex);
        } else {
            // Touch is released
            pthread_mutex_lock(&ctx->touch_mutex);
            ctx->touch_pressed = false;
            xpt2046_reset_filter(ctx);
            pthread_mutex_unlock(&ctx->touch_mutex);
        }
    }
    
    return NULL;
}

// Public API functions
int xpt2046_init(xpt2046_ctx_t* ctx, const touch_config_t* config) {
    memset(ctx, 0, sizeof(*ctx));
    
    // Initialize default configuration
    if (config) {
        memcpy(&ctx->calibration, config, sizeof(touch_config_t));
    } else {
        // Use default calibration
        ctx->calibration.cal_x_min = TOUCH_CAL_X_MIN;
        ctx->calibration.cal_x_max = TOUCH_CAL_X_MAX;
        ctx->calibration.cal_y_min = TOUCH_CAL_Y_MIN;
        ctx->calibration.cal_y_max = TOUCH_CAL_Y_MAX;
        ctx->calibration.swap_xy = false;
        ctx->calibration.invert_x = false;
        ctx->calibration.invert_y = false;
    }
    
    // Initialize GPIO pins
    if (gpio_export(GPIO_TOUCH_CS) < 0) {
        return RPI_DISPLAY_ERROR_GPIO;
    }
    
    if (gpio_set_direction(GPIO_TOUCH_CS, "out") < 0) {
        return RPI_DISPLAY_ERROR_GPIO;
    }
    
    // Set CS high initially
    gpio_set_value(GPIO_TOUCH_CS, 1);
    
    // Initialize SPI
    if (touch_spi_init(ctx) < 0) {
        return RPI_DISPLAY_ERROR_SPI;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&ctx->touch_mutex, NULL) != 0) {
        perror("Failed to initialize touch mutex");
        return RPI_DISPLAY_ERROR_INIT;
    }
    
    // Set up interrupt handling
    if (xpt2046_setup_interrupt(ctx) < 0) {
        return RPI_DISPLAY_ERROR_GPIO;
    }
    
    // Reset filter
    xpt2046_reset_filter(ctx);
    
    return RPI_DISPLAY_OK;
}

void xpt2046_destroy(xpt2046_ctx_t* ctx) {
    if (!ctx) return;
    
    // Stop interrupt thread
    xpt2046_stop_interrupt_thread(ctx);
    
    // Clean up interrupt handling
    xpt2046_cleanup_interrupt(ctx);
    
    // Clean up SPI
    touch_spi_destroy(ctx);
    
    // Clean up GPIO
    gpio_unexport(GPIO_TOUCH_CS);
    
    // Destroy mutex
    pthread_mutex_destroy(&ctx->touch_mutex);
}

int xpt2046_start_interrupt_thread(xpt2046_ctx_t* ctx) {
    ctx->thread_running = true;
    
    if (pthread_create(&ctx->touch_thread, NULL, xpt2046_interrupt_thread, ctx) != 0) {
        perror("Failed to create touch interrupt thread");
        ctx->thread_running = false;
        return -1;
    }
    
    return 0;
}

void xpt2046_stop_interrupt_thread(xpt2046_ctx_t* ctx) {
    if (ctx->thread_running) {
        ctx->thread_running = false;
        pthread_join(ctx->touch_thread, NULL);
    }
}

touch_point_t xpt2046_read_touch(xpt2046_ctx_t* ctx) {
    touch_point_t point = {0};
    
    pthread_mutex_lock(&ctx->touch_mutex);
    
    point.x = ctx->screen_x;
    point.y = ctx->screen_y;
    point.pressed = ctx->touch_pressed;
    point.timestamp = ctx->touch_timestamp;
    
    pthread_mutex_unlock(&ctx->touch_mutex);
    
    return point;
}

bool xpt2046_is_touched(xpt2046_ctx_t* ctx) {
    bool pressed;
    
    pthread_mutex_lock(&ctx->touch_mutex);
    pressed = ctx->touch_pressed;
    pthread_mutex_unlock(&ctx->touch_mutex);
    
    return pressed;
}

int xpt2046_calibrate(xpt2046_ctx_t* ctx) {
    // Basic calibration - in a real implementation, this would
    // display calibration points and collect user input
    printf("Touch calibration not implemented in this version.\n");
    printf("Use default calibration values or set them manually.\n");
    return RPI_DISPLAY_OK;
}

// Helper functions
static void delay_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static int median_filter(int16_t* values, int count) {
    // Simple bubble sort for median filtering
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (values[j] > values[j + 1]) {
                int16_t temp = values[j];
                values[j] = values[j + 1];
                values[j + 1] = temp;
            }
        }
    }
    
    return values[count / 2];
} 