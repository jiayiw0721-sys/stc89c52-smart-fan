"""Run the real NEC decoder against synthetic edge timings on the host.
This checks decoding logic, not physical pins, clock mode or interrupt latency.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'App/ired/ired.c').read_text()
source = source.replace('#include "ired.h"', '''#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t u8;
typedef uint16_t u16;
u8 EX0, IRED, TR0, ET0, TMOD, TH0, TL0, TF0, IT0, IE0, PX0, EA;''')
source = source.replace('interrupt 0 using 1', '')
source += r'''
static void edge(unsigned int us, int overflow) {
    TH0 = us >> 8; TL0 = us & 255; TF0 = overflow; ired();
}
static void data_bytes(u8 cmd, u8 inverse, unsigned int zero, unsigned int one) {
    u8 bytes[4] = {0x00, 0xff, cmd, inverse};
    unsigned int i, bit;
    for(i=0;i<4;i++) for(bit=0;bit<8;bit++)
        edge((bytes[i] & (1U << bit)) ? one : zero, 0);
}
int main(void) {
    unsigned int cmd;
    u8 result = 0;
    ired_init();
    assert(!ired_read(&result));
    for(cmd=0;cmd<256;cmd++) {
        edge(0, 1); edge(13500, 0);
        data_bytes(cmd, (u8)~cmd, 1125, 2250);
        assert(ired_read(&result) && result == cmd);
        assert(!ired_read(&result));
    }
    edge(0,1); edge(13500,0); data_bytes(12,12,1125,2250);
    assert(!ired_read(&result));
    edge(0,1); edge(11250,0); edge(1125,0);
    assert(!ired_read(&result));
    edge(13500,0); edge(1125,0); edge(4000,0);
    data_bytes(12,243,1125,2250); assert(!ired_read(&result));
    edge(13500,0); edge(1125,0); edge(0,1);
    data_bytes(12,243,1125,2250); assert(!ired_read(&result));
    edge(13500,0); data_bytes(12,243,1000,2000);
    assert(ired_read(&result) && result == 12);
    edge(13500,0); data_bytes(12,243,1300,2450);
    assert(ired_read(&result) && result == 12);
    edge(0,1); edge(27000,0); data_bytes(0x45,0xba,2250,4500);
    assert(ired_read(&result) && result==0x45);
    edge(0,1); edge(12442,0); data_bytes(0x16,0xe9,1037,2074);
    assert(ired_read(&result) && result==0x16);
    for(cmd=0;cmd<5;cmd++) {
        edge(0,1); edge(13500,0); data_bytes(cmd,(u8)~cmd,1125,2250);
    }
    for(cmd=0;cmd<4;cmd++) assert(ired_read(&result) && result==cmd);
    assert(!ired_read(&result));
    puts("PASS: calibrated clocks and command queue; all 256 commands, mailbox, inverse check, repeat, noise, timeout, recovery, timing tolerance");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='nec_test_') as folder:
    test = Path(folder) / 'test.c'
    exe = Path(folder) / 'test.exe'
    test.write_text(source)
    subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', str(test), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
