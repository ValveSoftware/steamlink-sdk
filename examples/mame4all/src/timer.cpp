/***************************************************************************

  timer.c

  Functions needed to generate timing and synchronization between several
  CPUs.

  Changes 2/27/99:
  	- added some rounding to the sorting of timers so that two timers
  		allocated to go off at the same time will go off in the order
  		they were allocated, without concern for floating point rounding
  		errors (thanks Juergen!)
  	- fixed a bug where the base_time was not updated when a CPU was
  		suspended, making subsequent calls to getabsolutetime() return an
  		incorrect time (thanks Nicola!)
  	- changed suspended CPUs so that they don't eat their timeslice until
  		all other CPUs have used up theirs; this allows a slave CPU to
  		trigger a higher priority CPU in the middle of the timeslice
  	- added the ability to call timer_reset() on a oneshot or pulse timer
  		from within that timer's callback; in this case, the timer won't
  		get removed (oneshot) or won't get reprimed (pulse)

  Changes 12/17/99 (HJB):
	- added overclocking factor and functions to set/get it at runtime.

  Changes 12/23/99 (HJB):
	- added burn() function pointer to tell CPU cores when we want to
	  burn cycles, because the cores might need to adjust internal
	  counters or timers.

***************************************************************************/

#include "cpuintrf.h"
#include "driver.h"
#include "timer.h"

#define MAX_TIMERS 256

/*
 *		internal timer structures
 */

typedef struct
{
	int *icount;
	void (*burn)(int cycles);
    int index;
	int suspended;
	int trigger;
	int nocount;
	int lost;
	timer_tm time;
	timer_tm sec_to_cycles;
	timer_tm cycles_to_sec;
	float overclock;
} cpu_entry;


/* conversion constants */
timer_tm cycles_to_sec[MAX_CPU];
timer_tm sec_to_cycles[MAX_CPU];

/* list of per-CPU timer data */
static cpu_entry cpudata[MAX_CPU+1];
static cpu_entry *lastcpu;
static cpu_entry *activecpu;
static cpu_entry *last_activecpu;

/* list of active timers */
static timer_entry timers[MAX_TIMERS];
static timer_entry *timer_head;
static timer_entry *timer_free_head;

/* other internal states */
static timer_tm base_time;
static timer_tm global_offset;
static timer_entry *callback_timer;
static int callback_timer_modified;

/* prototypes */
static int pick_cpu(int *cpu, int *cycles, timer_tm expire);

/*
 *		return the current absolute time
 */
timer_tm getabsolutetime(void)
{
	if (activecpu && (*activecpu->icount + activecpu->lost) > 0)
		return base_time - ((timer_tm)(*activecpu->icount + activecpu->lost) * activecpu->cycles_to_sec);
	else
		return base_time;
}


/*
 *		adjust the current CPU's timer so that a new event will fire at the right time
 */
INLINE void timer_adjust(timer_entry *timer, timer_tm time, timer_tm period)
{
	int newicount, diff;

	/* compute a new icount for the current CPU */
	if (period == TIME_NOW)
		newicount = 0;
	else
	    newicount = CYCLES_CALC(timer->expire - time,activecpu->sec_to_cycles)+1;

	/* determine if we're scheduled to run more cycles */
	diff = *activecpu->icount - newicount;

	/* if so, set the new icount and compute the amount of "lost" time */
	if (diff > 0)
	{
		activecpu->lost += diff;
		if (activecpu->burn)
			(*activecpu->burn)(diff);  /* let the CPU burn the cycles */
		else
			*activecpu->icount = newicount;  /* CPU doesn't care */
	}
}


/*
 *		allocate a new timer
 */
INLINE timer_entry *timer_new(void)
{
	timer_entry *timer;

	/* remove an empty entry */
	if (!timer_free_head)
		return NULL;
	timer = timer_free_head;
	timer_free_head = timer->next;

	return timer;
}


/*
 *		insert a new timer into the list at the appropriate location
 */
INLINE void timer_list_insert(timer_entry *timer)
{
	timer_entry *t=timer_head;
	timer_entry *lt = NULL;
    timer_tm expire = (t && timer->enabled) ? timer->expire : TIME_NEVER;

    if (expire==TIME_NOW)
    {
       	/* loop over the timer list */
       	for (t = timer_head; t; lt = t, t = t->next)
       	{
       		if (t->expire!=TIME_NOW)
       		{
       			/* link the new guy in before the current list entry */
       			timer->prev = t->prev;
       			timer->next = t;
       
       			if (t->prev)
       				t->prev->next = timer;
       			else
       				timer_head = timer;
       			t->prev = timer;
       			return;
       		}
       	}
    }
    else if (expire!=TIME_NEVER)
    {
       	/* loop over the timer list */
       	for (t = timer_head; t; lt = t, t = t->next)
       	{
       		/* if the current list entry expires after us, we should be inserted before it */
       		/* note that due to floating point rounding, we need to allow a bit of slop here */
       		/* because two equal entries -- within rounding precision -- need to sort in */
       		/* the order they were inserted into the list */
       		//if ((t->expire - expire) > TIME_IN_NSEC(1))
       		if (t->expire > expire)
       		{
       			/* link the new guy in before the current list entry */
       			timer->prev = t->prev;
       			timer->next = t;
        
       			if (t->prev)
       				t->prev->next = timer;
       			else
       				timer_head = timer;
       			t->prev = timer;
       			return;
       		}
       	}
    }
    else
    {
       	/* loop over the timer list */
       	for (t = timer_head; t; lt = t, t = t->next)
       	{
       	}
    }
    
	/* need to insert after the last one */
	if (lt)
		lt->next = timer;
	else
		timer_head = timer;
	timer->prev = lt;
	timer->next = NULL;
}


/*
 *		remove a timer from the linked list
 */
INLINE void timer_list_remove(timer_entry *timer)
{
	/* remove it from the list */
	if (timer->prev)
		timer->prev->next = timer->next;
	else
		timer_head = timer->next;
	if (timer->next)
		timer->next->prev = timer->prev;
}


/*
 *		initialize the timer system
 */
void timer_init(void)
{
	cpu_entry *cpu;
	int i;

	/* keep a local copy of how many total CPU's */
	lastcpu = cpudata + cpu_gettotalcpu() - 1;

	/* we need to wait until the first call to timer_cyclestorun before using real CPU times */
	base_time = 0;
	global_offset = 0;
	callback_timer = NULL;
	callback_timer_modified = 0;

	/* reset the timers */
	memset(timers, 0, sizeof(timers));

	/* initialize the lists */
	timer_head = NULL;
	timer_free_head = &timers[0];
	for (i = 0; i < MAX_TIMERS-1; i++)
		timers[i].next = &timers[i+1];

	/* reset the CPU timers */
	memset(cpudata, 0, sizeof(cpudata));
	activecpu = NULL;
	last_activecpu = lastcpu;

	/* compute the cycle times */
	for (cpu = cpudata, i = 0; cpu <= lastcpu; cpu++, i++)
	{
		/* make a pointer to this CPU's interface functions */
		cpu->icount = cpuintf[Machine->drv->cpu[i].cpu_type & ~CPU_FLAGS_MASK].icount;
		cpu->burn = cpuintf[Machine->drv->cpu[i].cpu_type & ~CPU_FLAGS_MASK].burn;

		/* get the CPU's overclocking factor */
		cpu->overclock = cpuintf[Machine->drv->cpu[i].cpu_type & ~CPU_FLAGS_MASK].overclock;

        	/* everyone is active but suspended by the reset line until further notice */
		cpu->suspended = SUSPEND_REASON_RESET;

		/* set the CPU index */
		cpu->index = i;

		/* compute the cycle times */
#ifndef MAME_UNDERCLOCK
		cpu->sec_to_cycles = sec_to_cycles[i] = cpu->overclock * Machine->drv->cpu[i].cpu_clock;
#else
		/* Franxis 21/01/2007 */
		{
			extern int underclock_sound;
			extern int underclock_cpu;
			if (Machine->drv->cpu[i].cpu_type&CPU_AUDIO_CPU)
				cpu->sec_to_cycles = sec_to_cycles[i] = ((float)(cpu->overclock * Machine->drv->cpu[i].cpu_clock))
					* ((((float)100)-((float)underclock_sound))/((float)100));
			else
				cpu->sec_to_cycles = sec_to_cycles[i] = ((float)(cpu->overclock * Machine->drv->cpu[i].cpu_clock))
					* ((((float)100)-((float)underclock_cpu))/((float)100));
		}
#endif
		cpu->cycles_to_sec = cycles_to_sec[i] = TIME_ONE_SEC / sec_to_cycles[i];
	}
}

/*
 *		get overclocking factor for a CPU
 */
float timer_get_overclock(int cpunum)
{
	cpu_entry *cpu = &cpudata[cpunum];
	return cpu->overclock;
}

/*
 *		set overclocking factor for a CPU
 */
void timer_set_overclock(int cpunum, float overclock)
{
	cpu_entry *cpu = &cpudata[cpunum];
	cpu->overclock = overclock;
	cpu->sec_to_cycles = sec_to_cycles[cpunum] = cpu->overclock * Machine->drv->cpu[cpunum].cpu_clock;
	cpu->cycles_to_sec = cycles_to_sec[cpunum] = TIME_ONE_SEC / sec_to_cycles[cpunum];
}

/*
 *		allocate a pulse timer, which repeatedly calls the callback using the given period
 */
void *timer_pulse(timer_tm period, int param, void (*callback)(int))
{
	timer_tm time = getabsolutetime();
	timer_entry *timer;

	/* allocate a new entry */
	timer = timer_new();
	if (!timer)
		return NULL;

	/* fill in the record */
	timer->callback = callback;
	timer->callback_param = param;
	timer->enabled = 1;
	timer->period = period;

	/* compute the time of the next firing and insert into the list */
	timer->start = time;
	if (period==TIME_NEVER)
	    timer->expire = TIME_NEVER;
	else
	    timer->expire = time + period;

	timer_list_insert(timer);

	/* if we're supposed to fire before the end of this cycle, adjust the counter */
	if (activecpu && timer->expire < base_time)
		timer_adjust(timer, time, period);

	/* return a handle */
	return timer;
}


/*
 *		allocate a one-shot timer, which calls the callback after the given duration
 */
void *timer_set(timer_tm duration, int param, void (*callback)(int))
{
	timer_tm time = getabsolutetime();
	timer_entry *timer;

	/* allocate a new entry */
	timer = timer_new();
	if (!timer)
		return NULL;

	/* fill in the record */
	timer->callback = callback;
	timer->callback_param = param;
	timer->enabled = 1;
	timer->period = 0;

	/* compute the time of the next firing and insert into the list */
	timer->start = time;
	if (duration==TIME_NOW) timer->expire = time;
	else if (duration==TIME_NEVER) timer->expire = TIME_NEVER;
	else timer->expire = time + duration;
	timer_list_insert(timer);

	/* if we're supposed to fire before the end of this cycle, adjust the counter */
	if (activecpu && timer->expire < base_time)
		timer_adjust(timer, time, duration);

	/* return a handle */
	return timer;
}


/*
 *		reset the timing on a timer
 */
void timer_reset(void *which, timer_tm duration)
{
	timer_tm time = getabsolutetime();
	timer_entry *timer = (timer_entry *)which;

	/* compute the time of the next firing */
	timer->start = time;
	if (duration==TIME_NOW) timer->expire = time;
	else if (duration==TIME_NEVER) timer->expire = TIME_NEVER;
	else timer->expire = time + duration;

	/* remove the timer and insert back into the list */
	timer_list_remove(timer);
	timer_list_insert(timer);

	/* if we're supposed to fire before the end of this cycle, adjust the counter */
	if (activecpu && timer->expire < base_time)
		timer_adjust(timer, time, duration);

	/* if this is the callback timer, mark it modified */
	if (timer == callback_timer)
		callback_timer_modified = 1;
}


/*
 *		remove a timer from the system
 */
void timer_remove(void *which)
{
	timer_entry *timer = (timer_entry *)which;

	/* remove it from the list */
	timer_list_remove(timer);

	/* free it up by adding it back to the free list */
	timer->next = timer_free_head;
	timer_free_head = timer;
}


/*
 *		enable/disable a timer
 */
int timer_enable(void *which, int enable)
{
	timer_entry *timer = (timer_entry *)which;
	int old;

	/* set the enable flag */
	old = timer->enabled;
	timer->enabled = enable;

	/* remove the timer and insert back into the list */
	timer_list_remove(timer);
	timer_list_insert(timer);

	return old;
}


/*
 *		return the time since the last trigger
 */
timer_tm timer_timeelapsed(void *which)
{
	timer_tm time = getabsolutetime();
	timer_entry *timer = (timer_entry *)which;

	return time - timer->start;
}


/*
 *		return the time until the next trigger
 */
timer_tm timer_timeleft(void *which)
{
	timer_tm time = getabsolutetime();
	timer_entry *timer = (timer_entry *)which;

    if (timer->expire==TIME_NEVER)
        return TIME_NEVER;
	else
	    return timer->expire - time;
}


/*
 *		return the current time
 */
float timer_get_time(void)
{
	return (float)global_offset + ((float)getabsolutetime()/(float)TIME_ONE_SEC);
}


/*
 *		begin CPU execution by determining how many cycles the CPU should run
 */
int timer_schedule_cpu(int *cpu, int *cycles)
{
	timer_tm end;

	/* then see if there are any CPUs that aren't suspended and haven't yet been updated */
	if (pick_cpu(cpu, cycles, timer_head->expire))
		return 1;

	/* everyone is up-to-date; expire any timers now */
	end = timer_head->expire;
	while (timer_head->expire <= end)
	{
		timer_entry *timer = timer_head;

		/* the base time is now the time of the timer */
		base_time = timer->expire;

		/* set the global state of which callback we're in */
		callback_timer_modified = 0;
		callback_timer = timer;

		/* call the callback */
		if (timer->callback)
		{
			profiler_mark(PROFILER_TIMER_CALLBACK);
			(*timer->callback)(timer->callback_param);
			profiler_mark(PROFILER_END);
		}

		/* clear the callback timer global */
		callback_timer = NULL;

		/* reset or remove the timer, but only if it wasn't modified during the callback */
		if (!callback_timer_modified)
		{
			if (timer->period)
			{
				timer->start = timer->expire;
				timer->expire += timer->period;

				timer_list_remove(timer);
				timer_list_insert(timer);
			}
			else
				timer_remove(timer);
		}
	}

	/* reset scheduling so it starts with CPU 0 */
	last_activecpu = lastcpu;

	/* go back to scheduling */
	return pick_cpu(cpu, cycles, timer_head->expire);
}


/*
 *		end CPU execution by updating the number of cycles the CPU actually ran
 */
void timer_update_cpu(int cpunum, int ran)
{
	cpu_entry *cpu = cpudata + cpunum;

	/* update the time if we haven't been suspended */
	if (!cpu->suspended)
	{
		cpu->time += (timer_tm)(ran - cpu->lost) * cpu->cycles_to_sec;
		cpu->lost = 0;
	}

	/* time to renormalize? */
	if (cpu->time >= TIME_ONE_SEC)
	{
		timer_entry *timer;
		cpu_entry *c;

		/* renormalize all the CPU timers */
		for (c = cpudata; c <= lastcpu; c++)
			c->time -= TIME_ONE_SEC;

		/* renormalize all the timers' times */
		for (timer = timer_head; timer; timer = timer->next)
		{
			timer->start -= TIME_ONE_SEC;
			if (timer->expire!=TIME_NEVER)
			    timer->expire -= TIME_ONE_SEC;
		}

		/* renormalize the global timers */
		global_offset += 1;
	}

	/* now stop counting cycles */
	base_time = cpu->time;
	activecpu = NULL;
}


/*
 *		suspend a CPU but continue to count time for it
 */
void timer_suspendcpu(int cpunum, int suspend, int reason)
{
	cpu_entry *cpu = cpudata + cpunum;
	int nocount = cpu->nocount;
	int old = cpu->suspended;

	/* mark the CPU */
	if (suspend)
		cpu->suspended |= reason;
	else
		cpu->suspended &= ~reason;
	cpu->nocount = 0;

	/* if this is the active CPU and we're halting, stop immediately */
	if (activecpu && cpu == activecpu && !old && cpu->suspended)
	{
		/* set the CPU's time to the current time */
		cpu->time = base_time = getabsolutetime();	/* ASG 990225 - also set base_time */
		cpu->lost = 0;

		/* no more instructions */
		if (cpu->burn)
			(*cpu->burn)(*cpu->icount); /* let the CPU burn the cycles */
		else
			*cpu->icount = 0;	/* CPU doesn't care */
    }

	/* else if we're unsuspending a CPU, reset its time */
	else if (old && !cpu->suspended && !nocount)
	{
		timer_tm time = getabsolutetime();

		/* only update the time if it's later than the CPU's time */
		if (time > cpu->time)
			cpu->time = time;
		cpu->lost = 0;
	}
}



/*
 *		hold a CPU and don't count time for it
 */
void timer_holdcpu(int cpunum, int hold, int reason)
{
	cpu_entry *cpu = cpudata + cpunum;

	/* same as suspend */
	timer_suspendcpu(cpunum, hold, reason);

	/* except that we don't count time */
	if (hold)
		cpu->nocount = 1;
}



/*
 *		query if a CPU is suspended or not
 */
int timer_iscpususpended(int cpunum, int reason)
{
	cpu_entry *cpu = cpudata + cpunum;
	return (cpu->suspended & reason) && !cpu->nocount;
}



/*
 *		query if a CPU is held or not
 */
int timer_iscpuheld(int cpunum, int reason)
{
	cpu_entry *cpu = cpudata + cpunum;
	return (cpu->suspended & reason) && cpu->nocount;
}



/*
 *		suspend a CPU until a specified trigger condition is met
 */
void timer_suspendcpu_trigger(int cpunum, int trigger)
{
	cpu_entry *cpu = cpudata + cpunum;

	/* suspend the CPU immediately if it's not already */
	timer_suspendcpu(cpunum, 1, SUSPEND_REASON_TRIGGER);

	/* set the trigger */
	cpu->trigger = trigger;
}



/*
 *		hold a CPU and don't count time for it
 */
void timer_holdcpu_trigger(int cpunum, int trigger)
{
	cpu_entry *cpu = cpudata + cpunum;

	/* suspend the CPU immediately if it's not already */
	timer_holdcpu(cpunum, 1, SUSPEND_REASON_TRIGGER);

	/* set the trigger */
	cpu->trigger = trigger;
}



/*
 *		generates a trigger to unsuspend any CPUs waiting for it
 */
void timer_trigger(int trigger)
{
	cpu_entry *cpu;

	/* cause an immediate resynchronization */
	if (activecpu)
	{
		int left = *activecpu->icount;
		if (left > 0)
		{
			activecpu->lost += left;
			if (activecpu->burn)
				(*activecpu->burn)(left); /* let the CPU burn the cycles */
			else
				*activecpu->icount = 0; /* CPU doesn't care */
		}
	}

	/* look for suspended CPUs waiting for this trigger and unsuspend them */
	for (cpu = cpudata; cpu <= lastcpu; cpu++)
	{
		if (cpu->suspended && cpu->trigger == trigger)
		{
			timer_suspendcpu(cpu->index, 0, SUSPEND_REASON_TRIGGER);
			cpu->trigger = 0;
		}
	}
}


/*
 *		pick the next CPU to run
 */
static int pick_cpu(int *cpunum, int *cycles, timer_tm end)
{
	cpu_entry *cpu = last_activecpu;

	/* look for a CPU that isn't suspended and hasn't run its full timeslice yet */
	do
	{
		/* wrap around */
		cpu += 1;
		if (cpu > lastcpu)
			cpu = cpudata;

		/* if this CPU isn't suspended and has time left.... */
		if ((!cpu->suspended) && (cpu->time < end))
		{
			/* mark the CPU active, and remember the CPU number locally */
			activecpu = last_activecpu = cpu;

			/* return the number of cycles to execute and the CPU number */
			*cpunum = cpu->index;
			*cycles=CYCLES_CALC(end - cpu->time,cpu->sec_to_cycles);

			if (*cycles > 0)
			{
				/* remember the base time for this CPU */
				base_time = cpu->time + ((timer_tm)*cycles * cpu->cycles_to_sec);

				/* success */
				return 1;
			}
		}
	}
	while (cpu != last_activecpu);

	/* ASG 990225 - bump all suspended CPU times after the slice has finished */
	for (cpu = cpudata; cpu <= lastcpu; cpu++)
		if (cpu->suspended && !cpu->nocount)
		{
			cpu->time = end;
			cpu->lost = 0;
		}

	/* failure */
	return 0;
}
