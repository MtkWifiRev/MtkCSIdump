#!/usr/bin/env python3

import socket
import struct
import sys
from datetime import datetime

# Struct format for CsiPacketHeader
# uint64_t timestamp, uint32_t antenna_idx, uint32_t packet_count, uint32_t total_samples
HEADER_FORMAT = '<QLLL'  # Little endian: Q=uint64, L=uint32
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

# Struct format for CsiSample
# double value
SAMPLE_FORMAT = '<d'  # Little endian: d=double
SAMPLE_SIZE = struct.calcsize(SAMPLE_FORMAT)

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 csi_udp_client.py <port>")
        print("Example: python3 csi_udp_client.py 8888")
        sys.exit(1)
    
    port = int(sys.argv[1])
    
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    try:
        # Bind to local address
        sock.bind(('0.0.0.0', port))
        print(f"UDP client listening on port {port}...")
        print(f"Make sure to add this client to the server with:")
        print(f"motion-detector: addUdpClient 127.0.0.1 {port}")
        
        while True:
            # Receive data
            data, addr = sock.recvfrom(65536)
            
            if len(data) < HEADER_SIZE:
                print("Received packet too small for header")
                continue
            
            # Parse header
            timestamp, antenna_idx, packet_count, total_samples = struct.unpack(
                HEADER_FORMAT, data[:HEADER_SIZE])
            
            print(f"\n--- CSI Data Received from {addr} ---")
            print(f"Timestamp: {timestamp} ({datetime.fromtimestamp(timestamp/1000)})")
            print(f"Antenna Index: {antenna_idx}")
            print(f"Packet Count: {packet_count}")
            print(f"Total Samples: {total_samples}")
            
            # Calculate expected data size
            expected_size = HEADER_SIZE + total_samples * SAMPLE_SIZE
            if len(data) != expected_size:
                print(f"Warning: Received {len(data)} bytes, expected {expected_size}")
            
            # Parse CSI samples
            samples_data = data[HEADER_SIZE:]
            samples = []
            
            for i in range(min(total_samples, len(samples_data) // SAMPLE_SIZE)):
                sample_bytes = samples_data[i * SAMPLE_SIZE:(i + 1) * SAMPLE_SIZE]
                sample_value = struct.unpack(SAMPLE_FORMAT, sample_bytes)[0]
                samples.append(sample_value)
            
            # Print first few samples
            samples_to_show = min(len(samples), 10)
            print(f"CSI Samples: {samples[:samples_to_show]}", end="")
            if len(samples) > 10:
                print(f" ... (showing first 10 of {len(samples)})")
            else:
                print()
                
    except KeyboardInterrupt:
        print("\nShutting down...")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
