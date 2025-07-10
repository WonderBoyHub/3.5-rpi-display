#!/bin/bash

# Efficient RPi Display Driver Installation Script
# Copyright (C) 2024 Efficient RPi Display Project
# Licensed under GPL-3.0

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
BUILD_DIR="${PROJECT_DIR}/build"
OVERLAY_DIR="/boot/overlays"
CONFIG_FILE="/boot/config.txt"
BACKUP_SUFFIX=".efficient-rpi-backup"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# Check system compatibility
check_system() {
    log_info "Checking system compatibility..."
    
    # Check if running on Raspberry Pi or compatible system
    if [[ ! -f /proc/device-tree/model ]]; then
        log_warn "Cannot determine system model. Proceeding anyway..."
    else
        local model=$(cat /proc/device-tree/model)
        log_info "Detected system: $model"
        
        if [[ "$model" == *"Raspberry Pi"* ]]; then
            log_success "Raspberry Pi detected"
        else
            log_warn "This may not be a Raspberry Pi. Proceeding anyway..."
        fi
    fi
    
    # Check for required files
    if [[ ! -f "$CONFIG_FILE" ]]; then
        log_error "Boot configuration file not found: $CONFIG_FILE"
        exit 1
    fi
    
    # Check for SPI support
    if ! grep -q "dtparam=spi=on" "$CONFIG_FILE"; then
        log_warn "SPI not enabled in boot config. Will be enabled during installation."
    fi
}

# Install dependencies
install_dependencies() {
    log_info "Installing dependencies..."
    
    # Update package list
    apt-get update
    
    # Install build dependencies
    apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        device-tree-compiler \
        linux-headers-$(uname -r) \
        python3-dev \
        libpthread-stubs0-dev \
        pkg-config \
        git
    
    log_success "Dependencies installed successfully"
}

# Build the driver
build_driver() {
    log_info "Building the driver..."
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure with CMake
    cmake -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -DBUILD_EXAMPLES=ON \
        -DBUILD_TESTS=OFF \
        -DBUILD_PYTHON=OFF \
        "$PROJECT_DIR"
    
    # Build
    ninja
    
    log_success "Driver built successfully"
}

# Install the driver
install_driver() {
    log_info "Installing the driver..."
    
    cd "$BUILD_DIR"
    
    # Install files
    ninja install
    
    # Update library cache
    ldconfig
    
    log_success "Driver installed successfully"
}

# Install device tree overlay
install_overlay() {
    log_info "Installing device tree overlay..."
    
    # Create overlay directory if it doesn't exist
    mkdir -p "$OVERLAY_DIR"
    
    # Copy overlay file
    if [[ -f "$BUILD_DIR/efficient-rpi35-overlay.dtbo" ]]; then
        cp "$BUILD_DIR/efficient-rpi35-overlay.dtbo" "$OVERLAY_DIR/"
        log_success "Device tree overlay installed"
    else
        log_error "Device tree overlay not found. Please compile manually."
        exit 1
    fi
}

# Configure boot settings
configure_boot() {
    log_info "Configuring boot settings..."
    
    # Backup config file
    if [[ ! -f "${CONFIG_FILE}${BACKUP_SUFFIX}" ]]; then
        cp "$CONFIG_FILE" "${CONFIG_FILE}${BACKUP_SUFFIX}"
        log_info "Boot config backed up to ${CONFIG_FILE}${BACKUP_SUFFIX}"
    fi
    
    # Enable SPI
    if ! grep -q "dtparam=spi=on" "$CONFIG_FILE"; then
        echo "dtparam=spi=on" >> "$CONFIG_FILE"
        log_info "SPI enabled in boot config"
    fi
    
    # Add display overlay
    if ! grep -q "dtoverlay=efficient-rpi35" "$CONFIG_FILE"; then
        echo "" >> "$CONFIG_FILE"
        echo "# Efficient RPi Display Configuration" >> "$CONFIG_FILE"
        echo "dtoverlay=efficient-rpi35,rotate=0,speed=80000000" >> "$CONFIG_FILE"
        log_info "Display overlay added to boot config"
    fi
    
    # Optional: Set display resolution
    if ! grep -q "hdmi_group=2" "$CONFIG_FILE"; then
        echo "" >> "$CONFIG_FILE"
        echo "# Display resolution settings" >> "$CONFIG_FILE"
        echo "hdmi_group=2" >> "$CONFIG_FILE"
        echo "hdmi_mode=87" >> "$CONFIG_FILE"
        echo "hdmi_cvt=480 320 60 1 0 0 0" >> "$CONFIG_FILE"
        echo "hdmi_force_hotplug=1" >> "$CONFIG_FILE"
        log_info "Display resolution configured"
    fi
    
    log_success "Boot configuration updated"
}

# Install udev rules
install_udev_rules() {
    log_info "Installing udev rules..."
    
    # Create udev rules file
    cat > /etc/udev/rules.d/99-efficient-rpi-display.rules << EOF
# Efficient RPi Display udev rules
SUBSYSTEM=="spidev", KERNEL=="spidev0.0", MODE="0666", GROUP="spi"
SUBSYSTEM=="spidev", KERNEL=="spidev0.1", MODE="0666", GROUP="spi"
SUBSYSTEM=="gpio", KERNEL=="gpio*", MODE="0666", GROUP="gpio"
EOF
    
    # Create groups if they don't exist
    getent group spi > /dev/null || groupadd spi
    getent group gpio > /dev/null || groupadd gpio
    
    # Add pi user to groups (if it exists)
    if id -u pi > /dev/null 2>&1; then
        usermod -a -G spi,gpio pi
        log_info "Added pi user to spi and gpio groups"
    fi
    
    # Reload udev rules
    udevadm control --reload-rules
    udevadm trigger
    
    log_success "Udev rules installed"
}

# Create test programs
create_test_programs() {
    log_info "Creating test programs..."
    
    # Create examples directory
    mkdir -p "$PROJECT_DIR/examples"
    
    # Create simple display test
    cat > "$PROJECT_DIR/examples/display_test.c" << 'EOF'
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
EOF
    
    # Create touch test
    cat > "$PROJECT_DIR/examples/touch_test.c" << 'EOF'
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
EOF
    
    log_success "Test programs created"
}

# Main installation function
main() {
    log_info "Starting Efficient RPi Display Driver installation..."
    
    check_root
    check_system
    install_dependencies
    create_test_programs
    build_driver
    install_driver
    install_overlay
    configure_boot
    install_udev_rules
    
    log_success "Installation completed successfully!"
    log_info ""
    log_info "Next steps:"
    log_info "1. Reboot your system to activate the new configuration"
    log_info "2. Test the display with: sudo display_test"
    log_info "3. Test touch functionality with: sudo touch_test"
    log_info "4. Calibrate touch if needed with: sudo calibrate-touch.sh"
    log_info ""
    log_warn "A reboot is required to activate the display driver."
    
    # Ask for reboot
    read -p "Would you like to reboot now? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        log_info "Rebooting..."
        reboot
    else
        log_info "Please reboot manually when ready."
    fi
}

# Run main function
main "$@" 