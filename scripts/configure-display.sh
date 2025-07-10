#!/bin/bash

# Display Configuration Script for Efficient RPi Display Driver
# Copyright (C) 2024 Efficient RPi Display Project
# Licensed under GPL-3.0

set -e

# Configuration
CONFIG_FILE="/boot/config.txt"
OVERLAY_PARAMS="dtoverlay=efficient-rpi35"
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

# Backup configuration file
backup_config() {
    if [[ ! -f "${CONFIG_FILE}${BACKUP_SUFFIX}" ]]; then
        cp "$CONFIG_FILE" "${CONFIG_FILE}${BACKUP_SUFFIX}"
        log_info "Configuration backed up to ${CONFIG_FILE}${BACKUP_SUFFIX}"
    fi
}

# Show current configuration
show_config() {
    log_info "Current display configuration:"
    echo
    
    if grep -q "dtoverlay=efficient-rpi35" "$CONFIG_FILE"; then
        log_success "Efficient RPi Display overlay is enabled"
        
        # Show overlay parameters
        local overlay_line=$(grep "dtoverlay=efficient-rpi35" "$CONFIG_FILE")
        echo "Overlay line: $overlay_line"
        
        # Parse parameters
        if [[ "$overlay_line" == *"rotate="* ]]; then
            local rotation=$(echo "$overlay_line" | sed -n 's/.*rotate=\([^,]*\).*/\1/p')
            echo "Rotation: $rotation"
        fi
        
        if [[ "$overlay_line" == *"speed="* ]]; then
            local speed=$(echo "$overlay_line" | sed -n 's/.*speed=\([^,]*\).*/\1/p')
            echo "SPI Speed: $speed Hz"
        fi
        
    else
        log_warn "Efficient RPi Display overlay is not enabled"
    fi
    
    echo
    
    # Check SPI
    if grep -q "dtparam=spi=on" "$CONFIG_FILE"; then
        log_success "SPI is enabled"
    else
        log_warn "SPI is not enabled"
    fi
    
    # Check HDMI settings
    if grep -q "hdmi_group=2" "$CONFIG_FILE"; then
        log_info "HDMI group 2 (DMT) is set"
    fi
    
    if grep -q "hdmi_mode=87" "$CONFIG_FILE"; then
        log_info "HDMI mode 87 (custom) is set"
    fi
    
    if grep -q "hdmi_cvt=" "$CONFIG_FILE"; then
        local cvt_line=$(grep "hdmi_cvt=" "$CONFIG_FILE")
        echo "Custom resolution: $cvt_line"
    fi
}

# Configure display rotation
configure_rotation() {
    local rotation="$1"
    
    if [[ ! "$rotation" =~ ^[0-3]$ ]]; then
        log_error "Invalid rotation value. Must be 0, 1, 2, or 3"
        return 1
    fi
    
    log_info "Setting display rotation to $rotation"
    
    # Remove existing overlay line
    sed -i '/dtoverlay=efficient-rpi35/d' "$CONFIG_FILE"
    
    # Add new overlay line with rotation
    echo "dtoverlay=efficient-rpi35,rotate=$rotation,speed=80000000" >> "$CONFIG_FILE"
    
    log_success "Display rotation set to $rotation"
}

# Configure SPI speed
configure_spi_speed() {
    local speed="$1"
    
    if [[ ! "$speed" =~ ^[0-9]+$ ]]; then
        log_error "Invalid SPI speed value. Must be a number"
        return 1
    fi
    
    log_info "Setting SPI speed to $speed Hz"
    
    # Update overlay line
    if grep -q "dtoverlay=efficient-rpi35" "$CONFIG_FILE"; then
        sed -i "s/dtoverlay=efficient-rpi35[^)]*/dtoverlay=efficient-rpi35,rotate=0,speed=$speed/" "$CONFIG_FILE"
    else
        echo "dtoverlay=efficient-rpi35,rotate=0,speed=$speed" >> "$CONFIG_FILE"
    fi
    
    log_success "SPI speed set to $speed Hz"
}

# Enable/disable display
toggle_display() {
    local action="$1"
    
    case "$action" in
        enable)
            log_info "Enabling display..."
            
            # Enable SPI
            if ! grep -q "dtparam=spi=on" "$CONFIG_FILE"; then
                echo "dtparam=spi=on" >> "$CONFIG_FILE"
            fi
            
            # Enable overlay
            if ! grep -q "dtoverlay=efficient-rpi35" "$CONFIG_FILE"; then
                echo "dtoverlay=efficient-rpi35,rotate=0,speed=80000000" >> "$CONFIG_FILE"
            fi
            
            log_success "Display enabled"
            ;;
        disable)
            log_info "Disabling display..."
            
            # Comment out overlay
            sed -i 's/^dtoverlay=efficient-rpi35/#dtoverlay=efficient-rpi35/' "$CONFIG_FILE"
            
            log_success "Display disabled"
            ;;
        *)
            log_error "Invalid action. Use 'enable' or 'disable'"
            return 1
            ;;
    esac
}

# Interactive configuration
interactive_config() {
    log_info "Interactive Display Configuration"
    echo
    
    # Show current settings
    show_config
    echo
    
    # Rotation
    echo "Display Rotation Options:"
    echo "  0 = Normal (0°)"
    echo "  1 = 90° clockwise"
    echo "  2 = 180° (upside down)"
    echo "  3 = 270° clockwise (90° counter-clockwise)"
    echo
    
    read -p "Enter rotation (0-3) [0]: " rotation
    rotation="${rotation:-0}"
    
    # SPI Speed
    echo
    echo "SPI Speed Options:"
    echo "  40000000  = 40 MHz (safe)"
    echo "  80000000  = 80 MHz (recommended)"
    echo "  100000000 = 100 MHz (experimental)"
    echo
    
    read -p "Enter SPI speed in Hz [80000000]: " speed
    speed="${speed:-80000000}"
    
    # Apply configuration
    echo
    log_info "Applying configuration..."
    
    backup_config
    configure_rotation "$rotation"
    configure_spi_speed "$speed"
    
    log_success "Configuration applied successfully"
    log_warn "Reboot required for changes to take effect"
}

# Display help
show_help() {
    echo "Display Configuration Script for Efficient RPi Display Driver"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "OPTIONS:"
    echo "  -h, --help               Show this help message"
    echo "  -s, --show               Show current configuration"
    echo "  -i, --interactive        Interactive configuration"
    echo "  -r, --rotation <0-3>     Set display rotation"
    echo "  -p, --speed <hz>         Set SPI speed in Hz"
    echo "  -e, --enable             Enable display"
    echo "  -d, --disable            Disable display"
    echo "  -b, --backup             Backup configuration"
    echo "  -t, --restore            Restore from backup"
    echo ""
    echo "Examples:"
    echo "  sudo $0 --show              # Show current settings"
    echo "  sudo $0 --interactive       # Interactive configuration"
    echo "  sudo $0 --rotation 1        # Set 90° rotation"
    echo "  sudo $0 --speed 80000000    # Set SPI speed to 80MHz"
    echo "  sudo $0 --enable            # Enable display"
    echo "  sudo $0 --disable           # Disable display"
    echo ""
    echo "Rotation values:"
    echo "  0 = Normal (0°)"
    echo "  1 = 90° clockwise"
    echo "  2 = 180° (upside down)"
    echo "  3 = 270° clockwise"
    echo ""
}

# Restore configuration from backup
restore_config() {
    if [[ -f "${CONFIG_FILE}${BACKUP_SUFFIX}" ]]; then
        log_info "Restoring configuration from backup..."
        cp "${CONFIG_FILE}${BACKUP_SUFFIX}" "$CONFIG_FILE"
        log_success "Configuration restored from backup"
    else
        log_error "No backup file found: ${CONFIG_FILE}${BACKUP_SUFFIX}"
        return 1
    fi
}

# Main function
main() {
    case "${1:-}" in
        -h|--help)
            show_help
            exit 0
            ;;
        -s|--show)
            show_config
            ;;
        -i|--interactive)
            check_root
            interactive_config
            ;;
        -r|--rotation)
            check_root
            if [[ -z "$2" ]]; then
                log_error "Rotation value required"
                exit 1
            fi
            backup_config
            configure_rotation "$2"
            ;;
        -p|--speed)
            check_root
            if [[ -z "$2" ]]; then
                log_error "SPI speed value required"
                exit 1
            fi
            backup_config
            configure_spi_speed "$2"
            ;;
        -e|--enable)
            check_root
            backup_config
            toggle_display "enable"
            ;;
        -d|--disable)
            check_root
            backup_config
            toggle_display "disable"
            ;;
        -b|--backup)
            check_root
            backup_config
            log_success "Configuration backed up"
            ;;
        -t|--restore)
            check_root
            restore_config
            ;;
        "")
            # Default action: show config
            show_config
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
}

# Run main function
main "$@" 