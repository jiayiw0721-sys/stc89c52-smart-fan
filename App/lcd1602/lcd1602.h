#ifndef _lcd1602_H
#define _lcd1602_H

#include "public.h"

sbit LCD_1602_RS=P2^6;
sbit LCD_1602_RW=P2^5;
sbit LCD_1602_E=P2^7;
#define LCD1602_DATAPORT P0

void lcd1602_init(void);
void lcd1602_show_string(u8 x,u8 y,u8 *str);
void lcd1602_clear(void);

#endif