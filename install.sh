#!/bin/bash

# Efficient RPi Display Driver - One-Line Installer
# Copyright (C) 2024 Efficient RPi Display Project
# Licensed under GPL-3.0
#
# Usage: curl -sSL https://raw.githubusercontent.com/your-repo/efficient-rpi-display/main/install.sh | sudo bash

set -e

# Configuration
REPO_URL="https://github.com/your-repo/efficient-rpi-display.git"
INSTALL_DIR="/tmp/efficient-rpi-display-install"
BRANCH="main"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (use sudo)"
        log_info "Run: curl -sSL https://raw.githubusercontent.com/your-repo/efficient-rpi-display/main/install.sh | sudo bash"
        exit 1
    fi
}

# Detect system and check compatibility
check_compatibility() {
    log_info "Checking system compatibility..."
    
    # Check if we're on a Raspberry Pi
    if [[ -f /proc/device-tree/model ]]; then
        local model=$(cat /proc/device-tree/model)
        if [[ "$model" == *"Raspberry Pi"* ]]; then
            log_success "Raspberry Pi detected: $model"
            if [[ "$model" == *"Raspberry Pi 5"* ]]; then
                log_success "Pi 5 detected - will use enhanced features"
            else
                log_info "Older Pi model detected - will use compatible mode"
            fi
        else
            log_warn "Not a Raspberry Pi - attempting generic installation"
        fi
    else
        log_warn "Cannot detect hardware model"
    fi
    
    # Check for required tools
    if ! command -v git &> /dev/null; then
        log_info "Installing git..."
        if command -v apt &> /dev/null; then
            apt update && apt install -y git
        elif command -v dnf &> /dev/null; then
            dnf install -y git
        elif command -v pacman &> /dev/null; then
            pacman -Sy --noconfirm git
        elif command -v apk &> /dev/null; then
            apk update && apk add git
        else
            log_error "Cannot install git - please install manually"
            exit 1
        fi
    fi
}

# Download and run installer
main() {
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                     Efficient RPi Display Driver - Quick Installer                      ║"
    echo "║                                                                                          ║"
    echo "║  🚀 Pi 5 Optimized • 🎮 GPU Accelerated • ⚡ Modern Graphics Stack                     ║"
    echo "║                                                                                          ║"
    echo "║  This will automatically:                                                               ║"
    echo "║  • Download the latest driver                                                           ║"
    echo "║  • Detect your system and hardware                                                      ║"
    echo "║  • Install all dependencies                                                             ║"
    echo "║  • Configure for optimal performance                                                    ║"
    echo "║  • Set up GPU acceleration and monitoring tools                                         ║"
    echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
    echo
    
    check_root
    check_compatibility
    
    # Clean up any previous installation
    if [[ -d "$INSTALL_DIR" ]]; then
        log_info "Cleaning up previous installation attempt..."
        rm -rf "$INSTALL_DIR"
    fi
    
    # Clone repository
    log_info "Downloading Efficient RPi Display Driver..."
    git clone --depth 1 --branch "$BRANCH" "$REPO_URL" "$INSTALL_DIR"
    
    if [[ ! -d "$INSTALL_DIR" ]]; then
        log_error "Failed to download repository"
        exit 1
    fi
    
    cd "$INSTALL_DIR"
    
    # Make scripts executable
    chmod +x scripts/*.sh
    
    # Run the enhanced installer
    log_info "Starting installation..."
    if [[ -f "scripts/enhanced_install.sh" ]]; then
        ./scripts/enhanced_install.sh --quiet
    elif [[ -f "scripts/install.sh" ]]; then
        ./scripts/install.sh
    else
        log_error "Installation script not found"
        exit 1
    fi
    
    # Clean up
    log_info "Cleaning up temporary files..."
    cd /
    rm -rf "$INSTALL_DIR"
    
    echo
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                               🎉 Installation Complete! 🎉                              ║"
    echo "╠══════════════════════════════════════════════════════════════════════════════════════════╣"
    echo "║                                                                                          ║"
    echo "║  Next Steps:                                                                             ║"
    echo "║  1. Reboot your system: sudo reboot                                                     ║"
    echo "║  2. Test your display: sudo display_test                                                ║"
    echo "║  3. Monitor performance: sudo performance_monitor                                       ║"
    echo "║                                                                                          ║"
    echo "║  Need help? Check the troubleshooting guide:                                            ║"
    echo "║  ./scripts/pi5_compatibility_check.sh                                                   ║"
    echo "║                                                                                          ║"
    echo "║  Advanced configuration:                                                                 ║"
    echo "║  sudo configure-display.sh --help                                                       ║"
    echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
    echo
    
    log_warn "A reboot is required to activate all changes."
    read -p "Would you like to reboot now? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        log_info "Rebooting in 3 seconds... (Press Ctrl+C to cancel)"
        sleep 3
        reboot
    else
        log_info "Remember to reboot manually: sudo reboot"
    fi
}

# Run main function
main "$@" 