#ifndef _ired_H
#define _ired_H
#include "public.h"
sbit IRED = P3^2;
extern volatile u8 gired_data[4];
void ired_init(void);
u8 ired_read(u8 *command);
#endif
