"""Integration: real decoder -> controller -> LCD renderer, mocked hardware."""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
def clean(text):
    text=re.sub(r'^#include.*$', '', text, flags=re.M)
    text=re.sub(r'sbit\s+(\w+)\s*=\s*P\d\^\d;', r'u8 \1;', text)
    text=re.sub(r'interrupt\s+\d+\s+using\s+\d+', '', text)
    text=re.sub(r'\b(code|idata)\b', '', text)
    return text
s='''#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t u8;
typedef uint16_t u16;
u8 EA,IE,IP,P0,EX0,IRED,TR0,ET0,TMOD,TH0,TL0,TF0,IT0,IE0,PX0;
u8 TR1,TH1,TL1,TF1,T2CON,ET2,RCAP2H,RCAP2L,TH2,TL2,PT2,TR2,TF2;
u8 LCD_1602_E,LCD_1602_RS,LCD_1602_RW;
static u8 panel[32];
static unsigned int writes;
void lcd1602_show_string(u8 x,u8 y,u8 *str) {
    assert(x<16 && y<2);
    while(*str && x<16) { panel[y*16+x++]=*str++; writes++; }
}
void lcd1602_init(void) {}
void delay_ms(u16 ms) {(void)ms;}
'''
s+=clean((root/'App/fan/fan.h').read_text())
s+=clean((root/'App/fan/fan.c').read_text())
s+=clean((root/'App/ired/ired.c').read_text())
s+=clean((root/'User/main.c').read_text()).replace('void main(void)', 'void firmware_main(void)')
s+=r'''
static void edge(u16 time,u8 overflow) {
    TH0=time>>8; TL0=time&255; TF0=overflow; ired();
}
static void press(u8 cmd,u16 distance) {
    u8 bytes[4]={0,255,cmd,(u8)~cmd},decoded=0,duty;
    unsigned int i,b;
    edge(0,1); edge(13500,0);
    for(i=0;i<4;i++) for(b=0;b<8;b++) edge((bytes[i]&(1U<<b))?2250:1125,0);
    assert(ired_read(&decoded) && decoded==cmd);
    duty=fan_control_update(distance,decoded,1);
    prepare_screen(distance,duty,decoded,1);
    for(i=0;i<32;i++) flush_one_character();
}
int main(void) {
    unsigned int before,i;
    fan_init(); ired_init(); memset(painted,255,32);
    press(0x45,10);
    assert(!fan_enabled && requested_duty==0);
    assert(!memcmp(panel,"D:010cm IR:45   ",16));
    assert(!memcmp(panel+16,"OFF AUTO 000%   ",16));
    press(0x16,65535);
    assert(fan_enabled && requested_duty==12);
    assert(!memcmp(panel,"D:---cm IR:16   ",16));
    assert(!memcmp(panel+16,"ON  FIX  060%   ",16));
    press(0x45,65535); assert(!fan_enabled && requested_duty==0);
    press(0x0c,10);
    assert(fan_enabled && requested_duty==16);
    assert(!memcmp(panel+16,"ON  AUTO 080%   ",16));
    assert(fan_control_update(65535,0,0)==16);
    assert(fan_control_update(30,0,0)==10);
    prepare_screen(30,10,0x0c,1);
    for(i=0;i<32;i++) flush_one_character();
    assert(!memcmp(panel+16,"ON  HOLD 050%   ",16));
    prepare_screen(10,16,0x0c,1);
    for(i=0;i<32;i++) flush_one_character();
    before=writes;
    prepare_screen(10,16,0x0c,1);
    for(i=0;i<32;i++) flush_one_character();
    assert(writes==before);
    fan_init();
    assert(fan_control_update(30,0,0)==10);
    prepare_screen(30,10,0,0);
    for(i=0;i<32;i++) flush_one_character();
    assert(!memcmp(panel+16,"ON  HOLD 050%   ",16));
    puts("PASS: default HOLD 050%; 45 OFF, 16 ON/FIX/060%, 0C ON/AUTO, exact LCD text, no writes for unchanged screen");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='remote_lcd_') as directory:
    p=Path(directory)
    (p/'test.c').write_text(s)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
    subprocess.run([str(p/'test.exe')],check=True)
