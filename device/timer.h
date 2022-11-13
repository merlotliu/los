#ifndef __DEVICE_TIMER_H
#define __DEVICE_TIMER_H

#include "global.h"

/* 以毫秒为单位的 sleep， 1s = 1000ms */
void mtime_sleep(uint32_t m_seconds);

/* init PIT */
void timer_init(void);

#endif /* __DEVICE_TIMER_H */