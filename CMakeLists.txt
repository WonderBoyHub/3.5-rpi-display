cmake_minimum_required(VERSION 3.16)
project(efficient_rpi_display VERSION 2.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Modern feature options
option(ENABLE_DRM_KMS "Enable DRM/KMS support" ON)
option(ENABLE_GPU_ACCELERATION "Enable GPU acceleration" ON)
option(ENABLE_WAYLAND "Enable Wayland support" ON)
option(ENABLE_HUGE_PAGES "Enable huge pages support" ON)
option(ENABLE_V3D_SUPPORT "Enable V3D GPU support" ON)
option(TARGET_PI5 "Target Raspberry Pi 5" OFF)

# Compiler flags
set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -DNDEBUG_PERFORMANCE")

# Add modern feature flags
if(ENABLE_DRM_KMS)
    add_definitions(-DENABLE_DRM_KMS=1)
endif()

if(ENABLE_GPU_ACCELERATION)
    add_definitions(-DENABLE_GPU_ACCELERATION=1)
endif()

if(ENABLE_WAYLAND)
    add_definitions(-DENABLE_WAYLAND=1)
endif()

if(ENABLE_HUGE_PAGES)
    add_definitions(-DENABLE_HUGE_PAGES=1)
endif()

if(ENABLE_V3D_SUPPORT)
    add_definitions(-DENABLE_V3D_SUPPORT=1)
endif()

if(TARGET_PI5)
    add_definitions(-DTARGET_PI5=1)
endif()

# Cross-compilation support
if(CROSS_COMPILE)
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    
    # Set toolchain
    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
    
    # Set sysroot if provided
    if(SYSROOT)
        set(CMAKE_SYSROOT ${SYSROOT})
    endif()
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# Check for required libraries
find_library(MATH_LIB m)
find_library(RT_LIB rt)

# Modern graphics libraries
if(ENABLE_DRM_KMS)
    pkg_check_modules(DRM libdrm)
    pkg_check_modules(GBM gbm)
    if(DRM_FOUND AND GBM_FOUND)
        message(STATUS "Found libdrm: ${DRM_VERSION}")
        message(STATUS "Found GBM: ${GBM_VERSION}")
        include_directories(${DRM_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
        link_directories(${DRM_LIBRARY_DIRS} ${GBM_LIBRARY_DIRS})
        add_definitions(${DRM_CFLAGS_OTHER} ${GBM_CFLAGS_OTHER})
        add_definitions(-DENABLE_DRM_KMS=1)
    else()
        message(WARNING "libdrm or GBM not found, disabling DRM/KMS support")
        set(ENABLE_DRM_KMS OFF)
    endif()
endif()

if(ENABLE_GPU_ACCELERATION)
    pkg_check_modules(EGL egl)
    pkg_check_modules(GLES2 glesv2)
    if(EGL_FOUND AND GLES2_FOUND)
        message(STATUS "Found EGL: ${EGL_VERSION}")
        message(STATUS "Found GLES2: ${GLES2_VERSION}")
        include_directories(${EGL_INCLUDE_DIRS} ${GLES2_INCLUDE_DIRS})
        link_directories(${EGL_LIBRARY_DIRS} ${GLES2_LIBRARY_DIRS})
        add_definitions(${EGL_CFLAGS_OTHER} ${GLES2_CFLAGS_OTHER})
        add_definitions(-DENABLE_GPU_ACCELERATION=1)
    else()
        message(WARNING "EGL or GLES2 not found, disabling GPU acceleration")
        set(ENABLE_GPU_ACCELERATION OFF)
    endif()
endif()

if(ENABLE_WAYLAND)
    pkg_check_modules(WAYLAND wayland-client wayland-server wayland-egl)
    if(WAYLAND_FOUND)
        message(STATUS "Found Wayland: ${WAYLAND_VERSION}")
        include_directories(${WAYLAND_INCLUDE_DIRS})
        link_directories(${WAYLAND_LIBRARY_DIRS})
        add_definitions(${WAYLAND_CFLAGS_OTHER})
        add_definitions(-DENABLE_WAYLAND=1)
    else()
        message(WARNING "Wayland not found, disabling Wayland support")
        set(ENABLE_WAYLAND OFF)
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Source files
set(DISPLAY_SOURCES
    src/ili9486l_driver.c
    src/xpt2046_touch.c
    src/efficient_rpi_display.c
)

# Add modern sources conditionally
if(ENABLE_DRM_KMS)
    list(APPEND DISPLAY_SOURCES src/modern_drm_interface.c)
endif()

# Header files
set(DISPLAY_HEADERS
    include/efficient_rpi_display.h
    include/ili9486l_driver.h
    include/xpt2046_touch.h
    include/display_context.h
)

# Add modern headers conditionally
if(ENABLE_DRM_KMS)
    list(APPEND DISPLAY_HEADERS include/modern_drm_interface.h)
endif()

# Create shared library
add_library(efficient_rpi_display SHARED ${DISPLAY_SOURCES})

# Build library link list
set(LINK_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}
    ${MATH_LIB}
    ${RT_LIB}
)

# Add modern graphics libraries
if(ENABLE_DRM_KMS)
    list(APPEND LINK_LIBRARIES ${DRM_LIBRARIES} ${GBM_LIBRARIES})
endif()

if(ENABLE_GPU_ACCELERATION)
    list(APPEND LINK_LIBRARIES ${EGL_LIBRARIES} ${GLES2_LIBRARIES})
endif()

if(ENABLE_WAYLAND)
    list(APPEND LINK_LIBRARIES ${WAYLAND_LIBRARIES})
endif()

target_link_libraries(efficient_rpi_display ${LINK_LIBRARIES})

# Create static library
add_library(efficient_rpi_display_static STATIC ${DISPLAY_SOURCES})
target_link_libraries(efficient_rpi_display_static ${LINK_LIBRARIES})

# Set library properties
set_target_properties(efficient_rpi_display PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${DISPLAY_HEADERS}"
)

set_target_properties(efficient_rpi_display_static PROPERTIES
    OUTPUT_NAME efficient_rpi_display
    PUBLIC_HEADER "${DISPLAY_HEADERS}"
)

# Compile device tree overlay
find_program(DTC_COMPILER dtc)
if(DTC_COMPILER)
    add_custom_target(overlay
        COMMAND ${DTC_COMPILER} -@ -I dts -O dtb -o efficient-rpi35-overlay.dtbo overlays/efficient-rpi35-overlay.dts
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Compiling device tree overlay"
    )
    
    add_dependencies(efficient_rpi_display overlay)
else()
    message(WARNING "Device tree compiler (dtc) not found. Device tree overlay will not be compiled.")
endif()

# Installation
include(GNUInstallDirs)

# Install libraries
install(TARGETS efficient_rpi_display efficient_rpi_display_static
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/efficient_rpi_display
)

# Install device tree overlay
if(DTC_COMPILER)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/efficient-rpi35-overlay.dtbo
        DESTINATION ${CMAKE_INSTALL_DATADIR}/efficient_rpi_display/overlays
    )
endif()

# Install scripts
install(PROGRAMS
    scripts/install.sh
    scripts/configure-display.sh
    scripts/calibrate-touch.sh
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install configuration files
install(FILES
    scripts/99-efficient-rpi-display.rules
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/udev/rules.d
)

# Create pkg-config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/efficient_rpi_display.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/efficient_rpi_display.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/efficient_rpi_display.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Example programs
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    # Simple test program
    add_executable(display_test examples/display_test.c)
    target_link_libraries(display_test efficient_rpi_display)
    
    # Touch test program  
    add_executable(touch_test examples/touch_test.c)
    target_link_libraries(touch_test efficient_rpi_display)
    
    # Benchmark program
    add_executable(display_benchmark examples/display_benchmark.c)
    target_link_libraries(display_benchmark efficient_rpi_display)
    
    # Install examples
    install(TARGETS display_test touch_test display_benchmark
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Testing
option(BUILD_TESTS "Build test suite" OFF)
if(BUILD_TESTS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
        enable_testing()
        add_subdirectory(test)
    else()
        message(WARNING "BUILD_TESTS is enabled but test/CMakeLists.txt not found. Skipping tests.")
    endif()
endif()

# Python bindings
option(BUILD_PYTHON "Build Python bindings" OFF)
if(BUILD_PYTHON)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    
    # Create Python extension
    add_library(py_efficient_rpi_display MODULE python/efficient_rpi_display_python.c)
    target_link_libraries(py_efficient_rpi_display efficient_rpi_display Python3::Python)
    
    # Set Python module properties
    set_target_properties(py_efficient_rpi_display PROPERTIES
        PREFIX ""
        OUTPUT_NAME "efficient_rpi_display"
        SUFFIX ".so"
    )
    
    # Install Python module
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import site; print(site.getsitepackages()[0])"
        OUTPUT_VARIABLE PYTHON_SITE_PACKAGES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    install(TARGETS py_efficient_rpi_display
        LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES}
    )
endif()

# Documentation
option(BUILD_DOCS "Build documentation" OFF)
if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in 
                       ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
        
        add_custom_target(docs
            ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
        )
    else()
        message(WARNING "Doxygen not found. Documentation will not be built.")
    endif()
endif()

# Package configuration
set(CPACK_PACKAGE_NAME "efficient-rpi-display")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Efficient 3.5\" RPi Display Driver")
set(CPACK_PACKAGE_DESCRIPTION "Modern, optimized driver for 3.5\" Raspberry Pi displays with ILI9486L controller and XPT2046 touch")
set(CPACK_PACKAGE_CONTACT "Efficient RPi Display Team")
set(CPACK_PACKAGE_VENDOR "Efficient RPi Display Project")

# Debian package configuration
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.17), libpthread-stubs0-dev")
set(CPACK_DEBIAN_PACKAGE_SECTION "electronics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# RPM package configuration
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc >= 2.17, glibc-devel")

include(CPack)

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Cross Compile: ${CROSS_COMPILE}")
message(STATUS "  Build Examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "  Build Python: ${BUILD_PYTHON}")
message(STATUS "  Build Docs: ${BUILD_DOCS}")
message(STATUS "  DTC Available: ${DTC_COMPILER}")
message(STATUS "")
message(STATUS "Modern Features:")
message(STATUS "  DRM/KMS Support: ${ENABLE_DRM_KMS}")
message(STATUS "  GPU Acceleration: ${ENABLE_GPU_ACCELERATION}")
message(STATUS "  Wayland Support: ${ENABLE_WAYLAND}")
message(STATUS "  Huge Pages: ${ENABLE_HUGE_PAGES}")
message(STATUS "  V3D Support: ${ENABLE_V3D_SUPPORT}")
message(STATUS "  Target Pi5: ${TARGET_PI5}")
message(STATUS "") 