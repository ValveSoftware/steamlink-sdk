/*
 * Alert: Not all timers of the SoC family run at a frequency of 75MHz,
 * but we should be fine as long as we make our timers an exact multiple of
 * HZ, any value that makes a 1->1 correspondence for the time conversion
 * functions to/from jiffies is acceptable.
 *
 * NOTE:CLOCK_TICK_RATE or LATCH (see include/linux/jiffies.h) should not be
 * used directly in code. Currently none of the code relevant to BERLIN
 * platform depends on these values directly.
 */
#ifndef __ASM_ARCH_TIMEX_H
#define __ASM_ARCH_TIMEX_H

#define CLOCK_TICK_RATE 75000000

#endif /* __ASM_ARCH_TIMEX_H__ */
