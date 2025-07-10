# 📋 Pi 5 Display Driver Cheat Sheet

## ⚡ Quick Commands

### Installation
```bash
# One-line install
curl -sSL https://raw.githubusercontent.com/your-repo/efficient-rpi-display/main/install.sh | sudo bash

# Manual install
git clone https://github.com/your-repo/efficient-rpi-display.git
cd efficient-rpi-display && sudo ./scripts/enhanced_install.sh
```

### Testing
```bash
sudo display_test                    # Test display
sudo performance_monitor             # Real-time monitoring  
sudo display_benchmark              # Run benchmarks
sudo touch_test                     # Test touch
```

### Configuration
```bash
sudo configure-display.sh           # Interactive config
sudo configure-display.sh --pi5     # Pi 5 optimizations
sudo calibrate-touch.sh            # Touch calibration
```

### Troubleshooting
```bash
./scripts/pi5_compatibility_check.sh  # Check compatibility
ls /dev/spi*                          # Check SPI enabled
ls /dev/dri/                          # Check GPU driver
sudo systemctl status display-driver  # Check service
```

---

## 🔧 Pi 5 Boot Config

Add to `/boot/firmware/config.txt`:
```ini
# Essential settings
dtparam=spi=on
dtoverlay=vc4-kms-v3d,composite
dtoverlay=efficient-rpi35,rotate=0,speed=80000000

# Performance settings
gpu_mem=128
max_framebuffers=2
transparent_hugepage=madvise
```

---

## 🚨 Quick Fixes

### Black Screen
```bash
# Check connections and run
sudo display_test
# If fails, check wiring diagram
```

### No Touch
```bash
sudo calibrate-touch.sh
# Then test with
sudo touch_test
```

### Poor Performance
```bash
sudo configure-display.sh --optimize --pi5
sudo performance_monitor  # Check results
```

### Installation Fails
```bash
# Check compatibility first
./scripts/pi5_compatibility_check.sh
# Fix issues, then retry
sudo ./scripts/enhanced_install.sh
```

---

## 🔌 Hardware Wiring

| Pin | GPIO | Function |
|-----|------|----------|
| VCC | 3.3V | Power |
| GND | GND | Ground |
| CS | GPIO 8 | SPI CS |
| RST | GPIO 25 | Reset |
| DC | GPIO 24 | Data/Cmd |
| MOSI | GPIO 10 | Data |
| SCK | GPIO 11 | Clock |
| T_IRQ | GPIO 17 | Touch |

---

## 📊 Performance Targets

| Metric | Pi 5 Target | Pi 4 Target |
|--------|-------------|-------------|
| FPS | 60+ | 30+ |
| Latency | <5ms | <10ms |
| CPU Usage | <30% | <50% |
| GPU Usage | 40-80% | N/A |

---

## 🆘 Emergency Recovery

### Reset to Defaults
```bash
sudo rm /boot/firmware/config.txt.efficient-rpi-backup
sudo cp /boot/firmware/config.txt.efficient-rpi-backup /boot/firmware/config.txt
sudo reboot
```

### Uninstall Driver
```bash
sudo rm /usr/local/lib/libefficient_rpi_display*
sudo rm /usr/local/bin/{display_test,performance_monitor}
sudo rm /boot/overlays/efficient-rpi35-overlay.dtbo
```

### Get Help
1. Run: `./scripts/pi5_compatibility_check.sh`
2. Check: [QUICK_START.md](QUICK_START.md)
3. Report: [GitHub Issues](https://github.com/your-repo/efficient-rpi-display/issues) 