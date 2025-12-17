#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ring_buffer.h"
#include "protocol_types.h"

/* Framing markers */
#define PROTOCOL_START_MARKER   0xAA
#define PROTOCOL_END_MARKER     0x55

/* Message types */
#define MSG_TYPE_TELEMETRY      0x01
#define MSG_TYPE_EVENT          0x02
#define MSG_TYPE_STATUS         0x03
#define MSG_TYPE_HEARTBEAT      0x04
#define MSG_TYPE_COMMAND        0x10

/* Limits */
#define PROTOCOL_MAX_PACKET_SIZE    64
#define PROTOCOL_EVENT_NAME_LEN     16


//Encode telemetry data into a binary protocol packet
bool protocol_encode_telemetry(const Telemetry_t *telemetry,
                               uint8_t *packet_out,
                               size_t *packet_len_out);


 // Encode event telemetry into a binary protocol packet
bool protocol_encode_event(const Telemetry_t *telemetry,
                           uint8_t *packet_out,
                           size_t *packet_len_out);


 //Verify packet framing and checksum
bool protocol_verify_packet(const uint8_t *packet, size_t packet_len);


//Print packet contents in hexadecimal -> debug only
void protocol_print_packet(const uint8_t *packet,
                           size_t packet_len,
                           const char *tag);

bool protocol_encode_status(const Status_t *status, uint8_t *packet_out, size_t *packet_len_out);

bool protocol_decode_command(const uint8_t *packet, size_t packet_len, uint8_t *command_out);

#endif // PROTOCOL_H