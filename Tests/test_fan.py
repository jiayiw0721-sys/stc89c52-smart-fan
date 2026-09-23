"""Exercise actual fan policy and PWM code using mocked 8051 registers."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
s = (root / 'App/fan/fan.c').read_text()
s = s.replace('#include "fan.h"', '''#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t u8;
typedef uint16_t u16;
#define FAN_NO_ECHO 0xffffU
u8 T2CON, ET2, RCAP2H, RCAP2L, TH2, TL2, PT2, TR2, TF2;''')
header = (root / 'App/fan/fan.h').read_text().replace('#include "public.h"', '')
s = s.replace('u8 T2CON,', header + '\nu8 T2CON,')
s = s.replace('sbit FAN_OUT = P1^0;', 'u8 FAN_OUT;')
s = s.replace('interrupt 5 using 2', '')
s += r'''
static unsigned int frame_highs(void) {
    unsigned int i, high = 0;
    for(i=0;i<20;i++) { fan_timer2_isr(); high += FAN_OUT; }
    return high;
}
int main(void) {
    unsigned int d, previous, duty, i;
    assert(fan_next_duty(19)==8);
    assert(fan_next_duty(15)==12);
    assert(fan_next_duty(10)==16);
    assert(fan_next_duty(5)==20);
    for(previous=0;previous<=20;previous++) {
        for(d=20;d<=40;d++) assert(fan_next_duty(d)==10);
        assert(fan_next_duty(41)==0);
        assert(fan_next_duty(65535)==0);
    }
    for(d=1;d<20;d++) assert(fan_next_duty(d-1)>=fan_next_duty(d));
    for(duty=0;duty<=20;duty++) {
        fan_init(); fan_set_duty(duty);
        assert(frame_highs()==duty);
        assert(frame_highs()==duty);
    }
    fan_init(); fan_set_duty(20); fan_timer2_isr();
    assert(FAN_OUT==1);
    fan_set_duty(0); assert(FAN_OUT==0);
    for(i=0;i<40;i++) { fan_timer2_isr(); assert(FAN_OUT==0); }
    fan_init(); fan_set_duty(2); fan_timer2_isr(); fan_set_duty(18);
    for(i=1;i<20;i++) { fan_timer2_isr(); assert(FAN_OUT==(i<2)); }
    assert(frame_highs()==18);
    fan_init(); fan_set_duty(255); assert(frame_highs()==20);
    fan_init();
    assert(fan_control_update(10,0,0)==16);
    assert(fan_control_update(10,FAN_KEY_POWER,1)==0 && !fan_enabled);
    assert(fan_control_update(5,0,0)==0);
    assert(fan_control_update(5,FAN_KEY_FIXED,1)==12 && fan_enabled && fan_mode==FAN_MODE_FIXED);
    assert(fan_control_update(65535,FAN_KEY_POWER,1)==0 && !fan_enabled);
    assert(fan_control_update(65535,FAN_KEY_POWER,1)==12 && fan_enabled);
    assert(fan_control_update(100,0,0)==12);
    assert(fan_control_update(30,FAN_KEY_AUTO,1)==10 && fan_mode==FAN_MODE_AUTO);
    assert(fan_control_update(41,0,0)==0);
    assert(fan_control_update(5,0,0)==20);
    assert(fan_control_update(65535,0,0)==20);
    milliseconds += FAN_ECHO_GRACE_MS;
    assert(fan_control_update(65535,0,0)==0);
    assert(fan_control_update(19,FAN_KEY_FIXED,1)==12);
    assert(fan_control_update(19,0x99,1)==12);
    assert(fan_control_update(19,FAN_KEY_POWER,0)==12);
    assert(fan_control_update(19,FAN_KEY_AUTO,1)==8);
    assert(fan_control_update(19,FAN_KEY_POWER,1)==0);
    assert(fan_control_update(30,FAN_KEY_POWER,1)==10);
    fan_init();
    for(i=0;i<4000;i++) fan_timer2_isr();
    assert(fan_millis()==1000 && ET2==1);
    /* Regression: a transient timeout followed by the hold band used to stop. */
    fan_init();
    assert(fan_control_update(10,0,0)==16);
    assert(fan_control_update(65535,0,0)==16);
    milliseconds += 200;
    assert(fan_control_update(20,0,0)==10);
    assert(fan_control_update(30,0,0)==10);
    assert(fan_control_update(40,0,0)==10);
    assert(fan_control_update(41,0,0)==0);
    assert(fan_control_update(30,0,0)==10);
    fan_init();
    assert(fan_control_update(65535,0,0)==0);
    assert(fan_control_update(15,0,0)==12);
    milliseconds = 65500;
    assert(fan_control_update(65535,0,0)==12);
    milliseconds = (u16)(65500U + 599U);
    assert(fan_control_update(65535,0x99,1)==12);
    milliseconds = (u16)(65500U + 600U);
    assert(fan_control_update(65535,0,0)==0);
    assert(fan_control_update(10,0,0)==16);
    assert(fan_control_update(65535,FAN_KEY_POWER,1)==0);
    assert(fan_control_update(65535,FAN_KEY_FIXED,1)==12);
    fan_init();
    assert(fan_control_update(30,0,0)==10);
    assert(fan_control_update(41,0,0)==0);
    assert(fan_control_update(40,0,0)==10);
    assert(fan_control_update(30,FAN_KEY_POWER,1)==0 && !fan_enabled);
    assert(fan_control_update(20,0,0)==0 && !fan_enabled);
    puts("PASS: 20..40 cm fixed 50%, key 0 fixed 60%; stopped-to-hold starts at 50%, power OFF remains OFF; hold after transient echo loss, sustained-loss stop, timer wrap; timer clock; power priority, modes, unknown/repeat commands, mode restoration; boundaries, hold band, inverse distance response, all PWM duties, immediate stop, frame updates, clamp");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='fan_test_') as directory:
    p = Path(directory)
    (p/'test.c').write_text(s)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
    subprocess.run([str(p/'test.exe')],check=True)
