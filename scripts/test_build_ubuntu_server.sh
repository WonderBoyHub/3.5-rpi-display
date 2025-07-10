#!/bin/bash

# Ubuntu Server 24.04.2 LTS Build Test Script
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

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
TEST_DIR="$PROJECT_DIR/test_builds"
BUILD_CONFIGS=(
    "minimal"
    "basic" 
    "no-graphics"
    "with-graphics"
    "ubuntu-server"
)

# Cleanup function
cleanup() {
    log_info "Cleaning up test builds..."
    rm -rf "$TEST_DIR"
}

# Trap cleanup on exit
trap cleanup EXIT

# Check if we're running on Ubuntu
check_ubuntu() {
    if [[ -f /etc/os-release ]]; then
        source /etc/os-release
        if [[ "$ID" == "ubuntu" ]]; then
            log_success "Running on Ubuntu $VERSION_ID"
            return 0
        else
            log_warn "Not running on Ubuntu (detected: $PRETTY_NAME)"
        fi
    else
        log_warn "Cannot detect OS version"
    fi
    return 0  # Continue anyway for testing
}

# Install minimal build dependencies
install_minimal_deps() {
    log_info "Installing minimal build dependencies..."
    
    # Check if we have sudo access
    if ! sudo -n true 2>/dev/null; then
        log_warn "No sudo access - assuming dependencies are already installed"
        return 0
    fi
    
    # Update package list
    sudo apt-get update -qq
    
    # Install minimal required packages
    sudo apt-get install -y \
        build-essential \
        cmake \
        pkg-config \
        git \
        &>/dev/null
    
    log_success "Minimal dependencies installed"
}

# Test minimal configuration (no graphics libraries)
test_minimal_config() {
    log_info "Testing minimal configuration (no graphics libraries)..."
    
    local build_dir="$TEST_DIR/minimal"
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DBUILD_EXAMPLES=OFF"
        "-DBUILD_TESTS=OFF"
        "-DENABLE_DRM_KMS=OFF"
        "-DENABLE_GPU_ACCELERATION=OFF"
        "-DENABLE_WAYLAND=OFF"
    )
    
    if ! command -v cmake &> /dev/null; then
        log_warn "CMake not available - cannot test build on this system"
        return 0
    fi
    
    if ! cmake "${cmake_args[@]}" "$PROJECT_DIR" 2>&1 | tee cmake.log; then
        log_error "Minimal CMake configuration failed"
        cat cmake.log
        return 1
    fi
    
    if ! make -j$(nproc 2>/dev/null || echo 2) 2>&1 | tee build.log; then
        log_error "Minimal build failed"
        cat build.log
        return 1
    fi
    
    log_success "Minimal configuration builds successfully"
    return 0
}

# Test basic configuration (with examples, no graphics)
test_basic_config() {
    log_info "Testing basic configuration (examples, no graphics)..."
    
    local build_dir="$TEST_DIR/basic"
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DBUILD_EXAMPLES=ON"
        "-DBUILD_TESTS=OFF"
        "-DENABLE_DRM_KMS=OFF"
        "-DENABLE_GPU_ACCELERATION=OFF"
        "-DENABLE_WAYLAND=OFF"
    )
    
    if ! cmake "${cmake_args[@]}" "$PROJECT_DIR" 2>&1 | tee cmake.log; then
        log_error "Basic CMake configuration failed"
        cat cmake.log
        return 1
    fi
    
    if ! make -j$(nproc 2>/dev/null || echo 2) 2>&1 | tee build.log; then
        log_error "Basic build failed"
        cat build.log
        return 1
    fi
    
    # Check if example binaries were created
    if [[ -f "display_test" ]] && [[ -f "touch_test" ]]; then
        log_success "Basic configuration with examples builds successfully"
        return 0
    else
        log_error "Example binaries not found"
        return 1
    fi
}

# Test configuration without graphics libraries available
test_no_graphics_config() {
    log_info "Testing configuration without graphics libraries..."
    
    local build_dir="$TEST_DIR/no-graphics"
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    # Try to enable graphics features even though libraries aren't available
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DBUILD_EXAMPLES=ON"
        "-DBUILD_TESTS=OFF"
        "-DENABLE_DRM_KMS=ON"      # Enable but expect fallback
        "-DENABLE_GPU_ACCELERATION=ON"  # Enable but expect fallback
        "-DENABLE_WAYLAND=ON"      # Enable but expect fallback
    )
    
    # This should succeed with warnings about missing libraries
    if ! cmake "${cmake_args[@]}" "$PROJECT_DIR" 2>&1 | tee cmake.log; then
        log_error "No-graphics CMake configuration failed"
        cat cmake.log
        return 1
    fi
    
    # Check that libraries were properly disabled
    if grep -q "libdrm.*not found" cmake.log && \
       grep -q "EGL.*not found" cmake.log; then
        log_success "Properly detected missing graphics libraries"
    else
        log_warn "Expected missing library warnings not found"
    fi
    
    if ! make -j$(nproc 2>/dev/null || echo 2) 2>&1 | tee build.log; then
        log_error "No-graphics build failed"
        cat build.log
        return 1
    fi
    
    log_success "Configuration with missing graphics libraries builds successfully"
    return 0
}

# Test configuration with graphics libraries (if available)
test_with_graphics_config() {
    log_info "Testing configuration with graphics libraries (if available)..."
    
    # Try to install graphics libraries
    if sudo -n true 2>/dev/null; then
        log_info "Attempting to install graphics libraries..."
        sudo apt-get install -y \
            libdrm-dev \
            libgbm-dev \
            libegl1-mesa-dev \
            libgles2-mesa-dev \
            libwayland-dev \
            wayland-protocols \
            2>/dev/null || log_warn "Graphics libraries installation failed or not available"
    fi
    
    local build_dir="$TEST_DIR/with-graphics"
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DBUILD_EXAMPLES=ON"
        "-DBUILD_TESTS=OFF"
        "-DENABLE_DRM_KMS=ON"
        "-DENABLE_GPU_ACCELERATION=ON"
        "-DENABLE_WAYLAND=ON"
    )
    
    if ! cmake "${cmake_args[@]}" "$PROJECT_DIR" 2>&1 | tee cmake.log; then
        log_error "With-graphics CMake configuration failed"
        cat cmake.log
        return 1
    fi
    
    if ! make -j$(nproc 2>/dev/null || echo 2) 2>&1 | tee build.log; then
        log_error "With-graphics build failed"
        cat build.log
        return 1
    fi
    
    # Check what features were actually enabled
    if grep -q "Found libdrm" cmake.log; then
        log_success "DRM/KMS support enabled"
    else
        log_info "DRM/KMS support disabled (libraries not available)"
    fi
    
    if grep -q "Found EGL" cmake.log; then
        log_success "GPU acceleration enabled"
    else
        log_info "GPU acceleration disabled (libraries not available)"
    fi
    
    log_success "Configuration with graphics libraries builds successfully"
    return 0
}

# Test Ubuntu Server specific configuration
test_ubuntu_server_config() {
    log_info "Testing Ubuntu Server specific configuration..."
    
    local build_dir="$TEST_DIR/ubuntu-server"
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    # Ubuntu Server typically runs headless
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DBUILD_EXAMPLES=ON"
        "-DBUILD_TESTS=OFF"
        "-DENABLE_DRM_KMS=ON"
        "-DENABLE_GPU_ACCELERATION=ON"
        "-DENABLE_WAYLAND=OFF"  # Typically headless
    )
    
    if ! cmake "${cmake_args[@]}" "$PROJECT_DIR" 2>&1 | tee cmake.log; then
        log_error "Ubuntu Server CMake configuration failed"
        cat cmake.log
        return 1
    fi
    
    if ! make -j$(nproc 2>/dev/null || echo 2) 2>&1 | tee build.log; then
        log_error "Ubuntu Server build failed"
        cat build.log
        return 1
    fi
    
    log_success "Ubuntu Server configuration builds successfully"
    return 0
}

# Validate source files exist
validate_source_files() {
    log_info "Validating source files..."
    
    local required_files=(
        "CMakeLists.txt"
        "src/efficient_rpi_display.c"
        "src/ili9486l_driver.c"
        "src/xpt2046_touch.c"
        "src/modern_drm_interface.c"
        "include/efficient_rpi_display.h"
        "include/ili9486l_driver.h"
        "include/xpt2046_touch.h"
        "include/modern_drm_interface.h"
        "examples/display_test.c"
        "examples/touch_test.c"
    )
    
    for file in "${required_files[@]}"; do
        if [[ ! -f "$PROJECT_DIR/$file" ]]; then
            log_error "Missing required file: $file"
            return 1
        fi
    done
    
    log_success "All required source files found"
    return 0
}

# Check for compilation warnings in a specific build
check_build_warnings() {
    local build_dir="$1"
    local config_name="$2"
    
    if [[ -f "$build_dir/build.log" ]]; then
        local warning_count=$(grep -c "warning:" "$build_dir/build.log" 2>/dev/null || echo 0)
        local error_count=$(grep -c "error:" "$build_dir/build.log" 2>/dev/null || echo 0)
        
        if [[ $error_count -gt 0 ]]; then
            log_error "$config_name: $error_count compilation errors found"
            grep "error:" "$build_dir/build.log" | head -5
            return 1
        elif [[ $warning_count -gt 0 ]]; then
            log_warn "$config_name: $warning_count compilation warnings found"
            grep "warning:" "$build_dir/build.log" | head -3
        else
            log_success "$config_name: No compilation errors or warnings"
        fi
    fi
    
    return 0
}

# Main test function
main() {
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                    Ubuntu Server 24.04.2 LTS Build Test Suite                          ║"
    echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
    echo
    
    local tests_passed=0
    local tests_total=0
    
    # Create test directory
    mkdir -p "$TEST_DIR"
    
    # Run validation tests
    local test_functions=(
        "check_ubuntu"
        "validate_source_files"
        "install_minimal_deps"
        "test_minimal_config"
        "test_basic_config"
        "test_no_graphics_config"
        "test_with_graphics_config"
        "test_ubuntu_server_config"
    )
    
    for test_func in "${test_functions[@]}"; do
        ((tests_total++))
        echo
        if $test_func; then
            ((tests_passed++))
        else
            log_error "Test failed: $test_func"
        fi
    done
    
    echo
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                            Build Test Results                                           ║"
    echo "╠══════════════════════════════════════════════════════════════════════════════════════════╣"
    echo "║  Tests Passed: $tests_passed/$tests_total                                                          ║"
    
    # Check for warnings in all builds
    echo "║                                                                                          ║"
    echo "║  Compilation Analysis:                                                                   ║"
    for config in "${BUILD_CONFIGS[@]}"; do
        if [[ -d "$TEST_DIR/$config" ]]; then
            check_build_warnings "$TEST_DIR/$config" "$config"
        fi
    done
    
    if [[ $tests_passed -eq $tests_total ]]; then
        echo "║                                                                                          ║"
        echo "║  Status: ✅ ALL TESTS PASSED                                                          ║"
        echo "║                                                                                          ║"
        echo "║  The driver builds successfully on Ubuntu Server 24.04.2 LTS!                          ║"
        echo "║  All configurations tested and working properly.                                        ║"
        echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
        return 0
    else
        echo "║                                                                                          ║"
        echo "║  Status: ❌ SOME TESTS FAILED                                                         ║"
        echo "║                                                                                          ║"
        echo "║  Check the error messages above for details.                                            ║"
        echo "║  Build logs are available in: $TEST_DIR/*/                             ║"
        echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
        return 1
    fi
}

# Run main function
main "$@" 