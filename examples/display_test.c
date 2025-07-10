#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "efficient_rpi_display.h"

static volatile int running = 1;

void signal_handler(int sig) {
    running = 0;
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Initializing display...\n");
    
    display_config_t config = {
        .spi_speed = 80000000,
        .spi_mode = 0,
        .rotation = ROTATE_0,
        .enable_dma = true,
        .enable_double_buffer = true,
        .refresh_rate = 60
    };
    
    display_handle_t display = rpi_display_init(&config);
    if (!display) {
        printf("Failed to initialize display\n");
        return 1;
    }
    
    printf("Display initialized successfully\n");
    printf("Resolution: %dx%d\n", rpi_display_get_width(display), rpi_display_get_height(display));
    
    // Clear screen to black
    rpi_display_clear(display, COLOR_BLACK);
    
    // Draw some test patterns
    rpi_display_fill_rect(display, 10, 10, 100, 50, COLOR_RED);
    rpi_display_fill_rect(display, 120, 10, 100, 50, COLOR_GREEN);
    rpi_display_fill_rect(display, 230, 10, 100, 50, COLOR_BLUE);
    
    // Draw text
    rpi_display_draw_text(display, 10, 80, "Hello, Efficient RPi Display!", COLOR_WHITE);
    rpi_display_draw_text(display, 10, 100, "Press Ctrl+C to exit", COLOR_YELLOW);
    
    // Refresh display
    rpi_display_refresh(display);
    
    printf("Test pattern displayed. Press Ctrl+C to exit.\n");
    
    while (running) {
        sleep(1);
    }
    
    printf("Cleaning up...\n");
    rpi_display_destroy(display);
    
    return 0;
} 