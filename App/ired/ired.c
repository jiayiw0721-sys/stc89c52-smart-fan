#include "ired.h"

/* NEC/extended NEC, 12 MHz / 12T: falling-edge intervals in microseconds.
 * No busy waits inside INT0, so the display can continue refreshing.
 */
volatile u8 gired_data[4];
static volatile u8 ready = 0;
static volatile u8 commands[4];
static volatile u8 read_index = 0;
static volatile u8 write_index = 0;
static u16 bit_unit = 562;
static u8 receiving = 0;
static u8 bit_count = 0;
static u8 frame[4];

void ired_init(void)
{
    EX0 = 0;
    IRED = 1;
    TR0 = 0;
    ET0 = 0;
    TMOD = (TMOD & 0xf0) | 0x01;
    TH0 = 0;
    TL0 = 0;
    TF0 = 0;
    ready = 0;
    read_index = write_index = 0;
    receiving = 0;
    TR0 = 1;
    IT0 = 1;
    IE0 = 0;
    PX0 = 1;
    EX0 = 1;
    EA = 1;
}

u8 ired_read(u8 *command)
{
    u8 available;
    EX0 = 0;
    available = ready;
    if(available)
    {
        *command = commands[read_index];
        read_index = (read_index + 1) & 3;
        ready--;
    }
    EX0 = 1;
    return available;
}

void ired(void) interrupt 0 using 1
{
    u16 elapsed;
    u8 overflow;
    u8 index;
    TR0 = 0;
    elapsed = ((u16)TH0 << 8) | TL0;
    overflow = TF0;
    TH0 = 0;
    TL0 = 0;
    TF0 = 0;
    TR0 = 1;
    if(overflow)
    {
        receiving = 0;
        return;
    }
    if(elapsed >= 11500U && elapsed <= 30000U)
    {
        /* Calibrate bit timing from the 24-unit NEC leader interval.
         * Accommodate timer-clock variation without changing key codes.
         */
        bit_unit = elapsed / 24U;
        receiving = 1;
        bit_count = 0;
        for(index = 0; index < 4; index++) frame[index] = 0;
        return;
    }
    if(!receiving) return;
    index = bit_count >> 3;
    if(elapsed >= bit_unit * 3U && elapsed <= bit_unit * 5U)
        frame[index] |= (u8)(1U << (bit_count & 7));
    else if(elapsed < bit_unit * 3U / 2U || elapsed > bit_unit * 5U / 2U)
    {
        receiving = 0;
        return;
    }
    bit_count++;
    if(bit_count == 32)
    {
        receiving = 0;
        /* Accept standard and extended addresses; require command inverse.
         * Explicit byte XOR avoids integer-promotion errors with ~byte.
         */
        if((u8)(frame[2] ^ frame[3]) == 0xff)
        {
            for(index = 0; index < 4; index++) gired_data[index] = frame[index];
            /* Preserve earlier presses if the main loop is measuring. */
            if(ready < 4)
            {
                commands[write_index] = frame[2];
                write_index = (write_index + 1) & 3;
                ready++;
            }
        }
    }
    /* Short NEC repeat frames leave the previous displayed key unchanged. */
}
