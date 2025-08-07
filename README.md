# CSI UDP Client with GUI Visualization

CSI UDP client to display real-time CSI data. Based on https://github.com/LukasVirecGL/meta-gl-motion-detection.

## Features

- Real-time CSI data visualization
- Multiple antenna support with separate plots
- Raw CSI samples display
- Magnitude spectrum (FFT) visualization
- Phase visualization

## Usage

### Setting up the Server

```bash
./CSIdump phy0-sta0 <rate> <port>
```

Example:
```bash
./CSIdump phy0-sta0 100 8888
```

### Running the GUI Client

```bash
python3 csi_udp_client_gui.py <port>
```

Example:
```bash
python3 csi_udp_client_gui.py 8888
```

## Dependencies OpenWRT
install with `opkg install <dependency>`
- libnl-tiny1
- libstdcpp

## Dependencies Python UI

- Python 3.6+
- PyQt5 >= 5.15.0
- pyqtgraph >= 0.13.1
- numpy >= 1.21.0
- matplotlib >= 3.5.0