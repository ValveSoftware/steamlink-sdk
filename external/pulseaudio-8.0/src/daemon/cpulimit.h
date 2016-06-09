#ifndef foocpulimithfoo
#define foocpulimithfoo

/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#include <pulse/mainloop-api.h>

/* This kills the pulseaudio process if it eats more than 70% of the
 * CPU time. This is build around setrlimit() and SIGXCPU. It is handy
 * in case of using SCHED_FIFO which may freeze the whole machine  */

int pa_cpu_limit_init(pa_mainloop_api *m);
void pa_cpu_limit_done(void);

#endif
