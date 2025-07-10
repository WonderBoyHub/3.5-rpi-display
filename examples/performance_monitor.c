#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <errno.h>

#include "efficient_rpi_display.h"
#ifdef ENABLE_DRM_KMS
#include "modern_drm_interface.h"
#endif

// Performance monitoring configuration
#define MONITOR_INTERVAL_MS 1000
#define HISTORY_SIZE 60
#define MAX_TESTS 10

// Performance metrics
typedef struct {
    double fps;
    double cpu_usage;
    double memory_usage;
    double gpu_usage;
    double power_usage;
    double temperature;
    uint64_t frame_count;
    uint64_t draw_calls;
    uint64_t memory_bandwidth;
    double latency_ms;
    time_t timestamp;
} perf_metrics_t;

// Performance history
typedef struct {
    perf_metrics_t metrics[HISTORY_SIZE];
    int current_index;
    int count;
} perf_history_t;

// Monitor context
typedef struct {
    display_handle_t display;
#ifdef ENABLE_DRM_KMS
    drm_context_t drm_ctx;
#endif
    perf_history_t history;
    bool running;
    bool use_drm;
    bool gpu_acceleration;
    bool wayland_mode;
    
    // Test parameters
    int test_width;
    int test_height;
    int test_iterations;
    
    // Performance counters
    uint64_t total_frames;
    uint64_t total_draw_calls;
    struct timeval start_time;
    struct timeval last_update;
    
    // System information
    char system_info[1024];
    char gpu_info[256];
    char display_info[256];
    
} monitor_context_t;

static monitor_context_t monitor_ctx;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\nShutting down performance monitor...\n");
    monitor_ctx.running = false;
}

// System information gathering
void gather_system_info(monitor_context_t *ctx) {
    FILE *fp;
    char buffer[256];
    
    // Get system model
    fp = fopen("/proc/device-tree/model", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            snprintf(ctx->system_info, sizeof(ctx->system_info), "Model: %s", buffer);
        }
        fclose(fp);
    }
    
    // Get CPU info
    fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "model name")) {
                char *cpu_name = strchr(buffer, ':');
                if (cpu_name) {
                    cpu_name += 2; // Skip ": "
                    strncat(ctx->system_info, "\nCPU: ", sizeof(ctx->system_info) - strlen(ctx->system_info) - 1);
                    strncat(ctx->system_info, cpu_name, sizeof(ctx->system_info) - strlen(ctx->system_info) - 1);
                }
                break;
            }
        }
        fclose(fp);
    }
    
    // Get memory info
    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "MemTotal:")) {
                char *mem_total = strchr(buffer, ':');
                if (mem_total) {
                    mem_total += 2; // Skip ": "
                    strncat(ctx->system_info, "\nMemory: ", sizeof(ctx->system_info) - strlen(ctx->system_info) - 1);
                    strncat(ctx->system_info, mem_total, sizeof(ctx->system_info) - strlen(ctx->system_info) - 1);
                }
                break;
            }
        }
        fclose(fp);
    }
    
    // Get GPU info
#ifdef ENABLE_DRM_KMS
    if (ctx->use_drm) {
        snprintf(ctx->gpu_info, sizeof(ctx->gpu_info), "GPU: %s", drm_get_gpu_info(&ctx->drm_ctx));
    } else
#endif
    {
        snprintf(ctx->gpu_info, sizeof(ctx->gpu_info), "GPU: Legacy SPI Display");
    }
    
    // Get display info
    if (ctx->display) {
        snprintf(ctx->display_info, sizeof(ctx->display_info), "Display: %dx%d", 
                rpi_display_get_width(ctx->display), rpi_display_get_height(ctx->display));
    }
}

// CPU usage calculation
double get_cpu_usage(void) {
    static long long prev_total = 0, prev_idle = 0;
    FILE *fp;
    char buffer[256];
    long long user, nice, system, idle, iowait, irq, softirq, steal;
    long long total, total_idle, total_diff, idle_diff;
    
    fp = fopen("/proc/stat", "r");
    if (!fp) return 0.0;
    
    if (fgets(buffer, sizeof(buffer), fp)) {
        sscanf(buffer, "cpu %lld %lld %lld %lld %lld %lld %lld %lld",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
        
        total = user + nice + system + idle + iowait + irq + softirq + steal;
        total_idle = idle + iowait;
        
        total_diff = total - prev_total;
        idle_diff = total_idle - prev_idle;
        
        prev_total = total;
        prev_idle = total_idle;
        
        if (total_diff > 0) {
            return (double)(total_diff - idle_diff) / total_diff * 100.0;
        }
    }
    
    fclose(fp);
    return 0.0;
}

// Memory usage calculation
double get_memory_usage(void) {
    FILE *fp;
    char buffer[256];
    long long mem_total = 0, mem_free = 0, mem_available = 0;
    
    fp = fopen("/proc/meminfo", "r");
    if (!fp) return 0.0;
    
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (sscanf(buffer, "MemTotal: %lld kB", &mem_total) == 1) continue;
        if (sscanf(buffer, "MemFree: %lld kB", &mem_free) == 1) continue;
        if (sscanf(buffer, "MemAvailable: %lld kB", &mem_available) == 1) continue;
    }
    
    fclose(fp);
    
    if (mem_total > 0) {
        return (double)(mem_total - mem_available) / mem_total * 100.0;
    }
    
    return 0.0;
}

// GPU usage calculation (Pi 5 with V3D support)
double get_gpu_usage(void) {
    FILE *fp;
    char buffer[256];
    double gpu_usage = 0.0;
    
    // Try V3D GPU utilization
    fp = fopen("/sys/class/drm/card0/device/gpu_busy_percent", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            gpu_usage = atof(buffer);
        }
        fclose(fp);
        return gpu_usage;
    }
    
    // Try alternative GPU monitoring
    fp = fopen("/opt/vc/bin/vcgencmd measure_temp", "r");
    if (fp) {
        // GPU temperature as a proxy for usage
        if (fgets(buffer, sizeof(buffer), fp)) {
            double temp = atof(buffer + 5); // Skip "temp="
            gpu_usage = (temp - 40.0) / 40.0 * 100.0; // Rough approximation
            if (gpu_usage < 0) gpu_usage = 0.0;
            if (gpu_usage > 100) gpu_usage = 100.0;
        }
        fclose(fp);
    }
    
    return gpu_usage;
}

// Temperature monitoring
double get_cpu_temperature(void) {
    FILE *fp;
    char buffer[64];
    double temp = 0.0;
    
    fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            temp = atof(buffer) / 1000.0; // Convert from millidegrees
        }
        fclose(fp);
    }
    
    return temp;
}

// Frame latency measurement
double measure_frame_latency(monitor_context_t *ctx) {
    struct timeval start, end;
    double latency_ms = 0.0;
    
    gettimeofday(&start, NULL);
    
    // Perform a simple drawing operation
    if (ctx->display) {
        rpi_display_fill_rect(ctx->display, 0, 0, 10, 10, 0xFFFF);
        rpi_display_refresh(ctx->display);
    }
    
    gettimeofday(&end, NULL);
    
    latency_ms = (end.tv_sec - start.tv_sec) * 1000.0 + 
                 (end.tv_usec - start.tv_usec) / 1000.0;
    
    return latency_ms;
}

// Performance metrics collection
void collect_metrics(monitor_context_t *ctx, perf_metrics_t *metrics) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // Calculate FPS
    if (ctx->last_update.tv_sec > 0) {
        double time_diff = (now.tv_sec - ctx->last_update.tv_sec) + 
                          (now.tv_usec - ctx->last_update.tv_usec) / 1000000.0;
        if (time_diff > 0) {
            metrics->fps = 1.0 / time_diff;
        }
    }
    
    // System metrics
    metrics->cpu_usage = get_cpu_usage();
    metrics->memory_usage = get_memory_usage();
    metrics->gpu_usage = get_gpu_usage();
    metrics->temperature = get_cpu_temperature();
    
    // Frame and draw call counters
    metrics->frame_count = ctx->total_frames;
    metrics->draw_calls = ctx->total_draw_calls;
    
    // Latency measurement
    metrics->latency_ms = measure_frame_latency(ctx);
    
    // Timestamp
    metrics->timestamp = now.tv_sec;
    
    ctx->last_update = now;
    ctx->total_frames++;
}

// Add metrics to history
void add_to_history(perf_history_t *history, const perf_metrics_t *metrics) {
    history->metrics[history->current_index] = *metrics;
    history->current_index = (history->current_index + 1) % HISTORY_SIZE;
    if (history->count < HISTORY_SIZE) {
        history->count++;
    }
}

// Display performance metrics
void display_metrics(monitor_context_t *ctx) {
    perf_metrics_t *current = &ctx->history.metrics[
        (ctx->history.current_index - 1 + HISTORY_SIZE) % HISTORY_SIZE];
    
    // Clear screen
    printf("\033[2J\033[H");
    
    // Header
    printf("╔════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                           Efficient RPi Display Performance Monitor                       ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════════════════════╣\n");
    
    // System Information
    printf("║ System Information:                                                                        ║\n");
    printf("║ %s%-85s ║\n", "", ctx->system_info);
    printf("║ %s%-85s ║\n", "", ctx->gpu_info);
    printf("║ %s%-85s ║\n", "", ctx->display_info);
    printf("║                                                                                            ║\n");
    
    // Current Performance
    printf("║ Current Performance:                                                                       ║\n");
    printf("║   FPS: %6.2f  │  CPU: %5.1f%%  │  Memory: %5.1f%%  │  GPU: %5.1f%%  │  Temp: %5.1f°C    ║\n",
           current->fps, current->cpu_usage, current->memory_usage, 
           current->gpu_usage, current->temperature);
    printf("║   Frames: %8llu  │  Draw Calls: %8llu  │  Latency: %6.2f ms                    ║\n",
           (unsigned long long)current->frame_count, (unsigned long long)current->draw_calls, 
           current->latency_ms);
    printf("║                                                                                            ║\n");
    
    // Feature Status
    printf("║ Feature Status:                                                                            ║\n");
    printf("║   DRM/KMS: %s  │  GPU Accel: %s  │  Wayland: %s  │  V3D: %s                    ║\n",
           ctx->use_drm ? "ON " : "OFF",
           ctx->gpu_acceleration ? "ON " : "OFF",
           ctx->wayland_mode ? "ON " : "OFF",
#ifdef ENABLE_V3D_SUPPORT
           "ON "
#else
           "OFF"
#endif
           );
    printf("║                                                                                            ║\n");
    
    // Performance History (mini graph)
    printf("║ FPS History (last 20 seconds):                                                            ║\n");
    printf("║ ");
    
    int graph_width = 80;
    int samples = (ctx->history.count < 20) ? ctx->history.count : 20;
    double max_fps = 0.0;
    
    // Find max FPS for scaling
    for (int i = 0; i < samples; i++) {
        int idx = (ctx->history.current_index - samples + i + HISTORY_SIZE) % HISTORY_SIZE;
        if (ctx->history.metrics[idx].fps > max_fps) {
            max_fps = ctx->history.metrics[idx].fps;
        }
    }
    
    // Draw graph
    for (int i = 0; i < samples; i++) {
        int idx = (ctx->history.current_index - samples + i + HISTORY_SIZE) % HISTORY_SIZE;
        double fps = ctx->history.metrics[idx].fps;
        int height = (int)(fps / max_fps * 10);
        
        if (height > 8) printf("█");
        else if (height > 6) printf("▇");
        else if (height > 4) printf("▆");
        else if (height > 2) printf("▅");
        else if (height > 0) printf("▃");
        else printf("▁");
    }
    
    for (int i = samples; i < graph_width; i++) {
        printf(" ");
    }
    printf("║\n");
    
    // Average performance
    double avg_fps = 0.0, avg_cpu = 0.0, avg_memory = 0.0;
    for (int i = 0; i < ctx->history.count; i++) {
        avg_fps += ctx->history.metrics[i].fps;
        avg_cpu += ctx->history.metrics[i].cpu_usage;
        avg_memory += ctx->history.metrics[i].memory_usage;
    }
    
    if (ctx->history.count > 0) {
        avg_fps /= ctx->history.count;
        avg_cpu /= ctx->history.count;
        avg_memory /= ctx->history.count;
    }
    
    printf("║                                                                                            ║\n");
    printf("║ Average Performance:                                                                       ║\n");
    printf("║   FPS: %6.2f  │  CPU: %5.1f%%  │  Memory: %5.1f%%  │  Max FPS: %6.2f                  ║\n",
           avg_fps, avg_cpu, avg_memory, max_fps);
    printf("║                                                                                            ║\n");
    
    // Runtime information
    struct timeval now;
    gettimeofday(&now, NULL);
    double runtime = (now.tv_sec - ctx->start_time.tv_sec) + 
                    (now.tv_usec - ctx->start_time.tv_usec) / 1000000.0;
    
    printf("║ Runtime: %6.1f seconds  │  Press Ctrl+C to exit                                        ║\n",
           runtime);
    printf("╚════════════════════════════════════════════════════════════════════════════════════════╝\n");
}

// Performance tests
void run_performance_tests(monitor_context_t *ctx) {
    printf("Running performance tests...\n");
    
    if (!ctx->display) {
        printf("Error: Display not initialized\n");
        return;
    }
    
    // Test 1: Fill rate test
    printf("Test 1: Fill rate test...\n");
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    int iterations = 100;
    for (int i = 0; i < iterations; i++) {
        rpi_display_clear(ctx->display, 0x0000);
        rpi_display_refresh(ctx->display);
        ctx->total_draw_calls++;
    }
    
    gettimeofday(&end, NULL);
    double fill_time = (end.tv_sec - start.tv_sec) + 
                      (end.tv_usec - start.tv_usec) / 1000000.0;
    
    printf("  Fill rate: %.2f fps (%.2f ms per frame)\n", 
           iterations / fill_time, fill_time * 1000.0 / iterations);
    
    // Test 2: Rectangle drawing test
    printf("Test 2: Rectangle drawing test...\n");
    gettimeofday(&start, NULL);
    
    for (int i = 0; i < iterations; i++) {
        rpi_display_clear(ctx->display, 0x0000);
        for (int j = 0; j < 10; j++) {
            rpi_display_fill_rect(ctx->display, j * 30, j * 20, 25, 15, 0xFFFF);
            ctx->total_draw_calls++;
        }
        rpi_display_refresh(ctx->display);
    }
    
    gettimeofday(&end, NULL);
    double rect_time = (end.tv_sec - start.tv_sec) + 
                      (end.tv_usec - start.tv_usec) / 1000000.0;
    
    printf("  Rectangle drawing: %.2f fps (%.2f ms per frame)\n", 
           iterations / rect_time, rect_time * 1000.0 / iterations);
    
    // Test 3: Text rendering test
    printf("Test 3: Text rendering test...\n");
    gettimeofday(&start, NULL);
    
    for (int i = 0; i < iterations; i++) {
        rpi_display_clear(ctx->display, 0x0000);
        rpi_display_draw_text(ctx->display, 10, 10, "Performance Test", 0xFFFF);
        rpi_display_draw_text(ctx->display, 10, 30, "Text Rendering", 0xFFE0);
        rpi_display_draw_text(ctx->display, 10, 50, "Benchmark", 0x07E0);
        rpi_display_refresh(ctx->display);
        ctx->total_draw_calls += 3;
    }
    
    gettimeofday(&end, NULL);
    double text_time = (end.tv_sec - start.tv_sec) + 
                      (end.tv_usec - start.tv_usec) / 1000000.0;
    
    printf("  Text rendering: %.2f fps (%.2f ms per frame)\n", 
           iterations / text_time, text_time * 1000.0 / iterations);
    
    printf("Performance tests completed.\n");
}

// Initialize monitoring
int initialize_monitor(monitor_context_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    
    // Initialize display
    display_config_t config = {
        .spi_speed = 80000000,
        .spi_mode = 0,
        .rotation = ROTATE_0,
        .enable_dma = true,
        .enable_double_buffer = true,
        .refresh_rate = 60
    };
    
    ctx->display = rpi_display_init(&config);
    if (!ctx->display) {
        printf("Error: Failed to initialize display\n");
        return -1;
    }
    
    ctx->test_width = rpi_display_get_width(ctx->display);
    ctx->test_height = rpi_display_get_height(ctx->display);
    
#ifdef ENABLE_DRM_KMS
    // Try to initialize DRM
    if (drm_init(&ctx->drm_ctx, NULL) == DRM_OK) {
        ctx->use_drm = true;
        printf("DRM/KMS initialized successfully\n");
        
        // Try GPU acceleration
        if (drm_init_gpu_acceleration(&ctx->drm_ctx) == DRM_OK) {
            ctx->gpu_acceleration = true;
            printf("GPU acceleration enabled\n");
        }
        
        // Check for Wayland
        if (drm_init_wayland_egl(&ctx->drm_ctx) == DRM_OK) {
            ctx->wayland_mode = true;
            printf("Wayland mode enabled\n");
        }
    } else {
        printf("Using legacy SPI mode\n");
    }
#endif
    
    // Gather system information
    gather_system_info(ctx);
    
    // Initialize timing
    gettimeofday(&ctx->start_time, NULL);
    ctx->running = true;
    
    return 0;
}

// Cleanup
void cleanup_monitor(monitor_context_t *ctx) {
#ifdef ENABLE_DRM_KMS
    if (ctx->use_drm) {
        drm_destroy(&ctx->drm_ctx);
    }
#endif
    
    if (ctx->display) {
        rpi_display_destroy(ctx->display);
    }
    
    printf("Performance monitor shutdown complete.\n");
}

// Main function
int main(int argc, char *argv[]) {
    printf("Efficient RPi Display Performance Monitor v2.0\n");
    printf("===============================================\n");
    
    // Parse command line arguments
    bool run_tests = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            run_tests = true;
        }
    }
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize monitor
    if (initialize_monitor(&monitor_ctx) < 0) {
        return 1;
    }
    
    // Run performance tests if requested
    if (run_tests) {
        run_performance_tests(&monitor_ctx);
    }
    
    // Main monitoring loop
    printf("Starting performance monitoring...\n");
    sleep(1);
    
    while (monitor_ctx.running) {
        perf_metrics_t metrics;
        collect_metrics(&monitor_ctx, &metrics);
        add_to_history(&monitor_ctx.history, &metrics);
        display_metrics(&monitor_ctx);
        
        // Sleep for monitoring interval
        usleep(MONITOR_INTERVAL_MS * 1000);
    }
    
    cleanup_monitor(&monitor_ctx);
    return 0;
} 