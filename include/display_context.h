#ifndef DISPLAY_CONTEXT_H
#define DISPLAY_CONTEXT_H

#include "efficient_rpi_display.h"
#include "ili9486l_driver.h"
#include "xpt2046_touch.h"

// Main display context structure
typedef struct rpi_display_ctx {
    // Display driver
    ili9486l_ctx_t display;
    
    // Touch driver
    xpt2046_ctx_t touch;
    
    // Combined state
    bool initialized;
    bool touch_enabled;
    
    // Configuration
    display_config_t config;
    
    // Threading and synchronization
    pthread_mutex_t context_mutex;
    
} rpi_display_ctx_t;

// Internal function prototypes
int display_context_init(rpi_display_ctx_t* ctx, const display_config_t* config);
void display_context_destroy(rpi_display_ctx_t* ctx);

#endif // DISPLAY_CONTEXT_H 