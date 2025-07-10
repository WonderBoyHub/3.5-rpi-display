#!/bin/bash

# Ubuntu Server 24.04.2 LTS Compatibility Script
# Copyright (C) 2024 Efficient RPi Display Project
# Licensed under GPL-3.0

set -e

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

# Check if running Ubuntu Server 24.04.2 LTS
check_ubuntu_server_24_04() {
    log_info "Checking Ubuntu Server 24.04.2 LTS compatibility..."
    
    if [[ ! -f /etc/os-release ]]; then
        log_error "Cannot determine OS version"
        return 1
    fi
    
    source /etc/os-release
    
    if [[ "$ID" == "ubuntu" ]]; then
        log_success "Ubuntu detected: $PRETTY_NAME"
        
        # Check version
        if [[ "$VERSION_ID" == "24.04" ]]; then
            log_success "Ubuntu 24.04 LTS detected - fully supported!"
            return 0
        elif [[ "$VERSION_ID" == "22.04" ]] || [[ "$VERSION_ID" == "23.04" ]] || [[ "$VERSION_ID" == "23.10" ]]; then
            log_success "Ubuntu $VERSION_ID detected - compatible"
            return 0
        else
            log_warn "Ubuntu $VERSION_ID detected - should work but untested"
            return 0
        fi
    else
        log_error "Not Ubuntu - this script is for Ubuntu Server 24.04.2 LTS"
        return 1
    fi
}

# Install Ubuntu Server 24.04.2 LTS specific packages
install_ubuntu_server_packages() {
    log_info "Installing Ubuntu Server 24.04.2 LTS packages..."
    
    # Update package list
    apt-get update
    
    # Core build tools
    apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        git \
        curl \
        wget
    
    # Linux headers for current kernel
    apt-get install -y linux-headers-$(uname -r)
    
    # Device Tree Compiler
    apt-get install -y device-tree-compiler
    
    # Modern graphics stack for Ubuntu Server
    apt-get install -y \
        libdrm-dev \
        libgbm-dev \
        libegl1-mesa-dev \
        libgles2-mesa-dev \
        libgl1-mesa-dev \
        mesa-common-dev \
        mesa-utils
    
    # Wayland support (optional for server)
    apt-get install -y \
        libwayland-dev \
        wayland-protocols \
        libwayland-egl1-mesa \
        libwayland-client0 \
        libwayland-server0
    
    # Python development (if needed)
    apt-get install -y python3-dev python3-pip
    
    # Threading and system libraries
    apt-get install -y \
        libpthread-stubs0-dev \
        libc6-dev \
        libsystemd-dev
    
    # Additional utilities
    apt-get install -y \
        udev \
        systemd \
        dbus \
        rsync
    
    log_success "Ubuntu Server 24.04.2 LTS packages installed"
}

# Configure Ubuntu Server specific settings
configure_ubuntu_server() {
    log_info "Configuring Ubuntu Server 24.04.2 LTS settings..."
    
    # Enable required services
    systemctl enable systemd-udevd
    systemctl enable dbus
    
    # Configure GPU access groups
    getent group video > /dev/null || groupadd video
    getent group render > /dev/null || groupadd render
    getent group gpio > /dev/null || groupadd gpio
    getent group spi > /dev/null || groupadd spi
    
    # Add current user to groups if not root
    if [[ -n "$SUDO_USER" ]]; then
        usermod -a -G video,render,gpio,spi "$SUDO_USER"
        log_info "Added $SUDO_USER to required groups"
    fi
    
    # Configure systemd tmpfiles for /dev/dri access
    cat > /etc/tmpfiles.d/dri-render.conf << EOF
# DRI render nodes
d /dev/dri 0755 root root
c /dev/dri/renderD128 0666 root render
EOF
    
    # Configure udev rules for Ubuntu Server
    cat > /etc/udev/rules.d/99-rpi-display-ubuntu.rules << EOF
# Ubuntu Server RPi Display rules
SUBSYSTEM=="drm", KERNEL=="card*", MODE="0666", GROUP="video"
SUBSYSTEM=="drm", KERNEL=="renderD*", MODE="0666", GROUP="render"
SUBSYSTEM=="spidev", KERNEL=="spidev*", MODE="0666", GROUP="spi"
SUBSYSTEM=="gpio", KERNEL=="gpio*", MODE="0666", GROUP="gpio"
EOF
    
    # Reload udev rules
    udevadm control --reload-rules
    udevadm trigger
    
    log_success "Ubuntu Server configuration complete"
}

# Check and configure boot settings for Ubuntu Server
configure_ubuntu_server_boot() {
    log_info "Configuring boot settings for Ubuntu Server..."
    
    # Ubuntu Server may use different boot configuration
    if [[ -f /boot/firmware/config.txt ]]; then
        CONFIG_FILE="/boot/firmware/config.txt"
        log_info "Using Ubuntu Server boot config: $CONFIG_FILE"
    elif [[ -f /boot/config.txt ]]; then
        CONFIG_FILE="/boot/config.txt"
        log_info "Using standard boot config: $CONFIG_FILE"
    else
        log_warn "No boot config file found - may need manual configuration"
        return 1
    fi
    
    # Backup config
    if [[ ! -f "${CONFIG_FILE}.backup" ]]; then
        cp "$CONFIG_FILE" "${CONFIG_FILE}.backup"
        log_info "Boot config backed up"
    fi
    
    # Enable SPI
    if ! grep -q "dtparam=spi=on" "$CONFIG_FILE"; then
        echo "dtparam=spi=on" >> "$CONFIG_FILE"
        log_info "SPI enabled"
    fi
    
    # Enable DRM/KMS
    if ! grep -q "dtoverlay=vc4-kms-v3d" "$CONFIG_FILE"; then
        echo "dtoverlay=vc4-kms-v3d" >> "$CONFIG_FILE"
        log_info "DRM/KMS enabled"
    fi
    
    # GPU memory allocation
    if ! grep -q "gpu_mem=" "$CONFIG_FILE"; then
        echo "gpu_mem=128" >> "$CONFIG_FILE"
        log_info "GPU memory set to 128MB"
    fi
    
    log_success "Boot configuration updated"
}

# Verify Ubuntu Server compatibility
verify_ubuntu_server_compatibility() {
    log_info "Verifying Ubuntu Server 24.04.2 LTS compatibility..."
    
    local issues=0
    
    # Check kernel version
    local kernel_version=$(uname -r)
    log_info "Kernel version: $kernel_version"
    
    # Check if required devices exist
    if [[ ! -d /sys/class/spi_master ]]; then
        log_warn "SPI master not found - may need reboot"
        ((issues++))
    fi
    
    # Check for DRM devices
    if [[ ! -d /sys/class/drm ]]; then
        log_warn "DRM devices not found - may need reboot"
        ((issues++))
    fi
    
    # Check for required libraries
    if ! ldconfig -p | grep -q libdrm; then
        log_error "libdrm not found in library path"
        ((issues++))
    fi
    
    if ! ldconfig -p | grep -q libgbm; then
        log_error "libgbm not found in library path"
        ((issues++))
    fi
    
    if [[ $issues -eq 0 ]]; then
        log_success "Ubuntu Server 24.04.2 LTS compatibility verified"
        return 0
    else
        log_warn "Found $issues potential issues - may need reboot"
        return 1
    fi
}

# Create Ubuntu Server specific test script
create_ubuntu_server_test() {
    log_info "Creating Ubuntu Server test script..."
    
    cat > /usr/local/bin/ubuntu-server-display-test << 'EOF'
#!/bin/bash

# Ubuntu Server Display Test
echo "Testing Efficient RPi Display on Ubuntu Server 24.04.2 LTS..."

# Check SPI
echo "Checking SPI devices..."
ls -la /dev/spi* 2>/dev/null || echo "No SPI devices found"

# Check DRM
echo "Checking DRM devices..."
ls -la /dev/dri/ 2>/dev/null || echo "No DRM devices found"

# Check GPU
echo "Checking GPU memory..."
vcgencmd get_mem gpu 2>/dev/null || echo "vcgencmd not available"

# Check groups
echo "Checking user groups..."
groups

# Test basic display functionality
echo "Testing display initialization..."
if [[ -f /usr/local/bin/display_test ]]; then
    echo "Running display test..."
    /usr/local/bin/display_test
else
    echo "Display test program not found"
fi

echo "Ubuntu Server display test complete"
EOF
    
    chmod +x /usr/local/bin/ubuntu-server-display-test
    log_success "Ubuntu Server test script created"
}

# Main function
main() {
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                    Ubuntu Server 24.04.2 LTS Compatibility Setup                       ║"
    echo "║                        Efficient RPi Display Driver                                     ║"
    echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
    echo
    
    # Check if running as root
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi
    
    # Check Ubuntu Server compatibility
    if ! check_ubuntu_server_24_04; then
        log_error "Ubuntu Server 24.04.2 LTS compatibility check failed"
        exit 1
    fi
    
    # Install packages
    install_ubuntu_server_packages
    
    # Configure system
    configure_ubuntu_server
    
    # Configure boot
    configure_ubuntu_server_boot
    
    # Create test script
    create_ubuntu_server_test
    
    # Verify compatibility
    verify_ubuntu_server_compatibility
    
    echo
    log_success "Ubuntu Server 24.04.2 LTS compatibility setup complete!"
    echo
    log_info "Next steps:"
    log_info "1. Run the main installation: sudo ./scripts/enhanced_install.sh"
    log_info "2. Reboot your system: sudo reboot"
    log_info "3. Test the display: sudo ubuntu-server-display-test"
    echo
    
    read -p "Would you like to proceed with the main installation now? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        log_info "Starting main installation..."
        exec "$(dirname "$0")/enhanced_install.sh"
    else
        log_info "Run 'sudo ./scripts/enhanced_install.sh' when ready"
    fi
}

# Run main function
main "$@" 