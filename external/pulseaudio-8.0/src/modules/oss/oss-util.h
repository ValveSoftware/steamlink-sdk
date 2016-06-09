#ifndef fooossutilhfoo
#define fooossutilhfoo

/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB

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

#include <pulse/sample.h>
#include <pulse/volume.h>

int pa_oss_open(const char *device, int *mode, int* pcaps);
int pa_oss_auto_format(int fd, pa_sample_spec *ss);

int pa_oss_set_fragments(int fd, int frags, int frag_size);

int pa_oss_set_volume(int fd, unsigned long mixer, const pa_sample_spec *ss, const pa_cvolume *volume);
int pa_oss_get_volume(int fd, unsigned long mixer, const pa_sample_spec *ss, pa_cvolume *volume);

int pa_oss_get_hw_description(const char *dev, char *name, size_t l);

int pa_oss_open_mixer_for_device(const char *device);

#endif
