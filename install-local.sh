#!/bin/bash
# Quick setup script for local ESPHome testing

echo "🚀 Setting up Fingerprint Doorbell for local ESPHome testing..."

# Detect ESPHome config directory
if [ -d "/config/esphome" ]; then
    ESPHOME_DIR="/config/esphome"
    echo "✅ Found Home Assistant ESPHome directory: $ESPHOME_DIR"
elif [ -d "$HOME/.esphome" ]; then
    ESPHOME_DIR="$HOME/.esphome"
    echo "✅ Found standalone ESPHome directory: $ESPHOME_DIR"
else
    echo "❌ ESPHome directory not found!"
    echo "Please specify your ESPHome config directory:"
    read -p "ESPHome directory path: " ESPHOME_DIR
fi

# Create directories
echo "📁 Creating directories..."
mkdir -p "$ESPHOME_DIR/components"
mkdir -p "$ESPHOME_DIR/packages"

# Copy component files
echo "📦 Copying component files..."
cp -r components/fingerprint_doorbell "$ESPHOME_DIR/components/"

# Copy package YAML
echo "📄 Copying package configuration..."
cp fingerprint-doorbell.yaml "$ESPHOME_DIR/packages/"

# Copy example config
echo "📝 Copying example configuration..."
cp example-config.yaml "$ESPHOME_DIR/fingerprint-doorbell-example.yaml"

echo ""
echo "✅ Setup complete!"
echo ""
echo "📋 Next steps:"
echo "1. Edit $ESPHOME_DIR/fingerprint-doorbell-example.yaml"
echo "2. Update WiFi credentials and API key"
echo "3. Run: esphome run $ESPHOME_DIR/fingerprint-doorbell-example.yaml"
echo ""
echo "📚 For more info, see QUICKSTART.md"
