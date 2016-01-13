#ifndef foopulsecardhfoo
#define foopulsecardhfoo

/***
  This file is part of PulseAudio.

  Copyright 2009 Lennart Poettering

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

typedef struct pa_card pa_card;

#include <pulse/proplist.h>
#include <pulsecore/core.h>
#include <pulsecore/module.h>
#include <pulsecore/idxset.h>

/* This enum replaces pa_port_available_t (defined in pulse/def.h) for
 * internal use, so make sure both enum types stay in sync. */
typedef enum pa_available {
    PA_AVAILABLE_UNKNOWN = 0,
    PA_AVAILABLE_NO = 1,
    PA_AVAILABLE_YES = 2,
} pa_available_t;

typedef struct pa_card_profile {
    pa_card *card;
    char *name;
    char *description;

    unsigned priority;
    pa_available_t available; /* PA_AVAILABLE_UNKNOWN, PA_AVAILABLE_NO or PA_AVAILABLE_YES */

    /* We probably want to have different properties later on here */
    unsigned n_sinks;
    unsigned n_sources;

    unsigned max_sink_channels;
    unsigned max_source_channels;

    /* .. followed by some implementation specific data */
} pa_card_profile;

#define PA_CARD_PROFILE_DATA(d) ((void*) ((uint8_t*) d + PA_ALIGN(sizeof(pa_card_profile))))

struct pa_card {
    uint32_t index;
    pa_core *core;

    char *name;

    pa_proplist *proplist;
    pa_module *module;
    char *driver;

    pa_idxset *sinks;
    pa_idxset *sources;

    pa_hashmap *profiles;
    pa_card_profile *active_profile;

    pa_hashmap *ports;

    bool save_profile:1;

    void *userdata;

    int (*set_profile)(pa_card *c, pa_card_profile *profile);
};

typedef struct pa_card_new_data {
    char *name;
    pa_proplist *proplist;

    const char *driver;
    pa_module *module;

    pa_hashmap *profiles;
    char *active_profile;

    pa_hashmap *ports;

    bool namereg_fail:1;

    bool save_profile:1;
} pa_card_new_data;

pa_card_profile *pa_card_profile_new(const char *name, const char *description, size_t extra);
void pa_card_profile_free(pa_card_profile *c);

/* The profile's available status has changed */
void pa_card_profile_set_available(pa_card_profile *c, pa_available_t available);

pa_card_new_data *pa_card_new_data_init(pa_card_new_data *data);
void pa_card_new_data_set_name(pa_card_new_data *data, const char *name);
void pa_card_new_data_set_profile(pa_card_new_data *data, const char *profile);
void pa_card_new_data_done(pa_card_new_data *data);

pa_card *pa_card_new(pa_core *c, pa_card_new_data *data);
void pa_card_free(pa_card *c);

void pa_card_add_profile(pa_card *c, pa_card_profile *profile);

int pa_card_set_profile(pa_card *c, pa_card_profile *profile, bool save);

int pa_card_suspend(pa_card *c, bool suspend, pa_suspend_cause_t cause);

#endif
