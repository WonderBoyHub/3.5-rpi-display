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
    
    printf("Initializing display with touch...\n");
    
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
    
    // Clear screen
    rpi_display_clear(display, COLOR_BLACK);
    rpi_display_draw_text(display, 10, 10, "Touch Test", COLOR_WHITE);
    rpi_display_draw_text(display, 10, 30, "Touch screen to see coordinates", COLOR_CYAN);
    rpi_display_draw_text(display, 10, 50, "Press Ctrl+C to exit", COLOR_YELLOW);
    rpi_display_refresh(display);
    
    printf("Touch test running. Touch the screen to see coordinates.\n");
    
    while (running) {
        if (rpi_touch_is_pressed(display)) {
            touch_point_t point = rpi_touch_read(display);
            printf("Touch at: %d, %d\n", point.x, point.y);
            
            // Draw a small circle at touch point
            rpi_display_draw_circle(display, point.x, point.y, 5, COLOR_RED);
            rpi_display_refresh(display);
        }
        
        usleep(50000); // 50ms
    }
    
    printf("Cleaning up...\n");
    rpi_display_destroy(display);
    
    return 0;
} 