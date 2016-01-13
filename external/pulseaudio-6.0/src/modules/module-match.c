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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(HAVE_REGEX_H)
#include <regex.h>
#elif defined(HAVE_PCREPOSIX_H)
#include <pcreposix.h>
#endif

#include <pulse/xmalloc.h>

#include <pulsecore/core-error.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/core-util.h>

#include "module-match-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Playback stream expression matching module");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE("table=<filename> "
                "key=<property_key>");

#define WHITESPACE "\n\r \t"

#define DEFAULT_MATCH_TABLE_FILE PA_DEFAULT_CONFIG_DIR"/match.table"
#define DEFAULT_MATCH_TABLE_FILE_USER "match.table"

#define UPDATE_REPLACE "replace"
#define UPDATE_MERGE "merge"

static const char* const valid_modargs[] = {
    "table",
    "key",
    NULL,
};

struct rule {
    regex_t regex;
    pa_volume_t volume;
    pa_proplist *proplist;
    pa_update_mode_t mode;
    struct rule *next;
};

struct userdata {
    struct rule *rules;
    char *property_key;
    pa_hook_slot *sink_input_fixate_hook_slot;
};

static int load_rules(struct userdata *u, const char *filename) {
    FILE *f;
    int n = 0;
    int ret = -1;
    struct rule *end = NULL;
    char *fn = NULL;

    pa_assert(u);

    if (filename)
        f = pa_fopen_cloexec(fn = pa_xstrdup(filename), "r");
    else
        f = pa_open_config_file(DEFAULT_MATCH_TABLE_FILE, DEFAULT_MATCH_TABLE_FILE_USER, NULL, &fn);

    if (!f) {
        pa_log("Failed to open file config file: %s", pa_cstrerror(errno));
        goto finish;
    }

    pa_lock_fd(fileno(f), 1);

    while (!feof(f)) {
        char *token_end, *value_str;
        pa_volume_t volume = PA_VOLUME_NORM;
        uint32_t k;
        regex_t regex;
        char ln[256];
        struct rule *rule;
        pa_proplist *proplist = NULL;
        pa_update_mode_t mode = (pa_update_mode_t) -1;

        if (!fgets(ln, sizeof(ln), f))
            break;

        n++;

        pa_strip_nl(ln);

        if (ln[0] == '#' || !*ln )
            continue;

        token_end = ln + strcspn(ln, WHITESPACE);
        value_str = token_end + strspn(token_end, WHITESPACE);
        *token_end = 0;

        if (!*ln) {
            pa_log("[%s:%u] failed to parse line - missing regexp", fn, n);
            goto finish;
        }

        if (!*value_str) {
            pa_log("[%s:%u] failed to parse line - too few words", fn, n);
            goto finish;
        }

        if (pa_atou(value_str, &k) >= 0)
            volume = (pa_volume_t) PA_CLAMP_VOLUME(k);
        else {
            size_t len;

            token_end = value_str + strcspn(value_str, WHITESPACE);

            len = token_end - value_str;
            if (len == (sizeof(UPDATE_REPLACE) - 1) && !strncmp(value_str, UPDATE_REPLACE, len))
                mode = PA_UPDATE_REPLACE;
            else if (len == (sizeof(UPDATE_MERGE) - 1) && !strncmp(value_str, UPDATE_MERGE, len))
                mode = PA_UPDATE_MERGE;

            if (mode != (pa_update_mode_t) -1) {
                value_str = token_end + strspn(token_end, WHITESPACE);

                if (!*value_str) {
                    pa_log("[%s:%u] failed to parse line - too few words", fn, n);
                    goto finish;
                }
            } else
                mode = PA_UPDATE_MERGE;

            if (*value_str == '"') {
                value_str++;

                token_end = strchr(value_str, '"');
                if (!token_end) {
                    pa_log("[%s:%u] failed to parse line - missing role closing quote", fn, n);
                    goto finish;
                }
            } else
                token_end = value_str + strcspn(value_str, WHITESPACE);

            *token_end = 0;

            value_str = pa_sprintf_malloc("media.role=\"%s\"", value_str);
            proplist = pa_proplist_from_string(value_str);
            pa_xfree(value_str);
        }

        if (regcomp(&regex, ln, REG_EXTENDED|REG_NOSUB) != 0) {
            pa_log("[%s:%u] invalid regular expression", fn, n);
            if (proplist)
                pa_proplist_free(proplist);
            goto finish;
        }

        rule = pa_xnew(struct rule, 1);
        rule->regex = regex;
        rule->proplist = proplist;
        rule->mode = mode;
        rule->volume = volume;
        rule->next = NULL;

        if (end)
            end->next = rule;
        else
            u->rules = rule;
        end = rule;
    }

    ret = 0;

finish:
    if (f) {
        pa_lock_fd(fileno(f), 0);
        fclose(f);
    }

    pa_xfree(fn);

    return ret;
}

static pa_hook_result_t sink_input_fixate_hook_callback(pa_core *c, pa_sink_input_new_data *si, struct userdata *u) {
    struct rule *r;
    const char *n;

    pa_assert(c);
    pa_assert(u);

    if (!(n = pa_proplist_gets(si->proplist, u->property_key)))
        return PA_HOOK_OK;

    pa_log_debug("Matching with %s", n);

    for (r = u->rules; r; r = r->next) {
        if (!regexec(&r->regex, n, 0, NULL, 0)) {
            if (r->proplist) {
                pa_log_debug("updating proplist of sink input '%s'", n);
                pa_proplist_update(si->proplist, r->mode, r->proplist);
            } else if (si->volume_writable) {
                pa_cvolume cv;
                pa_log_debug("changing volume of sink input '%s' to 0x%03x", n, r->volume);
                pa_cvolume_set(&cv, si->sample_spec.channels, r->volume);
                pa_sink_input_new_data_set_volume(si, &cv);
            } else
                pa_log_debug("the volume of sink input '%s' is not writable, can't change it", n);
        }
    }

    return PA_HOOK_OK;
}

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    struct userdata *u;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    u = pa_xnew0(struct userdata, 1);
    u->rules = NULL;
    m->userdata = u;

    u->property_key = pa_xstrdup(pa_modargs_get_value(ma, "key", PA_PROP_MEDIA_NAME));

    if (load_rules(u, pa_modargs_get_value(ma, "table", NULL)) < 0)
        goto fail;

    /* hook EARLY - 1, to match before stream-restore */
    u->sink_input_fixate_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_FIXATE], PA_HOOK_EARLY - 1, (pa_hook_cb_t) sink_input_fixate_hook_callback, u);

    pa_modargs_free(ma);
    return 0;

fail:
    pa__done(m);

    if (ma)
        pa_modargs_free(ma);
    return -1;
}

void pa__done(pa_module*m) {
    struct userdata* u;
    struct rule *r, *n;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sink_input_fixate_hook_slot)
        pa_hook_slot_free(u->sink_input_fixate_hook_slot);

    if (u->property_key)
        pa_xfree(u->property_key);

    for (r = u->rules; r; r = n) {
        n = r->next;

        regfree(&r->regex);
        if (r->proplist)
            pa_proplist_free(r->proplist);
        pa_xfree(r);
    }

    pa_xfree(u);
}
