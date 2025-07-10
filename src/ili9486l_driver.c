#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>
#include <linux/spi/spidev.h>

#include "ili9486l_driver.h"
#include "efficient_rpi_display.h"

// Static helper functions
static int write_command_data(ili9486l_ctx_t* ctx, uint8_t cmd, const uint8_t* data, int len);
static void delay_ms(int ms);
static uint64_t get_time_ns(void);

// GPIO helper functions
int gpio_export(int pin) {
    char buffer[64];
    int len, fd;
    
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        perror("Failed to open export for writing");
        return -1;
    }
    
    len = snprintf(buffer, sizeof(buffer), "%d", pin);
    if (write(fd, buffer, len) < 0) {
        perror("Failed to export gpio");
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}

int gpio_unexport(int pin) {
    char buffer[64];
    int len, fd;
    
    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0) {
        perror("Failed to open unexport for writing");
        return -1;
    }
    
    len = snprintf(buffer, sizeof(buffer), "%d", pin);
    if (write(fd, buffer, len) < 0) {
        perror("Failed to unexport gpio");
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}

int gpio_set_direction(int pin, const char* direction) {
    char path[64];
    int fd;
    
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open gpio direction for writing");
        return -1;
    }
    
    if (write(fd, direction, strlen(direction)) < 0) {
        perror("Failed to set direction");
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}

int gpio_set_value(int pin, int value) {
    char path[64];
    char value_str[8];
    int fd;
    
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open gpio value for writing");
        return -1;
    }
    
    snprintf(value_str, sizeof(value_str), "%d", value);
    if (write(fd, value_str, strlen(value_str)) < 0) {
        perror("Failed to write value");
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}

int gpio_get_value(int pin) {
    char path[64];
    char value_str[8];
    int fd;
    
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open gpio value for reading");
        return -1;
    }
    
    if (read(fd, value_str, sizeof(value_str)) < 0) {
        perror("Failed to read value");
        close(fd);
        return -1;
    }
    
    close(fd);
    return atoi(value_str);
}

// SPI helper functions
int spi_init(ili9486l_ctx_t* ctx) {
    uint8_t mode = SPI_MODE;
    uint8_t bits = SPI_BITS_PER_WORD;
    uint32_t speed = ctx->spi_speed;
    
    ctx->spi_fd = open(SPI_DEVICE, O_RDWR);
    if (ctx->spi_fd < 0) {
        perror("Failed to open SPI device");
        return -1;
    }
    
    // Set SPI mode
    if (ioctl(ctx->spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
        perror("Failed to set SPI mode");
        close(ctx->spi_fd);
        return -1;
    }
    
    // Set bits per word
    if (ioctl(ctx->spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        perror("Failed to set SPI bits per word");
        close(ctx->spi_fd);
        return -1;
    }
    
    // Set max speed
    if (ioctl(ctx->spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("Failed to set SPI speed");
        close(ctx->spi_fd);
        return -1;
    }
    
    // Allocate transfer buffers
    ctx->tx_buffer = malloc(DMA_BUFFER_SIZE);
    ctx->rx_buffer = malloc(DMA_BUFFER_SIZE);
    
    if (!ctx->tx_buffer || !ctx->rx_buffer) {
        perror("Failed to allocate SPI buffers");
        spi_destroy(ctx);
        return -1;
    }
    
    return 0;
}

void spi_destroy(ili9486l_ctx_t* ctx) {
    if (ctx->spi_fd >= 0) {
        close(ctx->spi_fd);
        ctx->spi_fd = -1;
    }
    
    if (ctx->tx_buffer) {
        free(ctx->tx_buffer);
        ctx->tx_buffer = NULL;
    }
    
    if (ctx->rx_buffer) {
        free(ctx->rx_buffer);
        ctx->rx_buffer = NULL;
    }
}

int spi_transfer(ili9486l_ctx_t* ctx, const uint8_t* tx_data, uint8_t* rx_data, uint32_t length) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_data,
        .rx_buf = (unsigned long)rx_data,
        .len = length,
        .speed_hz = ctx->spi_speed,
        .bits_per_word = SPI_BITS_PER_WORD,
        .delay_usecs = 0,
    };
    
    if (ioctl(ctx->spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("SPI transfer failed");
        return -1;
    }
    
    return 0;
}

// Display control functions
int ili9486l_write_command(ili9486l_ctx_t* ctx, uint8_t command) {
    // Set DC low for command
    gpio_set_value(GPIO_DC, 0);
    
    // Send command
    if (spi_transfer(ctx, &command, NULL, 1) < 0) {
        return -1;
    }
    
    return 0;
}

int ili9486l_write_data(ili9486l_ctx_t* ctx, const uint8_t* data, uint32_t length) {
    // Set DC high for data
    gpio_set_value(GPIO_DC, 1);
    
    // Send data
    if (spi_transfer(ctx, data, NULL, length) < 0) {
        return -1;
    }
    
    return 0;
}

static int write_command_data(ili9486l_ctx_t* ctx, uint8_t cmd, const uint8_t* data, int len) {
    if (ili9486l_write_command(ctx, cmd) < 0) {
        return -1;
    }
    
    if (len > 0 && data) {
        if (ili9486l_write_data(ctx, data, len) < 0) {
            return -1;
        }
    }
    
    return 0;
}

int ili9486l_reset(ili9486l_ctx_t* ctx) {
    // Hardware reset
    gpio_set_value(GPIO_RST, 0);
    delay_ms(10);
    gpio_set_value(GPIO_RST, 1);
    delay_ms(120);
    
    return 0;
}

int ili9486l_configure(ili9486l_ctx_t* ctx) {
    // Configuration sequence for ILI9486L
    uint8_t data[16];
    
    // Sleep out
    if (write_command_data(ctx, ILI9486L_SLPOUT, NULL, 0) < 0) return -1;
    delay_ms(120);
    
    // Pixel format: 16-bit RGB565
    data[0] = 0x55;
    if (write_command_data(ctx, ILI9486L_PIXFMT, data, 1) < 0) return -1;
    
    // Power Control 1
    data[0] = 0x0F;
    data[1] = 0x0F;
    if (write_command_data(ctx, ILI9486L_PWCTR1, data, 2) < 0) return -1;
    
    // Power Control 2
    data[0] = 0x41;
    if (write_command_data(ctx, ILI9486L_PWCTR2, data, 1) < 0) return -1;
    
    // VCOM Control 1
    data[0] = 0x00;
    data[1] = 0x35;
    data[2] = 0x80;
    if (write_command_data(ctx, ILI9486L_VMCTR1, data, 3) < 0) return -1;
    
    // VCOM Control 2
    data[0] = 0x00;
    if (write_command_data(ctx, ILI9486L_VMCTR2, data, 1) < 0) return -1;
    
    // Frame Rate Control
    data[0] = 0x00;
    data[1] = 0x1B;
    if (write_command_data(ctx, ILI9486L_FRMCTR1, data, 2) < 0) return -1;
    
    // Display Function Control
    data[0] = 0x00;
    data[1] = 0x02;
    data[2] = 0x3B;
    if (write_command_data(ctx, ILI9486L_DFUNCTR, data, 3) < 0) return -1;
    
    // Positive Gamma Correction
    data[0] = 0x0F; data[1] = 0x24; data[2] = 0x1C; data[3] = 0x0A;
    data[4] = 0x0F; data[5] = 0x08; data[6] = 0x43; data[7] = 0x88;
    data[8] = 0x32; data[9] = 0x0F; data[10] = 0x10; data[11] = 0x06;
    data[12] = 0x0F; data[13] = 0x07; data[14] = 0x00;
    if (write_command_data(ctx, ILI9486L_GMCTRP1, data, 15) < 0) return -1;
    
    // Negative Gamma Correction
    data[0] = 0x0F; data[1] = 0x38; data[2] = 0x30; data[3] = 0x09;
    data[4] = 0x0F; data[5] = 0x0F; data[6] = 0x4E; data[7] = 0x77;
    data[8] = 0x3C; data[9] = 0x07; data[10] = 0x10; data[11] = 0x05;
    data[12] = 0x23; data[13] = 0x1B; data[14] = 0x00;
    if (write_command_data(ctx, ILI9486L_GMCTRN1, data, 15) < 0) return -1;
    
    // Set rotation
    ili9486l_set_rotation(ctx, ctx->rotation);
    
    // Display on
    if (write_command_data(ctx, ILI9486L_DISPON, NULL, 0) < 0) return -1;
    delay_ms(100);
    
    return 0;
}

int ili9486l_set_rotation(ili9486l_ctx_t* ctx, uint8_t rotation) {
    uint8_t madctl = ILI9486L_MADCTL_BGR;
    
    switch (rotation) {
        case 0: // Portrait
            madctl |= ILI9486L_MADCTL_MX;
            ctx->width = DISPLAY_WIDTH;
            ctx->height = DISPLAY_HEIGHT;
            break;
        case 1: // Landscape
            madctl |= ILI9486L_MADCTL_MV;
            ctx->width = DISPLAY_HEIGHT;
            ctx->height = DISPLAY_WIDTH;
            break;
        case 2: // Portrait inverted
            madctl |= ILI9486L_MADCTL_MY;
            ctx->width = DISPLAY_WIDTH;
            ctx->height = DISPLAY_HEIGHT;
            break;
        case 3: // Landscape inverted
            madctl |= ILI9486L_MADCTL_MX | ILI9486L_MADCTL_MY | ILI9486L_MADCTL_MV;
            ctx->width = DISPLAY_HEIGHT;
            ctx->height = DISPLAY_WIDTH;
            break;
    }
    
    ctx->rotation = rotation;
    return write_command_data(ctx, ILI9486L_MADCTL, &madctl, 1);
}

int ili9486l_set_window(ili9486l_ctx_t* ctx, int x, int y, int width, int height) {
    uint8_t data[4];
    
    // Column address set
    data[0] = (x >> 8) & 0xFF;
    data[1] = x & 0xFF;
    data[2] = ((x + width - 1) >> 8) & 0xFF;
    data[3] = (x + width - 1) & 0xFF;
    if (write_command_data(ctx, ILI9486L_CASET, data, 4) < 0) return -1;
    
    // Page address set
    data[0] = (y >> 8) & 0xFF;
    data[1] = y & 0xFF;
    data[2] = ((y + height - 1) >> 8) & 0xFF;
    data[3] = (y + height - 1) & 0xFF;
    if (write_command_data(ctx, ILI9486L_PASET, data, 4) < 0) return -1;
    
    // Memory write
    if (ili9486l_write_command(ctx, ILI9486L_RAMWR) < 0) return -1;
    
    return 0;
}

int ili9486l_refresh_display(ili9486l_ctx_t* ctx) {
    if (has_dirty_rect(ctx)) {
        // Refresh only dirty rectangle
        int x = ctx->dirty_x_min;
        int y = ctx->dirty_y_min;
        int width = ctx->dirty_x_max - ctx->dirty_x_min + 1;
        int height = ctx->dirty_y_max - ctx->dirty_y_min + 1;
        
        int result = ili9486l_refresh_rect(ctx, x, y, width, height);
        clear_dirty_rect(ctx);
        return result;
    }
    
    // Full screen refresh
    return ili9486l_refresh_rect(ctx, 0, 0, ctx->width, ctx->height);
}

int ili9486l_refresh_rect(ili9486l_ctx_t* ctx, int x, int y, int width, int height) {
    if (x < 0 || y < 0 || x + width > ctx->width || y + height > ctx->height) {
        return RPI_DISPLAY_ERROR_INVALID;
    }
    
    // Set display window
    if (ili9486l_set_window(ctx, x, y, width, height) < 0) {
        return RPI_DISPLAY_ERROR_SPI;
    }
    
    // Calculate buffer offset and size
    uint32_t pixel_count = width * height;
    uint32_t byte_count = pixel_count * 2; // 16-bit pixels
    uint16_t* source_buffer = ctx->double_buffer_enabled ? ctx->backbuffer : ctx->framebuffer;
    
    // Copy pixel data to transfer buffer
    for (int row = 0; row < height; row++) {
        int fb_offset = ((y + row) * ctx->width + x) * 2;
        int tx_offset = row * width * 2;
        
        // Convert from little-endian to big-endian for SPI
        for (int col = 0; col < width; col++) {
            uint16_t pixel = source_buffer[(y + row) * ctx->width + x + col];
            ctx->tx_buffer[tx_offset + col * 2] = (pixel >> 8) & 0xFF;
            ctx->tx_buffer[tx_offset + col * 2 + 1] = pixel & 0xFF;
        }
    }
    
    // Transfer data
    if (ili9486l_write_data(ctx, ctx->tx_buffer, byte_count) < 0) {
        return RPI_DISPLAY_ERROR_SPI;
    }
    
    // Update performance tracking
    ctx->frame_count++;
    ctx->last_refresh_time = get_time_ns();
    
    return RPI_DISPLAY_OK;
}

int ili9486l_init(ili9486l_ctx_t* ctx, const display_config_t* config) {
    memset(ctx, 0, sizeof(*ctx));
    
    // Initialize configuration
    ctx->spi_speed = config->spi_speed > 0 ? config->spi_speed : SPI_MAX_SPEED_HZ;
    ctx->rotation = config->rotation;
    ctx->dma_enabled = config->enable_dma;
    ctx->double_buffer_enabled = config->enable_double_buffer;
    ctx->refresh_rate = config->refresh_rate > 0 ? config->refresh_rate : 60;
    ctx->width = DISPLAY_WIDTH;
    ctx->height = DISPLAY_HEIGHT;
    ctx->fb_size = ctx->width * ctx->height * 2; // 16-bit pixels
    
    // Initialize GPIO pins
    if (gpio_export(GPIO_DC) < 0 || gpio_export(GPIO_RST) < 0 || 
        gpio_export(GPIO_CS) < 0 || gpio_export(GPIO_LED) < 0) {
        return RPI_DISPLAY_ERROR_GPIO;
    }
    
    if (gpio_set_direction(GPIO_DC, "out") < 0 || gpio_set_direction(GPIO_RST, "out") < 0 ||
        gpio_set_direction(GPIO_CS, "out") < 0 || gpio_set_direction(GPIO_LED, "out") < 0) {
        return RPI_DISPLAY_ERROR_GPIO;
    }
    
    // Initialize SPI
    if (spi_init(ctx) < 0) {
        return RPI_DISPLAY_ERROR_SPI;
    }
    
    // Allocate framebuffer
    ctx->framebuffer = malloc(ctx->fb_size);
    if (!ctx->framebuffer) {
        ili9486l_destroy(ctx);
        return RPI_DISPLAY_ERROR_MEMORY;
    }
    
    // Allocate backbuffer if double buffering is enabled
    if (ctx->double_buffer_enabled) {
        ctx->backbuffer = malloc(ctx->fb_size);
        if (!ctx->backbuffer) {
            ili9486l_destroy(ctx);
            return RPI_DISPLAY_ERROR_MEMORY;
        }
    }
    
    // Initialize dirty rectangle tracking
    ctx->dirty_rect_enabled = true;
    clear_dirty_rect(ctx);
    
    // Turn on LED backlight
    gpio_set_value(GPIO_LED, 1);
    
    // Reset and configure display
    if (ili9486l_reset(ctx) < 0) {
        ili9486l_destroy(ctx);
        return RPI_DISPLAY_ERROR_INIT;
    }
    
    if (ili9486l_configure(ctx) < 0) {
        ili9486l_destroy(ctx);
        return RPI_DISPLAY_ERROR_INIT;
    }
    
    return RPI_DISPLAY_OK;
}

void ili9486l_destroy(ili9486l_ctx_t* ctx) {
    if (!ctx) return;
    
    // Turn off LED backlight
    gpio_set_value(GPIO_LED, 0);
    
    // Free framebuffer
    if (ctx->framebuffer) {
        free(ctx->framebuffer);
        ctx->framebuffer = NULL;
    }
    
    if (ctx->backbuffer) {
        free(ctx->backbuffer);
        ctx->backbuffer = NULL;
    }
    
    // Clean up SPI
    spi_destroy(ctx);
    
    // Clean up GPIO
    gpio_unexport(GPIO_DC);
    gpio_unexport(GPIO_RST);
    gpio_unexport(GPIO_CS);
    gpio_unexport(GPIO_LED);
}

// Performance helper functions
void mark_dirty_rect(ili9486l_ctx_t* ctx, int x, int y, int width, int height) {
    if (!ctx->dirty_rect_enabled) return;
    
    int x_max = x + width - 1;
    int y_max = y + height - 1;
    
    if (ctx->dirty_x_min == -1) {
        // First dirty rectangle
        ctx->dirty_x_min = x;
        ctx->dirty_y_min = y;
        ctx->dirty_x_max = x_max;
        ctx->dirty_y_max = y_max;
    } else {
        // Expand existing dirty rectangle
        if (x < ctx->dirty_x_min) ctx->dirty_x_min = x;
        if (y < ctx->dirty_y_min) ctx->dirty_y_min = y;
        if (x_max > ctx->dirty_x_max) ctx->dirty_x_max = x_max;
        if (y_max > ctx->dirty_y_max) ctx->dirty_y_max = y_max;
    }
}

void clear_dirty_rect(ili9486l_ctx_t* ctx) {
    ctx->dirty_x_min = -1;
    ctx->dirty_y_min = -1;
    ctx->dirty_x_max = -1;
    ctx->dirty_y_max = -1;
}

bool has_dirty_rect(ili9486l_ctx_t* ctx) {
    return ctx->dirty_x_min != -1;
}

// Utility functions
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