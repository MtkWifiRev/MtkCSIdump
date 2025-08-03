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
        self.setGeometry(100, 100, 1600, 900)
        
        # Create central widget and layout
        central_widget = QtWidgets.QWidget()
        self.setCentralWidget(central_widget)
        
        # Create status bar
        self.statusBar().showMessage("Ready")
        
        # Create plot widget
        self.plot_widget = pg.GraphicsLayoutWidget()
        
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
        
        # Configure plot widget
        self.plot_widget.setBackground('w')
        
        # Initialize axis settings
        self.auto_scale_x = True
        self.fixed_x_min = 0
        self.fixed_x_max = 100
        self.auto_scale_y = True
        self.fixed_y_min = -50
        self.fixed_y_max = 50
        
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
        """Create a control panel for axis settings"""
        panel = QtWidgets.QGroupBox("Axis Controls")
        panel.setMaximumHeight(210)
        
        layout = QtWidgets.QVBoxLayout()
        
        # X-axis controls
        x_layout = QtWidgets.QHBoxLayout()
        
        # Auto-scale X checkbox
        self.auto_scale_x_checkbox = QtWidgets.QCheckBox("Auto-scale X-axis")
        self.auto_scale_x_checkbox.setChecked(True)
        self.auto_scale_x_checkbox.stateChanged.connect(self.on_auto_scale_x_changed)
        x_layout.addWidget(self.auto_scale_x_checkbox)
        
        x_layout.addWidget(QtWidgets.QLabel("X-axis range:"))
        
        # X-axis min control
        x_layout.addWidget(QtWidgets.QLabel("Min:"))
        self.x_min_spinbox = QtWidgets.QSpinBox()
        self.x_min_spinbox.setRange(-10000, 10000)
        self.x_min_spinbox.setValue(0)
        self.x_min_spinbox.setEnabled(False)
        self.x_min_spinbox.valueChanged.connect(self.on_x_axis_range_changed)
        x_layout.addWidget(self.x_min_spinbox)
        
        # X-axis max control
        x_layout.addWidget(QtWidgets.QLabel("Max:"))
        self.x_max_spinbox = QtWidgets.QSpinBox()
        self.x_max_spinbox.setRange(-10000, 10000)
        self.x_max_spinbox.setValue(100)
        self.x_max_spinbox.setEnabled(False)
        self.x_max_spinbox.valueChanged.connect(self.on_x_axis_range_changed)
        x_layout.addWidget(self.x_max_spinbox)
        
        x_layout.addStretch()
        
        # Preset buttons for X-axis
        x_preset_layout = QtWidgets.QHBoxLayout()
        x_preset_layout.addWidget(QtWidgets.QLabel("X-axis presets:"))
        
        # Subcarrier presets
        sc_20_btn = QtWidgets.QPushButton("20MHz (0-64)")
        sc_20_btn.clicked.connect(lambda: self.set_x_preset(0, 64))
        x_preset_layout.addWidget(sc_20_btn)
        
        sc_40_btn = QtWidgets.QPushButton("40MHz (0-128)")
        sc_40_btn.clicked.connect(lambda: self.set_x_preset(0, 128))
        x_preset_layout.addWidget(sc_40_btn)
        
        sc_80_btn = QtWidgets.QPushButton("80MHz (0-256)")
        sc_80_btn.clicked.connect(lambda: self.set_x_preset(0, 256))
        x_preset_layout.addWidget(sc_80_btn)
        
        sc_160_btn = QtWidgets.QPushButton("160MHz (0-512)")
        sc_160_btn.clicked.connect(lambda: self.set_x_preset(0, 512))
        x_preset_layout.addWidget(sc_160_btn)
        
        x_preset_layout.addStretch()
        
        # Y-axis controls
        y_layout = QtWidgets.QHBoxLayout()
        
        # Auto-scale Y checkbox
        self.auto_scale_y_checkbox = QtWidgets.QCheckBox("Auto-scale Y-axis")
        self.auto_scale_y_checkbox.setChecked(True)
        self.auto_scale_y_checkbox.stateChanged.connect(self.on_auto_scale_y_changed)
        y_layout.addWidget(self.auto_scale_y_checkbox)
        
        y_layout.addWidget(QtWidgets.QLabel("Y-axis range:"))
        
        # Y-axis min control
        y_layout.addWidget(QtWidgets.QLabel("Min:"))
        self.y_min_spinbox = QtWidgets.QDoubleSpinBox()
        self.y_min_spinbox.setRange(-10000.0, 10000.0)
        self.y_min_spinbox.setValue(-50.0)
        self.y_min_spinbox.setDecimals(2)
        self.y_min_spinbox.setEnabled(False)
        self.y_min_spinbox.valueChanged.connect(self.on_y_axis_range_changed)
        y_layout.addWidget(self.y_min_spinbox)
        
        # Y-axis max control
        y_layout.addWidget(QtWidgets.QLabel("Max:"))
        self.y_max_spinbox = QtWidgets.QDoubleSpinBox()
        self.y_max_spinbox.setRange(-10000.0, 10000.0)
        self.y_max_spinbox.setValue(50.0)
        self.y_max_spinbox.setDecimals(2)
        self.y_max_spinbox.setEnabled(False)
        self.y_max_spinbox.valueChanged.connect(self.on_y_axis_range_changed)
        y_layout.addWidget(self.y_max_spinbox)
        
        y_layout.addStretch()
        
        # Preset buttons for Y-axis
        preset_layout = QtWidgets.QHBoxLayout()
        preset_layout.addWidget(QtWidgets.QLabel("Y-axis presets:"))
        
        # CSI Samples preset
        samples_preset_btn = QtWidgets.QPushButton("CSI (0-100)")
        samples_preset_btn.clicked.connect(lambda: self.set_y_preset(0, 100))
        preset_layout.addWidget(samples_preset_btn)
        
        # Magnitude preset
        magnitude_preset_btn = QtWidgets.QPushButton("Magnitude (-80-20 dB)")
        magnitude_preset_btn.clicked.connect(lambda: self.set_y_preset(-80, 20))
        preset_layout.addWidget(magnitude_preset_btn)
        
        # Phase preset
        phase_preset_btn = QtWidgets.QPushButton("Phase (-π-π)")
        phase_preset_btn.clicked.connect(lambda: self.set_y_preset(-3.14159, 3.14159))
        preset_layout.addWidget(phase_preset_btn)
        
        # Difference preset
        diff_preset_btn = QtWidgets.QPushButton("Difference (-20-20)")
        diff_preset_btn.clicked.connect(lambda: self.set_y_preset(-20, 20))
        preset_layout.addWidget(diff_preset_btn)
        
        preset_layout.addStretch()
        
        # Apply button
        apply_layout = QtWidgets.QHBoxLayout()
        apply_layout.addStretch()
        self.apply_axis_button = QtWidgets.QPushButton("Apply Axis Settings")
        self.apply_axis_button.setEnabled(False)
        self.apply_axis_button.clicked.connect(self.apply_axis_settings)
        apply_layout.addWidget(self.apply_axis_button)
        apply_layout.addStretch()
        
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
        
        # Add all layouts to main layout
        layout.addLayout(x_layout)
        layout.addLayout(x_preset_layout)
        layout.addLayout(y_layout)
        layout.addLayout(preset_layout)
        layout.addLayout(apply_layout)
        layout.addLayout(processing_layout)
        
        panel.setLayout(layout)
        return panel
    
    def on_auto_scale_x_changed(self, state):
        """Handle auto-scale X checkbox change"""
        self.auto_scale_x = state == QtCore.Qt.Checked
        self.x_min_spinbox.setEnabled(not self.auto_scale_x)
        self.x_max_spinbox.setEnabled(not self.auto_scale_x)
        self.update_apply_button_state()
        
        if not self.auto_scale_x:
            self.apply_axis_settings()
    
    def on_auto_scale_y_changed(self, state):
        """Handle auto-scale Y checkbox change"""
        self.auto_scale_y = state == QtCore.Qt.Checked
        self.y_min_spinbox.setEnabled(not self.auto_scale_y)
        self.y_max_spinbox.setEnabled(not self.auto_scale_y)
        self.update_apply_button_state()
        
        if not self.auto_scale_y:
            self.apply_axis_settings()
    
    def on_x_axis_range_changed(self):
        """Handle X-axis range spinbox changes"""
        if not self.auto_scale_x:
            self.fixed_x_min = self.x_min_spinbox.value()
            self.fixed_x_max = self.x_max_spinbox.value()
    
    def on_y_axis_range_changed(self):
        """Handle Y-axis range spinbox changes"""
        if not self.auto_scale_y:
            self.fixed_y_min = self.y_min_spinbox.value()
            self.fixed_y_max = self.y_max_spinbox.value()
    
    def update_apply_button_state(self):
        """Update the apply button enabled state"""
        self.apply_axis_button.setEnabled(not self.auto_scale_x or not self.auto_scale_y)
    
    def set_x_preset(self, min_val, max_val):
        """Set X-axis preset values"""
        self.x_min_spinbox.setValue(min_val)
        self.x_max_spinbox.setValue(max_val)
        if not self.auto_scale_x:
            self.apply_axis_settings()
    
    def set_y_preset(self, min_val, max_val):
        """Set Y-axis preset values"""
        self.y_min_spinbox.setValue(min_val)
        self.y_max_spinbox.setValue(max_val)
        if not self.auto_scale_y:
            self.apply_axis_settings()
    
    def on_processing_changed(self):
        """Handle processing mode change"""
        self.show_raw_amplitude = self.raw_amplitude_radio.isChecked()
        self.show_amplitude_diff = self.amplitude_diff_radio.isChecked()
        self.set_baseline_btn.setEnabled(self.show_amplitude_diff)
        
        # Update Y-axis presets based on processing mode
        if self.show_amplitude_diff:
            # Set suitable range for difference plots
            self.set_y_preset(-20, 20)
        else:
            # Set suitable range for raw amplitude
            self.set_y_preset(0, 100)
    
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
    
    def apply_axis_settings(self):
        """Apply axis settings to all plots"""
        # Update values from spinboxes
        if not self.auto_scale_x:
            self.fixed_x_min = self.x_min_spinbox.value()
            self.fixed_x_max = self.x_max_spinbox.value()
            
        if not self.auto_scale_y:
            self.fixed_y_min = self.y_min_spinbox.value()
            self.fixed_y_max = self.y_max_spinbox.value()
            
        # Apply to all existing plots
        for antenna_idx in self.plots:
            # Apply X-axis settings
            if not self.auto_scale_x:
                self.plots[antenna_idx].setXRange(self.fixed_x_min, self.fixed_x_max)
                self.magnitude_plots[antenna_idx].setXRange(self.fixed_x_min, self.fixed_x_max)
                self.phase_plots[antenna_idx].setXRange(self.fixed_x_min, self.fixed_x_max)
            
            # Apply Y-axis settings
            if not self.auto_scale_y:
                self.plots[antenna_idx].setYRange(self.fixed_y_min, self.fixed_y_max)
                self.magnitude_plots[antenna_idx].setYRange(self.fixed_y_min, self.fixed_y_max)
                self.phase_plots[antenna_idx].setYRange(self.fixed_y_min, self.fixed_y_max)
        
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
        
        # Apply current axis settings
        if not self.auto_scale_x:
            self.plots[antenna_idx].setXRange(self.fixed_x_min, self.fixed_x_max)
            self.magnitude_plots[antenna_idx].setXRange(self.fixed_x_min, self.fixed_x_max)
            self.phase_plots[antenna_idx].setXRange(self.fixed_x_min, self.fixed_x_max)
            
        if not self.auto_scale_y:
            self.plots[antenna_idx].setYRange(self.fixed_y_min, self.fixed_y_max)
            self.magnitude_plots[antenna_idx].setYRange(self.fixed_y_min, self.fixed_y_max)
            # Phase plot keeps its fixed range
        
        # Initialize history for this antenna
        self.csi_data_history[antenna_idx] = deque(maxlen=self.max_history_length)
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
        
        # Apply axis settings for magnitude and phase plots
        if not self.auto_scale_x:
            self.plots[antenna_idx].setXRange(self.fixed_x_min, self.fixed_x_max)
            self.phase_plots[antenna_idx].setXRange(self.fixed_x_min, self.fixed_x_max)
        if not self.auto_scale_y:
            self.plots[antenna_idx].setYRange(self.fixed_y_min, self.fixed_y_max)
            # Phase plot keeps its fixed range [-π, π]
        
        # Compute and update magnitude spectrum (FFT) - use complex samples for meaningful FFT
        if len(samples) > 1:
            # Compute FFT of complex CSI data
            fft_data = np.fft.fft(samples)
            magnitude_spectrum = np.abs(fft_data)
            magnitude_spectrum_db = 20 * np.log10(magnitude_spectrum + 1e-10)  # Add small value to avoid log(0)
            
            # Update magnitude spectrum plot
            freq_bins = np.arange(len(magnitude_spectrum_db))
            self.magnitude_lines[antenna_idx].setData(freq_bins, magnitude_spectrum_db)
            
            # Apply axis settings for FFT plot
            if not self.auto_scale_x:
                self.magnitude_plots[antenna_idx].setXRange(self.fixed_x_min, self.fixed_x_max)
            if not self.auto_scale_y:
                # Use appropriate range for magnitude spectrum in dB
                self.magnitude_plots[antenna_idx].setYRange(-60, 40)  # Typical range for dB magnitude
        
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
