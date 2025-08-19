#!/bin/bash

# ESP32-CAM Advanced Control Panel - Setup Script
# This script helps you set up the project for first use

set -e

echo "🚀 ESP32-CAM Advanced Control Panel Setup"
echo "=========================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're in the right directory
if [ ! -f "platformio.ini" ]; then
    print_error "platformio.ini not found. Please run this script from the project root directory."
    exit 1
fi

print_status "Setting up ESP32-CAM project..."

# Step 1: Create credentials file if it doesn't exist
if [ ! -f "include/credentials.h" ]; then
    print_status "Creating credentials.h from template..."
    cp include/credentials.h.example include/credentials.h
    print_warning "Please edit include/credentials.h with your WiFi credentials!"
    echo ""
    echo "You need to update the following in include/credentials.h:"
    echo "  - WIFI_SSID: Your WiFi network name"
    echo "  - WIFI_PWD: Your WiFi password"
    echo ""
else
    print_success "credentials.h already exists"
fi

# Step 2: Check for PlatformIO
if ! command -v pio &> /dev/null; then
    print_error "PlatformIO CLI not found!"
    echo ""
    echo "Please install PlatformIO:"
    echo "  1. Install VS Code: https://code.visualstudio.com/"
    echo "  2. Install PlatformIO extension"
    echo "  3. Or install PlatformIO Core: https://platformio.org/install/cli"
    echo ""
    exit 1
else
    print_success "PlatformIO CLI found"
fi

# Step 3: Install dependencies
print_status "Installing project dependencies..."
pio pkg install

# Step 4: Build the project
print_status "Building project..."
pio run

print_success "Setup completed successfully!"
echo ""
echo "🎯 Next steps:"
echo "1. Edit include/credentials.h with your WiFi credentials"
echo "2. Connect your ESP32-CAM via USB-to-Serial adapter"
echo "3. Upload firmware and filesystem:"
echo "   make full"
echo "   # or"
echo "   pio run --target upload && pio run --target uploadfs"
echo ""
echo "4. Open serial monitor to see the IP address:"
echo "   make monitor"
echo "   # or"
echo "   pio device monitor"
echo ""
echo "5. Access web interface at http://[ESP32_IP_ADDRESS]"
echo ""
echo "📚 For more information, see README.md"
echo ""
print_success "Happy coding! 🎉"
