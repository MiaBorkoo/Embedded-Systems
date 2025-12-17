#include "crc8.h"

uint8_t crc8_calculate(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;  //initailising to all zeros -> standard for crc8
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];  // XOR byte into CRC
        
        //processes each bit
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {  // If MSB is set
                crc = (crc << 1) ^ 0x07;  // Shift and XOR with polynomial
            } else {
                crc = crc << 1;  //just shift
            }
        }
    }
    
    return crc;
}