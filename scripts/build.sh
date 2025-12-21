#!/bin/bash
#
# Wrapper build script for Spresense security_camera project
# This script handles the external application directory build
#

set -e

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SPRESENSE_DIR="${PROJECT_ROOT}/spresense"
SDK_DIR="${SPRESENSE_DIR}/sdk"
NUTTX_DIR="${SPRESENSE_DIR}/nuttx"
EXTERNAL_APPS_DIR="${PROJECT_ROOT}/apps"

echo "=============================================="
echo "Spresense Security Camera Build Script"
echo "=============================================="
echo "Project root: ${PROJECT_ROOT}"
echo "Spresense SDK: ${SDK_DIR}"
echo "External Apps: ${EXTERNAL_APPS_DIR}"
echo "=============================================="

# Check if submodule is initialized
if [ ! -f "${SDK_DIR}/Makefile" ]; then
    echo "ERROR: Spresense submodule not initialized!"
    echo "Please run: git submodule update --init --recursive"
    exit 1
fi

# Check if toolchain is available
if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo "WARNING: arm-none-eabi-gcc not found in PATH"
    echo "Make sure toolchain is installed in ~/spresenseenv/usr/bin"
    echo "Setting PATH..."
    export PATH="${HOME}/spresenseenv/usr/bin:${PATH}"
fi

# Verify toolchain again
if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo "ERROR: Toolchain not found!"
    echo "Please install Spresense toolchain first"
    exit 1
fi

echo ""
echo "Toolchain version:"
arm-none-eabi-gcc --version | head -1
echo ""

# Set SPRESENSE_SDK environment variable for external apps
export SPRESENSE_SDK="${SDK_DIR}"

# Change to nuttx directory and build with external apps directory
cd "${NUTTX_DIR}"

# Set external apps directory
echo "Setting up external applications directory..."
export SPRESENSE_APPS="${EXTERNAL_APPS_DIR}"

# Build with external apps
echo ""
echo "Building with external applications from: ${EXTERNAL_APPS_DIR}"
echo ""

# Set PATH to include toolchain
export PATH="${HOME}/spresenseenv/usr/bin:/usr/bin:/bin:${PATH}"

# Run make with SPRESENSE_APPS environment variable
/usr/bin/make

# Check if nuttx.spk was created
if [ -f "${NUTTX_DIR}/nuttx.spk" ]; then
    echo ""
    echo "Copying nuttx.spk to sdk directory..."
    cp "${NUTTX_DIR}/nuttx.spk" "${SDK_DIR}/nuttx.spk"
    echo ""
    echo "=============================================="
    echo "Build successful!"
    echo "=============================================="
    echo "nuttx.spk location: ${SDK_DIR}/nuttx.spk"
    echo ""
    echo "To flash: ./scripts/flash.sh"
    echo "=============================================="
else
    echo "ERROR: nuttx.spk not found!"
    exit 1
fi
