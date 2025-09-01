# CSI UDP Client with GUI Visualization

CSI UDP client to display real-time CSI data. Based on https://github.com/LukasVirecGL/meta-gl-motion-detection.

## Features

- Real-time CSI data visualization
- Multiple antenna support with separate plots
- Raw CSI samples display
- Magnitude spectrum (FFT) visualization
- Phase visualization

## Usage (after dependencies are fulfilled)

### Connect to a wireless network as client
- can be done using the webinterface using "scan". 

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

### Base Image: tested on for OpenWRT One:
-  see releases for squashfs update bin, based on [24.10.1 (r28597-0425664679)](https://firmware-selector.openwrt.org/?version=24.10.1&target=mediatek%2Ffilogic&id=openwrt_one)

### Patched mt76
- see release for binaries: `mt76.ko`, `mt76-connac-lib.ko`, `mt7915e.ko`
- Use these binaries to overwrite whats in: `/lib/modules/6.6.86/`
- Reboot

### Additional Packages
install with `opkg install <dependency>`
- libnl-tiny1
- libstdcpp

## Dependencies Python UI

- Python 3.6+
- PyQt5 >= 5.15.0
- pyqtgraph >= 0.13.1
- numpy >= 1.21.0
- matplotlib >= 3.5.0
