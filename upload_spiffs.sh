#!/bin/bash

# ESP32 SPIFFS Upload Script
# This script uploads files from the data/ folder to ESP32's SPIFFS

echo "ESP32 SPIFFS Upload Script"
echo "=========================="

# Check if data folder exists
if [ ! -d "data" ]; then
    echo "Error: data/ folder not found!"
    exit 1
fi

# Check if ESP32 is connected
if [ ! -e "/dev/cu.usbmodem1101" ]; then
    echo "Error: ESP32 not found at /dev/cu.usbmodem1101"
    echo "Available ports:"
    ls /dev/cu.*
    exit 1
fi

echo "Found ESP32 at /dev/cu.usbmodem1101"
echo "Creating SPIFFS image..."

# Create SPIFFS image
~/Library/Arduino15/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs -c data -b 4096 -p 256 -s 0x100000 spiffs.img

if [ $? -ne 0 ]; then
    echo "Error: Failed to create SPIFFS image"
    exit 1
fi

echo "SPIFFS image created successfully"
echo "Files included:"
ls -la data/

echo ""
echo "IMPORTANT: To upload SPIFFS, you need to:"
echo "1. Put ESP32 in download mode (hold BOOT button while pressing RESET)"
echo "2. Run this command:"
echo "   ~/Library/Arduino15/packages/esp32/tools/esptool_py/4.5.1/esptool --chip esp32 --port /dev/cu.usbmodem1101 --baud 921600 write_flash 0x291000 spiffs.img"
echo ""
echo "Or use Arduino IDE:"
echo "1. Open Arduino IDE"
echo "2. Go to Tools -> ESP32 Sketch Data Upload"
echo "3. This will automatically upload files from data/ folder to SPIFFS" 