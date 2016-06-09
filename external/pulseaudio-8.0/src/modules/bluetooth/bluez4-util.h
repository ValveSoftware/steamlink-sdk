#ifndef foobluez4utilhfoo
#define foobluez4utilhfoo

/***
  This file is part of PulseAudio.

  Copyright 2008-2013 João Paulo Rechi Vita

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#include <dbus/dbus.h>

#include <pulsecore/llist.h>
#include <pulsecore/macro.h>

#define PA_BLUEZ4_ERROR_NOT_SUPPORTED "org.bluez.Error.NotSupported"

/* UUID copied from bluez/audio/device.h */
#define GENERIC_AUDIO_UUID      "00001203-0000-1000-8000-00805f9b34fb"

#define HSP_HS_UUID             "00001108-0000-1000-8000-00805f9b34fb"
#define HSP_AG_UUID             "00001112-0000-1000-8000-00805f9b34fb"

#define HFP_HS_UUID             "0000111e-0000-1000-8000-00805f9b34fb"
#define HFP_AG_UUID             "0000111f-0000-1000-8000-00805f9b34fb"

#define ADVANCED_AUDIO_UUID     "0000110d-0000-1000-8000-00805f9b34fb"

#define A2DP_SOURCE_UUID        "0000110a-0000-1000-8000-00805f9b34fb"
#define A2DP_SINK_UUID          "0000110b-0000-1000-8000-00805f9b34fb"

#define HSP_MAX_GAIN 15

typedef struct pa_bluez4_uuid pa_bluez4_uuid;
typedef struct pa_bluez4_device pa_bluez4_device;
typedef struct pa_bluez4_discovery pa_bluez4_discovery;
typedef struct pa_bluez4_transport pa_bluez4_transport;

struct userdata;

struct pa_bluez4_uuid {
    char *uuid;
    PA_LLIST_FIELDS(pa_bluez4_uuid);
};

typedef enum pa_bluez4_profile {
    PA_BLUEZ4_PROFILE_A2DP,
    PA_BLUEZ4_PROFILE_A2DP_SOURCE,
    PA_BLUEZ4_PROFILE_HSP,
    PA_BLUEZ4_PROFILE_HFGW,
    PA_BLUEZ4_PROFILE_OFF
} pa_bluez4_profile_t;

#define PA_BLUEZ4_PROFILE_COUNT PA_BLUEZ4_PROFILE_OFF

struct pa_bluez4_hook_uuid_data {
    pa_bluez4_device *device;
    const char *uuid;
};

/* Hook data: pa_bluez4_discovery pointer. */
typedef enum pa_bluez4_hook {
    PA_BLUEZ4_HOOK_DEVICE_CONNECTION_CHANGED, /* Call data: pa_bluez4_device */
    PA_BLUEZ4_HOOK_DEVICE_UUID_ADDED, /* Call data: pa_bluez4_hook_uuid_data */
    PA_BLUEZ4_HOOK_TRANSPORT_STATE_CHANGED, /* Call data: pa_bluez4_transport */
    PA_BLUEZ4_HOOK_TRANSPORT_NREC_CHANGED, /* Call data: pa_bluez4_transport */
    PA_BLUEZ4_HOOK_TRANSPORT_MICROPHONE_GAIN_CHANGED, /* Call data: pa_bluez4_transport */
    PA_BLUEZ4_HOOK_TRANSPORT_SPEAKER_GAIN_CHANGED, /* Call data: pa_bluez4_transport */
    PA_BLUEZ4_HOOK_MAX
} pa_bluez4_hook_t;

typedef enum pa_bluez4_transport_state {
    PA_BLUEZ4_TRANSPORT_STATE_DISCONNECTED,
    PA_BLUEZ4_TRANSPORT_STATE_IDLE, /* Connected but not playing */
    PA_BLUEZ4_TRANSPORT_STATE_PLAYING
} pa_bluez4_transport_state_t;

struct pa_bluez4_transport {
    pa_bluez4_device *device;
    char *owner;
    char *path;
    pa_bluez4_profile_t profile;
    uint8_t codec;
    uint8_t *config;
    int config_size;

    pa_bluez4_transport_state_t state;
    bool nrec;
    uint16_t microphone_gain; /* Used for HSP/HFP */
    uint16_t speaker_gain; /* Used for HSP/HFP */
};

/* This enum is shared among Audio, Headset, AudioSink, and AudioSource, although not all values are acceptable in all profiles */
typedef enum pa_bluez4_audio_state {
    PA_BLUEZ4_AUDIO_STATE_INVALID = -1,
    PA_BLUEZ4_AUDIO_STATE_DISCONNECTED,
    PA_BLUEZ4_AUDIO_STATE_CONNECTING,
    PA_BLUEZ4_AUDIO_STATE_CONNECTED,
    PA_BLUEZ4_AUDIO_STATE_PLAYING
} pa_bluez4_audio_state_t;

struct pa_bluez4_device {
    pa_bluez4_discovery *discovery;
    bool dead;

    int device_info_valid;      /* 0: no results yet; 1: good results; -1: bad results ... */

    /* Device information */
    char *name;
    char *path;
    pa_bluez4_transport *transports[PA_BLUEZ4_PROFILE_COUNT];
    int paired;
    char *alias;
    PA_LLIST_HEAD(pa_bluez4_uuid, uuids);
    char *address;
    int class;
    int trusted;

    /* Audio state */
    pa_bluez4_audio_state_t audio_state;

    /* AudioSink, AudioSource, Headset and HandsfreeGateway states */
    pa_bluez4_audio_state_t profile_state[PA_BLUEZ4_PROFILE_COUNT];
};

pa_bluez4_discovery* pa_bluez4_discovery_get(pa_core *core);
pa_bluez4_discovery* pa_bluez4_discovery_ref(pa_bluez4_discovery *y);
void pa_bluez4_discovery_unref(pa_bluez4_discovery *d);

pa_bluez4_device* pa_bluez4_discovery_get_by_path(pa_bluez4_discovery *d, const char* path);
pa_bluez4_device* pa_bluez4_discovery_get_by_address(pa_bluez4_discovery *d, const char* address);

bool pa_bluez4_device_any_audio_connected(const pa_bluez4_device *d);

int pa_bluez4_transport_acquire(pa_bluez4_transport *t, bool optional, size_t *imtu, size_t *omtu);
void pa_bluez4_transport_release(pa_bluez4_transport *t);

void pa_bluez4_transport_set_microphone_gain(pa_bluez4_transport *t, uint16_t value);
void pa_bluez4_transport_set_speaker_gain(pa_bluez4_transport *t, uint16_t value);

pa_hook* pa_bluez4_discovery_hook(pa_bluez4_discovery *y, pa_bluez4_hook_t hook);

typedef enum pa_bluez4_form_factor {
    PA_BLUEZ4_FORM_FACTOR_UNKNOWN,
    PA_BLUEZ4_FORM_FACTOR_HEADSET,
    PA_BLUEZ4_FORM_FACTOR_HANDSFREE,
    PA_BLUEZ4_FORM_FACTOR_MICROPHONE,
    PA_BLUEZ4_FORM_FACTOR_SPEAKER,
    PA_BLUEZ4_FORM_FACTOR_HEADPHONE,
    PA_BLUEZ4_FORM_FACTOR_PORTABLE,
    PA_BLUEZ4_FORM_FACTOR_CAR,
    PA_BLUEZ4_FORM_FACTOR_HIFI,
    PA_BLUEZ4_FORM_FACTOR_PHONE,
} pa_bluez4_form_factor_t;

pa_bluez4_form_factor_t pa_bluez4_get_form_factor(uint32_t class);
const char *pa_bluez4_form_factor_to_string(pa_bluez4_form_factor_t ff);

char *pa_bluez4_cleanup_name(const char *name);

bool pa_bluez4_uuid_has(pa_bluez4_uuid *uuids, const char *uuid);
const char *pa_bluez4_profile_to_string(pa_bluez4_profile_t profile);

#endif
