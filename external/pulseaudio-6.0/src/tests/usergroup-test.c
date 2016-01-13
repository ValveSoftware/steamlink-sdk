/***
  This file is part of PulseAudio.

  Copyright 2009 Ted Percival

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

#include <check.h>

#include <pulsecore/usergroup.h>
#include <pulsecore/core-util.h>

static int load_reference_structs(struct group **gr, struct passwd **pw) {
    setpwent();
    *pw = getpwent();
    endpwent();

    setgrent();
    *gr = getgrent();
    endgrent();

    return (*gr && *pw) ? 0 : 1;
}

static int compare_group(const struct group *a, const struct group *b) {
    char **amem, **bmem;

    if (!pa_streq(a->gr_name, b->gr_name)) {
        fprintf(stderr, "Group name mismatch: [%s] [%s]\n", a->gr_name, b->gr_name);
        return 1;
    }

    if (!pa_streq(a->gr_passwd, b->gr_passwd)) {
        fprintf(stderr, "Group password mismatch: [%s] [%s]\n", a->gr_passwd, b->gr_passwd);
        return 1;
    }

    if (a->gr_gid != b->gr_gid) {
        fprintf(stderr, "Gid mismatch: [%lu] [%lu]\n", (unsigned long) a->gr_gid, (unsigned long) b->gr_gid);
        return 1;
    }

    /* XXX: Assuming the group ordering is identical. */
    for (amem = a->gr_mem, bmem = b->gr_mem; *amem && *bmem; ++amem, ++bmem) {
        if (!pa_streq(*amem, *bmem)) {
            fprintf(stderr, "Group member mismatch: [%s] [%s]\n", *amem, *bmem);
            return 1;
        }
    }

    if (*amem || *bmem) {
        fprintf(stderr, "Mismatched group count\n");
        return 1;
    }

    return 0;
}

static int compare_passwd(const struct passwd *a, const struct passwd *b) {
    if (!pa_streq(a->pw_name, b->pw_name)) {
        fprintf(stderr, "pw_name mismatch: [%s] [%s]\n", a->pw_name, b->pw_name);
        return 1;
    }

    if (!pa_streq(a->pw_passwd, b->pw_passwd)) {
        fprintf(stderr, "pw_passwd mismatch: [%s] [%s]\n", a->pw_passwd, b->pw_passwd);
        return 1;
    }

    if (a->pw_uid != b->pw_uid) {
        fprintf(stderr, "pw_uid mismatch: [%lu] [%lu]\n", (unsigned long) a->pw_uid, (unsigned long) b->pw_uid);
        return 1;
    }

    if (a->pw_gid != b->pw_gid) {
        fprintf(stderr, "pw_gid mismatch: [%lu] [%lu]\n", (unsigned long) a->pw_gid, (unsigned long) b->pw_gid);
        return 1;
    }

    if (!pa_streq(a->pw_gecos, b->pw_gecos)) {
        fprintf(stderr, "pw_gecos mismatch: [%s] [%s]\n", a->pw_gecos, b->pw_gecos);
        return 1;
    }

    if (!pa_streq(a->pw_dir, b->pw_dir)) {
        fprintf(stderr, "pw_dir mismatch: [%s] [%s]\n", a->pw_dir, b->pw_dir);
        return 1;
    }

    if (!pa_streq(a->pw_shell, b->pw_shell)) {
        fprintf(stderr, "pw_shell mismatch: [%s] [%s]\n", a->pw_shell, b->pw_shell);
        return 1;
    }

    return 0;
}

START_TEST (usergroup_test) {
    struct group *gr;
    struct passwd *pw;
    struct group *reference_group = NULL;
    struct passwd *reference_passwd = NULL;

    fail_if(load_reference_structs(&reference_group, &reference_passwd));

    errno = 0;
    gr = pa_getgrgid_malloc(reference_group->gr_gid);
    fail_if(compare_group(reference_group, gr));
    pa_getgrgid_free(gr);

    errno = 0;
    gr = pa_getgrnam_malloc(reference_group->gr_name);
    fail_if(compare_group(reference_group, gr));
    pa_getgrnam_free(gr);

    errno = 0;
    pw = pa_getpwuid_malloc(reference_passwd->pw_uid);
    fail_if(compare_passwd(reference_passwd, pw));
    pa_getpwuid_free(pw);

    errno = 0;
    pw = pa_getpwnam_malloc(reference_passwd->pw_name);
    fail_if(compare_passwd(reference_passwd, pw));
    pa_getpwnam_free(pw);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Usergroup");
    tc = tcase_create("usergroup");
    tcase_add_test(tc, usergroup_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
