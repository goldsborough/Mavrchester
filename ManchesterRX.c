//
//  ManchesterRX.c
//  ManchesterRX
//
//  Created by Peter Goldsborough on 02/06/14.
//  Copyright (c) 2014 Peter Goldsborough. All rights reserved.
//

#include "ManchesterRX.h"
#include "ManchesterGlobal.h"

#include <avr/io.h>
#include <avr/interrupt.h>

volatile uint32_t samples = 0;

volatile uint8_t samplesReady = 0;

volatile int8_t sampleCount = DATA_SAMPLES - 1;

uint8_t receiving = 0, data = 0, connectionCount = 0;

int main(void)
{
    struct Queue q = {.size = 0};
    
    uint16_t chksm;
    
    initRX();
    
    startRX();
     
     for (;;)
     {
         if (receiving && samplesReady && interpretSamples(getSamples()))
         {
                pushQ(&q,data);
     
                if (q.size == 14) break;
         }
     }
     
     stopRX();
     
     chksm = computeChecksum(q.data,12);
     
     CLEAR_BIT(PORTB,LED);
     
     if (chksm == ((q.data[12] << 8) | q.data[13])) SET_BIT(PORTB,LED);
     
     return 0;
    
}

int8_t popQ(struct Queue* queue)
{
    unsigned char i = 0, j = 1;
    char item = queue->data[0];
    
    /* Push all items one further in the queue */
    for (; j < queue->size; ++j, ++i)
    { queue->data[i] = queue->data[j]; }
    
    /* Delete last item */
    queue->data[--queue->size] = 0;
    
    return item;
}

void pushQ(struct Queue* queue, const uint8_t c)
{
    if (queue->size < BUFFER_SIZE)
    { queue->data[queue->size++] = c; }
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

uint32_t getSamples(void)
{
    /* Reset flag */
    samplesReady = 0;
    
    return samples;
}

void initRX(void)
{
    /* Set the RX pin as input */
    CLEAR_BIT(DDRB,DDB3);
    
    /* Set LED as output */
    SET_BIT(DDRB,DDB4);
    
    /* Enable pin change interrupt */
    SET_BIT(GIMSK,PCIE);
    
    /* Watch pin change on pin 3 */
    SET_BIT(PCMSK,PCINT3);
    
    /* Enable Timer1 with clk/4096 prescaling. Timer1 is used for timing
     out the connection pipeline. */
    TCCR1 |= 0x0D;
    
    /* Enable the overflow interrupt for Timer1 */
    SET_BIT(TIMSK,TOIE1);
    
    /* Sample every 100 µs */
    OCR0A = SAMPLE_TIME;
	
    /* Enable the output compare interrupt for Timer0 */
    SET_BIT(TIMSK,OCIE0A);
    
    /* We are not using the Clear Timer on Compare Match feature. The reason
     is that it's not actually the ISR call that needs to be called regularly,
     but the sampling, therefore we clear the timer after the sample and don't
     let the timer be cleared automatically. */
    
    /* Turn on Timer0 with clk/8 prescaling, gives 1µs per cycle @8Mhz.
     Timer0 is used for sampling */
    SET_BIT(TCCR0B,CS01);
}

void startRX(void)
{
    uint8_t preambleBit, lows = 0, highs = 0;
    
    //SET_BIT(PORTB,LED);
    
    /* Enable global interrupts */
	asm("sei");
    
    /* Reset */
    TCNT0 = TCNT1 = 0;
    
    /* Enable while waiting for the preamble. Is turned false when the connection
     times out. */
    receiving = 1;
    
    /* Going to record individual bits now */
    sampleCount = 3;
    
    /* Reset this. Otherwise all RX attempts will fail after the first one. Not sure why. */
    samples = 0;
    
    /* receiving becomes false if the connection times out, so this will not go on forever */
    while (receiving)
    {
        if (samplesReady)
        {
            /* Grab bit */
            preambleBit = getSamples();
            
            sampleCount = 3;
            
            /* The preamble consists of 6 low bits (10) and 2 high bits (01), everything
             else or any other order than that resets the counter values to 0 */
            
            if (preambleBit == LOW)
            {
                if (!highs) ++lows;
                
                else lows = highs = 0;
            }
            
            else if (preambleBit == HIGH)
            {
                if (lows >= 6)
                {
                    if (++highs >= 2) break;
                }
                
                else lows = highs = 0;
            }
            
            else
            {
                lows = highs = 0;
                
                /*
                 * If there's a one-sample error, make the next samples finish earlier so the
                 * next sampling starts one sample earlier. This means that it takes at most
                 * 3 samplings to get valid bits. The timing is not a problem since the Pin
                 * change interrupt makes sure that any pin changes occur exactly between two
                 * samples, therefore there can only be sample errors and not timing errors
                 * (I think).
                 */
                
                sampleCount = 2;
            }
        }
    }
    
    /* Normal sampling now */
    sampleCount = DATA_SAMPLES - 1;
}


void stopRX(void)
{
    /* Stop the wait for the preamble if it's still running,
     else stop waiting for samples in main */
    receiving = 0;
    
    /* Disable sampling Timer/Counter */
    CLEAR_BIT(TIMSK,OCIE0A);
    
    //CLEAR_BIT(PORTB,LED);
}

uint8_t interpretSamples(const uint32_t samps)
{
    int8_t i = 7;
    
    uint8_t bit;
    
    for (; i >= 0; --i)
    {
        /* Grab the current bit */
        bit = (samps >> (i*4)) & 0x0F;
        
        if (bit == HIGH) SET_BIT(data,i);
        
        else if (bit == LOW) CLEAR_BIT(data,i);
        
        else return 0;
    }
    
    return 1;
}

ISR(TIMER1_OVF_vect)
{
    if (++connectionCount >= CONNECTION_TIME)
    {
        connectionCount = 0;
        
        stopRX();
    }
}

ISR(PCINT0_vect)
{
    /* Set the timer to what it should be at the pin change. 52 instead
     of 50 because of ISR-call overhead. */
	TCNT0 = 52;
}

ISR(TIMER0_COMPA_vect)
{
    /* If the current sample reads high, set the current bit in the samples */
    if (IS_SET(RX_PORT,RX_PIN))
    {
        /* Reset timer now, after sampling */
        TCNT0 = 1;
        
        SET_BIT(samples,sampleCount);
    }
    
    /* Else clear the current bit */
    else
    {
        /* Reset timer now, after sampling */
        TCNT0 = 1;
        
        CLEAR_BIT(samples,sampleCount);
    }
    
    /* If the bit is finished, set the samplesReady flag */
    if (! sampleCount--)
	{
		samplesReady = 1;
		sampleCount = DATA_SAMPLES - 1;
	}
}
