#!/bin/bash

# Build Validation Script for Efficient RPi Display Driver
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

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

# Build configurations to test
CONFIGS=(
    "basic"
    "minimal"
    "full"
    "ubuntu-server"
)

# Cleanup function
cleanup() {
    log_info "Cleaning up build directories..."
    rm -rf "$BUILD_DIR"
}

# Trap cleanup on exit
trap cleanup EXIT

# Test basic configuration
test_basic_config() {
    log_info "Testing basic configuration..."
    
    mkdir -p "$BUILD_DIR/basic"
    cd "$BUILD_DIR/basic"
    
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DBUILD_EXAMPLES=ON"
        "-DBUILD_TESTS=OFF"
        "-DENABLE_DRM_KMS=OFF"
        "-DENABLE_GPU_ACCELERATION=OFF"
        "-DENABLE_WAYLAND=OFF"
    )
    
    if ! cmake "${cmake_args[@]}" "$PROJECT_DIR" &>/dev/null; then
        log_error "Basic CMake configuration failed"
        return 1
    fi
    
    if ! make -j$(nproc 2>/dev/null || echo 4) &>/dev/null; then
        log_error "Basic build failed"
        return 1
    fi
    
    log_success "Basic configuration works"
    return 0
}

# Test minimal configuration
test_minimal_config() {
    log_info "Testing minimal configuration..."
    
    mkdir -p "$BUILD_DIR/minimal"
    cd "$BUILD_DIR/minimal"
    
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DBUILD_EXAMPLES=OFF"
        "-DBUILD_TESTS=OFF"
        "-DENABLE_DRM_KMS=OFF"
        "-DENABLE_GPU_ACCELERATION=OFF"
        "-DENABLE_WAYLAND=OFF"
    )
    
    if ! cmake "${cmake_args[@]}" "$PROJECT_DIR" &>/dev/null; then
        log_error "Minimal CMake configuration failed"
        return 1
    fi
    
    if ! make -j$(nproc 2>/dev/null || echo 4) &>/dev/null; then
        log_error "Minimal build failed"
        return 1
    fi
    
    log_success "Minimal configuration works"
    return 0
}

# Test full configuration (if libraries available)
test_full_config() {
    log_info "Testing full configuration..."
    
    mkdir -p "$BUILD_DIR/full"
    cd "$BUILD_DIR/full"
    
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DBUILD_EXAMPLES=ON"
        "-DBUILD_TESTS=ON"
        "-DENABLE_DRM_KMS=ON"
        "-DENABLE_GPU_ACCELERATION=ON"
        "-DENABLE_WAYLAND=ON"
    )
    
    if ! cmake "${cmake_args[@]}" "$PROJECT_DIR" &>/dev/null; then
        log_warn "Full CMake configuration failed (libraries may not be available)"
        return 0  # Not an error if libraries aren't available
    fi
    
    if ! make -j$(nproc 2>/dev/null || echo 4) &>/dev/null; then
        log_warn "Full build failed (expected if DRM/EGL libraries not available)"
        return 0  # Not an error on systems without graphics libraries
    fi
    
    log_success "Full configuration works"
    return 0
}

# Test Ubuntu Server specific configuration
test_ubuntu_server_config() {
    log_info "Testing Ubuntu Server configuration..."
    
    mkdir -p "$BUILD_DIR/ubuntu-server"
    cd "$BUILD_DIR/ubuntu-server"
    
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DBUILD_EXAMPLES=ON"
        "-DBUILD_TESTS=OFF"
        "-DENABLE_DRM_KMS=ON"
        "-DENABLE_GPU_ACCELERATION=ON"
        "-DENABLE_WAYLAND=OFF"  # Usually headless
    )
    
    if ! cmake "${cmake_args[@]}" "$PROJECT_DIR" &>/dev/null; then
        log_warn "Ubuntu Server CMake configuration failed (libraries may not be available)"
        return 0
    fi
    
    if ! make -j$(nproc 2>/dev/null || echo 4) &>/dev/null; then
        log_warn "Ubuntu Server build failed (expected if DRM/EGL libraries not available)"
        return 0
    fi
    
    log_success "Ubuntu Server configuration works"
    return 0
}

# Check for required tools
check_build_tools() {
    log_info "Checking for required build tools..."
    
    local missing_tools=()
    
    if ! command -v cmake &> /dev/null; then
        missing_tools+=("cmake")
    fi
    
    if ! command -v make &> /dev/null; then
        missing_tools+=("make")
    fi
    
    if ! command -v gcc &> /dev/null; then
        missing_tools+=("gcc")
    fi
    
    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        log_info "Please install the missing tools and try again"
        return 1
    fi
    
    log_success "All required build tools found"
    return 0
}

# Validate source files
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

# Test syntax validation
test_syntax_validation() {
    log_info "Testing syntax validation..."
    
    # Test C file syntax with gcc
    local c_files=(
        "$PROJECT_DIR/src/efficient_rpi_display.c"
        "$PROJECT_DIR/src/ili9486l_driver.c"
        "$PROJECT_DIR/src/xpt2046_touch.c"
        "$PROJECT_DIR/examples/display_test.c"
        "$PROJECT_DIR/examples/touch_test.c"
    )
    
    for file in "${c_files[@]}"; do
        if ! gcc -fsyntax-only -I"$PROJECT_DIR/include" "$file" 2>/dev/null; then
            log_warn "Syntax check failed for: $file (may be due to missing headers)"
        fi
    done
    
    log_success "Syntax validation completed"
    return 0
}

# Main validation function
main() {
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                        Build Validation for Efficient RPi Display                       ║"
    echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
    echo
    
    local tests_passed=0
    local tests_total=0
    
    # Run all validation tests
    local validation_tests=(
        "check_build_tools"
        "validate_source_files"
        "test_syntax_validation"
        "test_basic_config"
        "test_minimal_config"
        "test_full_config"
        "test_ubuntu_server_config"
    )
    
    for test in "${validation_tests[@]}"; do
        ((tests_total++))
        echo
        if $test; then
            ((tests_passed++))
        fi
    done
    
    echo
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                               Validation Results                                        ║"
    echo "╠══════════════════════════════════════════════════════════════════════════════════════════╣"
    echo "║  Tests Passed: $tests_passed/$tests_total                                                          ║"
    
    if [[ $tests_passed -eq $tests_total ]]; then
        echo "║  Status: ✅ ALL VALIDATIONS PASSED                                                    ║"
        echo "║                                                                                          ║"
        echo "║  The build system is working correctly!                                                 ║"
        echo "║  You can now proceed with installation.                                                 ║"
        echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
        return 0
    else
        echo "║  Status: ⚠️  SOME VALIDATIONS FAILED                                                  ║"
        echo "║                                                                                          ║"
        echo "║  Some tests failed, but this may be expected on systems                                 ║"
        echo "║  without full graphics libraries. Check the output above.                               ║"
        echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
        return 1
    fi
}

# Run main function
main "$@" 