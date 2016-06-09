#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <check.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>

#include <pulse/pulseaudio.h>
#include <pulsecore/log.h>
#include <pulsecore/core-util.h>
#include <pulsecore/strlist.h>
#include <modules/alsa/alsa-mixer.h>

/* This function was copied from alsa-mixer.c */
static const char *get_default_paths_dir(void) {
    if (pa_run_from_build_tree())
        return PA_SRCDIR "/modules/alsa/mixer/paths/";
    else
        return PA_ALSA_PATHS_DIR;
}

static pa_strlist *load_makefile() {
    FILE *f;
    bool lookforfiles = false;
    char buf[2048];
    pa_strlist *result = NULL;
    const char *Makefile = PA_BUILDDIR "/Makefile";

    f = pa_fopen_cloexec(Makefile, "r");
    fail_unless(f != NULL); /* Consider skipping this test instead of failing if Makefile not found? */
    while (!feof(f)) {
        if (!fgets(buf, sizeof(buf), f)) {
            fail_unless(feof(f));
            break;
        }
        if (strstr(buf, "dist_alsapaths_DATA = \\") != NULL) {
           lookforfiles = true;
           continue;
        }
        if (!lookforfiles)
           continue;
        if (!strstr(buf, "\\"))
           lookforfiles = false;
        else
           strstr(buf, "\\")[0] = '\0';
        pa_strip(buf);
        pa_log_debug("Shipping file '%s'", pa_path_get_filename(buf));
        result = pa_strlist_prepend(result, pa_path_get_filename(buf));
    }
    fclose(f);
    return result;
}

START_TEST (mixer_path_test) {
    DIR *dir;
    struct dirent *ent;
    pa_strlist *ship = load_makefile();
    const char *pathsdir = get_default_paths_dir();
    pa_log_debug("Analyzing directory: '%s'", pathsdir);

    dir = opendir(pathsdir);
    fail_unless(dir != NULL);
    while ((ent = readdir(dir)) != NULL) {
        pa_alsa_path *path;
        if (pa_streq(ent->d_name, ".") || pa_streq(ent->d_name, ".."))
            continue;
        pa_log_debug("Analyzing file: '%s'", ent->d_name);

        /* Can the file be parsed? */
        path = pa_alsa_path_new(pathsdir, ent->d_name, PA_ALSA_DIRECTION_ANY);
        fail_unless(path != NULL);

        /* Is the file shipped? */
        if (ship) {
            pa_strlist *n;
            bool found = false;
            for (n = ship; n; n = pa_strlist_next(n))
                found |= pa_streq(ent->d_name, pa_strlist_data(n));
            fail_unless(found);
        }
    }
    closedir(dir);
    pa_strlist_free(ship);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    s = suite_create("Alsa-mixer-path");
    tc = tcase_create("alsa-mixer-path");
    tcase_add_test(tc, mixer_path_test);
    tcase_set_timeout(tc, 30);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
