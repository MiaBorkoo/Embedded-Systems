"""
Binary Protocol Parser for Fire Alarm System
Parses binary packets sent by ESP32 using proprietary protocol
"""

import struct
import logging
from typing import Optional, Dict, Any
from datetime import datetime

logger = logging.getLogger(__name__)

# Protocol constants 
PROTOCOL_START_MARKER = 0xAA
PROTOCOL_END_MARKER = 0x55

# Message types
MSG_TYPE_TELEMETRY = 0x01
MSG_TYPE_EVENT = 0x02
MSG_TYPE_STATUS = 0x03

# Command IDs (for building command packets)
CMD_ARM = 0x01
CMD_DISARM = 0x02
CMD_TEST = 0x03
CMD_RESET = 0x04
CMD_OPEN_DOOR = 0x05


def crc8_calculate(data: bytes) -> int:
    """
    Calculate CRC8 checksum using polynomial 0x07
    
    Args:
        data: Bytes to calculate checksum for
        
    Returns:
        CRC8 checksum value (0-255)
    """
    crc = 0x00
    
    for byte in data:
        crc ^= byte
        
        # Process each bit
        for _ in range(8):
            if crc & 0x80:  # If MSB is set
                crc = (crc << 1) ^ 0x07
            else:
                crc = crc << 1
        
        # Keep it 8-bit
        crc &= 0xFF
    
    return crc


def verify_packet(packet: bytes) -> bool:
    """
    Verify packet integrity (markers and checksum)
    
    Args:
        packet: Raw packet bytes
        
    Returns:
        True if packet is valid, False otherwise
    """
    if len(packet) < 6:  # Minimum packet size
        logger.warning(f"Packet too short: {len(packet)} bytes")
        return False
    
    # Check start marker
    if packet[0] != PROTOCOL_START_MARKER:
        logger.warning(f"Invalid start marker: 0x{packet[0]:02X}")
        return False
    
    # Check end marker
    if packet[-1] != PROTOCOL_END_MARKER:
        logger.warning(f"Invalid end marker: 0x{packet[-1]:02X}")
        return False
    
    #verifies CRC
    received_crc = packet[-2]
    #calculate CRC on everything except start marker and last 2 bytes (CRC + END)
    calculated_crc = crc8_calculate(packet[1:-2])
    
    if received_crc != calculated_crc:
        logger.warning(
            f"CRC mismatch: expected 0x{calculated_crc:02X}, "
            f"got 0x{received_crc:02X}"
        )
        return False
    
    return True


def parse_telemetry_packet(packet: bytes) -> Optional[Dict[str, Any]]:
    """
    Parse telemetry packet (type 0x01)
    
    Packet format:
    [START(1)][TYPE(1)][LEN(1)][PAYLOAD(11)][CRC8(1)][END(1)]
    
    Payload (11 bytes):
    - Timestamp (4 bytes, big-endian uint32)
    - CO PPM (2 bytes, big-endian uint16, fixed-point *100)
    - Flags (1 byte): bit1=alarm, bit2=door
    - State (1 byte)
    - Reserved (3 bytes)
    
    Args:
        packet: Raw packet bytes
        
    Returns:
        Dictionary with parsed data, or None if parsing fails
    """
    try:
        # Verify packet first
        if not verify_packet(packet):
            return None
        
        msg_type = packet[1]
        payload_length = packet[2]
        
        # Verify message type
        if msg_type != MSG_TYPE_TELEMETRY:
            logger.warning(f"Not a telemetry packet: type 0x{msg_type:02X}")
            return None
        
        # Verify payload length
        if payload_length != 11:
            logger.warning(f"Invalid telemetry payload length: {payload_length}")
            return None
        
        # Extract payload (starts at byte 3)
        payload = packet[3:3+payload_length]
        
        # Parse timestamp (4 bytes, big-endian)
        timestamp = struct.unpack('>I', payload[0:4])[0]
        
        # Parse CO PPM (2 bytes, big-endian, fixed-point)
        co_ppm_fixed = struct.unpack('>H', payload[4:6])[0]
        co_ppm = co_ppm_fixed / 100.0  # Convert from fixed-point
        
        # Parse flags byte
        flags = payload[6]
        alarm_active = bool(flags & 0x02)  #Bit 1
        door_open = bool(flags & 0x04)     #Bit 2
        
        # Parse state
        state = payload[7]
        
        #reserved bytes at payload[8:11] (ignored for now)
        
        # Add received timestamp
        received_at = datetime.now().isoformat()
        
        result = {
            "timestamp": timestamp,
            "co_ppm": round(co_ppm, 2),  #rounds to 2 decimal places
            "alarm_active": alarm_active,
            "door_open": door_open,
            "state": state,
            "received_at": received_at,
            "packet_type": "telemetry"
        }
        
        logger.debug(
            f"Parsed telemetry: CO={co_ppm:.2f} PPM, "
            f"alarm={alarm_active}, door={door_open}, state={state}"
        )
        
        return result
        
    except Exception as e:
        logger.error(f"Error parsing telemetry packet: {e}", exc_info=True)
        return None

def parse_status_packet(packet: bytes) -> Optional[Dict[str, Any]]:
    if not verify_packet(packet):
        return None

    payload_length = packet[2]

    # Expecting at least 2 bytes: armed + state
    if payload_length < 2:
        logger.warning("Status payload too short")
        return None

    payload = packet[3:3 + payload_length]

    armed = bool(payload[0])
    state = payload[1]

    return {
        "armed": armed,
        "state": state,
        "packet_type": "status",
        "received_at": datetime.now().isoformat()
    }


def parse_event_packet(packet: bytes) -> Optional[Dict[str, Any]]:
    """
    Parse event packet (type 0x02)
    
    Packet format:
    [START(1)][TYPE(1)][LEN(1)][PAYLOAD(variable)][CRC8(1)][END(1)]
    
    Payload (variable):
    - Timestamp (4 bytes)
    - CO PPM (2 bytes, fixed-point)
    - Event name length (1 byte)
    - Event name (variable, up to 16 bytes)
    - Flags (1 byte)
    - State (1 byte)
    - Reserved (2 bytes)
        
    Returns:
        Dictionary with parsed data, or None if parsing fails
    """
    try:
        # Verify packet first
        if not verify_packet(packet):
            return None
        
        msg_type = packet[1]
        payload_length = packet[2]
        
        # Verify message type
        if msg_type != MSG_TYPE_EVENT:
            logger.warning(f"Not an event packet: type 0x{msg_type:02X}")
            return None
        
        # Extract payload
        payload = packet[3:3+payload_length]
        
        # Parse timestamp
        timestamp = struct.unpack('>I', payload[0:4])[0]
        
        # Parse CO PPM
        co_ppm_fixed = struct.unpack('>H', payload[4:6])[0]
        co_ppm = co_ppm_fixed / 100.0
        
        # Parse event name length
        event_name_length = payload[6]
        
        # Validate event name length
        if event_name_length > 16:
            logger.warning(f"Event name too long: {event_name_length}")
            return None
        
        # Parse event name (bytes 7 to 7+length)
        event_name_bytes = payload[7:7+event_name_length]
        event_name = event_name_bytes.decode('utf-8', errors='ignore')
        
        # Parse flags (after event name)
        flags_index = 7 + event_name_length
        flags = payload[flags_index]
        alarm_active = bool(flags & 0x02)
        door_open = bool(flags & 0x04)
        
        # Parse state
        state = payload[flags_index + 1]
        
        # Add received timestamp
        received_at = datetime.now().isoformat()
        
        result = {
            "timestamp": timestamp,
            "event": event_name,
            "co_ppm": round(co_ppm, 2),
            "alarm_active": alarm_active,
            "door_open": door_open,
            "state": state,
            "received_at": received_at,
            "packet_type": "event"
        }
        
        logger.debug(
            f"Parsed event: '{event_name}', CO={co_ppm:.2f} PPM, "
            f"alarm={alarm_active}, door={door_open}"
        )
        
        return result
        
    except Exception as e:
        logger.error(f"Error parsing event packet: {e}", exc_info=True)
        return None


def parse_packet(packet: bytes) -> Optional[Dict[str, Any]]:
    if len(packet) < 6:
        logger.warning(f"Packet too short: {len(packet)} bytes")
        return None
    
    # Log raw packet for debugging
    packet_hex = ' '.join(f'{b:02X}' for b in packet)
    logger.debug(f"Received packet ({len(packet)} bytes): {packet_hex}")
    
    # Check markers first (quick validation)
    if packet[0] != PROTOCOL_START_MARKER or packet[-1] != PROTOCOL_END_MARKER:
        logger.warning("Invalid packet markers")
        return None
    
    # Determine packet type
    msg_type = packet[1]
    
    if msg_type == MSG_TYPE_TELEMETRY:
        return parse_telemetry_packet(packet)
    elif msg_type == MSG_TYPE_EVENT:
        return parse_event_packet(packet)
    elif msg_type == MSG_TYPE_STATUS:
        return parse_status_packet(packet)
    else:
        logger.warning(f"Unknown message type: 0x{msg_type:02X}")
        return None


def print_packet_breakdown(packet: bytes):
    print(f"\n{'='*60}")
    print(f"PACKET BREAKDOWN ({len(packet)} bytes)")
    print(f"{'='*60}")
    
    # Print hex dump
    print("Hex dump:")
    hex_str = ' '.join(f'{b:02X}' for b in packet)
    print(f"  {hex_str}")
    
    if len(packet) < 6:
        print("Packet too short!")
        return
    
    # Print header
    print(f"\nHeader:")
    print(f"  Start marker: 0x{packet[0]:02X} {'✓' if packet[0] == PROTOCOL_START_MARKER else '✗'}")
    print(f"  Message type: 0x{packet[1]:02X}")
    print(f"  Payload len:  {packet[2]} bytes")
    
    # Print footer
    print(f"\nFooter:")
    print(f"  CRC8:        0x{packet[-2]:02X}")
    print(f"  End marker:  0x{packet[-1]:02X} {'✓' if packet[-1] == PROTOCOL_END_MARKER else '✗'}")
    
    # Verify CRC
    calculated_crc = crc8_calculate(packet[1:-2])
    crc_match = "✓" if packet[-2] == calculated_crc else "✗"
    print(f"  CRC check:   0x{calculated_crc:02X} {crc_match}")
    
    print(f"{'='*60}\n")


    # ================= COMMAND PACKET BUILDER =================

MSG_TYPE_COMMAND = 0x10  # must match ESP32

def build_command_packet(command_id: int) -> bytes:
    """
    Build binary command packet

    Payload:
    - Command ID (1 byte)
    - Reserved (2 bytes)

    Packet:
    [START][TYPE][LEN][PAYLOAD][CRC][END]
    """
    payload = struct.pack(">B2s", command_id, b'\x00\x00')
    length = len(payload)

    header = bytes([
        PROTOCOL_START_MARKER,
        MSG_TYPE_COMMAND,
        length
    ])

    crc = crc8_calculate(header[1:] + payload)

    return header + payload + bytes([crc, PROTOCOL_END_MARKER])