#include "protocol.h"
#include "crc8.h"
#include <string.h>
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "PROTOCOL";

//Convert float to fixed-point (2 decimal places)
static uint16_t float_to_fixed16(float value) {
    return (uint16_t)(value * 100.0f);  // e.g., 45.67 → 4567
} 
//converts a floating-point no to a fixed-point integer 
//-> Floats are 4 bytes, fixed-pt are 2 bytes(to save space in our packet)

//Pack 32-bit value as big-endian
static void pack_uint32_be(uint8_t *buf, uint32_t value) {
    buf[0] = (value >> 24) & 0xFF; //right shift by 24 bits and mask with 0xFF to get the last byte
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8) & 0xFF;
    buf[3] = value & 0xFF;
}

//Pack 16-bit value as big-endian
static void pack_uint16_be(uint8_t *buf, uint16_t value) {
    buf[0] = (value >> 8) & 0xFF;
    buf[1] = value & 0xFF;
}

bool protocol_encode_telemetry(const Telemetry_t *telemetry, 
                                uint8_t *packet_out, 
                                size_t *packet_len_out) {
    if (!telemetry || !packet_out || !packet_len_out) {
        ESP_LOGE(TAG, "Invalid parameters for encode_telemetry");
        return false;
    }

    /*
     * Telemetry Packet Format:
     * [START(1)][TYPE(1)][LEN(1)][PAYLOAD][CRC8(1)][END(1)]
     * 
     * Payload (11 bytes):
     *   - Timestamp (4 bytes, big-endian)
     *   - CO PPM (2 bytes, fixed-point 16-bit, big-endian)
     *   - Flags (1 byte): [7:unused][6:unused][5-4:unused][3:unused][2:door][1:alarm][0:unused]
     *   - State (1 byte)
     *   - Reserved (3 bytes for future use)
     */

    size_t idx = 0; //index variable to track where we are in the packet buffer

    packet_out[idx++] = PROTOCOL_START_MARKER;
    packet_out[idx++] = MSG_TYPE_TELEMETRY;

    const uint8_t payload_length = 11;
    packet_out[idx++] = payload_length;

    size_t payload_start = idx;

    pack_uint32_be(&packet_out[idx], telemetry->timestamp);
    idx += 4;

    uint16_t co_fixed = float_to_fixed16(telemetry->co_ppm);
    pack_uint16_be(&packet_out[idx], co_fixed);
    idx += 2;

    uint8_t flags = 0;
    if (telemetry->alarm_active) flags |= (1 << 1);
    if (telemetry->door_open) flags |= (1 << 2);
    packet_out[idx++] = flags;

    packet_out[idx++] = telemetry->state;

    packet_out[idx++] = 0x00;
    packet_out[idx++] = 0x00;
    packet_out[idx++] = 0x00;


    // Calculate CRC8 on [TYPE][LEN][PAYLOAD]
    size_t crc_length = idx - 1;  //everything except START marker
    uint8_t crc = crc8_calculate(&packet_out[1], crc_length);
    packet_out[idx++] = crc;

    packet_out[idx++] = PROTOCOL_END_MARKER;

    *packet_len_out = idx;

    ESP_LOGD(TAG, "Encoded telemetry packet (%zu bytes): CO=%.2f, alarm=%d, door=%d", 
             idx, telemetry->co_ppm, telemetry->alarm_active, telemetry->door_open);

    return true;
}

bool protocol_encode_event(const Telemetry_t *telemetry,
                           uint8_t *packet_out,
                           size_t *packet_len_out) {
    if (!telemetry || !packet_out || !packet_len_out) {
        ESP_LOGE(TAG, "Invalid parameters for encode_event");
        return false;
    }

    /*
     * Event Packet Format:
     * [START(1)][TYPE(1)][LEN(1)][PAYLOAD][CRC8(1)][END(1)]
     */

    size_t idx = 0;

    packet_out[idx++] = PROTOCOL_START_MARKER;

    packet_out[idx++] = MSG_TYPE_EVENT;

    // Calculate event name length (exclude null terminator if present)
    size_t event_name_len = strnlen(telemetry->event, PROTOCOL_EVENT_NAME_LEN);
    if (event_name_len > PROTOCOL_EVENT_NAME_LEN) {
        event_name_len = PROTOCOL_EVENT_NAME_LEN;
    }

    // Payload length
    uint8_t payload_length = 4 + 2 + 1 + event_name_len + 1 + 1 + 2;  // Total payload
    packet_out[idx++] = payload_length;

    // --- PAYLOAD START ---

    // Timestamp
    pack_uint32_be(&packet_out[idx], telemetry->timestamp);
    idx += 4;

    // CO PPM
    uint16_t co_fixed = float_to_fixed16(telemetry->co_ppm);
    pack_uint16_be(&packet_out[idx], co_fixed);
    idx += 2;

    // Event name length
    packet_out[idx++] = (uint8_t)event_name_len;

    // Event name (without null terminator)
    memcpy(&packet_out[idx], telemetry->event, event_name_len);
    idx += event_name_len;

    // Flags
    uint8_t flags = 0;
    if (telemetry->alarm_active) flags |= (1 << 1); 
    if (telemetry->door_open) flags |= (1 << 2);
    packet_out[idx++] = flags;

    // State
    packet_out[idx++] = telemetry->state;

    // Reserved (2 bytes)
    packet_out[idx++] = 0x00;
    packet_out[idx++] = 0x00;

    // --- PAYLOAD END ---

    // CRC8
    uint8_t crc = crc8_calculate(&packet_out[1], idx - 1);
    packet_out[idx++] = crc;

    // End marker
    packet_out[idx++] = PROTOCOL_END_MARKER;

    *packet_len_out = idx;

    ESP_LOGD(TAG, "Encoded event packet (%zu bytes): event='%.*s', CO=%.2f", 
             idx, (int)event_name_len, telemetry->event, telemetry->co_ppm);

    return true;
}

// Encode Status Packet
bool protocol_encode_status(const Status_t *status, uint8_t *packet_out, size_t *packet_len_out) {
    if (!status || !packet_out || !packet_len_out) return false;

    size_t idx = 0;
    packet_out[idx++] = PROTOCOL_START_MARKER;
    packet_out[idx++] = MSG_TYPE_STATUS;
    uint8_t payload_length = 4; // Armed (1) + State (1) + Reserved (2)
    packet_out[idx++] = payload_length;

    packet_out[idx++] = status->armed ? 1 : 0;
    packet_out[idx++] = status->state;
    packet_out[idx++] = 0x00;
    packet_out[idx++] = 0x00;

    // CRC8
    uint8_t crc = crc8_calculate(&packet_out[1], idx - 1);
    packet_out[idx++] = crc;

    // End marker
    packet_out[idx++] = PROTOCOL_END_MARKER;

    *packet_len_out = idx;
    ESP_LOGD(TAG, "Encoded STATUS packet: armed=%d, state=%d", status->armed, status->state);
    return true;
}

bool protocol_verify_packet(const uint8_t *packet, size_t packet_len) {
    if (!packet || packet_len < 6) {  // Minimum: START + TYPE + LEN + CRC + END
        return false;
    }

    // Check start marker
    if (packet[0] != PROTOCOL_START_MARKER) {
        ESP_LOGW(TAG, "Invalid start marker: 0x%02X", packet[0]);
        return false;
    }

    // Check end marker
    if (packet[packet_len - 1] != PROTOCOL_END_MARKER) {
        ESP_LOGW(TAG, "Invalid end marker: 0x%02X", packet[packet_len - 1]);
        return false;
    }

    // Verify CRC8
    uint8_t received_crc = packet[packet_len - 2];
    uint8_t calculated_crc = crc8_calculate(&packet[1], packet_len - 3);

    if (received_crc != calculated_crc) {
        ESP_LOGW(TAG, "CRC mismatch: expected 0x%02X, got 0x%02X", 
                 calculated_crc, received_crc);
        return false;
    }

    return true;
}

void protocol_print_packet(const uint8_t *packet, size_t packet_len, const char *tag) {
    if (!packet || packet_len == 0) {
        return;
    }

    ESP_LOGI(tag, "Packet (%zu bytes):", packet_len);
    
    char hex_str[256];
    size_t hex_idx = 0;
    
    for (size_t i = 0; i < packet_len && hex_idx < sizeof(hex_str) - 4; i++) {
        hex_idx += snprintf(&hex_str[hex_idx], sizeof(hex_str) - hex_idx, 
                           "%02X ", packet[i]);
    }
    
    ESP_LOGI(tag, "%s", hex_str);

    if (packet_len >= 3) {
        ESP_LOGI(tag, "  START: 0x%02X, TYPE: 0x%02X, LEN: %d", 
                 packet[0], packet[1], packet[2]);
    }
    if (packet_len >= 6) {
        ESP_LOGI(tag, "  CRC: 0x%02X, END: 0x%02X", 
                 packet[packet_len - 2], packet[packet_len - 1]);
    }
}