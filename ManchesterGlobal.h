//
//  ManchesterGlobal.h
//
//
//  Created by Peter Goldsborough on 02/06/14.
//
//

#ifndef _ManchesterGlobal_h
#define _ManchesterGlobal_h

#define F_CPU 8000000UL

/* 2500 bits per second*/
#define BAUD_RATE 2500

/* The time for one bit in µs/cycles */
#define BIT_TIME 400

#define HALF_TIME 200

/* The time between samples in µs/cycles */
#define SAMPLE_TIME 100

/* The number of samples per data unit */
#define DATA_SAMPLES 32

#define SET_BIT(byte, bit) ((byte) |= (1UL << (bit)))

#define CLEAR_BIT(byte,bit) ((byte) &= ~(1UL << (bit)))

#define IS_SET(byte,bit) (((byte) & (1UL << (bit))) >> (bit))

#endif
