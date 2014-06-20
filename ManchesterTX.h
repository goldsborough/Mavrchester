#ifndef _Manchester_h
#define _Manchester_h

#include <stdint.h>

#define TX_PORT PORTB
#define TX_PIN PB3

void initTX(void);

void sendByte(const uint8_t byte);

void sendData(const uint8_t* data, uint16_t bytes);

uint16_t computeChecksum(const uint8_t* data, uint16_t bytes);

#endif