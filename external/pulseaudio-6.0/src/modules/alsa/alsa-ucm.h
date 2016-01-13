#ifndef fooalsaucmhfoo
#define fooalsaucmhfoo

/***
  This file is part of PulseAudio.

  Copyright 2011 Wolfson Microelectronics PLC
  Author Margarita Olaya <magi@slimlogic.co.uk>
  Copyright 2012 Feng Wei <wei.feng@freescale.com>, Freescale Ltd.

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

#ifdef HAVE_ALSA_UCM
#include <use-case.h>
#else
typedef void snd_use_case_mgr_t;
#endif

#include "alsa-mixer.h"

/** For devices: List of verbs, devices or modifiers available */
#define PA_ALSA_PROP_UCM_NAME                       "alsa.ucm.name"

/** For devices: List of supported devices per verb*/
#define PA_ALSA_PROP_UCM_DESCRIPTION                "alsa.ucm.description"

/** For devices: Playback device name e.g PlaybackPCM */
#define PA_ALSA_PROP_UCM_SINK                       "alsa.ucm.sink"

/** For devices: Capture device name e.g CapturePCM*/
#define PA_ALSA_PROP_UCM_SOURCE                     "alsa.ucm.source"

/** For devices: Playback roles */
#define PA_ALSA_PROP_UCM_PLAYBACK_ROLES             "alsa.ucm.playback.roles"

/** For devices: Playback control volume ID string. e.g PlaybackVolume */
#define PA_ALSA_PROP_UCM_PLAYBACK_VOLUME            "alsa.ucm.playback.volume"

/** For devices: Playback switch e.g PlaybackSwitch */
#define PA_ALSA_PROP_UCM_PLAYBACK_SWITCH            "alsa.ucm.playback.switch"

/** For devices: Playback priority */
#define PA_ALSA_PROP_UCM_PLAYBACK_PRIORITY          "alsa.ucm.playback.priority"

/** For devices: Playback rate */
#define PA_ALSA_PROP_UCM_PLAYBACK_RATE              "alsa.ucm.playback.rate"

/** For devices: Playback channels */
#define PA_ALSA_PROP_UCM_PLAYBACK_CHANNELS          "alsa.ucm.playback.channels"

/** For devices: Capture roles */
#define PA_ALSA_PROP_UCM_CAPTURE_ROLES              "alsa.ucm.capture.roles"

/** For devices: Capture controls volume ID string. e.g CaptureVolume */
#define PA_ALSA_PROP_UCM_CAPTURE_VOLUME             "alsa.ucm.capture.volume"

/** For devices: Capture switch e.g CaptureSwitch */
#define PA_ALSA_PROP_UCM_CAPTURE_SWITCH             "alsa.ucm.capture.switch"

/** For devices: Capture priority */
#define PA_ALSA_PROP_UCM_CAPTURE_PRIORITY           "alsa.ucm.capture.priority"

/** For devices: Capture rate */
#define PA_ALSA_PROP_UCM_CAPTURE_RATE               "alsa.ucm.capture.rate"

/** For devices: Capture channels */
#define PA_ALSA_PROP_UCM_CAPTURE_CHANNELS           "alsa.ucm.capture.channels"

/** For devices: Quality of Service */
#define PA_ALSA_PROP_UCM_QOS                        "alsa.ucm.qos"

/** For devices: The modifier (if any) that this device corresponds to */
#define PA_ALSA_PROP_UCM_MODIFIER "alsa.ucm.modifier"

typedef struct pa_alsa_ucm_verb pa_alsa_ucm_verb;
typedef struct pa_alsa_ucm_modifier pa_alsa_ucm_modifier;
typedef struct pa_alsa_ucm_device pa_alsa_ucm_device;
typedef struct pa_alsa_ucm_config pa_alsa_ucm_config;
typedef struct pa_alsa_ucm_mapping_context pa_alsa_ucm_mapping_context;

int pa_alsa_ucm_query_profiles(pa_alsa_ucm_config *ucm, int card_index);
pa_alsa_profile_set* pa_alsa_ucm_add_profile_set(pa_alsa_ucm_config *ucm, pa_channel_map *default_channel_map);
int pa_alsa_ucm_set_profile(pa_alsa_ucm_config *ucm, const char *new_profile, const char *old_profile);

int pa_alsa_ucm_get_verb(snd_use_case_mgr_t *uc_mgr, const char *verb_name, const char *verb_desc, pa_alsa_ucm_verb **p_verb);

void pa_alsa_ucm_add_ports(
        pa_hashmap **hash,
        pa_proplist *proplist,
        pa_alsa_ucm_mapping_context *context,
        bool is_sink,
        pa_card *card);
void pa_alsa_ucm_add_ports_combination(
        pa_hashmap *hash,
        pa_alsa_ucm_mapping_context *context,
        bool is_sink,
        pa_hashmap *ports,
        pa_card_profile *cp,
        pa_core *core);
int pa_alsa_ucm_set_port(pa_alsa_ucm_mapping_context *context, pa_device_port *port, bool is_sink);

void pa_alsa_ucm_free(pa_alsa_ucm_config *ucm);
void pa_alsa_ucm_mapping_context_free(pa_alsa_ucm_mapping_context *context);

void pa_alsa_ucm_roled_stream_begin(pa_alsa_ucm_config *ucm, const char *role, pa_direction_t dir);
void pa_alsa_ucm_roled_stream_end(pa_alsa_ucm_config *ucm, const char *role, pa_direction_t dir);

/* UCM - Use Case Manager is available on some audio cards */

struct pa_alsa_ucm_device {
    PA_LLIST_FIELDS(pa_alsa_ucm_device);

    pa_proplist *proplist;

    unsigned playback_priority;
    unsigned capture_priority;

    unsigned playback_rate;
    unsigned capture_rate;

    unsigned playback_channels;
    unsigned capture_channels;

    pa_alsa_mapping *playback_mapping;
    pa_alsa_mapping *capture_mapping;

    pa_idxset *conflicting_devices;
    pa_idxset *supported_devices;

    pa_alsa_jack *input_jack;
    pa_alsa_jack *output_jack;
};

struct pa_alsa_ucm_modifier {
    PA_LLIST_FIELDS(pa_alsa_ucm_modifier);

    pa_proplist *proplist;

    int n_confdev;
    int n_suppdev;

    const char **conflicting_devices;
    const char **supported_devices;

    pa_direction_t action_direction;

    char *media_role;

    /* Non-NULL if the modifier has its own PlaybackPCM/CapturePCM */
    pa_alsa_mapping *playback_mapping;
    pa_alsa_mapping *capture_mapping;

    /* Count how many role matched streams are running */
    int enabled_counter;
};

struct pa_alsa_ucm_verb {
    PA_LLIST_FIELDS(pa_alsa_ucm_verb);

    pa_proplist *proplist;

    PA_LLIST_HEAD(pa_alsa_ucm_device, devices);
    PA_LLIST_HEAD(pa_alsa_ucm_modifier, modifiers);
};

struct pa_alsa_ucm_config {
    pa_core *core;
    snd_use_case_mgr_t *ucm_mgr;
    pa_alsa_ucm_verb *active_verb;

    PA_LLIST_HEAD(pa_alsa_ucm_verb, verbs);
    PA_LLIST_HEAD(pa_alsa_jack, jacks);
};

struct pa_alsa_ucm_mapping_context {
    pa_alsa_ucm_config *ucm;
    pa_direction_t direction;

    pa_idxset *ucm_devices;
    pa_idxset *ucm_modifiers;
};

#endif
