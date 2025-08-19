#!/bin/bash

# ESP32-CAM Build and Upload Script
# Автоматическая сборка и загрузка прошивки с SPIFFS

set -e  # Exit on any error

echo "🔧 ESP32-CAM Build and Upload Script"
echo "===================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
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

# Parse command line arguments
CLEAN=false
UPLOAD_FIRMWARE=false
UPLOAD_SPIFFS=false
MONITOR=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -f|--firmware)
            UPLOAD_FIRMWARE=true
            shift
            ;;
        -s|--spiffs)
            UPLOAD_SPIFFS=true
            shift
            ;;
        -a|--all)
            CLEAN=true
            UPLOAD_FIRMWARE=true
            UPLOAD_SPIFFS=true
            shift
            ;;
        -m|--monitor)
            MONITOR=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -c, --clean     Clean build directory"
            echo "  -f, --firmware  Upload firmware only"
            echo "  -s, --spiffs    Upload SPIFFS only"
            echo "  -a, --all       Clean, build and upload everything"
            echo "  -m, --monitor   Start serial monitor after upload"
            echo "  -h, --help      Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0 --all                    # Full rebuild and upload"
            echo "  $0 --spiffs                 # Upload only SPIFFS"
            echo "  $0 --firmware --monitor     # Upload firmware and start monitor"
            exit 0
            ;;
        *)
            print_error "Unknown option $1"
            exit 1
            ;;
    esac
done

# If no options specified, default to SPIFFS only (most common case)
if [ "$CLEAN" = false ] && [ "$UPLOAD_FIRMWARE" = false ] && [ "$UPLOAD_SPIFFS" = false ]; then
    print_warning "No options specified, defaulting to SPIFFS upload only"
    UPLOAD_SPIFFS=true
fi

# Step 1: Clean if requested
if [ "$CLEAN" = true ]; then
    print_status "Cleaning build directory..."
    pio run --target clean
    print_success "Clean completed"
fi

# Step 2: Build project
print_status "Building project..."
pio run
print_success "Build completed"

# Step 3: Upload firmware if requested
if [ "$UPLOAD_FIRMWARE" = true ]; then
    print_status "Uploading firmware..."
    pio run --target upload
    print_success "Firmware upload completed"
fi

# Step 4: Upload SPIFFS if requested
if [ "$UPLOAD_SPIFFS" = true ]; then
    print_status "Uploading SPIFFS filesystem..."
    pio run --target uploadfs
    print_success "SPIFFS upload completed"
fi

# Step 5: Start monitor if requested
if [ "$MONITOR" = true ]; then
    print_status "Starting serial monitor (Ctrl+C to exit)..."
    pio device monitor
fi

print_success "All operations completed successfully! 🎉"
