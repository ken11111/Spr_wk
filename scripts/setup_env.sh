#!/bin/bash
#
# Environment setup script for Spresense security_camera project
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "=============================================="
echo "Spresense Security Camera Environment Setup"
echo "=============================================="

# Check if running in WSL2
if grep -qi microsoft /proc/version; then
    echo "✓ Detected WSL2 environment"
else
    echo "WARNING: Not running in WSL2"
fi

echo ""
echo "Checking dependencies..."

# Check for required packages
REQUIRED_PACKAGES="build-essential python3 python3-serial git kconfig-frontends gperf libncurses5-dev flex bison genromfs xxd minicom"
MISSING_PACKAGES=""

for pkg in ${REQUIRED_PACKAGES}; do
    if ! dpkg -l | grep -q "^ii  ${pkg}"; then
        MISSING_PACKAGES="${MISSING_PACKAGES} ${pkg}"
    fi
done

if [ -n "${MISSING_PACKAGES}" ]; then
    echo "Missing packages:${MISSING_PACKAGES}"
    echo ""
    read -p "Install missing packages? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        sudo apt-get update
        sudo apt-get install -y ${MISSING_PACKAGES}
    fi
else
    echo "✓ All required packages installed"
fi

# Check for Spresense toolchain
echo ""
echo "Checking Spresense toolchain..."
if [ -d "${HOME}/spresenseenv/usr/bin" ]; then
    echo "✓ Toolchain found at ${HOME}/spresenseenv/usr/bin"
    if [ -f "${HOME}/spresenseenv/usr/bin/arm-none-eabi-gcc" ]; then
        echo "✓ GCC toolchain installed"
        "${HOME}/spresenseenv/usr/bin/arm-none-eabi-gcc" --version | head -1
    else
        echo "WARNING: GCC not found in toolchain directory"
    fi
else
    echo "✗ Toolchain not found"
    echo ""
    echo "Please install Spresense toolchain:"
    echo "1. Download from: https://developer.sony.com/develop/spresense/downloads-en"
    echo "2. Extract to ${HOME}/spresenseenv/"
    echo "3. Or run the install-tools.sh script from Spresense SDK"
fi

# Initialize submodule if not done
echo ""
echo "Checking git submodule..."
if [ ! -f "${PROJECT_ROOT}/spresense/sdk/Makefile" ]; then
    echo "Initializing submodule..."
    cd "${PROJECT_ROOT}"
    git submodule update --init --recursive
    echo "✓ Submodule initialized"
else
    echo "✓ Submodule already initialized"
fi

# Check USB drivers
echo ""
echo "Checking USB drivers..."
if lsmod | grep -q cp210x; then
    echo "✓ CP210x driver loaded"
else
    echo "Loading CP210x driver..."
    sudo modprobe cp210x || echo "WARNING: Failed to load CP210x driver"
fi

if lsmod | grep -q cdc_acm; then
    echo "✓ CDC-ACM driver loaded"
else
    echo "Loading CDC-ACM driver..."
    sudo modprobe cdc_acm || echo "WARNING: Failed to load CDC-ACM driver"
fi

echo ""
echo "=============================================="
echo "Setup completed!"
echo "=============================================="
echo ""
echo "Next steps:"
echo "1. Build: ./scripts/build.sh"
echo "2. Flash: ./scripts/flash.sh"
echo ""
echo "For detailed documentation, see docs/README.md"
echo "=============================================="
