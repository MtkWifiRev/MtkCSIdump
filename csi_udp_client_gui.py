#!/usr/bin/env python3

import sys
import socket
import struct
import time
import threading
import signal
from collections import deque
from datetime import datetime
import numpy as np
from PyQt5 import QtWidgets, QtCore, QtGui
import pyqtgraph as pg

# Struct format for CsiPacketHeader
# uint64_t timestamp, uint32_t antenna_idx, uint32_t packet_count, uint32_t total_samples
HEADER_FORMAT = '<QIII'  # Little endian: Q=uint64, I=uint32
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

# Struct format for CsiSample (I/Q pair)
# double i, double q
SAMPLE_FORMAT = '<dd'  # Little endian: d=double, d=double
SAMPLE_SIZE = struct.calcsize(SAMPLE_FORMAT)

class CSIData:
    def __init__(self):
        self.timestamp = 0
        self.antenna_idx = 0
        self.packet_count = 0
        self.samples = []  # Now contains complex numbers (I+jQ)
        self.addr = None

class CSIReceiver(QtCore.QObject):
    data_received = QtCore.pyqtSignal(object)
    
    def __init__(self, server_ip, server_port):
        super().__init__()
        self.server_ip = server_ip
        self.server_port = server_port
        self.socket = None
        self.running = False
        
    def start_receiving(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.settimeout(1.0)  # 1 second timeout
        
        try:
            # Register with the server
            self.socket.sendto(b'register', (self.server_ip, self.server_port))
            print(f"Registered with server at {self.server_ip}:{self.server_port}")

            self.running = True
            while self.running:
                try:
                    data, addr = self.socket.recvfrom(65536)
                    
                    if len(data) < HEADER_SIZE:
                        continue
                    
                    # Parse header
                    timestamp, antenna_idx, packet_count, total_samples = struct.unpack(
                        HEADER_FORMAT, data[:HEADER_SIZE])
                    
                    # Parse CSI samples (I/Q pairs)
                    samples_data = data[HEADER_SIZE:]
                    num_samples = len(samples_data) // SAMPLE_SIZE
                    
                    samples = []
                    if num_samples > 0:
                        # Unpack I/Q pairs and create complex numbers
                        iq_values = list(struct.unpack(f'<{num_samples * 2}d', samples_data))
                        for i in range(0, len(iq_values), 2):
                            complex_sample = complex(iq_values[i], iq_values[i + 1])
                            samples.append(complex_sample)
                    
                    # Create CSI data object
                    csi_data = CSIData()
                    csi_data.timestamp = timestamp
                    csi_data.antenna_idx = antenna_idx
                    csi_data.packet_count = packet_count
                    csi_data.samples = samples
                    csi_data.addr = addr
                    
                    # Emit signal
                    self.data_received.emit(csi_data)
                    
                except socket.timeout:
                    continue
                except Exception as e:
                    if self.running:
                        print(f"Error receiving data: {e}", file=sys.stderr)
                        
        except Exception as e:
            print(f"Error in receiver: {e}", file=sys.stderr)
        finally:
            if self.socket:
                self.socket.close()
            print("Receiver stopped.")
    
    def stop_receiving(self):
        self.running = False
    
    def cleanup(self):
        """Clean up socket resources"""
        pass
            

class CSIVisualizerWindow(QtWidgets.QMainWindow):
    def __init__(self, server_ip, server_port):
        super().__init__()
        self.server_ip = server_ip
        self.server_port = server_port
        self.csi_data_history = {}  # Store history per antenna
        self.max_history_length = 100
        self.packet_counts = {}
        self.last_update_time = time.time()
        self.fps = 0
        
        self.setupUI()
        self.setupReceiver()
        
    def setupUI(self):
        self.setWindowTitle(f"CSI Visualizer - Connected to {self.server_ip}:{self.server_port}")
        self.setGeometry(100, 100, 2000, 900)  # Increased width to accommodate waterfall plot
        
        # Create central widget and layout
        central_widget = QtWidgets.QWidget()
        self.setCentralWidget(central_widget)
        
        # Create status bar
        self.statusBar().showMessage("Ready")
        
        # Create plot widget
        self.plot_widget = pg.GraphicsLayoutWidget()
        
        # Configure plot widget
        self.plot_widget.setBackground('w')
        
        # Initialize processing options
        self.show_raw_amplitude = True
        self.show_amplitude_diff = False
        self.baseline_data = {}  # Store baseline for difference calculation
        
        # Initialize plots dictionary
        self.plots = {}
        self.plot_lines = {}
        self.phase_plots = {}
        self.phase_lines = {}
        self.magnitude_plots = {}
        self.magnitude_lines = {}
        self.waterfall_plots = {}
        self.waterfall_images = {}
        self.waterfall_data = {}  # Store waterfall data for each antenna
        self.waterfall_max_rows = 50  # Maximum number of time samples to display (reduced from 200)
        
        # Create layout
        layout = QtWidgets.QVBoxLayout()
        
        # Add axis control panel
        axis_control_panel = self.create_axis_control_panel()
        layout.addWidget(axis_control_panel)
        
        # Add info panel
        info_panel = QtWidgets.QHBoxLayout()
        self.info_label = QtWidgets.QLabel("No data received yet")
        self.info_label.setStyleSheet("QLabel { font-size: 12px; }")
        info_panel.addWidget(self.info_label)
        info_panel.addStretch()
        
        layout.addLayout(info_panel)
        layout.addWidget(self.plot_widget)
        
        central_widget.setLayout(layout)
        
        # Setup timer for UI updates
        self.update_timer = QtCore.QTimer()
        self.update_timer.timeout.connect(self.update_fps)
        self.update_timer.start(1000)  # Update every second
        
    def setupReceiver(self):
        # Create receiver thread
        self.receiver_thread = QtCore.QThread()
        self.receiver = CSIReceiver(self.server_ip, self.server_port)
        self.receiver.moveToThread(self.receiver_thread)
        
        # Connect signals
        self.receiver.data_received.connect(self.on_data_received)
        self.receiver_thread.started.connect(self.receiver.start_receiving)
        self.receiver_thread.finished.connect(self.receiver.cleanup)
        
        # Start receiver thread
        self.receiver_thread.start()
        
    def create_axis_control_panel(self):
        """Create a control panel for processing options"""
        panel = QtWidgets.QGroupBox("Processing Controls")
        panel.setMaximumHeight(110)  # Increased height for additional controls
        
        layout = QtWidgets.QVBoxLayout()
        
        # Processing options
        processing_layout = QtWidgets.QHBoxLayout()
        processing_layout.addWidget(QtWidgets.QLabel("Processing:"))
        
        self.raw_amplitude_radio = QtWidgets.QRadioButton("Raw Amplitude")
        self.raw_amplitude_radio.setChecked(True)
        self.raw_amplitude_radio.toggled.connect(self.on_processing_changed)
        processing_layout.addWidget(self.raw_amplitude_radio)
        
        self.amplitude_diff_radio = QtWidgets.QRadioButton("Amplitude Difference")
        self.amplitude_diff_radio.toggled.connect(self.on_processing_changed)
        processing_layout.addWidget(self.amplitude_diff_radio)
        
        self.set_baseline_btn = QtWidgets.QPushButton("Set Baseline")
        self.set_baseline_btn.clicked.connect(self.set_baseline)
        self.set_baseline_btn.setEnabled(False)
        processing_layout.addWidget(self.set_baseline_btn)
        
        processing_layout.addStretch()
        
        # Waterfall options
        waterfall_layout = QtWidgets.QHBoxLayout()
        waterfall_layout.addWidget(QtWidgets.QLabel("Waterfall:"))
        
        self.show_waterfall_checkbox = QtWidgets.QCheckBox("Show Waterfall Plots")
        self.show_waterfall_checkbox.setChecked(True)
        self.show_waterfall_checkbox.toggled.connect(self.on_waterfall_visibility_changed)
        waterfall_layout.addWidget(self.show_waterfall_checkbox)
        
        waterfall_layout.addWidget(QtWidgets.QLabel("History Length:"))
        self.waterfall_history_spinbox = QtWidgets.QSpinBox()
        self.waterfall_history_spinbox.setRange(20, 200)  # Reduced range from 50-500 to 20-200
        self.waterfall_history_spinbox.setValue(self.waterfall_max_rows)
        self.waterfall_history_spinbox.setSuffix(" packets")
        self.waterfall_history_spinbox.valueChanged.connect(self.on_waterfall_history_changed)
        waterfall_layout.addWidget(self.waterfall_history_spinbox)
        
        waterfall_layout.addStretch()
        
        # Add layouts to main layout
        layout.addLayout(processing_layout)
        layout.addLayout(waterfall_layout)
        
        panel.setLayout(layout)
        return panel
    
    def on_processing_changed(self):
        """Handle processing mode change"""
        self.show_raw_amplitude = self.raw_amplitude_radio.isChecked()
        self.show_amplitude_diff = self.amplitude_diff_radio.isChecked()
        self.set_baseline_btn.setEnabled(self.show_amplitude_diff)
    
    def on_waterfall_visibility_changed(self, checked):
        """Handle waterfall plot visibility toggle"""
        for antenna_idx in self.waterfall_plots:
            self.waterfall_plots[antenna_idx].setVisible(checked)
    
    def on_waterfall_history_changed(self, value):
        """Handle waterfall history length change"""
        self.waterfall_max_rows = value
        # Trim existing data if necessary
        for antenna_idx in self.waterfall_data:
            if len(self.waterfall_data[antenna_idx]) > value:
                self.waterfall_data[antenna_idx] = self.waterfall_data[antenna_idx][-value:]
    
    def set_baseline(self):
        """Set current CSI data as baseline for difference calculation"""
        self.baseline_data.clear()
        for antenna_idx, history in self.csi_data_history.items():
            if history:
                latest_data = history[-1]
                # Store magnitude of complex samples as baseline
                complex_samples = np.array(latest_data.samples)
                magnitude = np.abs(complex_samples)
                self.baseline_data[antenna_idx] = magnitude.copy()
        print(f"Baseline set for {len(self.baseline_data)} antennas")
    
    def create_plots_for_antenna(self, antenna_idx):
        """Create plots for a new antenna"""
        if antenna_idx in self.plots:
            return
            
        row = antenna_idx
        
        # Magnitude plot (from complex CSI data)
        self.plots[antenna_idx] = self.plot_widget.addPlot(
            row=row, col=0, title=f"CSI Magnitude - Antenna {antenna_idx}")
        self.plots[antenna_idx].setLabel('left', 'Magnitude')
        self.plots[antenna_idx].setLabel('bottom', 'Subcarrier Index')
        self.plots[antenna_idx].showGrid(True, True)
        
        # Create line for this antenna
        pen = pg.mkPen(color=(255, 0, 0), width=2)
        self.plot_lines[antenna_idx] = self.plots[antenna_idx].plot(pen=pen)
        
        # Phase plot (from complex CSI data)
        self.phase_plots[antenna_idx] = self.plot_widget.addPlot(
            row=row, col=1, title=f"CSI Phase - Antenna {antenna_idx}")
        self.phase_plots[antenna_idx].setLabel('left', 'Phase (radians)')
        self.phase_plots[antenna_idx].setLabel('bottom', 'Subcarrier Index')
        self.phase_plots[antenna_idx].showGrid(True, True)
        self.phase_plots[antenna_idx].setYRange(-3.15, 3.15)  # Phase range is [-π, π]
        
        pen = pg.mkPen(color=(0, 0, 255), width=2)
        self.phase_lines[antenna_idx] = self.phase_plots[antenna_idx].plot(pen=pen)
        
        # Magnitude Spectrum plot (FFT of CSI data)
        self.magnitude_plots[antenna_idx] = self.plot_widget.addPlot(
            row=row, col=2, title=f"Magnitude Spectrum (FFT) - Antenna {antenna_idx}")
        self.magnitude_plots[antenna_idx].setLabel('left', 'Magnitude (dB)')
        self.magnitude_plots[antenna_idx].setLabel('bottom', 'Frequency Bin')
        self.magnitude_plots[antenna_idx].showGrid(True, True)
        
        pen = pg.mkPen(color=(0, 255, 0), width=2)
        self.magnitude_lines[antenna_idx] = self.magnitude_plots[antenna_idx].plot(pen=pen)
        
        # Waterfall plot (CSI magnitude over time)
        self.waterfall_plots[antenna_idx] = self.plot_widget.addPlot(
            row=row, col=3, title=f"CSI Waterfall - Antenna {antenna_idx}")
        self.waterfall_plots[antenna_idx].setLabel('left', 'Time (Newest → Oldest)')
        self.waterfall_plots[antenna_idx].setLabel('bottom', 'Subcarrier Index')
        
        # Create ImageItem for waterfall display
        self.waterfall_images[antenna_idx] = pg.ImageItem()
        self.waterfall_plots[antenna_idx].addItem(self.waterfall_images[antenna_idx])
        
        # Set up colormap for waterfall (viridis-like colormap)
        colormap = pg.ColorMap(
            pos=[0.0, 0.25, 0.5, 0.75, 1.0],
            color=[(68, 1, 84), (59, 82, 139), (33, 144, 140), (94, 201, 98), (253, 231, 37)]
        )
        self.waterfall_images[antenna_idx].setColorMap(colormap)
        
        # Initialize waterfall data storage
        self.waterfall_data[antenna_idx] = []
        
        # Initialize history for this antenna
        self.csi_data_history[antenna_idx] = deque(maxlen=self.max_history_length)
        
    def on_data_received(self, csi_data):
        """Handle received CSI data"""
        antenna_idx = csi_data.antenna_idx
        
        # Create plots for this antenna if they don't exist
        if antenna_idx not in self.plots:
            self.create_plots_for_antenna(antenna_idx)
        
        # Store data in history
        self.csi_data_history[antenna_idx].append(csi_data)
        self.packet_counts[antenna_idx] = csi_data.packet_count
        
        # Update plots
        self.update_plots(antenna_idx, csi_data)
        
        # Update info
        self.update_info_panel()
        
    def update_plots(self, antenna_idx, csi_data):
        """Update plots with new CSI data"""
        if not csi_data.samples:
            return
            
        # Convert complex samples to numpy array
        samples = np.array(csi_data.samples)
        
        # Calculate magnitude and phase from complex data
        magnitude = np.abs(samples)
        phase = np.angle(samples)
        
        # Process data based on selected mode
        if self.show_amplitude_diff and antenna_idx in self.baseline_data:
            # Show difference from baseline
            baseline = self.baseline_data[antenna_idx]
            if len(baseline) == len(magnitude):
                processed_magnitude = magnitude - baseline
                plot_title_suffix = " (Difference from Baseline)"
            else:
                processed_magnitude = magnitude
                plot_title_suffix = " (Raw - Baseline size mismatch)"
        else:
            # Show raw magnitude
            processed_magnitude = magnitude
            plot_title_suffix = " (Raw)"
        
        # Update magnitude plot
        x_data = np.arange(len(processed_magnitude))
        self.plot_lines[antenna_idx].setData(x_data, processed_magnitude)
        
        # Update magnitude plot title
        self.plots[antenna_idx].setTitle(f"CSI Magnitude - Antenna {antenna_idx}{plot_title_suffix}")
        
        # Update phase plot (always use raw phase, not difference)
        self.phase_lines[antenna_idx].setData(x_data, phase)
        
        # Compute and update magnitude spectrum (FFT) - use complex samples for meaningful FFT
        if len(samples) > 1:
            # Compute FFT of complex CSI data
            fft_data = np.fft.fft(samples)
            magnitude_spectrum = np.abs(fft_data)
            magnitude_spectrum_db = 20 * np.log10(magnitude_spectrum + 1e-10)  # Add small value to avoid log(0)
            
            # Update magnitude spectrum plot
            freq_bins = np.arange(len(magnitude_spectrum_db))
            self.magnitude_lines[antenna_idx].setData(freq_bins, magnitude_spectrum_db)
        
        # Update waterfall plot
        self.update_waterfall_plot(antenna_idx, processed_magnitude)
        
    def update_waterfall_plot(self, antenna_idx, magnitude_data):
        """Update the waterfall plot with new magnitude data"""
        if antenna_idx not in self.waterfall_data:
            return
            
        # Add new magnitude data to waterfall history
        self.waterfall_data[antenna_idx].append(magnitude_data.copy())
        
        # Limit the number of rows (time samples)
        if len(self.waterfall_data[antenna_idx]) > self.waterfall_max_rows:
            self.waterfall_data[antenna_idx].pop(0)
        
        # Convert to numpy array for display
        if len(self.waterfall_data[antenna_idx]) > 1:
            waterfall_array = np.array(self.waterfall_data[antenna_idx])
            
            # Ensure consistent array shape by padding shorter arrays with zeros
            max_length = max(len(row) for row in self.waterfall_data[antenna_idx])
            padded_data = []
            for row in self.waterfall_data[antenna_idx]:
                if len(row) < max_length:
                    padded_row = np.pad(row, (0, max_length - len(row)), 'constant', constant_values=0)
                    padded_data.append(padded_row)
                else:
                    padded_data.append(row)
            
            waterfall_array = np.array(padded_data)
            
            # Transpose so time is on Y-axis (rows) and subcarriers on X-axis (columns)
            # Note: Most recent data should be at the top, so we flip the array
            waterfall_array = np.flipud(waterfall_array)
            
            # Update the image with proper scaling
            self.waterfall_images[antenna_idx].setImage(
                waterfall_array,
                autoLevels=False,  # Use manual levels for better control
                autoDownsample=True
            )
            
            # Set manual levels for better contrast
            data_min = np.min(waterfall_array)
            data_max = np.max(waterfall_array)
            if data_max > data_min:
                self.waterfall_images[antenna_idx].setLevels([data_min, data_max])
            
            # Set the correct positioning and scaling
            num_time_samples = len(self.waterfall_data[antenna_idx])
            num_subcarriers = waterfall_array.shape[1]
            
            # Set the image rectangle (x, y, width, height)
            # Position image so that bottom is time=0 and top is most recent
            self.waterfall_images[antenna_idx].setRect(
                QtCore.QRectF(0, 0, num_subcarriers, num_time_samples)
            )
            
            # Update plot range to show the full waterfall
            self.waterfall_plots[antenna_idx].setXRange(0, num_subcarriers)
            self.waterfall_plots[antenna_idx].setYRange(0, num_time_samples)
        
    def update_info_panel(self):
        """Update the information panel"""
        try:
            current_time = time.time()
            self.fps = 1.0 / (current_time - self.last_update_time) if current_time > self.last_update_time else 0
            self.last_update_time = current_time
            
            # Count active antennas
            active_antennas = len(self.csi_data_history)
            
            # Get total packets received
            total_packets = sum(self.packet_counts.values()) if self.packet_counts else 0
            
            # Get sample count info from the most recent data
            sample_info = ""
            if self.csi_data_history:
                for antenna_idx, history in self.csi_data_history.items():
                    if history:
                        latest_data = history[-1]
                        sample_count = len(latest_data.samples)
                        sample_info = f" | Samples per packet: {sample_count}"
                        break
            
            info_text = f"Active Antennas: {active_antennas} | Total Packets: {total_packets} | FPS: {self.fps:.1f}{sample_info}"
            
            # Check if widgets still exist before updating
            if hasattr(self, 'info_label') and self.info_label is not None:
                self.info_label.setText(info_text)
            
            # Update window title
            self.setWindowTitle(f"CSI Visualizer - Port {self.server_port} | FPS: {self.fps:.1f}")
        except Exception as e:
            # Silently ignore errors during shutdown
            pass
        
    def update_fps(self):
        """Update FPS display"""
        try:
            if hasattr(self, 'info_label') and self.info_label is not None:
                self.update_info_panel()
        except Exception as e:
            # Silently ignore errors during shutdown
            pass
        
    def closeEvent(self, event):
        """Handle window close event"""
        try:
            print("Closing application...")
            
            # Stop timer first to prevent further callbacks
            if hasattr(self, 'update_timer') and self.update_timer.isActive():
                self.update_timer.stop()
            
            # Stop receiver
            if hasattr(self, 'receiver'):
                self.receiver.stop_receiving()
            
            # Wait for thread to finish
            if hasattr(self, 'receiver_thread') and self.receiver_thread.isRunning():
                self.receiver_thread.quit()
                if not self.receiver_thread.wait(3000):  # Wait up to 3 seconds
                    print("Warning: Receiver thread did not terminate cleanly")
                    
        except Exception as e:
            print(f"Error during cleanup: {e}")
        finally:
            event.accept()

def signal_handler(signum, frame):
    """Handle Ctrl+C signal"""
    print("\nReceived interrupt signal. Exiting...")
    QtWidgets.QApplication.quit()

def main():
    if len(sys.argv) != 3:
        print("Usage: ./csi_udp_client_gui.py <server_ip> <server_port>")
        sys.exit(1)
    
    server_ip = sys.argv[1]
    server_port = int(sys.argv[2])
    
    # Set up signal handler for Ctrl+C
    signal.signal(signal.SIGINT, signal_handler)
    
    app = QtWidgets.QApplication(sys.argv)
    
    # Set application style
    app.setStyle('Fusion')
    
    # Create and show main window
    window = CSIVisualizerWindow(server_ip, server_port)
    window.show()
    
    print(f"CSI Visualizer started, connecting to {server_ip}:{server_port}")
    print("Press Ctrl+C to exit")
    
    # Enable timer to process events during signal handling
    timer = QtCore.QTimer()
    timer.start(500)  # Let the interpreter run every 500ms
    timer.timeout.connect(lambda: None)
    
    try:
        exit_code = app.exec_()
    except KeyboardInterrupt:
        print("\nKeyboard interrupt received. Exiting...")
        exit_code = 0
    finally:
        # Ensure cleanup
        if hasattr(window, 'receiver'):
            window.receiver.stop_receiving()
        if hasattr(window, 'receiver_thread') and window.receiver_thread.isRunning():
            window.receiver_thread.quit()
            window.receiver_thread.wait(3000)
    
    sys.exit(exit_code)

if __name__ == "__main__":
    main()
