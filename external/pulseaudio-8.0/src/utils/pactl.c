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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <locale.h>
#include <ctype.h>

#include <sndfile.h>

#include <pulse/pulseaudio.h>
#include <pulse/ext-device-restore.h>

#include <pulsecore/i18n.h>
#include <pulsecore/macro.h>
#include <pulsecore/core-util.h>
#include <pulsecore/log.h>
#include <pulsecore/sndfile-util.h>

static pa_context *context = NULL;
static pa_mainloop_api *mainloop_api = NULL;

static char
    *list_type = NULL,
    *sample_name = NULL,
    *sink_name = NULL,
    *source_name = NULL,
    *module_name = NULL,
    *module_args = NULL,
    *card_name = NULL,
    *profile_name = NULL,
    *port_name = NULL,
    *formats = NULL;

static uint32_t
    sink_input_idx = PA_INVALID_INDEX,
    source_output_idx = PA_INVALID_INDEX,
    sink_idx = PA_INVALID_INDEX;

static bool short_list_format = false;
static uint32_t module_index;
static int32_t latency_offset;
static bool suspend;
static pa_cvolume volume;
static enum volume_flags {
    VOL_UINT     = 0,
    VOL_PERCENT  = 1,
    VOL_LINEAR   = 2,
    VOL_DECIBEL  = 3,
    VOL_ABSOLUTE = 0 << 4,
    VOL_RELATIVE = 1 << 4,
} volume_flags;

static enum mute_flags {
    INVALID_MUTE = -1,
    UNMUTE = 0,
    MUTE = 1,
    TOGGLE_MUTE = 2
} mute = INVALID_MUTE;

static pa_proplist *proplist = NULL;

static SNDFILE *sndfile = NULL;
static pa_stream *sample_stream = NULL;
static pa_sample_spec sample_spec;
static pa_channel_map channel_map;
static size_t sample_length = 0;

/* This variable tracks the number of ongoing asynchronous operations. When a
 * new operation begins, this is incremented simply with actions++, and when
 * an operation finishes, this is decremented with the complete_action()
 * function, which shuts down the program if actions reaches zero. */
static int actions = 0;

static bool nl = false;

static enum {
    NONE,
    EXIT,
    STAT,
    INFO,
    UPLOAD_SAMPLE,
    PLAY_SAMPLE,
    REMOVE_SAMPLE,
    LIST,
    MOVE_SINK_INPUT,
    MOVE_SOURCE_OUTPUT,
    LOAD_MODULE,
    UNLOAD_MODULE,
    SUSPEND_SINK,
    SUSPEND_SOURCE,
    SET_CARD_PROFILE,
    SET_SINK_PORT,
    SET_DEFAULT_SINK,
    SET_SOURCE_PORT,
    SET_DEFAULT_SOURCE,
    SET_SINK_VOLUME,
    SET_SOURCE_VOLUME,
    SET_SINK_INPUT_VOLUME,
    SET_SOURCE_OUTPUT_VOLUME,
    SET_SINK_MUTE,
    SET_SOURCE_MUTE,
    SET_SINK_INPUT_MUTE,
    SET_SOURCE_OUTPUT_MUTE,
    SET_SINK_FORMATS,
    SET_PORT_LATENCY_OFFSET,
    SUBSCRIBE
} action = NONE;

static void quit(int ret) {
    pa_assert(mainloop_api);
    mainloop_api->quit(mainloop_api, ret);
}

static void context_drain_complete(pa_context *c, void *userdata) {
    pa_context_disconnect(c);
}

static void drain(void) {
    pa_operation *o;

    if (!(o = pa_context_drain(context, context_drain_complete, NULL)))
        pa_context_disconnect(context);
    else
        pa_operation_unref(o);
}

static void complete_action(void) {
    pa_assert(actions > 0);

    if (!(--actions))
        drain();
}

static void stat_callback(pa_context *c, const pa_stat_info *i, void *userdata) {
    char s[PA_BYTES_SNPRINT_MAX];
    if (!i) {
        pa_log(_("Failed to get statistics: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    pa_bytes_snprint(s, sizeof(s), i->memblock_total_size);
    printf(_("Currently in use: %u blocks containing %s bytes total.\n"), i->memblock_total, s);

    pa_bytes_snprint(s, sizeof(s), i->memblock_allocated_size);
    printf(_("Allocated during whole lifetime: %u blocks containing %s bytes total.\n"), i->memblock_allocated, s);

    pa_bytes_snprint(s, sizeof(s), i->scache_size);
    printf(_("Sample cache size: %s\n"), s);

    complete_action();
}

static void get_server_info_callback(pa_context *c, const pa_server_info *i, void *useerdata) {
    char ss[PA_SAMPLE_SPEC_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];

    if (!i) {
        pa_log(_("Failed to get server information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    printf(_("Server String: %s\n"
             "Library Protocol Version: %u\n"
             "Server Protocol Version: %u\n"
             "Is Local: %s\n"
             "Client Index: %u\n"
             "Tile Size: %zu\n"),
             pa_context_get_server(c),
             pa_context_get_protocol_version(c),
             pa_context_get_server_protocol_version(c),
             pa_yes_no_localised(pa_context_is_local(c)),
             pa_context_get_index(c),
             pa_context_get_tile_size(c, NULL));

    pa_sample_spec_snprint(ss, sizeof(ss), &i->sample_spec);
    pa_channel_map_snprint(cm, sizeof(cm), &i->channel_map);

    printf(_("User Name: %s\n"
             "Host Name: %s\n"
             "Server Name: %s\n"
             "Server Version: %s\n"
             "Default Sample Specification: %s\n"
             "Default Channel Map: %s\n"
             "Default Sink: %s\n"
             "Default Source: %s\n"
             "Cookie: %04x:%04x\n"),
           i->user_name,
           i->host_name,
           i->server_name,
           i->server_version,
           ss,
           cm,
           i->default_sink_name,
           i->default_source_name,
           i->cookie >> 16,
           i->cookie & 0xFFFFU);

    complete_action();
}

static const char* get_available_str_ynonly(int available) {
    switch (available) {
        case PA_PORT_AVAILABLE_YES: return ", available";
        case PA_PORT_AVAILABLE_NO: return ", not available";
    }
    return "";
}

static void get_sink_info_callback(pa_context *c, const pa_sink_info *i, int is_last, void *userdata) {

    static const char *state_table[] = {
        [1+PA_SINK_INVALID_STATE] = "n/a",
        [1+PA_SINK_RUNNING] = "RUNNING",
        [1+PA_SINK_IDLE] = "IDLE",
        [1+PA_SINK_SUSPENDED] = "SUSPENDED"
    };

    char
        s[PA_SAMPLE_SPEC_SNPRINT_MAX],
        cv[PA_CVOLUME_SNPRINT_VERBOSE_MAX],
        v[PA_VOLUME_SNPRINT_VERBOSE_MAX],
        cm[PA_CHANNEL_MAP_SNPRINT_MAX],
        f[PA_FORMAT_INFO_SNPRINT_MAX];
    char *pl;

    if (is_last < 0) {
        pa_log(_("Failed to get sink information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last) {
        complete_action();
        return;
    }

    pa_assert(i);

    if (nl && !short_list_format)
        printf("\n");
    nl = true;

    if (short_list_format) {
        printf("%u\t%s\t%s\t%s\t%s\n",
               i->index,
               i->name,
               pa_strnull(i->driver),
               pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec),
               state_table[1+i->state]);
        return;
    }

    printf(_("Sink #%u\n"
             "\tState: %s\n"
             "\tName: %s\n"
             "\tDescription: %s\n"
             "\tDriver: %s\n"
             "\tSample Specification: %s\n"
             "\tChannel Map: %s\n"
             "\tOwner Module: %u\n"
             "\tMute: %s\n"
             "\tVolume: %s\n"
             "\t        balance %0.2f\n"
             "\tBase Volume: %s\n"
             "\tMonitor Source: %s\n"
             "\tLatency: %0.0f usec, configured %0.0f usec\n"
             "\tFlags: %s%s%s%s%s%s%s\n"
             "\tProperties:\n\t\t%s\n"),
           i->index,
           state_table[1+i->state],
           i->name,
           pa_strnull(i->description),
           pa_strnull(i->driver),
           pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec),
           pa_channel_map_snprint(cm, sizeof(cm), &i->channel_map),
           i->owner_module,
           pa_yes_no_localised(i->mute),
           pa_cvolume_snprint_verbose(cv, sizeof(cv), &i->volume, &i->channel_map, i->flags & PA_SINK_DECIBEL_VOLUME),
           pa_cvolume_get_balance(&i->volume, &i->channel_map),
           pa_volume_snprint_verbose(v, sizeof(v), i->base_volume, i->flags & PA_SINK_DECIBEL_VOLUME),
           pa_strnull(i->monitor_source_name),
           (double) i->latency, (double) i->configured_latency,
           i->flags & PA_SINK_HARDWARE ? "HARDWARE " : "",
           i->flags & PA_SINK_NETWORK ? "NETWORK " : "",
           i->flags & PA_SINK_HW_MUTE_CTRL ? "HW_MUTE_CTRL " : "",
           i->flags & PA_SINK_HW_VOLUME_CTRL ? "HW_VOLUME_CTRL " : "",
           i->flags & PA_SINK_DECIBEL_VOLUME ? "DECIBEL_VOLUME " : "",
           i->flags & PA_SINK_LATENCY ? "LATENCY " : "",
           i->flags & PA_SINK_SET_FORMATS ? "SET_FORMATS " : "",
           pl = pa_proplist_to_string_sep(i->proplist, "\n\t\t"));

    pa_xfree(pl);

    if (i->ports) {
        pa_sink_port_info **p;

        printf(_("\tPorts:\n"));
        for (p = i->ports; *p; p++)
            printf("\t\t%s: %s (priority: %u%s)\n", (*p)->name, (*p)->description,
                    (*p)->priority, get_available_str_ynonly((*p)->available));
    }

    if (i->active_port)
        printf(_("\tActive Port: %s\n"),
               i->active_port->name);

    if (i->formats) {
        uint8_t j;

        printf(_("\tFormats:\n"));
        for (j = 0; j < i->n_formats; j++)
            printf("\t\t%s\n", pa_format_info_snprint(f, sizeof(f), i->formats[j]));
    }
}

static void get_source_info_callback(pa_context *c, const pa_source_info *i, int is_last, void *userdata) {

    static const char *state_table[] = {
        [1+PA_SOURCE_INVALID_STATE] = "n/a",
        [1+PA_SOURCE_RUNNING] = "RUNNING",
        [1+PA_SOURCE_IDLE] = "IDLE",
        [1+PA_SOURCE_SUSPENDED] = "SUSPENDED"
    };

    char
        s[PA_SAMPLE_SPEC_SNPRINT_MAX],
        cv[PA_CVOLUME_SNPRINT_VERBOSE_MAX],
        v[PA_VOLUME_SNPRINT_VERBOSE_MAX],
        cm[PA_CHANNEL_MAP_SNPRINT_MAX],
        f[PA_FORMAT_INFO_SNPRINT_MAX];
    char *pl;

    if (is_last < 0) {
        pa_log(_("Failed to get source information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last) {
        complete_action();
        return;
    }

    pa_assert(i);

    if (nl && !short_list_format)
        printf("\n");
    nl = true;

    if (short_list_format) {
        printf("%u\t%s\t%s\t%s\t%s\n",
               i->index,
               i->name,
               pa_strnull(i->driver),
               pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec),
               state_table[1+i->state]);
        return;
    }

    printf(_("Source #%u\n"
             "\tState: %s\n"
             "\tName: %s\n"
             "\tDescription: %s\n"
             "\tDriver: %s\n"
             "\tSample Specification: %s\n"
             "\tChannel Map: %s\n"
             "\tOwner Module: %u\n"
             "\tMute: %s\n"
             "\tVolume: %s\n"
             "\t        balance %0.2f\n"
             "\tBase Volume: %s\n"
             "\tMonitor of Sink: %s\n"
             "\tLatency: %0.0f usec, configured %0.0f usec\n"
             "\tFlags: %s%s%s%s%s%s\n"
             "\tProperties:\n\t\t%s\n"),
           i->index,
           state_table[1+i->state],
           i->name,
           pa_strnull(i->description),
           pa_strnull(i->driver),
           pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec),
           pa_channel_map_snprint(cm, sizeof(cm), &i->channel_map),
           i->owner_module,
           pa_yes_no_localised(i->mute),
           pa_cvolume_snprint_verbose(cv, sizeof(cv), &i->volume, &i->channel_map, i->flags & PA_SOURCE_DECIBEL_VOLUME),
           pa_cvolume_get_balance(&i->volume, &i->channel_map),
           pa_volume_snprint_verbose(v, sizeof(v), i->base_volume, i->flags & PA_SOURCE_DECIBEL_VOLUME),
           i->monitor_of_sink_name ? i->monitor_of_sink_name : _("n/a"),
           (double) i->latency, (double) i->configured_latency,
           i->flags & PA_SOURCE_HARDWARE ? "HARDWARE " : "",
           i->flags & PA_SOURCE_NETWORK ? "NETWORK " : "",
           i->flags & PA_SOURCE_HW_MUTE_CTRL ? "HW_MUTE_CTRL " : "",
           i->flags & PA_SOURCE_HW_VOLUME_CTRL ? "HW_VOLUME_CTRL " : "",
           i->flags & PA_SOURCE_DECIBEL_VOLUME ? "DECIBEL_VOLUME " : "",
           i->flags & PA_SOURCE_LATENCY ? "LATENCY " : "",
           pl = pa_proplist_to_string_sep(i->proplist, "\n\t\t"));

    pa_xfree(pl);

    if (i->ports) {
        pa_source_port_info **p;

        printf(_("\tPorts:\n"));
        for (p = i->ports; *p; p++)
            printf("\t\t%s: %s (priority: %u%s)\n", (*p)->name, (*p)->description,
                    (*p)->priority, get_available_str_ynonly((*p)->available));
    }

    if (i->active_port)
        printf(_("\tActive Port: %s\n"),
               i->active_port->name);

    if (i->formats) {
        uint8_t j;

        printf(_("\tFormats:\n"));
        for (j = 0; j < i->n_formats; j++)
            printf("\t\t%s\n", pa_format_info_snprint(f, sizeof(f), i->formats[j]));
    }
}

static void get_module_info_callback(pa_context *c, const pa_module_info *i, int is_last, void *userdata) {
    char t[32];
    char *pl;

    if (is_last < 0) {
        pa_log(_("Failed to get module information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last) {
        complete_action();
        return;
    }

    pa_assert(i);

    if (nl && !short_list_format)
        printf("\n");
    nl = true;

    pa_snprintf(t, sizeof(t), "%u", i->n_used);

    if (short_list_format) {
        printf("%u\t%s\t%s\t\n", i->index, i->name, i->argument ? i->argument : "");
        return;
    }

    printf(_("Module #%u\n"
             "\tName: %s\n"
             "\tArgument: %s\n"
             "\tUsage counter: %s\n"
             "\tProperties:\n\t\t%s\n"),
           i->index,
           i->name,
           i->argument ? i->argument : "",
           i->n_used != PA_INVALID_INDEX ? t : _("n/a"),
           pl = pa_proplist_to_string_sep(i->proplist, "\n\t\t"));

    pa_xfree(pl);
}

static void get_client_info_callback(pa_context *c, const pa_client_info *i, int is_last, void *userdata) {
    char t[32];
    char *pl;

    if (is_last < 0) {
        pa_log(_("Failed to get client information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last) {
        complete_action();
        return;
    }

    pa_assert(i);

    if (nl && !short_list_format)
        printf("\n");
    nl = true;

    pa_snprintf(t, sizeof(t), "%u", i->owner_module);

    if (short_list_format) {
        printf("%u\t%s\t%s\n",
               i->index,
               pa_strnull(i->driver),
               pa_strnull(pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_PROCESS_BINARY)));
        return;
    }

    printf(_("Client #%u\n"
             "\tDriver: %s\n"
             "\tOwner Module: %s\n"
             "\tProperties:\n\t\t%s\n"),
           i->index,
           pa_strnull(i->driver),
           i->owner_module != PA_INVALID_INDEX ? t : _("n/a"),
           pl = pa_proplist_to_string_sep(i->proplist, "\n\t\t"));

    pa_xfree(pl);
}

static void get_card_info_callback(pa_context *c, const pa_card_info *i, int is_last, void *userdata) {
    char t[32];
    char *pl;

    if (is_last < 0) {
        pa_log(_("Failed to get card information: %s"), pa_strerror(pa_context_errno(c)));
        complete_action();
        return;
    }

    if (is_last) {
        complete_action();
        return;
    }

    pa_assert(i);

    if (nl && !short_list_format)
        printf("\n");
    nl = true;

    pa_snprintf(t, sizeof(t), "%u", i->owner_module);

    if (short_list_format) {
        printf("%u\t%s\t%s\n", i->index, i->name, pa_strnull(i->driver));
        return;
    }

    printf(_("Card #%u\n"
             "\tName: %s\n"
             "\tDriver: %s\n"
             "\tOwner Module: %s\n"
             "\tProperties:\n\t\t%s\n"),
           i->index,
           i->name,
           pa_strnull(i->driver),
           i->owner_module != PA_INVALID_INDEX ? t : _("n/a"),
           pl = pa_proplist_to_string_sep(i->proplist, "\n\t\t"));

    pa_xfree(pl);

    if (i->n_profiles > 0) {
        pa_card_profile_info2 **p;

        printf(_("\tProfiles:\n"));
        for (p = i->profiles2; *p; p++)
            printf(_("\t\t%s: %s (sinks: %u, sources: %u, priority: %u, available: %s)\n"), (*p)->name,
                (*p)->description, (*p)->n_sinks, (*p)->n_sources, (*p)->priority, pa_yes_no_localised((*p)->available));
    }

    if (i->active_profile)
        printf(_("\tActive Profile: %s\n"),
               i->active_profile->name);

    if (i->ports) {
        pa_card_port_info **p;

        printf(_("\tPorts:\n"));
        for (p = i->ports; *p; p++) {
            pa_card_profile_info **pr = (*p)->profiles;
            printf("\t\t%s: %s (priority: %u, latency offset: %" PRId64 " usec%s)\n", (*p)->name,
                (*p)->description, (*p)->priority, (*p)->latency_offset,
                get_available_str_ynonly((*p)->available));

            if (!pa_proplist_isempty((*p)->proplist)) {
                printf(_("\t\t\tProperties:\n\t\t\t\t%s\n"), pl = pa_proplist_to_string_sep((*p)->proplist, "\n\t\t\t\t"));
                pa_xfree(pl);
            }

            if (pr) {
                printf(_("\t\t\tPart of profile(s): %s"), pa_strnull((*pr)->name));
                pr++;
                while (*pr) {
                    printf(", %s", pa_strnull((*pr)->name));
                    pr++;
                }
                printf("\n");
            }
        }
    }
}

static void get_sink_input_info_callback(pa_context *c, const pa_sink_input_info *i, int is_last, void *userdata) {
    char t[32], k[32], s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_VERBOSE_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX], f[PA_FORMAT_INFO_SNPRINT_MAX];
    char *pl;

    if (is_last < 0) {
        pa_log(_("Failed to get sink input information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last) {
        complete_action();
        return;
    }

    pa_assert(i);

    if (nl && !short_list_format)
        printf("\n");
    nl = true;

    pa_snprintf(t, sizeof(t), "%u", i->owner_module);
    pa_snprintf(k, sizeof(k), "%u", i->client);

    if (short_list_format) {
        printf("%u\t%u\t%s\t%s\t%s\n",
               i->index,
               i->sink,
               i->client != PA_INVALID_INDEX ? k : "-",
               pa_strnull(i->driver),
               pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec));
        return;
    }

    printf(_("Sink Input #%u\n"
             "\tDriver: %s\n"
             "\tOwner Module: %s\n"
             "\tClient: %s\n"
             "\tSink: %u\n"
             "\tSample Specification: %s\n"
             "\tChannel Map: %s\n"
             "\tFormat: %s\n"
             "\tCorked: %s\n"
             "\tMute: %s\n"
             "\tVolume: %s\n"
             "\t        balance %0.2f\n"
             "\tBuffer Latency: %0.0f usec\n"
             "\tSink Latency: %0.0f usec\n"
             "\tResample method: %s\n"
             "\tProperties:\n\t\t%s\n"),
           i->index,
           pa_strnull(i->driver),
           i->owner_module != PA_INVALID_INDEX ? t : _("n/a"),
           i->client != PA_INVALID_INDEX ? k : _("n/a"),
           i->sink,
           pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec),
           pa_channel_map_snprint(cm, sizeof(cm), &i->channel_map),
           pa_format_info_snprint(f, sizeof(f), i->format),
           pa_yes_no_localised(i->corked),
           pa_yes_no_localised(i->mute),
           pa_cvolume_snprint_verbose(cv, sizeof(cv), &i->volume, &i->channel_map, true),
           pa_cvolume_get_balance(&i->volume, &i->channel_map),
           (double) i->buffer_usec,
           (double) i->sink_usec,
           i->resample_method ? i->resample_method : _("n/a"),
           pl = pa_proplist_to_string_sep(i->proplist, "\n\t\t"));

    pa_xfree(pl);
}

static void get_source_output_info_callback(pa_context *c, const pa_source_output_info *i, int is_last, void *userdata) {
    char t[32], k[32], s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_VERBOSE_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX], f[PA_FORMAT_INFO_SNPRINT_MAX];
    char *pl;

    if (is_last < 0) {
        pa_log(_("Failed to get source output information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last) {
        complete_action();
        return;
    }

    pa_assert(i);

    if (nl && !short_list_format)
        printf("\n");
    nl = true;

    pa_snprintf(t, sizeof(t), "%u", i->owner_module);
    pa_snprintf(k, sizeof(k), "%u", i->client);

    if (short_list_format) {
        printf("%u\t%u\t%s\t%s\t%s\n",
               i->index,
               i->source,
               i->client != PA_INVALID_INDEX ? k : "-",
               pa_strnull(i->driver),
               pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec));
        return;
    }

    printf(_("Source Output #%u\n"
             "\tDriver: %s\n"
             "\tOwner Module: %s\n"
             "\tClient: %s\n"
             "\tSource: %u\n"
             "\tSample Specification: %s\n"
             "\tChannel Map: %s\n"
             "\tFormat: %s\n"
             "\tCorked: %s\n"
             "\tMute: %s\n"
             "\tVolume: %s\n"
             "\t        balance %0.2f\n"
             "\tBuffer Latency: %0.0f usec\n"
             "\tSource Latency: %0.0f usec\n"
             "\tResample method: %s\n"
             "\tProperties:\n\t\t%s\n"),
           i->index,
           pa_strnull(i->driver),
           i->owner_module != PA_INVALID_INDEX ? t : _("n/a"),
           i->client != PA_INVALID_INDEX ? k : _("n/a"),
           i->source,
           pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec),
           pa_channel_map_snprint(cm, sizeof(cm), &i->channel_map),
           pa_format_info_snprint(f, sizeof(f), i->format),
           pa_yes_no_localised(i->corked),
           pa_yes_no_localised(i->mute),
           pa_cvolume_snprint_verbose(cv, sizeof(cv), &i->volume, &i->channel_map, true),
           pa_cvolume_get_balance(&i->volume, &i->channel_map),
           (double) i->buffer_usec,
           (double) i->source_usec,
           i->resample_method ? i->resample_method : _("n/a"),
           pl = pa_proplist_to_string_sep(i->proplist, "\n\t\t"));

    pa_xfree(pl);
}

static void get_sample_info_callback(pa_context *c, const pa_sample_info *i, int is_last, void *userdata) {
    char t[PA_BYTES_SNPRINT_MAX], s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_VERBOSE_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    char *pl;

    if (is_last < 0) {
        pa_log(_("Failed to get sample information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last) {
        complete_action();
        return;
    }

    pa_assert(i);

    if (nl && !short_list_format)
        printf("\n");
    nl = true;

    pa_bytes_snprint(t, sizeof(t), i->bytes);

    if (short_list_format) {
        printf("%u\t%s\t%s\t%0.3f\n",
               i->index,
               i->name,
               pa_sample_spec_valid(&i->sample_spec) ? pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec) : "-",
               (double) i->duration/1000000.0);
        return;
    }

    printf(_("Sample #%u\n"
             "\tName: %s\n"
             "\tSample Specification: %s\n"
             "\tChannel Map: %s\n"
             "\tVolume: %s\n"
             "\t        balance %0.2f\n"
             "\tDuration: %0.1fs\n"
             "\tSize: %s\n"
             "\tLazy: %s\n"
             "\tFilename: %s\n"
             "\tProperties:\n\t\t%s\n"),
           i->index,
           i->name,
           pa_sample_spec_valid(&i->sample_spec) ? pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec) : _("n/a"),
           pa_sample_spec_valid(&i->sample_spec) ? pa_channel_map_snprint(cm, sizeof(cm), &i->channel_map) : _("n/a"),
           pa_cvolume_snprint_verbose(cv, sizeof(cv), &i->volume, &i->channel_map, true),
           pa_cvolume_get_balance(&i->volume, &i->channel_map),
           (double) i->duration/1000000.0,
           t,
           pa_yes_no_localised(i->lazy),
           i->filename ? i->filename : _("n/a"),
           pl = pa_proplist_to_string_sep(i->proplist, "\n\t\t"));

    pa_xfree(pl);
}

static void simple_callback(pa_context *c, int success, void *userdata) {
    if (!success) {
        pa_log(_("Failure: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    complete_action();
}

static void index_callback(pa_context *c, uint32_t idx, void *userdata) {
    if (idx == PA_INVALID_INDEX) {
        pa_log(_("Failure: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    printf("%u\n", idx);

    complete_action();
}

static void volume_relative_adjust(pa_cvolume *cv) {
    pa_assert(volume_flags & VOL_RELATIVE);

    /* Relative volume change is additive in case of UINT or PERCENT
     * and multiplicative for LINEAR or DECIBEL */
    if ((volume_flags & 0x0F) == VOL_UINT || (volume_flags & 0x0F) == VOL_PERCENT) {
        unsigned i;
        for (i = 0; i < cv->channels; i++) {
            if (cv->values[i] + volume.values[i] < PA_VOLUME_NORM)
                cv->values[i] = PA_VOLUME_MUTED;
            else
                cv->values[i] = cv->values[i] + volume.values[i] - PA_VOLUME_NORM;
        }
    }
    if ((volume_flags & 0x0F) == VOL_LINEAR || (volume_flags & 0x0F) == VOL_DECIBEL)
        pa_sw_cvolume_multiply(cv, cv, &volume);
}

static void unload_module_by_name_callback(pa_context *c, const pa_module_info *i, int is_last, void *userdata) {
    static bool unloaded = false;

    if (is_last < 0) {
        pa_log(_("Failed to get module information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last) {
        if (unloaded == false)
            pa_log(_("Failed to unload module: Module %s not loaded"), module_name);
        complete_action();
        return;
    }

    pa_assert(i);

    if (pa_streq(module_name, i->name)) {
        unloaded = true;
        actions++;
        pa_operation_unref(pa_context_unload_module(c, i->index, simple_callback, NULL));
    }
}

static void fill_volume(pa_cvolume *cv, unsigned supported) {
    if (volume.channels == 1) {
        pa_cvolume_set(&volume, supported, volume.values[0]);
    } else if (volume.channels != supported) {
        pa_log(_("Failed to set volume: You tried to set volumes for %d channels, whereas channel/s supported = %d\n"),
            volume.channels, supported);
        quit(1);
        return;
    }

    if (volume_flags & VOL_RELATIVE)
        volume_relative_adjust(cv);
    else
        *cv = volume;
}

static void get_sink_volume_callback(pa_context *c, const pa_sink_info *i, int is_last, void *userdata) {
    pa_cvolume cv;

    if (is_last < 0) {
        pa_log(_("Failed to get sink information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last)
        return;

    pa_assert(i);

    cv = i->volume;
    fill_volume(&cv, i->channel_map.channels);

    pa_operation_unref(pa_context_set_sink_volume_by_name(c, sink_name, &cv, simple_callback, NULL));
}

static void get_source_volume_callback(pa_context *c, const pa_source_info *i, int is_last, void *userdata) {
    pa_cvolume cv;

    if (is_last < 0) {
        pa_log(_("Failed to get source information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last)
        return;

    pa_assert(i);

    cv = i->volume;
    fill_volume(&cv, i->channel_map.channels);

    pa_operation_unref(pa_context_set_source_volume_by_name(c, source_name, &cv, simple_callback, NULL));
}

static void get_sink_input_volume_callback(pa_context *c, const pa_sink_input_info *i, int is_last, void *userdata) {
    pa_cvolume cv;

    if (is_last < 0) {
        pa_log(_("Failed to get sink input information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last)
        return;

    pa_assert(i);

    cv = i->volume;
    fill_volume(&cv, i->channel_map.channels);

    pa_operation_unref(pa_context_set_sink_input_volume(c, sink_input_idx, &cv, simple_callback, NULL));
}

static void get_source_output_volume_callback(pa_context *c, const pa_source_output_info *o, int is_last, void *userdata) {
    pa_cvolume cv;

    if (is_last < 0) {
        pa_log(_("Failed to get source output information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last)
        return;

    pa_assert(o);

    cv = o->volume;
    fill_volume(&cv, o->channel_map.channels);

    pa_operation_unref(pa_context_set_source_output_volume(c, source_output_idx, &cv, simple_callback, NULL));
}

static void sink_toggle_mute_callback(pa_context *c, const pa_sink_info *i, int is_last, void *userdata) {
    if (is_last < 0) {
        pa_log(_("Failed to get sink information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last)
        return;

    pa_assert(i);

    pa_operation_unref(pa_context_set_sink_mute_by_name(c, i->name, !i->mute, simple_callback, NULL));
}

static void source_toggle_mute_callback(pa_context *c, const pa_source_info *o, int is_last, void *userdata) {
    if (is_last < 0) {
        pa_log(_("Failed to get source information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last)
        return;

    pa_assert(o);

    pa_operation_unref(pa_context_set_source_mute_by_name(c, o->name, !o->mute, simple_callback, NULL));
}

static void sink_input_toggle_mute_callback(pa_context *c, const pa_sink_input_info *i, int is_last, void *userdata) {
    if (is_last < 0) {
        pa_log(_("Failed to get sink input information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last)
        return;

    pa_assert(i);

    pa_operation_unref(pa_context_set_sink_input_mute(c, i->index, !i->mute, simple_callback, NULL));
}

static void source_output_toggle_mute_callback(pa_context *c, const pa_source_output_info *o, int is_last, void *userdata) {
    if (is_last < 0) {
        pa_log(_("Failed to get source output information: %s"), pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (is_last)
        return;

    pa_assert(o);

    pa_operation_unref(pa_context_set_source_output_mute(c, o->index, !o->mute, simple_callback, NULL));
}

/* PA_MAX_FORMATS is defined in internal.h so we just define a sane value here */
#define MAX_FORMATS 256

static void set_sink_formats(pa_context *c, uint32_t sink, const char *str) {
    pa_format_info *f_arr[MAX_FORMATS];
    char *format = NULL;
    const char *state = NULL;
    int i = 0;
    pa_operation *o = NULL;

    while ((format = pa_split(str, ";", &state))) {
        pa_format_info *f = pa_format_info_from_string(pa_strip(format));

        if (!f) {
            pa_log(_("Failed to set format: invalid format string %s"), format);
            goto error;
        }

        f_arr[i++] = f;
        pa_xfree(format);
    }

    o = pa_ext_device_restore_save_formats(c, PA_DEVICE_TYPE_SINK, sink, i, f_arr, simple_callback, NULL);
    if (o) {
        pa_operation_unref(o);
        actions++;
    }

done:
    if (format)
        pa_xfree(format);
    while(i--)
        pa_format_info_free(f_arr[i]);

    return;

error:
    while(i--)
        pa_format_info_free(f_arr[i]);
    quit(1);
    goto done;
}

static void stream_state_callback(pa_stream *s, void *userdata) {
    pa_assert(s);

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_CREATING:
        case PA_STREAM_READY:
            break;

        case PA_STREAM_TERMINATED:
            drain();
            break;

        case PA_STREAM_FAILED:
        default:
            pa_log(_("Failed to upload sample: %s"), pa_strerror(pa_context_errno(pa_stream_get_context(s))));
            quit(1);
    }
}

static void stream_write_callback(pa_stream *s, size_t length, void *userdata) {
    sf_count_t l;
    float *d;
    pa_assert(s && length && sndfile);

    d = pa_xmalloc(length);

    pa_assert(sample_length >= length);
    l = (sf_count_t) (length/pa_frame_size(&sample_spec));

    if ((sf_readf_float(sndfile, d, l)) != l) {
        pa_xfree(d);
        pa_log(_("Premature end of file"));
        quit(1);
        return;
    }

    pa_stream_write(s, d, length, pa_xfree, 0, PA_SEEK_RELATIVE);

    sample_length -= length;

    if (sample_length <= 0) {
        pa_stream_set_write_callback(sample_stream, NULL, NULL);
        pa_stream_finish_upload(sample_stream);
    }
}

static const char *subscription_event_type_to_string(pa_subscription_event_type_t t) {

    switch (t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {

    case PA_SUBSCRIPTION_EVENT_NEW:
        return _("new");

    case PA_SUBSCRIPTION_EVENT_CHANGE:
        return _("change");

    case PA_SUBSCRIPTION_EVENT_REMOVE:
        return _("remove");
    }

    return _("unknown");
}

static const char *subscription_event_facility_to_string(pa_subscription_event_type_t t) {

    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {

    case PA_SUBSCRIPTION_EVENT_SINK:
        return _("sink");

    case PA_SUBSCRIPTION_EVENT_SOURCE:
        return _("source");

    case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
        return _("sink-input");

    case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
        return _("source-output");

    case PA_SUBSCRIPTION_EVENT_MODULE:
        return _("module");

    case PA_SUBSCRIPTION_EVENT_CLIENT:
        return _("client");

    case PA_SUBSCRIPTION_EVENT_SAMPLE_CACHE:
        return _("sample-cache");

    case PA_SUBSCRIPTION_EVENT_SERVER:
        return _("server");

    case PA_SUBSCRIPTION_EVENT_CARD:
        return _("card");
    }

    return _("unknown");
}

static void context_subscribe_callback(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
    pa_assert(c);

    printf(_("Event '%s' on %s #%u\n"),
           subscription_event_type_to_string(t),
           subscription_event_facility_to_string(t),
           idx);
    fflush(stdout);
}

static void context_state_callback(pa_context *c, void *userdata) {
    pa_operation *o = NULL;

    pa_assert(c);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
            switch (action) {
                case STAT:
                    o = pa_context_stat(c, stat_callback, NULL);
                    break;

                case INFO:
                    o = pa_context_get_server_info(c, get_server_info_callback, NULL);
                    break;

                case PLAY_SAMPLE:
                    o = pa_context_play_sample(c, sample_name, sink_name, PA_VOLUME_NORM, simple_callback, NULL);
                    break;

                case REMOVE_SAMPLE:
                    o = pa_context_remove_sample(c, sample_name, simple_callback, NULL);
                    break;

                case UPLOAD_SAMPLE:
                    sample_stream = pa_stream_new(c, sample_name, &sample_spec, NULL);
                    pa_assert(sample_stream);

                    pa_stream_set_state_callback(sample_stream, stream_state_callback, NULL);
                    pa_stream_set_write_callback(sample_stream, stream_write_callback, NULL);
                    pa_stream_connect_upload(sample_stream, sample_length);
                    actions++;
                    break;

                case EXIT:
                    o = pa_context_exit_daemon(c, simple_callback, NULL);
                    break;

                case LIST:
                    if (list_type) {
                        if (pa_streq(list_type, "modules"))
                            o = pa_context_get_module_info_list(c, get_module_info_callback, NULL);
                        else if (pa_streq(list_type, "sinks"))
                            o = pa_context_get_sink_info_list(c, get_sink_info_callback, NULL);
                        else if (pa_streq(list_type, "sources"))
                            o = pa_context_get_source_info_list(c, get_source_info_callback, NULL);
                        else if (pa_streq(list_type, "sink-inputs"))
                            o = pa_context_get_sink_input_info_list(c, get_sink_input_info_callback, NULL);
                        else if (pa_streq(list_type, "source-outputs"))
                            o = pa_context_get_source_output_info_list(c, get_source_output_info_callback, NULL);
                        else if (pa_streq(list_type, "clients"))
                            o = pa_context_get_client_info_list(c, get_client_info_callback, NULL);
                        else if (pa_streq(list_type, "samples"))
                            o = pa_context_get_sample_info_list(c, get_sample_info_callback, NULL);
                        else if (pa_streq(list_type, "cards"))
                            o = pa_context_get_card_info_list(c, get_card_info_callback, NULL);
                        else
                            pa_assert_not_reached();
                    } else {
                        o = pa_context_get_module_info_list(c, get_module_info_callback, NULL);
                        if (o) {
                            pa_operation_unref(o);
                            actions++;
                        }

                        o = pa_context_get_sink_info_list(c, get_sink_info_callback, NULL);
                        if (o) {
                            pa_operation_unref(o);
                            actions++;
                        }

                        o = pa_context_get_source_info_list(c, get_source_info_callback, NULL);
                        if (o) {
                            pa_operation_unref(o);
                            actions++;
                        }
                        o = pa_context_get_sink_input_info_list(c, get_sink_input_info_callback, NULL);
                        if (o) {
                            pa_operation_unref(o);
                            actions++;
                        }

                        o = pa_context_get_source_output_info_list(c, get_source_output_info_callback, NULL);
                        if (o) {
                            pa_operation_unref(o);
                            actions++;
                        }

                        o = pa_context_get_client_info_list(c, get_client_info_callback, NULL);
                        if (o) {
                            pa_operation_unref(o);
                            actions++;
                        }

                        o = pa_context_get_sample_info_list(c, get_sample_info_callback, NULL);
                        if (o) {
                            pa_operation_unref(o);
                            actions++;
                        }

                        o = pa_context_get_card_info_list(c, get_card_info_callback, NULL);
                        if (o) {
                            pa_operation_unref(o);
                            actions++;
                        }

                        o = NULL;
                    }
                    break;

                case MOVE_SINK_INPUT:
                    o = pa_context_move_sink_input_by_name(c, sink_input_idx, sink_name, simple_callback, NULL);
                    break;

                case MOVE_SOURCE_OUTPUT:
                    o = pa_context_move_source_output_by_name(c, source_output_idx, source_name, simple_callback, NULL);
                    break;

                case LOAD_MODULE:
                    o = pa_context_load_module(c, module_name, module_args, index_callback, NULL);
                    break;

                case UNLOAD_MODULE:
                    if (module_name)
                        o = pa_context_get_module_info_list(c, unload_module_by_name_callback, NULL);
                    else
                        o = pa_context_unload_module(c, module_index, simple_callback, NULL);
                    break;

                case SUSPEND_SINK:
                    if (sink_name)
                        o = pa_context_suspend_sink_by_name(c, sink_name, suspend, simple_callback, NULL);
                    else
                        o = pa_context_suspend_sink_by_index(c, PA_INVALID_INDEX, suspend, simple_callback, NULL);
                    break;

                case SUSPEND_SOURCE:
                    if (source_name)
                        o = pa_context_suspend_source_by_name(c, source_name, suspend, simple_callback, NULL);
                    else
                        o = pa_context_suspend_source_by_index(c, PA_INVALID_INDEX, suspend, simple_callback, NULL);
                    break;

                case SET_CARD_PROFILE:
                    o = pa_context_set_card_profile_by_name(c, card_name, profile_name, simple_callback, NULL);
                    break;

                case SET_SINK_PORT:
                    o = pa_context_set_sink_port_by_name(c, sink_name, port_name, simple_callback, NULL);
                    break;

                case SET_DEFAULT_SINK:
                    o = pa_context_set_default_sink(c, sink_name, simple_callback, NULL);
                    break;

                case SET_SOURCE_PORT:
                    o = pa_context_set_source_port_by_name(c, source_name, port_name, simple_callback, NULL);
                    break;

                case SET_DEFAULT_SOURCE:
                    o = pa_context_set_default_source(c, source_name, simple_callback, NULL);
                    break;

                case SET_SINK_MUTE:
                    if (mute == TOGGLE_MUTE)
                        o = pa_context_get_sink_info_by_name(c, sink_name, sink_toggle_mute_callback, NULL);
                    else
                        o = pa_context_set_sink_mute_by_name(c, sink_name, mute, simple_callback, NULL);
                    break;

                case SET_SOURCE_MUTE:
                    if (mute == TOGGLE_MUTE)
                        o = pa_context_get_source_info_by_name(c, source_name, source_toggle_mute_callback, NULL);
                    else
                        o = pa_context_set_source_mute_by_name(c, source_name, mute, simple_callback, NULL);
                    break;

                case SET_SINK_INPUT_MUTE:
                    if (mute == TOGGLE_MUTE)
                        o = pa_context_get_sink_input_info(c, sink_input_idx, sink_input_toggle_mute_callback, NULL);
                    else
                        o = pa_context_set_sink_input_mute(c, sink_input_idx, mute, simple_callback, NULL);
                    break;

                case SET_SOURCE_OUTPUT_MUTE:
                    if (mute == TOGGLE_MUTE)
                        o = pa_context_get_source_output_info(c, source_output_idx, source_output_toggle_mute_callback, NULL);
                    else
                        o = pa_context_set_source_output_mute(c, source_output_idx, mute, simple_callback, NULL);
                    break;

                case SET_SINK_VOLUME:
                    o = pa_context_get_sink_info_by_name(c, sink_name, get_sink_volume_callback, NULL);
                    break;

                case SET_SOURCE_VOLUME:
                    o = pa_context_get_source_info_by_name(c, source_name, get_source_volume_callback, NULL);
                    break;

                case SET_SINK_INPUT_VOLUME:
                    o = pa_context_get_sink_input_info(c, sink_input_idx, get_sink_input_volume_callback, NULL);
                    break;

                case SET_SOURCE_OUTPUT_VOLUME:
                    o = pa_context_get_source_output_info(c, source_output_idx, get_source_output_volume_callback, NULL);
                    break;

                case SET_SINK_FORMATS:
                    set_sink_formats(c, sink_idx, formats);
                    break;

                case SET_PORT_LATENCY_OFFSET:
                    o = pa_context_set_port_latency_offset(c, card_name, port_name, latency_offset, simple_callback, NULL);
                    break;

                case SUBSCRIBE:
                    pa_context_set_subscribe_callback(c, context_subscribe_callback, NULL);

                    o = pa_context_subscribe(c,
                                             PA_SUBSCRIPTION_MASK_SINK|
                                             PA_SUBSCRIPTION_MASK_SOURCE|
                                             PA_SUBSCRIPTION_MASK_SINK_INPUT|
                                             PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT|
                                             PA_SUBSCRIPTION_MASK_MODULE|
                                             PA_SUBSCRIPTION_MASK_CLIENT|
                                             PA_SUBSCRIPTION_MASK_SAMPLE_CACHE|
                                             PA_SUBSCRIPTION_MASK_SERVER|
                                             PA_SUBSCRIPTION_MASK_CARD,
                                             NULL,
                                             NULL);
                    break;

                default:
                    pa_assert_not_reached();
            }

            if (o) {
                pa_operation_unref(o);
                actions++;
            }

            if (actions == 0) {
                pa_log("Operation failed: %s", pa_strerror(pa_context_errno(c)));
                quit(1);
            }

            break;

        case PA_CONTEXT_TERMINATED:
            quit(0);
            break;

        case PA_CONTEXT_FAILED:
        default:
            pa_log(_("Connection failure: %s"), pa_strerror(pa_context_errno(c)));
            quit(1);
    }
}

static void exit_signal_callback(pa_mainloop_api *m, pa_signal_event *e, int sig, void *userdata) {
    pa_log(_("Got SIGINT, exiting."));
    quit(0);
}

static int parse_volume(const char *vol_spec, pa_volume_t *vol, enum volume_flags *vol_flags) {
    double v;
    char *vs;
    const char *atod_input;

    pa_assert(vol_spec);
    pa_assert(vol);
    pa_assert(vol_flags);

    vs = pa_xstrdup(vol_spec);

    *vol_flags = (pa_startswith(vs, "+") || pa_startswith(vs, "-")) ? VOL_RELATIVE : VOL_ABSOLUTE;
    if (strchr(vs, '.'))
        *vol_flags |= VOL_LINEAR;
    if (pa_endswith(vs, "%")) {
        *vol_flags |= VOL_PERCENT;
        vs[strlen(vs)-1] = 0;
    }
    if (pa_endswith(vs, "db") || pa_endswith(vs, "dB")) {
        *vol_flags |= VOL_DECIBEL;
        vs[strlen(vs)-2] = 0;
    }

    atod_input = vs;

    if (atod_input[0] == '+')
        atod_input++; /* pa_atod() doesn't accept leading '+', so skip it. */

    if (pa_atod(atod_input, &v) < 0) {
        pa_log(_("Invalid volume specification"));
        pa_xfree(vs);
        return -1;
    }

    pa_xfree(vs);

    if (*vol_flags & VOL_RELATIVE) {
        if ((*vol_flags & 0x0F) == VOL_UINT)
            v += (double) PA_VOLUME_NORM;
        if ((*vol_flags & 0x0F) == VOL_PERCENT)
            v += 100.0;
        if ((*vol_flags & 0x0F) == VOL_LINEAR)
            v += 1.0;
    }
    if ((*vol_flags & 0x0F) == VOL_PERCENT)
        v = v * (double) PA_VOLUME_NORM / 100;
    if ((*vol_flags & 0x0F) == VOL_LINEAR)
        v = pa_sw_volume_from_linear(v);
    if ((*vol_flags & 0x0F) == VOL_DECIBEL)
        v = pa_sw_volume_from_dB(v);

    if (!PA_VOLUME_IS_VALID((pa_volume_t) v)) {
        pa_log(_("Volume outside permissible range.\n"));
        return -1;
    }

    *vol = (pa_volume_t) v;

    return 0;
}

static int parse_volumes(char *args[], unsigned n) {
    unsigned i;

    if (n >= PA_CHANNELS_MAX) {
        pa_log(_("Invalid number of volume specifications.\n"));
        return -1;
    }

    volume.channels = n;
    for (i = 0; i < volume.channels; i++) {
        enum volume_flags flags;

        if (parse_volume(args[i], &volume.values[i], &flags) < 0)
            return -1;

        if (i > 0 && flags != volume_flags) {
            pa_log(_("Inconsistent volume specification.\n"));
            return -1;
        } else
            volume_flags = flags;
    }

    return 0;
}

static enum mute_flags parse_mute(const char *mute_text) {
    int b;

    pa_assert(mute_text);

    if (pa_streq("toggle", mute_text))
        return TOGGLE_MUTE;

    b = pa_parse_boolean(mute_text);
    switch (b) {
        case 0:
            return UNMUTE;
        case 1:
            return MUTE;
        default:
            return INVALID_MUTE;
    }
}

static void help(const char *argv0) {

    printf("%s %s %s\n",    argv0, _("[options]"), "stat");
    printf("%s %s %s\n",    argv0, _("[options]"), "info");
    printf("%s %s %s %s\n", argv0, _("[options]"), "list [short]", _("[TYPE]"));
    printf("%s %s %s\n",    argv0, _("[options]"), "exit");
    printf("%s %s %s %s\n", argv0, _("[options]"), "upload-sample", _("FILENAME [NAME]"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "play-sample ", _("NAME [SINK]"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "remove-sample ", _("NAME"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "load-module ", _("NAME [ARGS ...]"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "unload-module ", _("NAME|#N"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "move-(sink-input|source-output)", _("#N SINK|SOURCE"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "suspend-(sink|source)", _("NAME|#N 1|0"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "set-card-profile ", _("CARD PROFILE"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "set-default-(sink|source)", _("NAME"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "set-(sink|source)-port", _("NAME|#N PORT"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "set-(sink|source)-volume", _("NAME|#N VOLUME [VOLUME ...]"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "set-(sink-input|source-output)-volume", _("#N VOLUME [VOLUME ...]"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "set-(sink|source)-mute", _("NAME|#N 1|0|toggle"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "set-(sink-input|source-output)-mute", _("#N 1|0|toggle"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "set-sink-formats", _("#N FORMATS"));
    printf("%s %s %s %s\n", argv0, _("[options]"), "set-port-latency-offset", _("CARD-NAME|CARD-#N PORT OFFSET"));
    printf("%s %s %s\n",    argv0, _("[options]"), "subscribe");
    printf(_("\nThe special names @DEFAULT_SINK@, @DEFAULT_SOURCE@ and @DEFAULT_MONITOR@\n"
             "can be used to specify the default sink, source and monitor.\n"));

    printf(_("\n"
             "  -h, --help                            Show this help\n"
             "      --version                         Show version\n\n"
             "  -s, --server=SERVER                   The name of the server to connect to\n"
             "  -n, --client-name=NAME                How to call this client on the server\n"));
}

enum {
    ARG_VERSION = 256
};

int main(int argc, char *argv[]) {
    pa_mainloop *m = NULL;
    int ret = 1, c;
    char *server = NULL, *bn;

    static const struct option long_options[] = {
        {"server",      1, NULL, 's'},
        {"client-name", 1, NULL, 'n'},
        {"version",     0, NULL, ARG_VERSION},
        {"help",        0, NULL, 'h'},
        {NULL,          0, NULL, 0}
    };

    setlocale(LC_ALL, "");
#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, PULSE_LOCALEDIR);
#endif

    bn = pa_path_get_filename(argv[0]);

    proplist = pa_proplist_new();

    while ((c = getopt_long(argc, argv, "+s:n:h", long_options, NULL)) != -1) {
        switch (c) {
            case 'h' :
                help(bn);
                ret = 0;
                goto quit;

            case ARG_VERSION:
                printf(_("pactl %s\n"
                         "Compiled with libpulse %s\n"
                         "Linked with libpulse %s\n"),
                       PACKAGE_VERSION,
                       pa_get_headers_version(),
                       pa_get_library_version());
                ret = 0;
                goto quit;

            case 's':
                pa_xfree(server);
                server = pa_xstrdup(optarg);
                break;

            case 'n': {
                char *t;

                if (!(t = pa_locale_to_utf8(optarg)) ||
                    pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME, t) < 0) {

                    pa_log(_("Invalid client name '%s'"), t ? t : optarg);
                    pa_xfree(t);
                    goto quit;
                }

                pa_xfree(t);
                break;
            }

            default:
                goto quit;
        }
    }

    if (optind < argc) {
        if (pa_streq(argv[optind], "stat")) {
            action = STAT;

        } else if (pa_streq(argv[optind], "info"))
            action = INFO;

        else if (pa_streq(argv[optind], "exit"))
            action = EXIT;

        else if (pa_streq(argv[optind], "list")) {
            action = LIST;

            for (int i = optind+1; i < argc; i++) {
                if (pa_streq(argv[i], "modules") || pa_streq(argv[i], "clients") ||
                    pa_streq(argv[i], "sinks")   || pa_streq(argv[i], "sink-inputs") ||
                    pa_streq(argv[i], "sources") || pa_streq(argv[i], "source-outputs") ||
                    pa_streq(argv[i], "samples") || pa_streq(argv[i], "cards")) {
                    list_type = pa_xstrdup(argv[i]);
                } else if (pa_streq(argv[i], "short")) {
                    short_list_format = true;
                } else {
                    pa_log(_("Specify nothing, or one of: %s"), "modules, sinks, sources, sink-inputs, source-outputs, clients, samples, cards");
                    goto quit;
                }
            }

        } else if (pa_streq(argv[optind], "upload-sample")) {
            struct SF_INFO sfi;
            action = UPLOAD_SAMPLE;

            if (optind+1 >= argc) {
                pa_log(_("Please specify a sample file to load"));
                goto quit;
            }

            if (optind+2 < argc)
                sample_name = pa_xstrdup(argv[optind+2]);
            else {
                char *f = pa_path_get_filename(argv[optind+1]);
                sample_name = pa_xstrndup(f, strcspn(f, "."));
            }

            pa_zero(sfi);
            if (!(sndfile = sf_open(argv[optind+1], SFM_READ, &sfi))) {
                pa_log(_("Failed to open sound file."));
                goto quit;
            }

            if (pa_sndfile_read_sample_spec(sndfile, &sample_spec) < 0) {
                pa_log(_("Failed to determine sample specification from file."));
                goto quit;
            }
            sample_spec.format = PA_SAMPLE_FLOAT32;

            if (pa_sndfile_read_channel_map(sndfile, &channel_map) < 0) {
                if (sample_spec.channels > 2)
                    pa_log(_("Warning: Failed to determine sample specification from file."));
                pa_channel_map_init_extend(&channel_map, sample_spec.channels, PA_CHANNEL_MAP_DEFAULT);
            }

            pa_assert(pa_channel_map_compatible(&channel_map, &sample_spec));
            sample_length = (size_t) sfi.frames*pa_frame_size(&sample_spec);

        } else if (pa_streq(argv[optind], "play-sample")) {
            action = PLAY_SAMPLE;
            if (argc != optind+2 && argc != optind+3) {
                pa_log(_("You have to specify a sample name to play"));
                goto quit;
            }

            sample_name = pa_xstrdup(argv[optind+1]);

            if (optind+2 < argc)
                sink_name = pa_xstrdup(argv[optind+2]);

        } else if (pa_streq(argv[optind], "remove-sample")) {
            action = REMOVE_SAMPLE;
            if (argc != optind+2) {
                pa_log(_("You have to specify a sample name to remove"));
                goto quit;
            }

            sample_name = pa_xstrdup(argv[optind+1]);

        } else if (pa_streq(argv[optind], "move-sink-input")) {
            action = MOVE_SINK_INPUT;
            if (argc != optind+3) {
                pa_log(_("You have to specify a sink input index and a sink"));
                goto quit;
            }

            sink_input_idx = (uint32_t) atoi(argv[optind+1]);
            sink_name = pa_xstrdup(argv[optind+2]);

        } else if (pa_streq(argv[optind], "move-source-output")) {
            action = MOVE_SOURCE_OUTPUT;
            if (argc != optind+3) {
                pa_log(_("You have to specify a source output index and a source"));
                goto quit;
            }

            source_output_idx = (uint32_t) atoi(argv[optind+1]);
            source_name = pa_xstrdup(argv[optind+2]);

        } else if (pa_streq(argv[optind], "load-module")) {
            int i;
            size_t n = 0;
            char *p;

            action = LOAD_MODULE;

            if (argc <= optind+1) {
                pa_log(_("You have to specify a module name and arguments."));
                goto quit;
            }

            module_name = argv[optind+1];

            for (i = optind+2; i < argc; i++)
                n += strlen(argv[i])+1;

            if (n > 0) {
                p = module_args = pa_xmalloc(n);

                for (i = optind+2; i < argc; i++)
                    p += sprintf(p, "%s%s", p == module_args ? "" : " ", argv[i]);
            }

        } else if (pa_streq(argv[optind], "unload-module")) {
            action = UNLOAD_MODULE;

            if (argc != optind+2) {
                pa_log(_("You have to specify a module index or name"));
                goto quit;
            }

            if (pa_atou(argv[optind + 1], &module_index) < 0)
                module_name = argv[optind + 1];

        } else if (pa_streq(argv[optind], "suspend-sink")) {
            int b;

            action = SUSPEND_SINK;

            if (argc > optind+3 || optind+1 >= argc) {
                pa_log(_("You may not specify more than one sink. You have to specify a boolean value."));
                goto quit;
            }

            if ((b = pa_parse_boolean(argv[argc-1])) < 0) {
                pa_log(_("Invalid suspend specification."));
                goto quit;
            }

            suspend = !!b;

            if (argc > optind+2)
                sink_name = pa_xstrdup(argv[optind+1]);

        } else if (pa_streq(argv[optind], "suspend-source")) {
            int b;

            action = SUSPEND_SOURCE;

            if (argc > optind+3 || optind+1 >= argc) {
                pa_log(_("You may not specify more than one source. You have to specify a boolean value."));
                goto quit;
            }

            if ((b = pa_parse_boolean(argv[argc-1])) < 0) {
                pa_log(_("Invalid suspend specification."));
                goto quit;
            }

            suspend = !!b;

            if (argc > optind+2)
                source_name = pa_xstrdup(argv[optind+1]);
        } else if (pa_streq(argv[optind], "set-card-profile")) {
            action = SET_CARD_PROFILE;

            if (argc != optind+3) {
                pa_log(_("You have to specify a card name/index and a profile name"));
                goto quit;
            }

            card_name = pa_xstrdup(argv[optind+1]);
            profile_name = pa_xstrdup(argv[optind+2]);

        } else if (pa_streq(argv[optind], "set-sink-port")) {
            action = SET_SINK_PORT;

            if (argc != optind+3) {
                pa_log(_("You have to specify a sink name/index and a port name"));
                goto quit;
            }

            sink_name = pa_xstrdup(argv[optind+1]);
            port_name = pa_xstrdup(argv[optind+2]);

        } else if (pa_streq(argv[optind], "set-default-sink")) {
            action = SET_DEFAULT_SINK;

            if (argc != optind+2) {
                pa_log(_("You have to specify a sink name"));
                goto quit;
            }

            sink_name = pa_xstrdup(argv[optind+1]);

        } else if (pa_streq(argv[optind], "set-source-port")) {
            action = SET_SOURCE_PORT;

            if (argc != optind+3) {
                pa_log(_("You have to specify a source name/index and a port name"));
                goto quit;
            }

            source_name = pa_xstrdup(argv[optind+1]);
            port_name = pa_xstrdup(argv[optind+2]);

        } else if (pa_streq(argv[optind], "set-default-source")) {
            action = SET_DEFAULT_SOURCE;

            if (argc != optind+2) {
                pa_log(_("You have to specify a source name"));
                goto quit;
            }

            source_name = pa_xstrdup(argv[optind+1]);

        } else if (pa_streq(argv[optind], "set-sink-volume")) {
            action = SET_SINK_VOLUME;

            if (argc < optind+3) {
                pa_log(_("You have to specify a sink name/index and a volume"));
                goto quit;
            }

            sink_name = pa_xstrdup(argv[optind+1]);

            if (parse_volumes(argv+optind+2, argc-(optind+2)) < 0)
                goto quit;

        } else if (pa_streq(argv[optind], "set-source-volume")) {
            action = SET_SOURCE_VOLUME;

            if (argc < optind+3) {
                pa_log(_("You have to specify a source name/index and a volume"));
                goto quit;
            }

            source_name = pa_xstrdup(argv[optind+1]);

            if (parse_volumes(argv+optind+2, argc-(optind+2)) < 0)
                goto quit;

        } else if (pa_streq(argv[optind], "set-sink-input-volume")) {
            action = SET_SINK_INPUT_VOLUME;

            if (argc < optind+3) {
                pa_log(_("You have to specify a sink input index and a volume"));
                goto quit;
            }

            if (pa_atou(argv[optind+1], &sink_input_idx) < 0) {
                pa_log(_("Invalid sink input index"));
                goto quit;
            }

            if (parse_volumes(argv+optind+2, argc-(optind+2)) < 0)
                goto quit;

        } else if (pa_streq(argv[optind], "set-source-output-volume")) {
            action = SET_SOURCE_OUTPUT_VOLUME;

            if (argc < optind+3) {
                pa_log(_("You have to specify a source output index and a volume"));
                goto quit;
            }

            if (pa_atou(argv[optind+1], &source_output_idx) < 0) {
                pa_log(_("Invalid source output index"));
                goto quit;
            }

            if (parse_volumes(argv+optind+2, argc-(optind+2)) < 0)
                goto quit;

        } else if (pa_streq(argv[optind], "set-sink-mute")) {
            action = SET_SINK_MUTE;

            if (argc != optind+3) {
                pa_log(_("You have to specify a sink name/index and a mute action (0, 1, or 'toggle')"));
                goto quit;
            }

            if ((mute = parse_mute(argv[optind+2])) == INVALID_MUTE) {
                pa_log(_("Invalid mute specification"));
                goto quit;
            }

            sink_name = pa_xstrdup(argv[optind+1]);

        } else if (pa_streq(argv[optind], "set-source-mute")) {
            action = SET_SOURCE_MUTE;

            if (argc != optind+3) {
                pa_log(_("You have to specify a source name/index and a mute action (0, 1, or 'toggle')"));
                goto quit;
            }

            if ((mute = parse_mute(argv[optind+2])) == INVALID_MUTE) {
                pa_log(_("Invalid mute specification"));
                goto quit;
            }

            source_name = pa_xstrdup(argv[optind+1]);

        } else if (pa_streq(argv[optind], "set-sink-input-mute")) {
            action = SET_SINK_INPUT_MUTE;

            if (argc != optind+3) {
                pa_log(_("You have to specify a sink input index and a mute action (0, 1, or 'toggle')"));
                goto quit;
            }

            if (pa_atou(argv[optind+1], &sink_input_idx) < 0) {
                pa_log(_("Invalid sink input index specification"));
                goto quit;
            }

            if ((mute = parse_mute(argv[optind+2])) == INVALID_MUTE) {
                pa_log(_("Invalid mute specification"));
                goto quit;
            }

        } else if (pa_streq(argv[optind], "set-source-output-mute")) {
            action = SET_SOURCE_OUTPUT_MUTE;

            if (argc != optind+3) {
                pa_log(_("You have to specify a source output index and a mute action (0, 1, or 'toggle')"));
                goto quit;
            }

            if (pa_atou(argv[optind+1], &source_output_idx) < 0) {
                pa_log(_("Invalid source output index specification"));
                goto quit;
            }

            if ((mute = parse_mute(argv[optind+2])) == INVALID_MUTE) {
                pa_log(_("Invalid mute specification"));
                goto quit;
            }

        } else if (pa_streq(argv[optind], "subscribe"))

            action = SUBSCRIBE;

        else if (pa_streq(argv[optind], "set-sink-formats")) {
            int32_t tmp;

            if (argc != optind+3 || pa_atoi(argv[optind+1], &tmp) < 0) {
                pa_log(_("You have to specify a sink index and a semicolon-separated list of supported formats"));
                goto quit;
            }

            sink_idx = tmp;
            action = SET_SINK_FORMATS;
            formats = pa_xstrdup(argv[optind+2]);

        } else if (pa_streq(argv[optind], "set-port-latency-offset")) {
            action = SET_PORT_LATENCY_OFFSET;

            if (argc != optind+4) {
                pa_log(_("You have to specify a card name/index, a port name and a latency offset"));
                goto quit;
            }

            card_name = pa_xstrdup(argv[optind+1]);
            port_name = pa_xstrdup(argv[optind+2]);
            if (pa_atoi(argv[optind + 3], &latency_offset) < 0) {
                pa_log(_("Could not parse latency offset"));
                goto quit;
            }

        } else if (pa_streq(argv[optind], "help")) {
            help(bn);
            ret = 0;
            goto quit;
        }
    }

    if (action == NONE) {
        pa_log(_("No valid command specified."));
        goto quit;
    }

    if (!(m = pa_mainloop_new())) {
        pa_log(_("pa_mainloop_new() failed."));
        goto quit;
    }

    mainloop_api = pa_mainloop_get_api(m);

    pa_assert_se(pa_signal_init(mainloop_api) == 0);
    pa_signal_new(SIGINT, exit_signal_callback, NULL);
    pa_signal_new(SIGTERM, exit_signal_callback, NULL);
    pa_disable_sigpipe();

    if (!(context = pa_context_new_with_proplist(mainloop_api, NULL, proplist))) {
        pa_log(_("pa_context_new() failed."));
        goto quit;
    }

    pa_context_set_state_callback(context, context_state_callback, NULL);
    if (pa_context_connect(context, server, 0, NULL) < 0) {
        pa_log(_("pa_context_connect() failed: %s"), pa_strerror(pa_context_errno(context)));
        goto quit;
    }

    if (pa_mainloop_run(m, &ret) < 0) {
        pa_log(_("pa_mainloop_run() failed."));
        goto quit;
    }

quit:
    if (sample_stream)
        pa_stream_unref(sample_stream);

    if (context)
        pa_context_unref(context);

    if (m) {
        pa_signal_done();
        pa_mainloop_free(m);
    }

    pa_xfree(server);
    pa_xfree(list_type);
    pa_xfree(sample_name);
    pa_xfree(sink_name);
    pa_xfree(source_name);
    pa_xfree(module_args);
    pa_xfree(card_name);
    pa_xfree(profile_name);
    pa_xfree(port_name);
    pa_xfree(formats);

    if (sndfile)
        sf_close(sndfile);

    if (proplist)
        pa_proplist_free(proplist);

    return ret;
}
