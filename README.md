# CSI UDP Client with GUI Visualization

This is an enhanced version of the CSI UDP client that includes a real-time graphical visualization similar to the nexmon_csi_gui project.

## Features

- Real-time CSI data visualization
- Multiple antenna support with separate plots
- Raw CSI samples display
- Magnitude spectrum (FFT) visualization
- Phase visualization
- Configurable axis ranges to prevent jumping
- FPS monitoring
- Proper signal handling for clean exit

## Installation

1. Install the required dependencies:
   ```bash
   ./install_deps.sh
   ```
   
   Or manually install with pip:
   ```bash
   pip3 install -r requirements.txt
   ```

2. Test the installation:
   ```bash
   python3 test_gui.py
   ```

## Usage

### Running the GUI Client

```bash
python3 csi_udp_client_gui.py <port>
```

Example:
```bash
python3 csi_udp_client_gui.py 8888
```

### Running the Simple Console Client

If you prefer the original console-based client:
```bash
python3 csi_udp_client_simple.py <port>
```

### Setting up the Server

Make sure to add this client to your motion detector server:
```
motion-detector: addUdpClient 127.0.0.1 <port>
```

## Recent Fixes

### CSI Data Structure Fix
- **Issue**: X-axis was showing up to 2000 values instead of expected 64/128/256 subcarriers
- **Root Cause**: 
  1. Parser was concatenating multiple CSI packets into a single stream
  2. Parser was only including non-zero I/Q values instead of all subcarriers
- **Fix**: 
  1. Modified `motion_detector.cpp` to send each CSI packet separately
  2. Updated `parser_mt76.cpp` to process all subcarriers based on channel bandwidth
  3. Added bandwidth-specific subcarrier processing (20MHz=64, 40MHz=128, 80MHz=256, 160MHz=512)

### Motion Detection Enhancements
- **Issue**: Hand movements near receiver showed minimal CSI magnitude changes
- **Improvements**:
  1. **DC Offset Removal**: Skip first few subcarriers to eliminate the spike at the beginning
  2. **Amplitude Normalization**: Normalize CSI values to 0-100 range for better sensitivity
  3. **Baseline Comparison**: Added "Amplitude Difference" mode to show changes from a baseline
  4. **Enhanced Y-axis Presets**: Added preset for difference plots (-20 to 20)

## GUI Controls

- **Auto-scale X-axis**: Enable/disable automatic scaling of the X-axis
- **X-axis range controls**: Set fixed min/max values for the X-axis when auto-scale is disabled
- **X-axis presets**: Quick preset buttons for common bandwidth configurations:
  - **20MHz (0-64)**: For 20MHz channel bandwidth (64 subcarriers)
  - **40MHz (0-128)**: For 40MHz channel bandwidth (128 subcarriers)
  - **80MHz (0-256)**: For 80MHz channel bandwidth (256 subcarriers)
- **Auto-scale Y-axis**: Enable/disable automatic scaling of the Y-axis
- **Y-axis range controls**: Set fixed min/max values for the Y-axis when auto-scale is disabled
- **Y-axis presets**: Quick preset buttons for common Y-axis ranges:
  - **CSI (0-100)**: Suitable for raw CSI amplitude values
  - **Magnitude (-80-20 dB)**: Suitable for FFT magnitude plots
  - **Phase (-π-π)**: Suitable for phase plots
  - **Difference (-20-20)**: Suitable for amplitude difference plots
- **Processing Options**:
  - **Raw Amplitude**: Show normalized CSI amplitude values (0-100)
  - **Amplitude Difference**: Show difference from baseline (better for motion detection)
  - **Set Baseline**: Capture current CSI as baseline for difference calculation
- **Apply Axis Settings**: Apply the new axis settings to all plots

## Motion Detection Usage

For best motion detection sensitivity:

1. **Start the GUI** with your desired port
2. **Wait for stable CSI data** to appear
3. **Switch to "Amplitude Difference" mode** 
4. **Click "Set Baseline"** when the environment is stable (no movement)
5. **Use the "Difference (-20-20)" Y-axis preset** for optimal scale
6. **Move your hand near the receiver** - you should now see dramatic changes in the difference plot

The difference mode will show positive and negative changes from the baseline, making motion detection much more sensitive than raw amplitude alone.

## Visualization

The GUI shows three types of plots for each antenna:

1. **CSI Samples**: Raw CSI amplitude values over sample indices
2. **Magnitude Spectrum**: FFT magnitude in dB scale
3. **Phase**: FFT phase in radians

## Exiting the Application

- **GUI**: Close the window or press Ctrl+C in the terminal
- **Console**: Press Ctrl+C

## Troubleshooting

### Import Errors

If you get import errors, make sure all dependencies are installed:
```bash
pip3 install PyQt5 pyqtgraph numpy matplotlib
```

### GUI Won't Start

1. Check if you have a display available (X11 forwarding for SSH)
2. Try running the test script: `python3 test_gui.py`
3. For headless systems, you might need to set up a virtual display

### Signal Handling Issues

The application now properly handles Ctrl+C signals. If you still encounter issues:
1. Make sure you're using the latest version of the script
2. Try closing the GUI window instead of using Ctrl+C

## Dependencies

- Python 3.6+
- PyQt5 >= 5.15.0
- pyqtgraph >= 0.13.1
- numpy >= 1.21.0
- matplotlib >= 3.5.0

## File Structure

- `csi_udp_client_gui.py` - Main GUI application
- `csi_udp_client_simple.py` - Original console client
- `requirements.txt` - Python dependencies
- `install_deps.sh` - Installation script
- `test_gui.py` - Test script for verifying installation
- `README.md` - This file
