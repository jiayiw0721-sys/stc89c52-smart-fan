#include "fan.h"

/* Same active-high P1.0 control as the user's 09 motor experiment.
 * J47 OUT1 must be driven by this signal through the board driver.
 * HC-SR04 TRIG is moved to P1.4 to avoid sharing P1.0.
 */
sbit FAN_OUT = P1^0;
static volatile u8 requested_duty = 0;
static u8 active_duty = 0;
static u8 phase = 19;
u8 fan_enabled = 1;
u8 fan_mode = FAN_MODE_AUTO;
static u8 control_duty = 0;
static volatile u16 milliseconds = 0;
static u8 clock_divider = 0;
static u8 echo_missing = 0;
static u16 echo_missing_since = 0;

/* A numeric mode key starts that mode, including from power OFF. */
u8 fan_control_update(u16 distance, u8 command, u8 has_command)
{
    if(has_command)
    {
        if(command == FAN_KEY_POWER) fan_enabled = !fan_enabled;
        else if(command == FAN_KEY_FIXED)
        { fan_mode = FAN_MODE_FIXED; fan_enabled = 1; }
        else if(command == FAN_KEY_AUTO)
        { fan_mode = FAN_MODE_AUTO; fan_enabled = 1; }
    }
    if(!fan_enabled)
    {
        control_duty = 0;
        echo_missing = 0;
    }
    else if(fan_mode == FAN_MODE_FIXED)
    {
        control_duty = 12; /* Fixed 60 percent. */
        echo_missing = 0;
    }
    else if(distance == FAN_NO_ECHO)
    {
        /* A single missed echo must not erase the speed held at 20..40 cm.
         * Stop only if the missing-echo condition persists for 600 ms.
         * Repeated key events must not restart this timeout.
         */
        if(!echo_missing)
        {
            echo_missing = 1;
            echo_missing_since = fan_millis();
        }
        if((u16)(fan_millis() - echo_missing_since) >= FAN_ECHO_GRACE_MS)
            control_duty = 0;
    }
    else
    {
        echo_missing = 0;
        control_duty = fan_next_duty(distance);
    }
    fan_set_duty(control_duty);
    return control_duty;
}

u8 fan_next_duty(u16 distance)
{
    if(distance == FAN_NO_ECHO || distance > 40U) return 0;
    /* 20..40 cm always selects 50%, independent of the previous speed. */
    if(distance >= 20U) return 10;
    if(distance <= 5U) return 20;
    /* 19 cm: 40%; 15 cm: 60%; 10 cm: 80%; <=5 cm: 100%.
     * Starting duty is a nominal value; motor starting torque is untested.
     */
    return 8U + (20U - distance) * 12U / 15U;
}

void fan_set_duty(u8 duty)
{
    if(duty > 20) duty = 20;
    requested_duty = duty;
    if(duty == 0) FAN_OUT = 0;
}

void fan_init(void)
{
    FAN_OUT = 0;
    requested_duty = 0;
    active_duty = 0;
    phase = 19;
    fan_enabled = 1;
    fan_mode = FAN_MODE_AUTO;
    control_duty = 0;
    milliseconds = 0;
    clock_divider = 0;
    echo_missing = 0;
    echo_missing_since = 0;
    T2CON = 0;
    ET2 = 0;
    /* 250 us reload at 12 MHz / 12T; 20 slots => 200 Hz PWM. */
    RCAP2H = 0xff;
    RCAP2L = 0x06;
    TH2 = 0xff;
    TL2 = 0x06;
    PT2 = 0;
    ET2 = 1;
    TR2 = 1;
}

u16 fan_millis(void)
{
    u16 value;
    u8 was_enabled = ET2;
    ET2 = 0;
    value = milliseconds;
    ET2 = was_enabled;
    return value;
}

void fan_timer2_isr(void) interrupt 5 using 2
{
    TF2 = 0;
    clock_divider++;
    if(clock_divider >= 4)
    {
        clock_divider = 0;
        milliseconds++;
    }
    phase++;
    if(phase >= 20)
    {
        phase = 0;
        active_duty = requested_duty;
    }
    /* An OFF command takes effect without waiting for the next frame. */
    if(requested_duty == 0) FAN_OUT = 0;
    else if(phase < active_duty) FAN_OUT = 1;
    else FAN_OUT = 0;
}
