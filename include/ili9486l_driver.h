#ifndef ILI9486L_DRIVER_H
#define ILI9486L_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <linux/spi/spidev.h>

// ILI9486L Commands
#define ILI9486L_SLPOUT     0x11  // Sleep Out
#define ILI9486L_DISPON     0x29  // Display On
#define ILI9486L_CASET      0x2A  // Column Address Set
#define ILI9486L_PASET      0x2B  // Page Address Set
#define ILI9486L_RAMWR      0x2C  // Memory Write
#define ILI9486L_RAMRD      0x2E  // Memory Read
#define ILI9486L_MADCTL     0x36  // Memory Access Control
#define ILI9486L_PIXFMT     0x3A  // Interface Pixel Format
#define ILI9486L_FRMCTR1    0xB1  // Frame Rate Control (Normal Mode)
#define ILI9486L_DFUNCTR    0xB6  // Display Function Control
#define ILI9486L_PWCTR1     0xC0  // Power Control 1
#define ILI9486L_PWCTR2     0xC1  // Power Control 2
#define ILI9486L_VMCTR1     0xC5  // VCOM Control 1
#define ILI9486L_VMCTR2     0xC7  // VCOM Control 2
#define ILI9486L_GMCTRP1    0xE0  // Positive Gamma Correction
#define ILI9486L_GMCTRN1    0xE1  // Negative Gamma Correction

// MADCTL bits
#define ILI9486L_MADCTL_MY  0x80  // Row Address Order
#define ILI9486L_MADCTL_MX  0x40  // Column Address Order
#define ILI9486L_MADCTL_MV  0x20  // Row/Column Exchange
#define ILI9486L_MADCTL_ML  0x10  // Vertical Refresh Order
#define ILI9486L_MADCTL_BGR 0x08  // BGR Order
#define ILI9486L_MADCTL_MH  0x04  // Horizontal Refresh Order

// GPIO pins
#define GPIO_DC    24  // Data/Command pin
#define GPIO_RST   25  // Reset pin
#define GPIO_CS    8   // Chip Select pin
#define GPIO_LED   18  // LED Backlight pin

// SPI settings
#define SPI_DEVICE         "/dev/spidev0.0"
#define SPI_MODE           SPI_MODE_0
#define SPI_BITS_PER_WORD  8
#define SPI_MAX_SPEED_HZ   80000000

// DMA settings
#define DMA_CHANNEL        5
#define DMA_BUFFER_SIZE    (320 * 480 * 2)  // Full screen buffer

// Display context structure
typedef struct {
    // SPI interface
    int spi_fd;
    struct spi_ioc_transfer spi_tr;
    uint8_t* tx_buffer;
    uint8_t* rx_buffer;
    
    // GPIO interface
    int gpio_fd_dc;
    int gpio_fd_rst;
    int gpio_fd_cs;
    int gpio_fd_led;
    
    // DMA interface
    int dma_fd;
    void* dma_buffer;
    volatile uint32_t* dma_cb;
    
    // Display state
    uint16_t* framebuffer;
    uint16_t* backbuffer;
    uint32_t fb_size;
    bool double_buffer_enabled;
    
    // Display configuration
    uint32_t width;
    uint32_t height;
    uint32_t spi_speed;
    uint8_t rotation;
    bool dma_enabled;
    
    // Performance tracking
    uint32_t frame_count;
    uint64_t last_refresh_time;
    uint32_t refresh_rate;
    
    // Dirty rectangle tracking
    bool dirty_rect_enabled;
    int16_t dirty_x_min;
    int16_t dirty_y_min;
    int16_t dirty_x_max;
    int16_t dirty_y_max;
    
} ili9486l_ctx_t;

// Function prototypes
int ili9486l_init(ili9486l_ctx_t* ctx, const display_config_t* config);
void ili9486l_destroy(ili9486l_ctx_t* ctx);
int ili9486l_reset(ili9486l_ctx_t* ctx);
int ili9486l_configure(ili9486l_ctx_t* ctx);
int ili9486l_set_rotation(ili9486l_ctx_t* ctx, uint8_t rotation);
int ili9486l_set_window(ili9486l_ctx_t* ctx, int x, int y, int width, int height);
int ili9486l_write_data(ili9486l_ctx_t* ctx, const uint8_t* data, uint32_t length);
int ili9486l_write_command(ili9486l_ctx_t* ctx, uint8_t command);
int ili9486l_refresh_display(ili9486l_ctx_t* ctx);
int ili9486l_refresh_rect(ili9486l_ctx_t* ctx, int x, int y, int width, int height);

// GPIO helpers
int gpio_export(int pin);
int gpio_unexport(int pin);
int gpio_set_direction(int pin, const char* direction);
int gpio_set_value(int pin, int value);
int gpio_get_value(int pin);

// DMA helpers
int dma_init(ili9486l_ctx_t* ctx);
void dma_destroy(ili9486l_ctx_t* ctx);
int dma_transfer(ili9486l_ctx_t* ctx, const void* src, uint32_t length);

// SPI helpers
int spi_init(ili9486l_ctx_t* ctx);
void spi_destroy(ili9486l_ctx_t* ctx);
int spi_transfer(ili9486l_ctx_t* ctx, const uint8_t* tx_data, uint8_t* rx_data, uint32_t length);

// Performance helpers
void mark_dirty_rect(ili9486l_ctx_t* ctx, int x, int y, int width, int height);
void clear_dirty_rect(ili9486l_ctx_t* ctx);
bool has_dirty_rect(ili9486l_ctx_t* ctx);

#endif // ILI9486L_DRIVER_H 