#ifndef foocmdlinehfoo
#define foocmdlinehfoo

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

#include "daemon-conf.h"

/* Parse the command line and store its data in *c. Return the index
 * of the first unparsed argument in *d. */
int pa_cmdline_parse(pa_daemon_conf*c, int argc, char *const argv [], int *d);

/* Show the command line help. The command name is extracted from
 * argv[0] which should be passed in argv0. */
void pa_cmdline_help(const char *argv0);

#endif
