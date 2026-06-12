#!/bin/bash
# GuitarAcc Firmware Build Script
# Builds basestation or client firmware with automatic nRF Connect SDK environment setup

set -e  # Exit on error

PROJECT_ROOT="/Users/richmaes/src/guitaracc_test"
TARGET="${1:-basestation}"  # Default to basestation
BUILD_TYPE="${2:-}"  # Empty for incremental, "pristine" for clean

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "GuitarAcc Build Script"
echo "Target: $TARGET"
echo "Build Type: ${BUILD_TYPE:-incremental}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Step 1: Activate venv
echo ""
echo "→ Activating Python virtual environment..."
cd "$PROJECT_ROOT"
if [ -d "venv" ]; then
    source venv/bin/activate
    echo "✓ Virtual environment activated: $(which python)"
else
    echo "⚠ No venv found, continuing without it"
fi

# Step 2: Determine board target and app directory
if [ "$TARGET" = "basestation" ]; then
    BOARD="nrf5340_audio_dk/nrf5340/cpuapp"
    APP_DIR="basestation"
elif [ "$TARGET" = "client" ]; then
    BOARD="thingy53/nrf5340/cpuapp"
    APP_DIR="client"
else
    echo "✗ Unknown target: $TARGET"
    echo "Usage: $0 [basestation|client] [pristine]"
    exit 1
fi

# Step 3: Build using nrfutil toolchain-manager
echo ""
echo "→ Building $TARGET for $BOARD..."

BUILD_CMD="west build -d $APP_DIR/build -b $BOARD $APP_DIR"

# Check if build directory needs pristine build
if [ "$BUILD_TYPE" = "pristine" ] || [ ! -f "$APP_DIR/build/build.ninja" ]; then
    if [ "$BUILD_TYPE" != "pristine" ]; then
        echo "→ Build directory incomplete or corrupted, running pristine build..."
    else
        echo "→ Running pristine build (clean)..."
    fi
    BUILD_CMD="$BUILD_CMD --pristine"
else
    echo "→ Running incremental build..."
fi

# Use nrfutil to launch build with proper toolchain environment
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- $BUILD_CMD

# Step 4: Verify build
echo ""
echo "→ Verifying build artifacts..."
BUILD_DIR="$APP_DIR/build/$APP_DIR/zephyr"
if [ -f "$BUILD_DIR/zephyr.hex" ]; then
    SIZE=$(ls -lh "$BUILD_DIR/zephyr.hex" | awk '{print $5}')
    echo "✓ Build successful!"
    echo "  - zephyr.hex: $SIZE"
    echo "  - Location: $BUILD_DIR/"
else
    echo "✗ Build failed: zephyr.hex not found at $BUILD_DIR/"
    exit 1
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ Build complete!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Build artifacts:"
echo "  $BUILD_DIR/zephyr.hex"
echo "  $BUILD_DIR/zephyr.elf"
echo "  $BUILD_DIR/zephyr.bin"
echo ""
echo "To flash:"
echo "  nrfutil toolchain-manager launch --ncs-version v3.2.1 -- west flash -d $APP_DIR/build"
