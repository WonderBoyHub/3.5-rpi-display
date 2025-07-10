#!/bin/bash

# Enhanced Efficient RPi Display Driver Installation Script
# Copyright (C) 2024 Efficient RPi Display Project
# Licensed under GPL-3.0
# Supports: Raspberry Pi OS, Ubuntu, Debian, Fedora, Arch Linux, Alpine Linux

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
BUILD_DIR="${PROJECT_DIR}/build"
OVERLAY_DIR="/boot/overlays"
CONFIG_FILE="/boot/config.txt"
BACKUP_SUFFIX=".efficient-rpi-backup"

# Distribution detection
DISTRO=""
PACKAGE_MANAGER=""
INIT_SYSTEM=""

# Hardware detection
PI_MODEL=""
GPU_TYPE=""
HAS_V3D=false
HAS_WAYLAND=false

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
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

log_debug() {
    echo -e "${PURPLE}[DEBUG]${NC} $1"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# Detect Linux distribution
detect_distro() {
    log_info "Detecting Linux distribution..."
    
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        DISTRO="$ID"
        log_info "Detected distribution: $PRETTY_NAME"
    elif [[ -f /etc/debian_version ]]; then
        DISTRO="debian"
        log_info "Detected distribution: Debian"
    elif [[ -f /etc/redhat-release ]]; then
        DISTRO="fedora"
        log_info "Detected distribution: Red Hat based"
    elif [[ -f /etc/arch-release ]]; then
        DISTRO="arch"
        log_info "Detected distribution: Arch Linux"
    elif [[ -f /etc/alpine-release ]]; then
        DISTRO="alpine"
        log_info "Detected distribution: Alpine Linux"
    else
        log_warn "Unknown distribution, assuming Debian-based"
        DISTRO="debian"
    fi
    
    # Normalize distribution names
    case "$DISTRO" in
        "raspbian"|"debian"|"ubuntu"|"linuxmint"|"elementary")
            DISTRO="debian"
            PACKAGE_MANAGER="apt"
            ;;
        "fedora"|"rhel"|"centos"|"rocky"|"almalinux")
            DISTRO="fedora"
            PACKAGE_MANAGER="dnf"
            ;;
        "arch"|"manjaro"|"endeavouros")
            DISTRO="arch"
            PACKAGE_MANAGER="pacman"
            ;;
        "alpine")
            DISTRO="alpine"
            PACKAGE_MANAGER="apk"
            ;;
        "opensuse"|"sles")
            DISTRO="opensuse"
            PACKAGE_MANAGER="zypper"
            ;;
        *)
            log_warn "Unsupported distribution: $DISTRO, trying Debian packages"
            DISTRO="debian"
            PACKAGE_MANAGER="apt"
            ;;
    esac
    
    log_success "Using package manager: $PACKAGE_MANAGER"
}

# Detect init system
detect_init_system() {
    if [[ $(ps -p 1 -o comm=) == "systemd" ]]; then
        INIT_SYSTEM="systemd"
    elif [[ -f /sbin/openrc ]]; then
        INIT_SYSTEM="openrc"
    else
        INIT_SYSTEM="sysv"
    fi
    
    log_info "Detected init system: $INIT_SYSTEM"
}

# Hardware detection
detect_hardware() {
    log_info "Detecting hardware..."
    
    # Detect Raspberry Pi model
    if [[ -f /proc/device-tree/model ]]; then
        local model=$(cat /proc/device-tree/model)
        log_info "System model: $model"
        
        case "$model" in
            *"Raspberry Pi 5"*)
                PI_MODEL="pi5"
                GPU_TYPE="v3d7"
                HAS_V3D=true
                ;;
            *"Raspberry Pi 4"*)
                PI_MODEL="pi4"
                GPU_TYPE="v3d6"
                HAS_V3D=true
                ;;
            *"Raspberry Pi 3"*)
                PI_MODEL="pi3"
                GPU_TYPE="vc4"
                HAS_V3D=false
                ;;
            *"Raspberry Pi"*)
                PI_MODEL="pi_legacy"
                GPU_TYPE="vc4"
                HAS_V3D=false
                ;;
            *)
                PI_MODEL="unknown"
                GPU_TYPE="unknown"
                log_warn "Unknown hardware, attempting generic installation"
                ;;
        esac
    else
        log_warn "Cannot detect hardware model"
        PI_MODEL="unknown"
        GPU_TYPE="unknown"
    fi
    
    # Check for V3D support
    if [[ -c /dev/dri/renderD128 ]]; then
        HAS_V3D=true
        log_info "V3D GPU driver detected"
    fi
    
    # Check for Wayland support
    if [[ -n "$WAYLAND_DISPLAY" ]] || [[ -n "$XDG_SESSION_TYPE" && "$XDG_SESSION_TYPE" == "wayland" ]]; then
        HAS_WAYLAND=true
        log_info "Wayland session detected"
    fi
    
    log_success "Hardware detection complete: $PI_MODEL with $GPU_TYPE"
}

# Package installation functions
install_debian_packages() {
    log_info "Installing Debian/Ubuntu packages..."
    
    # Update package list
    apt-get update
    
    # Base packages
    local packages=(
        "build-essential"
        "cmake"
        "ninja-build"
        "device-tree-compiler"
        "linux-headers-$(uname -r)"
        "pkg-config"
        "git"
        "python3-dev"
        "libpthread-stubs0-dev"
    )
    
    # Modern graphics stack
    packages+=(
        "libdrm-dev"
        "libgbm-dev"
        "libegl1-mesa-dev"
        "libgles2-mesa-dev"
        "libwayland-dev"
        "wayland-protocols"
        "libwayland-egl1-mesa"
        "mesa-common-dev"
        "libgl1-mesa-dev"
    )
    
    # Pi-specific packages
    if [[ "$PI_MODEL" != "unknown" ]]; then
        packages+=(
            "libraspberrypi-dev"
            "libraspberrypi0"
        )
    fi
    
    # Wayland compositor packages
    if [[ "$HAS_WAYLAND" == true ]]; then
        packages+=(
            "weston"
            "wayfire"
            "sway"
            "libwayland-cursor0"
            "libwayland-client0"
            "libwayland-server0"
        )
    fi
    
    # Install packages
    DEBIAN_FRONTEND=noninteractive apt-get install -y "${packages[@]}"
    
    log_success "Debian packages installed successfully"
}

install_fedora_packages() {
    log_info "Installing Fedora packages..."
    
    # Update package list
    dnf check-update || true
    
    # Base packages
    local packages=(
        "gcc"
        "gcc-c++"
        "make"
        "cmake"
        "ninja-build"
        "dtc"
        "kernel-devel"
        "pkgconfig"
        "git"
        "python3-devel"
    )
    
    # Modern graphics stack
    packages+=(
        "libdrm-devel"
        "mesa-libgbm-devel"
        "mesa-libEGL-devel"
        "mesa-libGLES-devel"
        "wayland-devel"
        "wayland-protocols-devel"
        "mesa-libwayland-egl-devel"
        "mesa-dri-drivers"
    )
    
    # Wayland compositor packages
    if [[ "$HAS_WAYLAND" == true ]]; then
        packages+=(
            "weston"
            "wayfire"
            "sway"
            "wayland-utils"
        )
    fi
    
    # Install packages
    dnf install -y "${packages[@]}"
    
    log_success "Fedora packages installed successfully"
}

install_arch_packages() {
    log_info "Installing Arch Linux packages..."
    
    # Update package database
    pacman -Sy --noconfirm
    
    # Base packages
    local packages=(
        "base-devel"
        "cmake"
        "ninja"
        "dtc"
        "linux-headers"
        "pkgconfig"
        "git"
        "python"
    )
    
    # Modern graphics stack
    packages+=(
        "libdrm"
        "mesa"
        "wayland"
        "wayland-protocols"
        "egl-wayland"
        "mesa-utils"
    )
    
    # Wayland compositor packages
    if [[ "$HAS_WAYLAND" == true ]]; then
        packages+=(
            "weston"
            "wayfire"
            "sway"
            "wayland-utils"
        )
    fi
    
    # Install packages
    pacman -S --noconfirm "${packages[@]}"
    
    log_success "Arch packages installed successfully"
}

install_alpine_packages() {
    log_info "Installing Alpine Linux packages..."
    
    # Update package index
    apk update
    
    # Base packages
    local packages=(
        "build-base"
        "cmake"
        "ninja"
        "dtc"
        "linux-headers"
        "pkgconfig"
        "git"
        "python3-dev"
    )
    
    # Modern graphics stack
    packages+=(
        "libdrm-dev"
        "mesa-dev"
        "wayland-dev"
        "wayland-protocols"
        "mesa-egl"
        "mesa-gles"
        "mesa-gbm"
    )
    
    # Wayland compositor packages
    if [[ "$HAS_WAYLAND" == true ]]; then
        packages+=(
            "weston"
            "sway"
            "wayland-utils"
        )
    fi
    
    # Install packages
    apk add "${packages[@]}"
    
    log_success "Alpine packages installed successfully"
}

# Main package installation dispatcher
install_dependencies() {
    log_info "Installing dependencies for $DISTRO..."
    
    case "$PACKAGE_MANAGER" in
        "apt")
            install_debian_packages
            ;;
        "dnf")
            install_fedora_packages
            ;;
        "pacman")
            install_arch_packages
            ;;
        "apk")
            install_alpine_packages
            ;;
        "zypper")
            log_warn "OpenSUSE support is experimental"
            zypper refresh
            zypper install -y gcc gcc-c++ make cmake ninja dtc kernel-devel pkg-config git python3-devel libdrm-devel Mesa-libgbm-devel Mesa-libEGL-devel wayland-devel wayland-protocols-devel
            ;;
        *)
            log_error "Unsupported package manager: $PACKAGE_MANAGER"
            return 1
            ;;
    esac
}

# Enhanced build system
build_driver() {
    log_info "Building the enhanced driver..."
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure CMake with modern options
    local cmake_args=(
        "-G" "Ninja"
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX"
        "-DBUILD_EXAMPLES=ON"
        "-DBUILD_TESTS=ON"
        "-DBUILD_PYTHON=OFF"
        "-DENABLE_DRM_KMS=ON"
        "-DENABLE_GPU_ACCELERATION=ON"
        "-DENABLE_WAYLAND=ON"
        "-DENABLE_HUGE_PAGES=ON"
    )
    
    # Hardware-specific options
    if [[ "$HAS_V3D" == true ]]; then
        cmake_args+=("-DENABLE_V3D_SUPPORT=ON")
    fi
    
    if [[ "$PI_MODEL" == "pi5" ]]; then
        cmake_args+=("-DTARGET_PI5=ON")
    fi
    
    cmake "${cmake_args[@]}" "$PROJECT_DIR"
    
    # Build with optimal CPU usage
    local cpu_count=$(nproc)
    ninja -j$cpu_count
    
    log_success "Driver built successfully"
}

# Enhanced system configuration
configure_system() {
    log_info "Configuring system for modern graphics stack..."
    
    # Configure boot settings
    if [[ -f "$CONFIG_FILE" ]]; then
        configure_boot_traditional
    else
        configure_boot_modern
    fi
    
    # Configure udev rules
    configure_udev_rules
    
    # Configure systemd services if available
    if [[ "$INIT_SYSTEM" == "systemd" ]]; then
        configure_systemd_services
    fi
    
    # Configure Wayland if available
    if [[ "$HAS_WAYLAND" == true ]]; then
        configure_wayland
    fi
}

configure_boot_traditional() {
    log_info "Configuring traditional boot configuration..."
    
    # Backup config file
    if [[ ! -f "${CONFIG_FILE}${BACKUP_SUFFIX}" ]]; then
        cp "$CONFIG_FILE" "${CONFIG_FILE}${BACKUP_SUFFIX}"
        log_info "Boot config backed up to ${CONFIG_FILE}${BACKUP_SUFFIX}"
    fi
    
    # Enable DRM
    if ! grep -q "dtoverlay=vc4-kms-v3d" "$CONFIG_FILE"; then
        echo "" >> "$CONFIG_FILE"
        echo "# Modern DRM/KMS graphics stack" >> "$CONFIG_FILE"
        echo "dtoverlay=vc4-kms-v3d" >> "$CONFIG_FILE"
        log_info "DRM/KMS overlay enabled"
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
    
    # GPU memory allocation
    if ! grep -q "gpu_mem=" "$CONFIG_FILE"; then
        echo "gpu_mem=128" >> "$CONFIG_FILE"
        log_info "GPU memory allocation set to 128MB"
    fi
    
    # Enable huge pages support
    if ! grep -q "transparent_hugepage=" "$CONFIG_FILE"; then
        echo "transparent_hugepage=madvise" >> "$CONFIG_FILE"
        log_info "Transparent huge pages enabled"
    fi
}

configure_boot_modern() {
    log_info "Configuring modern boot system..."
    
    # Check for systemd-boot or GRUB
    if [[ -d /boot/loader/entries ]]; then
        configure_systemd_boot
    elif [[ -f /etc/default/grub ]]; then
        configure_grub
    else
        log_warn "Unknown boot system, manual configuration may be required"
    fi
}

configure_systemd_boot() {
    log_info "Configuring systemd-boot..."
    
    # Add kernel parameters for DRM
    local entry_file="/boot/loader/entries/raspberry-pi.conf"
    if [[ -f "$entry_file" ]]; then
        if ! grep -q "vc4-kms-v3d" "$entry_file"; then
            sed -i '/^options/ s/$/ vc4-kms-v3d.enable=1/' "$entry_file"
            log_info "DRM kernel parameter added to systemd-boot"
        fi
    fi
}

configure_grub() {
    log_info "Configuring GRUB..."
    
    # Add kernel parameters
    if ! grep -q "vc4-kms-v3d" /etc/default/grub; then
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT/ s/"$/ vc4-kms-v3d.enable=1"/' /etc/default/grub
        update-grub
        log_info "DRM kernel parameter added to GRUB"
    fi
}

configure_udev_rules() {
    log_info "Installing enhanced udev rules..."
    
    # Create comprehensive udev rules
    cat > /etc/udev/rules.d/99-efficient-rpi-display.rules << 'EOF'
# Efficient RPi Display enhanced udev rules
SUBSYSTEM=="spidev", KERNEL=="spidev0.0", MODE="0666", GROUP="spi"
SUBSYSTEM=="spidev", KERNEL=="spidev0.1", MODE="0666", GROUP="spi"
SUBSYSTEM=="gpio", KERNEL=="gpio*", MODE="0666", GROUP="gpio"
SUBSYSTEM=="drm", KERNEL=="card*", MODE="0666", GROUP="video"
SUBSYSTEM=="drm", KERNEL=="renderD*", MODE="0666", GROUP="render"
SUBSYSTEM=="drm", KERNEL=="controlD*", MODE="0666", GROUP="video"

# V3D GPU access
SUBSYSTEM=="drm", KERNEL=="renderD128", MODE="0666", GROUP="render", TAG+="uaccess"
SUBSYSTEM=="drm", KERNEL=="card0", MODE="0666", GROUP="video", TAG+="uaccess"

# Wayland access
KERNEL=="event*", SUBSYSTEM=="input", MODE="0664", GROUP="input"
EOF
    
    # Create groups if they don't exist
    getent group spi > /dev/null || groupadd spi
    getent group gpio > /dev/null || groupadd gpio
    getent group render > /dev/null || groupadd render
    getent group video > /dev/null || groupadd video
    getent group input > /dev/null || groupadd input
    
    # Add user to groups
    local user_name=$(logname 2>/dev/null || echo $SUDO_USER)
    if [[ -n "$user_name" ]]; then
        usermod -a -G spi,gpio,render,video,input "$user_name"
        log_info "Added $user_name to required groups"
    fi
    
    # Reload udev rules
    udevadm control --reload-rules
    udevadm trigger
    
    log_success "Enhanced udev rules installed"
}

configure_systemd_services() {
    log_info "Configuring systemd services..."
    
    # Enable DRM service if available
    if systemctl list-unit-files | grep -q "drm"; then
        systemctl enable drm.service
        log_info "DRM service enabled"
    fi
    
    # Enable GPU memory management
    if systemctl list-unit-files | grep -q "gpu-mem"; then
        systemctl enable gpu-mem.service
        log_info "GPU memory service enabled"
    fi
}

configure_wayland() {
    log_info "Configuring Wayland support..."
    
    # Set up Wayland environment
    mkdir -p /etc/environment.d
    cat > /etc/environment.d/wayland.conf << 'EOF'
# Wayland environment variables
GBM_BACKEND=v3d
WAYLAND_DISPLAY=wayland-0
XDG_SESSION_TYPE=wayland
WLR_DRM_NO_ATOMIC=1
EOF
    
    # Configure Wayfire if available
    if command -v wayfire &> /dev/null; then
        log_info "Wayfire compositor detected"
        # Add Wayfire specific configuration
    fi
    
    log_success "Wayland configuration complete"
}

# Enhanced testing
run_tests() {
    log_info "Running comprehensive tests..."
    
    cd "$BUILD_DIR"
    
    # Run built-in tests
    if [[ -f "test/test_suite" ]]; then
        ./test/test_suite
    fi
    
    # Test DRM functionality
    if [[ -f "test_drm" ]]; then
        ./test_drm
    fi
    
    # Test GPU acceleration
    if [[ -f "test_gpu" ]]; then
        ./test_gpu
    fi
    
    log_success "Tests completed"
}

# Performance optimization
optimize_performance() {
    log_info "Applying performance optimizations..."
    
    # CPU governor
    if [[ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]]; then
        echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
        log_info "CPU governor set to performance"
    fi
    
    # GPU frequency
    if [[ -f /sys/class/drm/card0/device/power_dpm_force_performance_level ]]; then
        echo "high" > /sys/class/drm/card0/device/power_dpm_force_performance_level
        log_info "GPU performance level set to high"
    fi
    
    # Enable huge pages
    if [[ -f /sys/kernel/mm/transparent_hugepage/enabled ]]; then
        echo "madvise" > /sys/kernel/mm/transparent_hugepage/enabled
        log_info "Transparent huge pages enabled"
    fi
    
    log_success "Performance optimizations applied"
}

# Pre-installation compatibility check
run_compatibility_check() {
    log_info "Running Pi 5 compatibility check..."
    
    # Run the compatibility check script if it exists
    if [[ -f "$SCRIPT_DIR/pi5_compatibility_check.sh" ]]; then
        chmod +x "$SCRIPT_DIR/pi5_compatibility_check.sh"
        if ! "$SCRIPT_DIR/pi5_compatibility_check.sh"; then
            log_warn "Compatibility check found issues. Continue anyway? (y/n)"
            read -r response
            if [[ ! "$response" =~ ^[Yy]$ ]]; then
                log_info "Installation cancelled. Please fix compatibility issues first."
                exit 1
            fi
        fi
    else
        log_warn "Compatibility check script not found, proceeding with installation"
    fi
}

# Simplified one-command installation
simple_install() {
    log_info "╔══════════════════════════════════════════════════════════════════════════════════════╗"
    log_info "║                     Quick Pi 5 Display Driver Installation                          ║"
    log_info "║                                                                                      ║"
    log_info "║  This will automatically:                                                           ║"
    log_info "║  • Detect your Pi 5 and OS version                                                  ║"
    log_info "║  • Install all required dependencies                                                ║"
    log_info "║  • Build and install the optimized driver                                           ║"
    log_info "║  • Configure your system for best performance                                       ║"
    log_info "║  • Enable GPU acceleration and modern graphics stack                                ║"
    log_info "║                                                                                      ║"
    log_info "╚══════════════════════════════════════════════════════════════════════════════════════╝"
    echo
    
    log_warn "This installation will modify system files. Continue? (y/n)"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        log_info "Installation cancelled."
        exit 0
    fi
}

# Check for Pi 5 specific optimizations
configure_pi5_optimizations() {
    if [[ "$PI_MODEL" == "pi5" ]]; then
        log_info "Applying Raspberry Pi 5 specific optimizations..."
        
        # Pi 5 specific GPU settings
        local config_file=""
        if [[ -f /boot/firmware/config.txt ]]; then
            config_file="/boot/firmware/config.txt"
        elif [[ -f /boot/config.txt ]]; then
            config_file="/boot/config.txt"
        fi
        
        if [[ -n "$config_file" ]]; then
            # Pi 5 optimizations
            if ! grep -q "# Pi 5 Display Optimizations" "$config_file"; then
                echo "" >> "$config_file"
                echo "# Pi 5 Display Optimizations" >> "$config_file"
                echo "gpu_mem=128" >> "$config_file"
                echo "dtoverlay=vc4-kms-v3d,composite" >> "$config_file"
                echo "max_framebuffers=2" >> "$config_file"
                log_info "Applied Pi 5 specific optimizations"
            fi
            
            # Enable huge pages for Pi 5
            if ! grep -q "transparent_hugepage" "$config_file"; then
                echo "transparent_hugepage=madvise" >> "$config_file"
                log_info "Enabled transparent huge pages for Pi 5"
            fi
        fi
        
        # Pi 5 specific CPU governor
        if [[ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]]; then
            echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
            log_info "Set CPU governor to performance mode"
        fi
    fi
}

# Create desktop shortcuts and menu entries
create_desktop_integration() {
    log_info "Creating desktop integration..."
    
    # Create desktop shortcuts
    local desktop_dir="/home/$SUDO_USER/Desktop"
    if [[ -d "$desktop_dir" ]]; then
        # Performance monitor shortcut
        cat > "$desktop_dir/RPi-Display-Monitor.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=RPi Display Performance Monitor
Comment=Real-time display performance monitoring
Exec=x-terminal-emulator -e 'sudo performance_monitor'
Icon=utilities-system-monitor
Terminal=false
Categories=System;Monitor;
EOF
        chmod +x "$desktop_dir/RPi-Display-Monitor.desktop"
        
        # Display test shortcut
        cat > "$desktop_dir/RPi-Display-Test.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=RPi Display Test
Comment=Test display functionality
Exec=x-terminal-emulator -e 'sudo display_test; read -p "Press Enter to close..."'
Icon=video-display
Terminal=false
Categories=System;
EOF
        chmod +x "$desktop_dir/RPi-Display-Test.desktop"
        
        log_info "Created desktop shortcuts"
    fi
    
    # Create applications menu entries
    if [[ -d /usr/share/applications ]]; then
        cat > /usr/share/applications/rpi-display-tools.desktop << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=RPi Display Tools
Comment=Raspberry Pi Display Driver Tools
Exec=x-terminal-emulator -e 'echo "RPi Display Tools:"; echo "1. performance_monitor - Real-time monitoring"; echo "2. display_test - Test display"; echo "3. configure-display.sh - Configuration"; echo ""; echo "Run any command with sudo"; read -p "Press Enter to close..."'
Icon=video-display
Terminal=false
Categories=System;Settings;
EOF
        log_info "Created application menu entry"
    fi
}

# Post-installation verification
verify_installation() {
    log_info "Verifying installation..."
    
    # Check if library is installed
    if ldconfig -p | grep -q "libefficient_rpi_display"; then
        log_success "Library installed successfully"
    else
        log_error "Library installation verification failed"
        return 1
    fi
    
    # Check if device tree overlay is available
    if [[ -f "$OVERLAY_DIR/efficient-rpi35-overlay.dtbo" ]]; then
        log_success "Device tree overlay installed"
    else
        log_warn "Device tree overlay not found"
    fi
    
    # Check if executables are available
    local tools=("display_test" "performance_monitor" "configure-display.sh")
    for tool in "${tools[@]}"; do
        if command -v "$tool" &> /dev/null; then
            log_success "$tool installed"
        else
            log_warn "$tool not found in PATH"
        fi
    done
    
    log_success "Installation verification completed"
}

# Main installation function
main() {
    # Parse command line arguments
    local run_tests=false
    local enable_gpu=true
    local enable_monitoring=true
    local quiet_mode=false
    
    for arg in "$@"; do
        case $arg in
            --test)
                run_tests=true
                ;;
            --no-gpu)
                enable_gpu=false
                ;;
            --no-monitoring)
                enable_monitoring=false
                ;;
            --quiet)
                quiet_mode=true
                ;;
            --help)
                echo "Enhanced RPi Display Driver Installation"
                echo "Usage: $0 [options]"
                echo ""
                echo "Options:"
                echo "  --test         Run performance tests after installation"
                echo "  --no-gpu       Disable GPU acceleration"
                echo "  --no-monitoring Disable performance monitoring tools"
                echo "  --quiet        Minimal output mode"
                echo "  --help         Show this help message"
                exit 0
                ;;
        esac
    done
    
    if [[ "$quiet_mode" != true ]]; then
        simple_install
    fi
    
    log_info "Starting Enhanced Efficient RPi Display Driver installation..."
    
    check_root
    
    if [[ "$quiet_mode" != true ]]; then
        run_compatibility_check
    fi
    
    detect_distro
    detect_init_system
    detect_hardware
    
    install_dependencies
    build_driver
    
    cd "$BUILD_DIR"
    ninja install
    
    # Update library cache
    ldconfig
    
    configure_system
    configure_pi5_optimizations
    optimize_performance
    
    # Create desktop integration
    if [[ -n "$SUDO_USER" ]]; then
        create_desktop_integration
    fi
    
    # Verify installation
    verify_installation
    
    # Run tests if requested
    if [[ "$run_tests" == true ]]; then
        run_tests
    fi
    
    log_success "Enhanced installation completed successfully!"
    echo
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                               Installation Complete!                                     ║"
    echo "╠══════════════════════════════════════════════════════════════════════════════════════════╣"
    echo "║ System Information:                                                                      ║"
    echo "║ • Distribution: $DISTRO                                                                  ║"
    echo "║ • Pi Model: $PI_MODEL                                                                    ║"
    echo "║ • GPU Type: $GPU_TYPE                                                                    ║"
    echo "║ • V3D Support: $HAS_V3D                                                                  ║"
    echo "║ • Wayland: $HAS_WAYLAND                                                                  ║"
    echo "║                                                                                          ║"
    echo "║ Available Commands:                                                                      ║"
    echo "║ • sudo display_test              - Test display functionality                           ║"
    echo "║ • sudo performance_monitor       - Real-time performance monitoring                     ║"
    echo "║ • sudo display_benchmark         - Run performance benchmarks                           ║"
    echo "║ • sudo configure-display.sh      - Configure display settings                           ║"
    echo "║ • sudo calibrate-touch.sh        - Calibrate touch interface                            ║"
    echo "║                                                                                          ║"
    echo "║ Quick Start:                                                                             ║"
    echo "║ 1. Reboot your system: sudo reboot                                                      ║"
    echo "║ 2. Test display: sudo display_test                                                      ║"
    echo "║ 3. Monitor performance: sudo performance_monitor                                        ║"
    echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
    echo
    
    log_warn "A reboot is required to activate all driver components."
    
    # Ask for reboot
    if [[ "$quiet_mode" != true ]]; then
        read -p "Would you like to reboot now? (y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            log_info "Rebooting in 3 seconds... (Press Ctrl+C to cancel)"
            sleep 3
            reboot
        else
            log_info "Remember to reboot manually: sudo reboot"
        fi
    else
        log_info "Quiet mode: Remember to reboot manually"
    fi
}

# Run main function
main "$@" 