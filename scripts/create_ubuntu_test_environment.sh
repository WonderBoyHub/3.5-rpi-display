#!/bin/bash

# Create Ubuntu Server 24.04.2 LTS Test Environment
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
DOCKERFILE_PATH="$PROJECT_DIR/Dockerfile.ubuntu-test"

# Create Dockerfile for testing
create_dockerfile() {
    log_info "Creating Dockerfile for Ubuntu Server 24.04.2 LTS test environment..."
    
    cat > "$DOCKERFILE_PATH" << 'EOF'
FROM ubuntu:24.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install base packages
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    wget \
    curl \
    sudo \
    && rm -rf /var/lib/apt/lists/*

# Create a test user
RUN useradd -m -s /bin/bash testuser && \
    echo "testuser ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Set working directory
WORKDIR /workspace

# Copy project files
COPY . /workspace/

# Change ownership to test user
RUN chown -R testuser:testuser /workspace

# Switch to test user
USER testuser

# Default command
CMD ["/bin/bash"]
EOF
    
    log_success "Dockerfile created at $DOCKERFILE_PATH"
}

# Build Docker image
build_docker_image() {
    log_info "Building Docker image for Ubuntu Server 24.04.2 LTS..."
    
    cd "$PROJECT_DIR"
    
    if ! docker build -f "$DOCKERFILE_PATH" -t efficient-rpi-display-test . 2>&1 | tee docker-build.log; then
        log_error "Docker image build failed"
        cat docker-build.log
        return 1
    fi
    
    log_success "Docker image built successfully"
    return 0
}

# Run tests in Docker container
run_docker_tests() {
    log_info "Running tests in Ubuntu Server 24.04.2 LTS container..."
    
    # Run the test script inside the container
    if ! docker run --rm -v "$PROJECT_DIR:/workspace" efficient-rpi-display-test \
        bash -c "cd /workspace && ./scripts/test_build_ubuntu_server.sh" 2>&1 | tee docker-test.log; then
        log_error "Docker tests failed"
        cat docker-test.log
        return 1
    fi
    
    log_success "Docker tests completed successfully"
    return 0
}

# Test with graphics libraries
test_with_graphics_libraries() {
    log_info "Testing with graphics libraries in Ubuntu Server container..."
    
    # Create a modified Dockerfile with graphics libraries
    cat > "$DOCKERFILE_PATH.graphics" << 'EOF'
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install base packages + graphics libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    wget \
    curl \
    sudo \
    libdrm-dev \
    libgbm-dev \
    libegl1-mesa-dev \
    libgles2-mesa-dev \
    libwayland-dev \
    wayland-protocols \
    device-tree-compiler \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash testuser && \
    echo "testuser ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

WORKDIR /workspace
COPY . /workspace/
RUN chown -R testuser:testuser /workspace
USER testuser

CMD ["/bin/bash"]
EOF
    
    # Build image with graphics libraries
    if ! docker build -f "$DOCKERFILE_PATH.graphics" -t efficient-rpi-display-test-graphics . 2>&1 | tee docker-graphics-build.log; then
        log_error "Docker graphics image build failed"
        return 1
    fi
    
    # Run tests with graphics libraries
    if ! docker run --rm -v "$PROJECT_DIR:/workspace" efficient-rpi-display-test-graphics \
        bash -c "cd /workspace && ./scripts/test_build_ubuntu_server.sh" 2>&1 | tee docker-graphics-test.log; then
        log_error "Docker graphics tests failed"
        return 1
    fi
    
    log_success "Docker graphics tests completed successfully"
    return 0
}

# Run native tests (if on Ubuntu)
run_native_tests() {
    log_info "Running native tests..."
    
    if [[ -f /etc/os-release ]]; then
        source /etc/os-release
        if [[ "$ID" == "ubuntu" ]]; then
            log_info "Running on Ubuntu $VERSION_ID - executing native tests"
            cd "$PROJECT_DIR"
            if ! ./scripts/test_build_ubuntu_server.sh 2>&1 | tee native-test.log; then
                log_error "Native tests failed"
                return 1
            fi
            log_success "Native tests completed successfully"
            return 0
        else
            log_info "Not running on Ubuntu - skipping native tests"
            return 0
        fi
    else
        log_warn "Cannot detect OS - skipping native tests"
        return 0
    fi
}

# Cleanup function
cleanup() {
    log_info "Cleaning up test files..."
    rm -f "$DOCKERFILE_PATH" "$DOCKERFILE_PATH.graphics"
    rm -f "$PROJECT_DIR/docker-*.log" "$PROJECT_DIR/native-test.log"
}

# Check if Docker is available
check_docker() {
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed or not in PATH"
        log_info "Please install Docker to run containerized tests"
        return 1
    fi
    
    if ! docker info &> /dev/null; then
        log_error "Docker daemon is not running or not accessible"
        log_info "Please start Docker daemon or check permissions"
        return 1
    fi
    
    log_success "Docker is available and running"
    return 0
}

# Main function
main() {
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                Ubuntu Server 24.04.2 LTS Test Environment Creator                      ║"
    echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
    echo
    
    local mode="${1:-all}"
    
    case "$mode" in
        "docker")
            if check_docker; then
                create_dockerfile
                build_docker_image
                run_docker_tests
                test_with_graphics_libraries
            fi
            ;;
        "native")
            run_native_tests
            ;;
        "all"|*)
            log_info "Running comprehensive test suite..."
            
            # Run native tests first if possible
            run_native_tests || log_warn "Native tests skipped or failed"
            
            # Run Docker tests if available
            if check_docker; then
                create_dockerfile
                build_docker_image
                run_docker_tests
                test_with_graphics_libraries
            else
                log_warn "Docker tests skipped - Docker not available"
            fi
            ;;
    esac
    
    echo
    log_success "Test environment setup and execution completed!"
    
    # Show results summary
    echo
    echo "╔══════════════════════════════════════════════════════════════════════════════════════════╗"
    echo "║                                Test Summary                                              ║"
    echo "╠══════════════════════════════════════════════════════════════════════════════════════════╣"
    
    if [[ -f "$PROJECT_DIR/native-test.log" ]]; then
        if grep -q "ALL TESTS PASSED" "$PROJECT_DIR/native-test.log"; then
            echo "║  Native Ubuntu Tests: ✅ PASSED                                                      ║"
        else
            echo "║  Native Ubuntu Tests: ❌ FAILED                                                      ║"
        fi
    else
        echo "║  Native Ubuntu Tests: ⏭️  SKIPPED                                                       ║"
    fi
    
    if [[ -f "$PROJECT_DIR/docker-test.log" ]]; then
        if grep -q "ALL TESTS PASSED" "$PROJECT_DIR/docker-test.log"; then
            echo "║  Docker Basic Tests:  ✅ PASSED                                                      ║"
        else
            echo "║  Docker Basic Tests:  ❌ FAILED                                                      ║"
        fi
    else
        echo "║  Docker Basic Tests:  ⏭️  SKIPPED                                                       ║"
    fi
    
    if [[ -f "$PROJECT_DIR/docker-graphics-test.log" ]]; then
        if grep -q "ALL TESTS PASSED" "$PROJECT_DIR/docker-graphics-test.log"; then
            echo "║  Docker Graphics Tests: ✅ PASSED                                                    ║"
        else
            echo "║  Docker Graphics Tests: ❌ FAILED                                                    ║"
        fi
    else
        echo "║  Docker Graphics Tests: ⏭️  SKIPPED                                                     ║"
    fi
    
    echo "╚══════════════════════════════════════════════════════════════════════════════════════════╝"
    
    # Cleanup unless requested to keep files
    if [[ "$2" != "--keep-files" ]]; then
        cleanup
    else
        log_info "Test files kept as requested"
    fi
}

# Trap cleanup on exit
trap cleanup EXIT

# Run main function
main "$@" 