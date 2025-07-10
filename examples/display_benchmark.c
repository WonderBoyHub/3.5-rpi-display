#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "efficient_rpi_display.h"

static volatile int running = 1;

void signal_handler(int sig) {
    running = 0;
}

double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

void benchmark_clear(display_handle_t display, int iterations) {
    printf("Benchmarking clear operations...\n");
    
    double start_time = get_time_ms();
    
    for (int i = 0; i < iterations && running; i++) {
        rpi_display_clear(display, (i % 2) ? COLOR_BLACK : COLOR_WHITE);
        rpi_display_refresh(display);
    }
    
    double end_time = get_time_ms();
    double elapsed = end_time - start_time;
    
    printf("Clear benchmark: %d iterations in %.2f ms\n", iterations, elapsed);
    printf("Average time per clear: %.2f ms\n", elapsed / iterations);
    printf("Clear operations per second: %.2f\n", (iterations * 1000.0) / elapsed);
}

void benchmark_pixel_fill(display_handle_t display, int iterations) {
    printf("\nBenchmarking pixel fill operations...\n");
    
    int width = rpi_display_get_width(display);
    int height = rpi_display_get_height(display);
    
    double start_time = get_time_ms();
    
    for (int i = 0; i < iterations && running; i++) {
        uint16_t color = (i % 8) << 13; // Cycle through colors
        
        for (int y = 0; y < height; y += 4) {
            for (int x = 0; x < width; x += 4) {
                rpi_display_set_pixel(display, x, y, color);
            }
        }
        rpi_display_refresh(display);
    }
    
    double end_time = get_time_ms();
    double elapsed = end_time - start_time;
    
    printf("Pixel fill benchmark: %d iterations in %.2f ms\n", iterations, elapsed);
    printf("Average time per iteration: %.2f ms\n", elapsed / iterations);
    printf("Pixel fill operations per second: %.2f\n", (iterations * 1000.0) / elapsed);
}

void benchmark_rectangle_fill(display_handle_t display, int iterations) {
    printf("\nBenchmarking rectangle fill operations...\n");
    
    int width = rpi_display_get_width(display);
    int height = rpi_display_get_height(display);
    
    double start_time = get_time_ms();
    
    for (int i = 0; i < iterations && running; i++) {
        uint16_t color = (i % 8) << 13; // Cycle through colors
        
        int rect_width = 50;
        int rect_height = 50;
        int x = i % (width - rect_width);
        int y = (i / (width - rect_width)) % (height - rect_height);
        
        rpi_display_fill_rect(display, x, y, rect_width, rect_height, color);
        
        if (i % 10 == 0) {
            rpi_display_refresh(display);
        }
    }
    
    rpi_display_refresh(display);
    
    double end_time = get_time_ms();
    double elapsed = end_time - start_time;
    
    printf("Rectangle fill benchmark: %d iterations in %.2f ms\n", iterations, elapsed);
    printf("Average time per rectangle: %.2f ms\n", elapsed / iterations);
    printf("Rectangle operations per second: %.2f\n", (iterations * 1000.0) / elapsed);
}

void benchmark_text_rendering(display_handle_t display, int iterations) {
    printf("\nBenchmarking text rendering...\n");
    
    const char* test_text = "Hello, World! 123";
    
    double start_time = get_time_ms();
    
    for (int i = 0; i < iterations && running; i++) {
        uint16_t color = (i % 8) << 13; // Cycle through colors
        int x = (i * 20) % 240;
        int y = ((i * 20) / 240) * 10 % 400;
        
        rpi_display_draw_text(display, x, y, test_text, color);
        
        if (i % 5 == 0) {
            rpi_display_refresh(display);
        }
    }
    
    rpi_display_refresh(display);
    
    double end_time = get_time_ms();
    double elapsed = end_time - start_time;
    
    printf("Text rendering benchmark: %d iterations in %.2f ms\n", iterations, elapsed);
    printf("Average time per text: %.2f ms\n", elapsed / iterations);
    printf("Text operations per second: %.2f\n", (iterations * 1000.0) / elapsed);
}

void benchmark_line_drawing(display_handle_t display, int iterations) {
    printf("\nBenchmarking line drawing...\n");
    
    int width = rpi_display_get_width(display);
    int height = rpi_display_get_height(display);
    
    double start_time = get_time_ms();
    
    for (int i = 0; i < iterations && running; i++) {
        uint16_t color = (i % 8) << 13; // Cycle through colors
        
        int x0 = rand() % width;
        int y0 = rand() % height;
        int x1 = rand() % width;
        int y1 = rand() % height;
        
        rpi_display_draw_line(display, x0, y0, x1, y1, color);
        
        if (i % 20 == 0) {
            rpi_display_refresh(display);
        }
    }
    
    rpi_display_refresh(display);
    
    double end_time = get_time_ms();
    double elapsed = end_time - start_time;
    
    printf("Line drawing benchmark: %d iterations in %.2f ms\n", iterations, elapsed);
    printf("Average time per line: %.2f ms\n", elapsed / iterations);
    printf("Line operations per second: %.2f\n", (iterations * 1000.0) / elapsed);
}

void benchmark_circle_drawing(display_handle_t display, int iterations) {
    printf("\nBenchmarking circle drawing...\n");
    
    int width = rpi_display_get_width(display);
    int height = rpi_display_get_height(display);
    
    double start_time = get_time_ms();
    
    for (int i = 0; i < iterations && running; i++) {
        uint16_t color = (i % 8) << 13; // Cycle through colors
        
        int x = rand() % width;
        int y = rand() % height;
        int radius = (rand() % 30) + 5;
        
        rpi_display_draw_circle(display, x, y, radius, color);
        
        if (i % 10 == 0) {
            rpi_display_refresh(display);
        }
    }
    
    rpi_display_refresh(display);
    
    double end_time = get_time_ms();
    double elapsed = end_time - start_time;
    
    printf("Circle drawing benchmark: %d iterations in %.2f ms\n", iterations, elapsed);
    printf("Average time per circle: %.2f ms\n", elapsed / iterations);
    printf("Circle operations per second: %.2f\n", (iterations * 1000.0) / elapsed);
}

void benchmark_refresh_rate(display_handle_t display, int duration_seconds) {
    printf("\nBenchmarking refresh rate for %d seconds...\n", duration_seconds);
    
    int width = rpi_display_get_width(display);
    int height = rpi_display_get_height(display);
    
    double start_time = get_time_ms();
    double end_time = start_time + (duration_seconds * 1000.0);
    int frame_count = 0;
    
    while (get_time_ms() < end_time && running) {
        // Draw a simple animation
        uint16_t color = (frame_count % 8) << 13;
        int x = (frame_count * 5) % width;
        int y = (frame_count * 3) % height;
        
        rpi_display_clear(display, COLOR_BLACK);
        rpi_display_fill_rect(display, x, y, 50, 50, color);
        rpi_display_draw_text(display, 10, 10, "FPS Test", COLOR_WHITE);
        
        char fps_text[32];
        snprintf(fps_text, sizeof(fps_text), "Frame: %d", frame_count);
        rpi_display_draw_text(display, 10, 30, fps_text, COLOR_YELLOW);
        
        rpi_display_refresh(display);
        
        frame_count++;
    }
    
    double actual_time = get_time_ms() - start_time;
    double fps = (frame_count * 1000.0) / actual_time;
    
    printf("Refresh rate benchmark: %d frames in %.2f ms\n", frame_count, actual_time);
    printf("Average FPS: %.2f\n", fps);
    printf("Average frame time: %.2f ms\n", actual_time / frame_count);
}

void run_all_benchmarks(display_handle_t display) {
    printf("\n=== EFFICIENT RPI DISPLAY BENCHMARKS ===\n");
    printf("Display Resolution: %dx%d\n", 
           rpi_display_get_width(display), 
           rpi_display_get_height(display));
    printf("Running comprehensive performance tests...\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    // Clear screen
    rpi_display_clear(display, COLOR_BLACK);
    rpi_display_refresh(display);
    
    // Run benchmarks
    benchmark_clear(display, 50);
    benchmark_pixel_fill(display, 10);
    benchmark_rectangle_fill(display, 100);
    benchmark_text_rendering(display, 50);
    benchmark_line_drawing(display, 200);
    benchmark_circle_drawing(display, 100);
    benchmark_refresh_rate(display, 5);
    
    printf("\n=== BENCHMARK COMPLETE ===\n");
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Efficient RPi Display Driver - Performance Benchmark\n");
    printf("Press Ctrl+C to stop at any time\n\n");
    
    // Initialize display
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
    
    // Run benchmarks
    run_all_benchmarks(display);
    
    // Clean up
    rpi_display_clear(display, COLOR_BLACK);
    rpi_display_draw_text(display, 10, 10, "Benchmark Complete", COLOR_GREEN);
    rpi_display_draw_text(display, 10, 30, "Check console for results", COLOR_WHITE);
    rpi_display_refresh(display);
    
    printf("\nBenchmark complete. Display will remain active.\n");
    printf("Press Ctrl+C to exit.\n");
    
    // Wait for user to exit
    while (running) {
        sleep(1);
    }
    
    printf("\nCleaning up...\n");
    rpi_display_destroy(display);
    
    return 0;
} 