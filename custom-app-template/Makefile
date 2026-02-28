# Standalone Makefile for building outside retro-go workspace
# Requires ESP-IDF to be set up (idf.py must be in PATH)

# Project name
PROJECT_NAME := my-custom-app

# Build target (esp32, esp32s3, etc.)
# Default to esp32s3 for jaf1 compatibility
TARGET ?= esp32s3

# Path to retro-go (modify if standalone)
RETRO_GO_PATH ?= $(shell pwd)/..

# Flash settings
FLASH_PORT ?= /dev/ttyUSB0
FLASH_BAUD ?= 921600

.PHONY: all build flash monitor clean menuconfig fullclean

# Default target
all: build

# Build the project
build:
	@echo "Building $(PROJECT_NAME) for $(TARGET)..."
	IDF_TARGET=$(TARGET) idf.py -DRETRO_GO_PATH=$(RETRO_GO_PATH) build

# Flash to device
flash: build
	@echo "Flashing to $(FLASH_PORT)..."
	IDF_TARGET=$(TARGET) idf.py -p $(FLASH_PORT) -b $(FLASH_BAUD) flash

# Open serial monitor
monitor:
	IDF_TARGET=$(TARGET) idf.py -p $(FLASH_PORT) monitor

# Flash and monitor in one command
flash-monitor: build
	IDF_TARGET=$(TARGET) idf.py -p $(FLASH_PORT) -b $(FLASH_BAUD) flash monitor

# Open configuration menu
menuconfig:
	IDF_TARGET=$(TARGET) idf.py menuconfig

# Clean build artifacts
clean:
	IDF_TARGET=$(TARGET) idf.py clean

# Full clean including sdkconfig
fullclean:
	IDF_TARGET=$(TARGET) idf.py fullclean

# Set target chip
set-target:
	IDF_TARGET=$(TARGET) idf.py set-target $(TARGET)

# Get app size
size:
	IDF_TARGET=$(TARGET) idf.py size

# Help
help:
	@echo "Available targets:"
	@echo "  make build          - Build the project"
	@echo "  make flash          - Flash to device"
	@echo "  make monitor        - Open serial monitor"
	@echo "  make flash-monitor  - Flash and monitor"
	@echo "  make menuconfig     - Open configuration menu"
	@echo "  make clean          - Clean build files"
	@echo "  make fullclean      - Full clean including config"
	@echo "  make set-target     - Set target chip (TARGET=esp32s3)"
	@echo "  make size           - Show binary size analysis"
	@echo ""
	@echo "Environment variables:"
	@echo "  TARGET              - Target chip (default: esp32)"
	@echo "  FLASH_PORT          - Serial port (default: /dev/ttyUSB0)"
	@echo "  FLASH_BAUD          - Baud rate (default: 921600)"
	@echo "  RETRO_GO_PATH       - Path to retro-go (default: ../)"
