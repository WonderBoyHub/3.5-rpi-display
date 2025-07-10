#!/bin/bash

# Pi 5 Compatibility Check Script
# Copyright (C) 2024 Efficient RPi Display Project
# Licensed under GPL-3.0

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Compatibility check results
COMPATIBILITY_SCORE=0
MAX_SCORE=10
ISSUES_FOUND=()
RECOMMENDATIONS=()

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_check() { echo -e "${CYAN}[CHECK]${NC} $1"; }

add_issue() {
    ISSUES_FOUND+=("$1")
}

add_recommendation() {
    RECOMMENDATIONS+=("$1")
}

increment_score() {
    COMPATIBILITY_SCORE=$((COMPATIBILITY_SCORE + 1))
}

# Check Pi 5 hardware
check_pi5_hardware() {
    log_check "Checking Raspberry Pi 5 hardware..."
    
    if [[ -f /proc/device-tree/model ]]; then
        local model=$(cat /proc/device-tree/model)
        if [[ "$model" == *"Raspberry Pi 5"* ]]; then
            log_success "Raspberry Pi 5 detected: $model"
            increment_score
            
            # Check Pi 5 specific features
            if [[ -f /proc/device-tree/soc/ranges ]]; then
                log_success "Pi 5 SoC detected"
                increment_score
            fi
            
            # Check for Pi 5 GPU (VideoCore VII)
            if [[ -d /sys/class/drm/card0 ]]; then
                log_success "DRM/KMS graphics driver available"
                increment_score
            else
                add_issue "DRM/KMS graphics driver not available"
                add_recommendation "Enable GPU in raspi-config: Advanced Options > GL Driver > Legacy"
            fi
            
        else
            add_issue "Not running on Raspberry Pi 5: $model"
            add_recommendation "This driver is optimized for Pi 5. Consider using legacy version for older Pi models."
        fi
    else
        add_issue "Cannot detect hardware model"
        add_recommendation "Ensure /proc/device-tree/model exists"
    fi
}

# Check OS compatibility
check_os_compatibility() {
    log_check "Checking OS compatibility..."
    
    if [[ -f /etc/os-release ]]; then
        source /etc/os-release
        log_info "Detected OS: $PRETTY_NAME"
        
        case "$ID" in
            "raspbian"|"debian")
                if [[ "$VERSION_ID" == "12" ]] || [[ "$VERSION_ID" == "11" ]]; then
                    log_success "Supported Debian/Raspbian version: $VERSION_ID"
                    increment_score
                else
                    add_issue "Untested Debian/Raspbian version: $VERSION_ID"
                    add_recommendation "Update to Debian 11 (Bullseye) or 12 (Bookworm) for best compatibility"
                fi
                ;;
            "ubuntu")
                if [[ "$VERSION_ID" == "22.04" ]] || [[ "$VERSION_ID" == "23.04" ]] || [[ "$VERSION_ID" == "24.04" ]]; then
                    log_success "Supported Ubuntu version: $VERSION_ID"
                    increment_score
                else
                    add_issue "Untested Ubuntu version: $VERSION_ID"
                    add_recommendation "Update to Ubuntu 22.04 LTS or newer for best compatibility"
                fi
                ;;
            "fedora")
                local version=$(echo $VERSION_ID | cut -d. -f1)
                if [[ $version -ge 38 ]]; then
                    log_success "Supported Fedora version: $VERSION_ID"
                    increment_score
                else
                    add_issue "Old Fedora version: $VERSION_ID"
                    add_recommendation "Update to Fedora 38 or newer"
                fi
                ;;
            "arch")
                log_success "Arch Linux detected (rolling release)"
                increment_score
                ;;
            "alpine")
                if [[ "$VERSION_ID" == "3.18"* ]] || [[ "$VERSION_ID" == "3.19"* ]]; then
                    log_success "Supported Alpine version: $VERSION_ID"
                    increment_score
                else
                    add_issue "Untested Alpine version: $VERSION_ID"
                    add_recommendation "Update to Alpine 3.18 or newer"
                fi
                ;;
            *)
                add_issue "Unsupported or untested OS: $ID"
                add_recommendation "Use Raspberry Pi OS, Ubuntu, Debian, Fedora, Arch, or Alpine for best support"
                ;;
        esac
    else
        add_issue "Cannot detect OS version"
        add_recommendation "Ensure /etc/os-release exists"
    fi
}

# Check kernel version
check_kernel_compatibility() {
    log_check "Checking kernel compatibility..."
    
    local kernel_version=$(uname -r)
    local major_version=$(echo $kernel_version | cut -d. -f1)
    local minor_version=$(echo $kernel_version | cut -d. -f2)
    
    log_info "Kernel version: $kernel_version"
    
    if [[ $major_version -ge 6 ]] || [[ $major_version -eq 5 && $minor_version -ge 15 ]]; then
        log_success "Compatible kernel version (>= 5.15)"
        increment_score
        
        # Check for specific Pi 5 features
        if [[ $major_version -ge 6 && $minor_version -ge 1 ]]; then
            log_success "Kernel supports Pi 5 hardware acceleration"
            increment_score
        fi
    else
        add_issue "Old kernel version: $kernel_version"
        add_recommendation "Update to kernel 5.15 or newer for Pi 5 support"
    fi
}

# Check boot configuration
check_boot_config() {
    log_check "Checking boot configuration..."
    
    local config_file=""
    if [[ -f /boot/firmware/config.txt ]]; then
        config_file="/boot/firmware/config.txt"
    elif [[ -f /boot/config.txt ]]; then
        config_file="/boot/config.txt"
    else
        add_issue "Boot configuration file not found"
        add_recommendation "Ensure /boot/config.txt or /boot/firmware/config.txt exists"
        return
    fi
    
    log_info "Using boot config: $config_file"
    
    # Check GPU memory
    if grep -q "gpu_mem=" "$config_file"; then
        local gpu_mem=$(grep "gpu_mem=" "$config_file" | tail -1 | cut -d= -f2)
        if [[ $gpu_mem -ge 128 ]]; then
            log_success "GPU memory allocated: ${gpu_mem}MB"
            increment_score
        else
            add_issue "Insufficient GPU memory: ${gpu_mem}MB"
            add_recommendation "Set gpu_mem=128 or higher in $config_file"
        fi
    else
        add_issue "GPU memory not configured"
        add_recommendation "Add gpu_mem=128 to $config_file"
    fi
    
    # Check DRM/KMS
    if grep -q "dtoverlay=vc4-kms-v3d" "$config_file"; then
        log_success "DRM/KMS graphics driver enabled"
        increment_score
    else
        add_issue "DRM/KMS not enabled"
        add_recommendation "Add 'dtoverlay=vc4-kms-v3d' to $config_file"
    fi
    
    # Check SPI
    if grep -q "dtparam=spi=on" "$config_file"; then
        log_success "SPI interface enabled"
        increment_score
    else
        add_issue "SPI not enabled"
        add_recommendation "Add 'dtparam=spi=on' to $config_file"
    fi
}

# Check required packages
check_required_packages() {
    log_check "Checking required packages..."
    
    local packages_ok=true
    
    # Check for build tools
    if ! command -v gcc &> /dev/null; then
        add_issue "GCC compiler not found"
        add_recommendation "Install build-essential or equivalent package"
        packages_ok=false
    fi
    
    if ! command -v cmake &> /dev/null; then
        add_issue "CMake not found"
        add_recommendation "Install cmake package"
        packages_ok=false
    fi
    
    # Check for graphics libraries
    if ! pkg-config --exists libdrm; then
        add_issue "libdrm development package not found"
        add_recommendation "Install libdrm-dev package"
        packages_ok=false
    fi
    
    if ! pkg-config --exists gbm; then
        add_issue "GBM development package not found"
        add_recommendation "Install libgbm-dev package"
        packages_ok=false
    fi
    
    if $packages_ok; then
        log_success "Required packages available"
        increment_score
    fi
}

# Check graphics capabilities
check_graphics_capabilities() {
    log_check "Checking graphics capabilities..."
    
    # Check for DRM devices
    if [[ -c /dev/dri/card0 ]]; then
        log_success "DRM card0 device available"
        increment_score
        
        # Check for render node
        if [[ -c /dev/dri/renderD128 ]]; then
            log_success "GPU render node available (hardware acceleration)"
            increment_score
        else
            add_issue "GPU render node not available"
            add_recommendation "Enable GPU acceleration in raspi-config"
        fi
    else
        add_issue "DRM graphics device not available"
        add_recommendation "Enable DRM/KMS graphics driver in boot config"
    fi
    
    # Check for V3D support
    if [[ -d /sys/kernel/debug/dri/0 ]]; then
        log_success "V3D GPU driver loaded"
    else
        add_issue "V3D GPU driver not loaded"
        add_recommendation "Ensure vc4-kms-v3d overlay is enabled"
    fi
}

# Check Wayland support
check_wayland_support() {
    log_check "Checking Wayland support..."
    
    if [[ -n "$WAYLAND_DISPLAY" ]]; then
        log_success "Running under Wayland"
        increment_score
    elif [[ "$XDG_SESSION_TYPE" == "wayland" ]]; then
        log_success "Wayland session type detected"
        increment_score
    else
        log_info "Not running under Wayland (this is optional)"
        add_recommendation "Consider using Wayland for better performance (optional)"
    fi
    
    # Check for Wayland compositors
    if command -v wayfire &> /dev/null; then
        log_success "Wayfire compositor available"
    elif command -v sway &> /dev/null; then
        log_success "Sway compositor available"
    elif command -v weston &> /dev/null; then
        log_success "Weston compositor available"
    else
        log_info "No Wayland compositors found (optional)"
    fi
}

# Generate compatibility report
generate_report() {
    echo
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                             Pi 5 Compatibility Report                                       ║"
    echo "╠══════════════════════════════════════════════════════════════════════════════════════════╣"
    
    # Overall score
    local percentage=$((COMPATIBILITY_SCORE * 100 / MAX_SCORE))
    if [[ $percentage -ge 90 ]]; then
        echo -e "║ Overall Compatibility: ${GREEN}EXCELLENT${NC} (${COMPATIBILITY_SCORE}/${MAX_SCORE}) - ${percentage}%                                    ║"
    elif [[ $percentage -ge 70 ]]; then
        echo -e "║ Overall Compatibility: ${YELLOW}GOOD${NC} (${COMPATIBILITY_SCORE}/${MAX_SCORE}) - ${percentage}%                                         ║"
    elif [[ $percentage -ge 50 ]]; then
        echo -e "║ Overall Compatibility: ${YELLOW}FAIR${NC} (${COMPATIBILITY_SCORE}/${MAX_SCORE}) - ${percentage}%                                         ║"
    else
        echo -e "║ Overall Compatibility: ${RED}POOR${NC} (${COMPATIBILITY_SCORE}/${MAX_SCORE}) - ${percentage}%                                         ║"
    fi
    
    echo "║                                                                                              ║"
    
    # Issues found
    if [[ ${#ISSUES_FOUND[@]} -gt 0 ]]; then
        echo "║ Issues Found:                                                                            ║"
        for issue in "${ISSUES_FOUND[@]}"; do
            echo -e "║ ${RED}✗${NC} $(printf "%-83s" "$issue") ║"
        done
        echo "║                                                                                              ║"
    fi
    
    # Recommendations
    if [[ ${#RECOMMENDATIONS[@]} -gt 0 ]]; then
        echo "║ Recommendations:                                                                         ║"
        for rec in "${RECOMMENDATIONS[@]}"; do
            echo -e "║ ${YELLOW}•${NC} $(printf "%-83s" "$rec") ║"
        done
        echo "║                                                                                              ║"
    fi
    
    # Installation recommendation
    if [[ $percentage -ge 80 ]]; then
        echo -e "║ ${GREEN}✓ Your system is ready for installation!${NC}                                            ║"
        echo "║ Run: sudo ./scripts/enhanced_install.sh                                                 ║"
    elif [[ $percentage -ge 60 ]]; then
        echo -e "║ ${YELLOW}⚠ Installation may work but please address the issues above first${NC}                   ║"
        echo "║ Run compatibility check again after fixes: ./scripts/pi5_compatibility_check.sh       ║"
    else
        echo -e "║ ${RED}✗ Please fix the critical issues before installing${NC}                                   ║"
        echo "║ Run compatibility check again after fixes: ./scripts/pi5_compatibility_check.sh       ║"
    fi
    
    echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
    echo
}

# Quick fix function
apply_quick_fixes() {
    echo
    log_info "Would you like to apply automatic fixes? (y/n)"
    read -r response
    
    if [[ "$response" =~ ^[Yy]$ ]]; then
        log_info "Applying quick fixes..."
        
        # Enable SPI if not enabled
        local config_file=""
        if [[ -f /boot/firmware/config.txt ]]; then
            config_file="/boot/firmware/config.txt"
        elif [[ -f /boot/config.txt ]]; then
            config_file="/boot/config.txt"
        fi
        
        if [[ -n "$config_file" ]]; then
            if ! grep -q "dtparam=spi=on" "$config_file"; then
                echo "dtparam=spi=on" >> "$config_file"
                log_success "Enabled SPI interface"
            fi
            
            if ! grep -q "dtoverlay=vc4-kms-v3d" "$config_file"; then
                echo "dtoverlay=vc4-kms-v3d" >> "$config_file"
                log_success "Enabled DRM/KMS graphics driver"
            fi
            
            if ! grep -q "gpu_mem=" "$config_file"; then
                echo "gpu_mem=128" >> "$config_file"
                log_success "Set GPU memory to 128MB"
            fi
        fi
        
        log_warn "Reboot required to apply changes"
        echo "Run 'sudo reboot' and then run this check again"
    fi
}

# Main function
main() {
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                      Raspberry Pi 5 Compatibility Check v2.0                            ║"
    echo "║                        Efficient RPi Display Driver                                     ║"
    echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
    echo
    
    check_pi5_hardware
    check_os_compatibility
    check_kernel_compatibility
    check_boot_config
    check_required_packages
    check_graphics_capabilities
    check_wayland_support
    
    generate_report
    
    if [[ ${#ISSUES_FOUND[@]} -gt 0 ]]; then
        apply_quick_fixes
    fi
}

# Run main function
main "$@" 