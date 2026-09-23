#include "lcd1602.h"
#include <intrins.h>

/* The board's seven-segment display shares P0 with the LCD.
 * Keep P0 blank outside short LCD transfers; do not leave character bytes
 * driving the segments throughout the main-loop delay.
 */
static void lcd1602_bus_write(u8 value, u8 is_data)
{
    u8 interrupts_enabled;
    interrupts_enabled = EA;
    EA = 0;
    LCD_1602_E = 0;
    LCD_1602_RS = is_data;
    LCD_1602_RW = 0;
    LCD1602_DATAPORT = value;
    _nop_();
    _nop_();
    LCD_1602_E = 1;
    _nop_();
    _nop_();
    _nop_();
    LCD_1602_E = 0;
    _nop_();
    _nop_();
    LCD1602_DATAPORT = 0x00;
    EA = interrupts_enabled;
}

void lcd1602_write_cmd(u8 cmd)
{
    lcd1602_bus_write(cmd, 0);
    delay_ms(2);
}

void lcd1602_write_data(u8 dat)
{
    lcd1602_bus_write(dat, 1);
    delay_ms(1);
}

void lcd1602_init(void)
{
    delay_ms(50);
    lcd1602_write_cmd(0x38);
    delay_ms(5);
    lcd1602_write_cmd(0x38);
    delay_ms(5);
    lcd1602_write_cmd(0x38);
    lcd1602_write_cmd(0x0c);
    lcd1602_write_cmd(0x06);
    lcd1602_write_cmd(0x01);
}

void lcd1602_clear(void)
{
    lcd1602_write_cmd(0x01);
}

void lcd1602_show_string(u8 x, u8 y, u8 *str)
{
    if(y > 1 || x > 15) return;
    lcd1602_write_cmd((y ? 0xc0 : 0x80) + x);
    while(*str && x < 16)
    {
        lcd1602_write_data(*str++);
        x++;
    }
}
