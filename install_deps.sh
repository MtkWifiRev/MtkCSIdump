#!/bin/bash

# Install dependencies for CSI UDP Client GUI
echo "Installing dependencies for CSI UDP Client GUI..."

# Check if pip is available
if ! command -v pip3 &> /dev/null; then
    echo "pip3 not found. Please install pip3 first."
    exit 1
fi

# Install Python dependencies
echo "Installing Python packages..."
pip3 install -r requirements.txt

# Check if installation was successful
if [ $? -eq 0 ]; then
    echo "Dependencies installed successfully!"
    echo ""
    echo "You can now run the CSI UDP Client GUI with:"
    echo "python3 csi_udp_client_gui.py <port>"
    echo "Example: python3 csi_udp_client_gui.py 8888"
else
    echo "Failed to install dependencies. Please check the error messages above."
    exit 1
fi
