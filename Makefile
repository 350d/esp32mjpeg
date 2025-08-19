# ESP32-CAM Makefile
# Convenient commands for development

.PHONY: help clean build upload spiffs full monitor test info

# Default target
help:
	@echo "ESP32-CAM Development Commands"
	@echo "=============================="
	@echo ""
	@echo "Available commands:"
	@echo "  make help      - Show this help message"
	@echo "  make clean     - Clean build directory"
	@echo "  make build     - Build project only"
	@echo "  make upload    - Upload firmware only"
	@echo "  make spiffs    - Upload SPIFFS filesystem only (most common)"
	@echo "  make full      - Clean, build, upload firmware and SPIFFS"
	@echo "  make monitor   - Start serial monitor"
	@echo "  make test      - Build and test (no upload)"
	@echo "  make info      - Show project info"
	@echo ""
	@echo "Quick examples:"
	@echo "  make spiffs    # Update web files after HTML/CSS/JS changes"
	@echo "  make full      # Complete rebuild and upload"

# Clean build directory
clean:
	@echo "🧹 Cleaning build directory..."
	pio run --target clean

# Build project
build:
	@echo "🔨 Building project..."
	pio run

# Upload firmware only
upload: build
	@echo "📤 Uploading firmware..."
	pio run --target upload

# Upload SPIFFS only (most common for web development)
spiffs: build
	@echo "📁 Uploading SPIFFS filesystem..."
	pio run --target uploadfs
	@echo "✅ SPIFFS updated! Web interface should now show latest changes."

# Full rebuild and upload everything
full: clean build upload spiffs
	@echo "🎉 Full update completed!"

# Start serial monitor
monitor:
	@echo "📺 Starting serial monitor (Ctrl+C to exit)..."
	pio device monitor

# Build and test without uploading
test: clean build
	@echo "✅ Build test completed successfully!"

# Show project information
info:
	@echo "📋 Project Information"
	@echo "====================="
	@pio project config
	@echo ""
	@echo "Available environments:"
	@pio run --list-targets

# Quick development cycle: build and upload SPIFFS
dev: spiffs
	@echo "🚀 Development update completed!"

# Emergency: erase flash completely and upload everything
reset:
	@echo "⚠️  WARNING: This will erase ALL flash memory!"
	@read -p "Are you sure? (y/N): " confirm && [ "$$confirm" = "y" ] || exit 1
	pio run --target erase
	$(MAKE) full
	@echo "🔄 Flash reset and full upload completed!"
