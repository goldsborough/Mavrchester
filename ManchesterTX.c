#include "ManchesterTX.h"
#include "ManchesterGlobal.h"

#include <avr/io.h>

int main(void)
{
    uint8_t msg [] = "Hello World!";

	initTX();
    
    sendData(msg,12);
    
	return 0;
}

void initTX(void)
{
    /* Set TX_PIN as output */
    SET_BIT(DDRB,DDB3);
	
	/* Enable TCNT0 with clk/8 pre-scaling */
	SET_BIT(TCCR0B,CS01);
}

uint16_t computeChecksum(const uint8_t* data, uint16_t bytes)
{
    /* * * * * * * * * * * * * * * * * * * * * * * * * *
     *                                                 *
     *  Apparently 0xFF (255) is preferred over 0      *
     *  because it provides better shielding against   *
     *  overflow and prevents the checksum from ever   *
     *  becoming zero, therefore providing a checksum  *
     *  computed flag when the returned value is non-  *
     *  zero. Taken from here: http://goo.gl/Cjt4AO    *
     *                                                 *
     * * * * * * * * * * * * * * * * * * * * * * * * * */
    
    uint16_t sum1 = 0xFF;
    uint16_t sum2 = 0xFF;
    
    uint16_t wordLen;
    
    while (bytes)
    {
        /* maximum word size before overflow would occur */
        wordLen = (bytes > 20) ? 20 : bytes;
        
        bytes -= wordLen;
        
        do
        {
            sum1 += *(data++);
            sum2 += sum1;
        }
        
        while (--wordLen);
        
        /* * * * * * * * * * * * * * * * * * * * * *
         * Do first modular reduction: mask off
         * lower byte and add right shifted upper
         * byte to it
         * * * * * * * * * * * * * * * * * * * * * */
        
        sum1 = (sum1 & 0xFF) + (sum1 >> 8);
        sum2 = (sum2 & 0xFF) + (sum2 >> 8);
    }
    
    /* Safety reduction */
    sum1 = (sum1 & 0xFF) + (sum1 >> 8);
    sum2 = (sum2 & 0xFF) + (sum2 >> 8);
    
    /* Return both sums in a 16 bit value */
    return sum2 << 8 | sum1;
}

void sendData(const uint8_t* data, uint16_t bytes)
{
    uint8_t i;
    uint16_t chksm;
    
    /* Compute the checksum for the given data */
    chksm = computeChecksum(data,bytes);
    
    /* Send lots of 10s to let the receiver adjust to the signal */
    for(i = 0; i < 75; ++i)
    { sendByte(0); }
    
    /* Send last 10s and the two 01 start pulses */
    sendByte(3);
    
    /* Send data byte for byte */
    for (i = 0; i < bytes; ++i)
    { sendByte(data[i]); }
    
    /* Append checksum, upper and lower byte */
    sendByte((chksm >> 8));
    sendByte((chksm & 0xFF)); // you can leave the & 0xFF away here if you want
    
    /* No more bits are sent but the last bit needs to finish delaying, otherwise if the
     * last bit is a one it won't be recognized because I'm turning the TX off after, which
     * would turn it into a 0 (Have to turn it off, else the LED will stay on). */
    
    while(TCNT0 < HALF_TIME);
    
    /* Clear pin after */
    CLEAR_BIT(TX_PORT,TX_PIN);
}

void sendByte(const uint8_t byte)
{
    int8_t bit = 7;
    
    for (; bit >= 0; --bit)
    {
        if ( IS_SET(byte,bit) )
        {
            /* * * * * * * * * * * * * * * * * * *
             * Set bit low, wait for mid-period,
             * then set high again and wait for
             * end of period. This indicates a
             * high/set bit according to Manchester
             * Encoding.
             * * * * * * * * * * * * * * * * * * */
            
            while(TCNT0 < HALF_TIME);
            CLEAR_BIT(TX_PORT,TX_PIN);
            
            TCNT0 = 0;
			
            while(TCNT0 < HALF_TIME);
            SET_BIT(TX_PORT,TX_PIN);
            
            TCNT0 = 1;
        }
        
        else
        {
            /* * * * * * * * * * * * * * * * * * *
             * Set bit high, wait for mid-period,
             * then set low again and wait for
             * end of period. This indicates a
             * low/cleared bit according to
             * Manchester Encoding.
             * * * * * * * * * * * * * * * * * * */
            
			while(TCNT0 < HALF_TIME);
            SET_BIT(TX_PORT,TX_PIN);
            
            TCNT0 = 0;
			
            while(TCNT0 < HALF_TIME);
            CLEAR_BIT(TX_PORT,TX_PIN);
            
            TCNT0 = 1;
        }
        
    }
    
}