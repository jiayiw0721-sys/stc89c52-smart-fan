#include "public.h"
#include "lcd1602.h"
#include "ired.h"
#include "fan.h"

/* STC89C52RC, 12 MHz / 12T. Timer0: IR; Timer1: ultrasonic.
 * LCD wiring is taken from the supplied lcd1602.h, not guessed.
 */
sbit HC_TRIG = P1^4; /* Move TRIG wire from P1.0 to P1.4. */
sbit MOTOR_OFF = P1^0;
sbit HC_ECHO = P1^1;
#define NO_ECHO 0xffffU
static u8 code hex_digits[] = "0123456789ABCDEF";

static void echo_timer_start(void)
{
    TR1 = 0;
    TH1 = 0;
    TL1 = 0;
    TF1 = 0;
    TR1 = 1;
}

static u16 read_distance(void)
{
    u16 ticks;
    echo_timer_start();
    while(HC_ECHO)
        if(TF1 || TH1 >= 0x75) goto failed;
    HC_TRIG = 1;
    echo_timer_start();
    while(TH1 == 0 && TL1 < 20);
    HC_TRIG = 0;
    echo_timer_start();
    while(!HC_ECHO)
        if(TF1 || TH1 >= 0x75) goto failed;
    echo_timer_start();
    while(HC_ECHO)
        if(TF1 || TH1 >= 0x75) goto failed;
    TR1 = 0;
    if(TF1) goto failed;
    ticks = ((u16)TH1 << 8) | TL1;
    return (ticks + 29U) / 58U;
failed:
    TR1 = 0;
    TF1 = 0;
    HC_TRIG = 0;
    return NO_ECHO;
}

/* Screen shadow: change only differing characters; never clear during use. */
static u8 idata screen[32];
static u8 idata painted[32];
static u8 paint_pos = 0;

static void prepare_screen(u16 distance, u8 duty, u8 command, u8 has_command)
{
    u8 i;
    u8 percent = duty * 5;
    u8 top[17] = "D:---cm IR:--   ";
    u8 bottom[17] = "ON  AUTO 000%   ";
    if(distance != NO_ECHO && distance <= 999U)
    {
        top[2] = '0' + distance / 100;
        top[3] = '0' + (distance / 10) % 10;
        top[4] = '0' + distance % 10;
    }
    if(has_command)
    {
        top[11] = hex_digits[command >> 4];
        top[12] = hex_digits[command & 15];
    }
    if(!fan_enabled)
    { bottom[0] = 'O'; bottom[1] = 'F'; bottom[2] = 'F'; }
    if(fan_mode == FAN_MODE_FIXED)
    { bottom[4] = 'F'; bottom[5] = 'I'; bottom[6] = 'X'; bottom[7] = ' ';  }
    else if(fan_enabled && distance >= 20U && distance <= 40U)
    { bottom[4] = 'H'; bottom[5] = 'O'; bottom[6] = 'L'; bottom[7] = 'D'; }
    bottom[9] = '0' + percent / 100;
    bottom[10] = '0' + (percent / 10) % 10;
    bottom[11] = '0' + percent % 10;
    for(i = 0; i < 16; i++)
    {
        screen[i] = top[i];
        screen[i + 16] = bottom[i];
    }
}

static void flush_one_character(void)
{
    u8 i, index;
    u8 character[2];
    for(i = 0; i < 32; i++)
    {
        index = paint_pos;
        paint_pos = (paint_pos + 1) & 31;
        if(screen[index] != painted[index])
        {
            character[0] = screen[index];
            character[1] = 0;
            lcd1602_show_string(index & 15, index >> 4, character);
            painted[index] = character[0];
            break;
        }
    }
}

void main(void)
{
    u8 command = 0;
    u8 has_command = 0;
    u8 duty = 0;
    u8 i;
    u8 accept;
    u8 last_key = 0;
    u8 key_seen = 0;
    u16 now;
    u16 last_key_time = 0;
    u16 last_measure = 0;
    u16 distance = NO_ECHO;
    EA = 0;
    MOTOR_OFF = 0;
    IE = 0;
    IP = 0;
    TR0 = 0;
    TR1 = 0;
    T2CON = 0;
    TMOD = 0x11;
    HC_TRIG = 0;
    HC_ECHO = 1;
    LCD_1602_E = 0;
    LCD_1602_RS = 0;
    LCD_1602_RW = 0;
    P0 = 0;
    lcd1602_init();
    fan_init();
    ired_init();
    for(i = 0; i < 32; i++) painted[i] = 0xff;
    prepare_screen(distance, duty, 0, 0);
    while(1)
    {
        now = fan_millis();
        if(ired_read(&command))
        {
            accept = 1;
            /* Suppress full-frame power repeats as well as NEC short repeats.
             * Refresh the timestamp on every frame while a key is held.
             */
            if(command == FAN_KEY_POWER && key_seen && last_key == command
               && (u16)(now - last_key_time) < 250U) accept = 0;
            last_key = command;
            last_key_time = now;
            key_seen = 1;
            has_command = 1;
            if(accept) duty = fan_control_update(distance, command, 1);
            prepare_screen(distance, duty, command, has_command);
        }
        if((u16)(now - last_measure) >= 150U)
        {
            distance = read_distance();
            last_measure = fan_millis();
            duty = fan_control_update(distance, 0, 0);
            prepare_screen(distance, duty, command, has_command);
        }
        flush_one_character();
    }
}
