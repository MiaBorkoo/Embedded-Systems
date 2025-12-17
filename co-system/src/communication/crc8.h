#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>
#include <stddef.h>

//CRC is a checksum algorithm to detect inconsistency of data, e.g. bit errors during data transmission
//A checksum, calculated by CRC, is attached to the data to help the receiver to detect such errors


uint8_t crc8_calculate(const uint8_t *data, size_t length); //length is for amount of bits we wantto process

#endif // CRC8_H