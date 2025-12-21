#!/bin/bash
#
# Wrapper flash script for Spresense security_camera project
#

set -e

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SPRESENSE_DIR="${PROJECT_ROOT}/spresense"
SDK_DIR="${SPRESENSE_DIR}/sdk"

# Default device
DEVICE="${1:-/dev/ttyUSB0}"

echo "=============================================="
echo "Spresense Security Camera Flash Script"
echo "=============================================="
echo "Device: ${DEVICE}"
echo "=============================================="

# Check if nuttx.spk exists
if [ ! -f "${SDK_DIR}/nuttx.spk" ]; then
    echo "ERROR: nuttx.spk not found!"
    echo "Please build first: ./scripts/build.sh"
    exit 1
fi

# Check if device exists
if [ ! -e "${DEVICE}" ]; then
    echo "ERROR: Device ${DEVICE} not found!"
    echo "Usage: $0 [device]"
    echo "Example: $0 /dev/ttyUSB0"
    exit 1
fi

# Load CP2102 driver if needed
if ! lsmod | grep -q cp210x; then
    echo "Loading CP2102 driver..."
    sudo modprobe cp210x
fi

# Flash using SDK tools
cd "${SDK_DIR}"
echo ""
echo "Flashing..."
sudo ./tools/flash.sh -c "${DEVICE}" nuttx.spk

echo ""
echo "=============================================="
echo "Flash completed successfully!"
echo "=============================================="
echo ""
echo "Next steps:"
echo "1. Connect to Spresense console:"
echo "   minicom (configured for ${DEVICE} 115200bps)"
echo "2. Run application:"
echo "   nsh> sercon"
echo "   nsh> security_camera"
echo "=============================================="
