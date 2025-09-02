# CSI UDP Client with GUI Visualization

CSI server and client to display real-time CSI data. Based on https://github.com/LukasVirecGL/meta-gl-motion-detection.

## Features

- Real-time CSI data visualization
- Multiple antenna support with separate plots
- Raw CSI samples display
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
python3 csi_udp_client_gui.py <server ip> <port>
```

Example:
```bash
python3 csi_udp_client_gui.py 192.168.1.1 8888
```

## Dependencies OpenWRT

### Base Image: tested on for OpenWRT One:
-  see releases for squashfs update bin, based on [24.10.1 (r28597-0425664679)](https://firmware-selector.openwrt.org/?version=24.10.1&target=mediatek%2Ffilogic&id=openwrt_one)

### Copy mt76 firmware:
- see releses for binaries: `mt7981_rom_patch.bin`, `mt7981_wa.bin`, `mt7981_wm.bin`, `mt7981_wo.bin`
- copy them to `/lib/firmware/mediatek/`
- Reboot

### Additional Packages (might not be necessary)
install with `opkg install <dependency>`
- libnl-tiny1
- libstdcpp

### Copy CSIdump binary to OpenWRT
- see releases for `CSIDump` binary

## Dependencies Python UI

- Python 3.6+
- PyQt5 >= 5.15.0
- pyqtgraph >= 0.13.1
- numpy >= 1.21.0
- matplotlib >= 3.5.0
