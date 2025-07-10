# Makefile for Efficient RPi Display Driver
# Simple alternative to CMake build system

# Configuration
PREFIX ?= /usr/local
CC ?= gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -fPIC
LDFLAGS = -lm -lrt -lpthread

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
LIBDIR = lib
BINDIR = bin

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
HEADERS = $(wildcard $(INCDIR)/*.h)

# Library name
LIBNAME = libefficient_rpi_display
SHARED_LIB = $(LIBDIR)/$(LIBNAME).so.1.0.0
STATIC_LIB = $(LIBDIR)/$(LIBNAME).a

# Example programs
EXAMPLES = $(BINDIR)/display_test $(BINDIR)/touch_test $(BINDIR)/display_benchmark

# Default target
all: directories $(SHARED_LIB) $(STATIC_LIB) $(EXAMPLES) overlay

# Create directories
directories:
	@mkdir -p $(OBJDIR) $(LIBDIR) $(BINDIR)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# Create shared library
$(SHARED_LIB): $(OBJECTS)
	$(CC) -shared -Wl,-soname,$(LIBNAME).so.1 -o $@ $^ $(LDFLAGS)
	cd $(LIBDIR) && ln -sf $(LIBNAME).so.1.0.0 $(LIBNAME).so.1
	cd $(LIBDIR) && ln -sf $(LIBNAME).so.1 $(LIBNAME).so

# Create static library
$(STATIC_LIB): $(OBJECTS)
	ar rcs $@ $^

# Compile device tree overlay
overlay: efficient-rpi35-overlay.dtbo

efficient-rpi35-overlay.dtbo: overlays/efficient-rpi35-overlay.dts
	dtc -@ -I dts -O dtb -o $@ $<

# Example programs
$(BINDIR)/display_test: examples/display_test.c $(SHARED_LIB)
	$(CC) $(CFLAGS) -I$(INCDIR) -L$(LIBDIR) -o $@ $< -lefficient_rpi_display $(LDFLAGS)

$(BINDIR)/touch_test: examples/touch_test.c $(SHARED_LIB)
	$(CC) $(CFLAGS) -I$(INCDIR) -L$(LIBDIR) -o $@ $< -lefficient_rpi_display $(LDFLAGS)

$(BINDIR)/display_benchmark: examples/display_benchmark.c $(SHARED_LIB)
	$(CC) $(CFLAGS) -I$(INCDIR) -L$(LIBDIR) -o $@ $< -lefficient_rpi_display $(LDFLAGS)

# Install
install: all
	install -d $(PREFIX)/lib
	install -d $(PREFIX)/include/efficient_rpi_display
	install -d $(PREFIX)/bin
	install -d $(PREFIX)/share/efficient_rpi_display/overlays
	install -d /boot/overlays
	install -d /etc/udev/rules.d
	
	# Install libraries
	install -m 755 $(SHARED_LIB) $(PREFIX)/lib/
	install -m 644 $(STATIC_LIB) $(PREFIX)/lib/
	cd $(PREFIX)/lib && ln -sf $(LIBNAME).so.1.0.0 $(LIBNAME).so.1
	cd $(PREFIX)/lib && ln -sf $(LIBNAME).so.1 $(LIBNAME).so
	
	# Install headers
	install -m 644 $(HEADERS) $(PREFIX)/include/efficient_rpi_display/
	
	# Install binaries
	install -m 755 $(EXAMPLES) $(PREFIX)/bin/
	install -m 755 scripts/install.sh $(PREFIX)/bin/
	install -m 755 scripts/configure-display.sh $(PREFIX)/bin/
	install -m 755 scripts/calibrate-touch.sh $(PREFIX)/bin/
	
	# Install device tree overlay
	install -m 644 efficient-rpi35-overlay.dtbo $(PREFIX)/share/efficient_rpi_display/overlays/
	install -m 644 efficient-rpi35-overlay.dtbo /boot/overlays/
	
	# Install udev rules
	echo 'SUBSYSTEM=="spidev", KERNEL=="spidev0.0", MODE="0666", GROUP="spi"' > /etc/udev/rules.d/99-efficient-rpi-display.rules
	echo 'SUBSYSTEM=="spidev", KERNEL=="spidev0.1", MODE="0666", GROUP="spi"' >> /etc/udev/rules.d/99-efficient-rpi-display.rules
	echo 'SUBSYSTEM=="gpio", KERNEL=="gpio*", MODE="0666", GROUP="gpio"' >> /etc/udev/rules.d/99-efficient-rpi-display.rules
	
	# Update library cache
	ldconfig

# Uninstall
uninstall:
	rm -f $(PREFIX)/lib/$(LIBNAME).*
	rm -rf $(PREFIX)/include/efficient_rpi_display
	rm -f $(PREFIX)/bin/display_test
	rm -f $(PREFIX)/bin/touch_test
	rm -f $(PREFIX)/bin/display_benchmark
	rm -f $(PREFIX)/bin/install.sh
	rm -f $(PREFIX)/bin/configure-display.sh
	rm -f $(PREFIX)/bin/calibrate-touch.sh
	rm -rf $(PREFIX)/share/efficient_rpi_display
	rm -f /boot/overlays/efficient-rpi35-overlay.dtbo
	rm -f /etc/udev/rules.d/99-efficient-rpi-display.rules
	ldconfig

# Clean
clean:
	rm -rf $(OBJDIR) $(LIBDIR) $(BINDIR)
	rm -f efficient-rpi35-overlay.dtbo

# Clean all
distclean: clean
	rm -rf build

# Test
test: all
	@echo "Running basic functionality tests..."
	@echo "Note: These tests require actual hardware"
	@echo "Use 'make test-display' and 'make test-touch' on the Pi"

test-display: $(BINDIR)/display_test
	sudo LD_LIBRARY_PATH=$(LIBDIR) $(BINDIR)/display_test

test-touch: $(BINDIR)/touch_test
	sudo LD_LIBRARY_PATH=$(LIBDIR) $(BINDIR)/touch_test

# Help
help:
	@echo "Efficient RPi Display Driver Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all          Build libraries and examples"
	@echo "  install      Install to system (requires root)"
	@echo "  uninstall    Remove from system (requires root)"
	@echo "  clean        Clean build files"
	@echo "  distclean    Clean all files"
	@echo "  test         Run tests (requires hardware)"
	@echo "  test-display Test display functionality"
	@echo "  test-touch   Test touch functionality"
	@echo "  help         Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  PREFIX       Installation prefix (default: /usr/local)"
	@echo "  CC           C compiler (default: gcc)"
	@echo "  CFLAGS       Compiler flags"
	@echo "  LDFLAGS      Linker flags"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build everything"
	@echo "  make install            # Install to system"
	@echo "  make PREFIX=/usr        # Install to /usr"
	@echo "  make CC=clang           # Use clang compiler"
	@echo ""

.PHONY: all directories install uninstall clean distclean test test-display test-touch help overlay 