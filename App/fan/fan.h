#ifndef FAN_H
#define FAN_H
#include "public.h"
#define FAN_NO_ECHO 0xffffU
#define FAN_KEY_POWER 0x45
#define FAN_KEY_FIXED 0x16
#define FAN_KEY_AUTO 0x0c
#define FAN_MODE_AUTO 0
#define FAN_MODE_FIXED 1
#define FAN_ECHO_GRACE_MS 600U
extern u8 fan_enabled;
extern u8 fan_mode;
u8 fan_control_update(u16 distance, u8 command, u8 has_command);
/* Duty steps 0..20, each step is 5 percent. */
u8 fan_next_duty(u16 distance);
void fan_init(void);
void fan_set_duty(u8 duty);
u16 fan_millis(void);
#endif
