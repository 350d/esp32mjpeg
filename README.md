# ESP32-CAM Advanced Control Panel

A professional-grade web interface for ESP32-CAM with advanced camera controls, real-time streaming, and multi-client support.

## ✨ Features

- **Real-time MJPEG streaming** with multi-client support (up to 6 clients)
- **Professional camera controls** organized by function:
  - Image Quality (JPEG quality, brightness, contrast, saturation)
  - Exposure & Gain (AEC, AGC, gain ceiling)
  - White Balance & Color (AWB, color effects)
  - Orientation & Geometry (mirror, flip, downsize)
  - Image Processing (pixel correction, gamma, lens correction)
  - Debug & Test tools
- **Modern responsive web UI** with real-time status updates
- **Cache-busting system** for instant web interface updates
- **Persistent settings** stored in NVS (Non-Volatile Storage)
- **System controls** (settings reset, device reboot)

## 🚀 Quick Start

### Hardware Requirements

- ESP32-CAM (AI-Thinker or compatible)
- MicroSD card (optional)
- USB-to-Serial adapter for programming

### Software Requirements

- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
- [Visual Studio Code](https://code.visualstudio.com/) with PlatformIO extension

### Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/yourusername/esp32-cam-advanced-control.git
   cd esp32-cam-advanced-control
   ```

2. **Configure WiFi credentials:**
   ```cpp
   // Edit include/credentials.h
   #define WIFI_SSID "YourWiFiNetwork"
   #define WIFI_PASSWORD "YourPassword"
   ```

3. **Build and upload:**
   ```bash
   # Quick SPIFFS update (most common)
   make spiffs
   
   # Full build and upload
   make full
   
   # Or using PlatformIO directly
   pio run --target upload
   pio run --target uploadfs
   ```

## 🛠️ Development

### Project Structure

```
├── data/                 # Web interface files (HTML, CSS, JS)
│   ├── index.html       # Main web interface
│   ├── css/main.css     # Styles
│   └── js/              # JavaScript files
├── include/             # Header files
│   ├── credentials.h    # WiFi configuration
│   ├── camera_pins.h    # Camera pin definitions
│   └── *.h             # Other headers
├── src/                 # Source code
│   ├── main.cpp         # Main application
│   ├── streaming.cpp    # Web server and streaming
│   └── *.cpp           # Other source files
├── Makefile            # Convenient build commands
└── build-and-upload.sh # Automated build script
```

### Quick Development Commands

```bash
# Update web interface only (fastest)
make spiffs

# Full rebuild and upload
make full

# Build only (no upload)
make build

# Start serial monitor
make monitor

# Show available commands
make help
```

### Advanced Build Script

```bash
# Upload SPIFFS only
./build-and-upload.sh --spiffs

# Full clean build and upload
./build-and-upload.sh --all

# Upload firmware and start monitor
./build-and-upload.sh --firmware --monitor
```

## 🎛️ Camera Controls

### Image Quality
- **JPEG Quality**: Compression level (4-63)
- **Brightness**: Image brightness (-2 to +2)
- **Contrast**: Image contrast (-2 to +2)
- **Saturation**: Color saturation (-2 to +2)

### Exposure & Gain
- **Auto Exposure Control (AEC)**: Automatic exposure adjustment
- **AE Level**: Exposure compensation (-2 to +2)
- **AEC Value**: Manual exposure value (0-1200)
- **AEC2**: DSP-based exposure control
- **Auto Gain Control (AGC)**: Automatic gain adjustment
- **AGC Gain**: Manual gain value (0-30)
- **Gain Ceiling**: Maximum gain limit (2x-128x)

### White Balance & Color
- **Auto White Balance (AWB)**: Automatic color temperature adjustment
- **AWB Gain**: White balance gain control
- **WB Mode**: Preset white balance modes (Auto, Sunny, Cloudy, Office, Home)
- **Special Effects**: Color effects (Negative, Grayscale, Sepia, etc.)

### Orientation & Geometry
- **Horizontal Mirror**: Mirror image horizontally
- **Vertical Flip**: Flip image vertically
- **DCW (Downsize)**: Enable image downscaling

### Image Processing
- **Bad Pixel Correction**: Remove defective pixels
- **White Pixel Correction**: Correct overexposed pixels
- **Gamma Correction**: Adjust gamma curve
- **Lens Correction**: Compensate for lens distortion

## 🌐 Web Interface

Access the camera at `http://[ESP32_IP_ADDRESS]` after successful connection.

### Features:
- **Real-time streaming** with automatic reconnection
- **Responsive design** works on desktop and mobile
- **Live status updates** showing connection state
- **Instant settings application** with visual feedback
- **Professional control layout** organized by function

## ⚙️ Configuration

### Camera Settings

Edit `include/camera_pins.h` for different ESP32-CAM variants:

```cpp
// AI-Thinker ESP32-CAM
#define CAMERA_MODEL_AI_THINKER

// Uncomment for other models:
// #define CAMERA_MODEL_WROVER_KIT
// #define CAMERA_MODEL_ESP_EYE
```

### Network Configuration

Edit `include/credentials.h`:

```cpp
#define WIFI_SSID "YourNetwork"
#define WIFI_PASSWORD "YourPassword"
#define HOSTNAME "esp32cam"
```

### Performance Tuning

Edit `platformio.ini` build flags:

```ini
build_flags = 
    -D MAX_CLIENTS=6          # Maximum concurrent clients
    -D JPEG_QUALITY=10        # Default JPEG quality
    -D FPS=30                 # Target frame rate
    -D FRAME_SIZE=FRAMESIZE_HD # Default resolution
```

## 🔧 Troubleshooting

### Common Issues

1. **Camera not detected:**
   - Check camera cable connection
   - Verify correct camera model in `camera_pins.h`

2. **Web interface not loading:**
   - Ensure SPIFFS was uploaded: `make spiffs`
   - Check serial monitor for IP address

3. **Poor streaming performance:**
   - Reduce JPEG quality or frame size
   - Check WiFi signal strength
   - Reduce number of concurrent clients

4. **Settings not saving:**
   - Check NVS partition in `platformio.ini`
   - Try "Clear Settings" button in web interface

### Serial Monitor

```bash
make monitor
# or
pio device monitor
```

Look for:
- WiFi connection status
- IP address assignment
- Camera initialization messages
- Error codes and debugging info

## 📱 Supported Devices

### ESP32-CAM Modules
- ✅ AI-Thinker ESP32-CAM
- ✅ ESP32-CAM-MB (with USB)
- ✅ ESP-EYE
- ✅ WROVER-CAM
- ✅ Freenove ESP32-WROVER-CAM

### Browsers
- ✅ Chrome/Chromium
- ✅ Firefox
- ✅ Safari
- ✅ Edge
- ✅ Mobile browsers

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature-name`
3. Make your changes
4. Test thoroughly
5. Submit a pull request

### Development Guidelines

- Follow existing code style
- Test on real hardware
- Update documentation
- Add comments for complex logic

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- ESP32 Arduino Core team
- PlatformIO development team
- ESP32-CAM community contributors

---

**Made with ❤️ for the ESP32 community**
