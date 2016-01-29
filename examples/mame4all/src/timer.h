/***************************************************************************

  timer.c

  Functions needed to generate timing and synchronization between several
  CPUs.

***************************************************************************/

#ifndef __TIMER_H__
#define __TIMER_H__

typedef INT32 timer_tm;

extern timer_tm cycles_to_sec[];
extern timer_tm sec_to_cycles[];

#define TIME_NOW              (0)
#define TIME_NEVER            (2147483647)

/* nanoseconds mode
#define TIME_ONE_SEC 1000000000
#define TIME_ONE_MSEC 1000000
#define TIME_ONE_USEC 1000
#define TIME_IN_HZ(hz)        (TIME_ONE_SEC / ((timer_tm)(hz)))
#define TIME_IN_CYCLES(c,cpu) ((c) * cycles_to_sec[(cpu)])
#define TIME_IN_SEC(s)        ((timer_tm)((s) * (TIME_ONE_SEC)))
#define TIME_IN_MSEC(ms)      ((timer_tm)((ms) * (TIME_ONE_MSEC)))
#define TIME_IN_USEC(us)      ((timer_tm)((us) * (TIME_ONE_USEC)))
#define TIME_IN_NSEC(ns)      ((timer_tm)(ns)) */

// 2^30 ticks per second mode
#define TIME_ONE_SEC (1<<30)
#define TIME_IN_HZ(hz)        (TIME_ONE_SEC / ((timer_tm)(hz)))
#define TIME_IN_CYCLES(c,cpu) ((c) * cycles_to_sec[(cpu)])
#define TIME_IN_SEC(s)        ((timer_tm)((s) * (TIME_ONE_SEC)))
#define TIME_IN_MSEC(ms)      ((timer_tm)(((float)(ms)*(float)(TIME_ONE_SEC))/(float)1000))
#define TIME_IN_USEC(us)      ((timer_tm)(((float)(us)*(float)(TIME_ONE_SEC))/(float)1000000))
#define TIME_IN_NSEC(ns)      (((INT64)(ns)*(INT64)(TIME_ONE_SEC))/(INT64)1000000000)

#define CYCLES_CALC(t,cycles) ((((INT64)(t))*((INT64)(cycles)))/((INT64)TIME_ONE_SEC))
#define TIME_TO_CYCLES(cpu,t) CYCLES_CALC((t),sec_to_cycles[(cpu)])

#define SUSPEND_REASON_HALT		0x0001
#define SUSPEND_REASON_RESET	0x0002
#define SUSPEND_REASON_SPIN		0x0004
#define SUSPEND_REASON_TRIGGER	0x0008
#define SUSPEND_REASON_DISABLE	0x0010
#define SUSPEND_ANY_REASON		((UINT32)-1)

void timer_init(void);
void *timer_pulse(timer_tm period, int param, void(*callback)(int));
void *timer_set(timer_tm duration, int param, void(*callback)(int));
void timer_reset(void *which, timer_tm duration);
void timer_remove(void *which);
int timer_enable(void *which, int enable);
timer_tm timer_timeelapsed(void *which);
timer_tm timer_timeleft(void *which);
float timer_get_time(void);
int timer_schedule_cpu(int *cpu, int *cycles);
void timer_update_cpu(int cpu, int ran);
void timer_suspendcpu(int cpu, int suspend, int reason);
void timer_holdcpu(int cpu, int hold, int reason);
int timer_iscpususpended(int cpu, int reason);
int timer_iscpuheld(int cpunum, int reason);
void timer_suspendcpu_trigger(int cpu, int trigger);
void timer_holdcpu_trigger(int cpu, int trigger);
void timer_trigger(int trigger);
float timer_get_overclock(int cpunum);
void timer_set_overclock(int cpunum, float overclock);

timer_tm getabsolutetime(void);
typedef struct timer_entry
{
	struct timer_entry *next;
	struct timer_entry *prev;
	void (*callback)(int);
	int callback_param;
	int enabled;
	timer_tm period;
	timer_tm start;
	timer_tm expire;
} timer_entry;

#endif
