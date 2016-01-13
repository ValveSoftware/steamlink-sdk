/***
  This file is part of PulseAudio.

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

#include <stdio.h>

#include <check.h>

#include <pulse/proplist.h>
#include <pulse/xmalloc.h>
#include <pulsecore/macro.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>

START_TEST (proplist_test) {
    pa_modargs *ma;
    pa_proplist *a, *b, *c, *d;
    char *s, *t, *u, *v;
    const char *text;
    const char *x[] = { "foo", NULL };

    a = pa_proplist_new();
    fail_unless(pa_proplist_sets(a, PA_PROP_MEDIA_TITLE, "Brandenburgische Konzerte") == 0);
    fail_unless(pa_proplist_sets(a, PA_PROP_MEDIA_ARTIST, "Johann Sebastian Bach") == 0);

    b = pa_proplist_new();
    fail_unless(pa_proplist_sets(b, PA_PROP_MEDIA_TITLE, "Goldbergvariationen") == 0);
    fail_unless(pa_proplist_set(b, PA_PROP_MEDIA_ICON, "\0\1\2\3\4\5\6\7", 8) == 0);

    pa_proplist_update(a, PA_UPDATE_MERGE, b);

    fail_unless(!pa_proplist_gets(a, PA_PROP_MEDIA_ICON));

    pa_log_debug("%s", pa_strnull(pa_proplist_gets(a, PA_PROP_MEDIA_TITLE)));
    fail_unless(pa_proplist_unset(b, PA_PROP_MEDIA_TITLE) == 0);

    s = pa_proplist_to_string(a);
    t = pa_proplist_to_string(b);
    pa_log_debug("---\n%s---\n%s", s, t);

    c = pa_proplist_from_string(s);
    u = pa_proplist_to_string(c);
    fail_unless(pa_streq(s, u));

    pa_xfree(s);
    pa_xfree(t);
    pa_xfree(u);

    pa_proplist_free(a);
    pa_proplist_free(b);
    pa_proplist_free(c);

    text = "  eins = zwei drei = \"\\\"vier\\\"\" fuenf=sechs sieben ='\\a\\c\\h\\t\\'\\\"' neun= hex:0123456789abCDef ";

    pa_log_debug("%s", text);
    d = pa_proplist_from_string(text);
    v = pa_proplist_to_string(d);
    pa_proplist_free(d);
    pa_log_debug("%s", v);
    d = pa_proplist_from_string(v);
    pa_xfree(v);
    v = pa_proplist_to_string(d);
    pa_proplist_free(d);
    pa_log_debug("%s", v);
    pa_xfree(v);

    ma = pa_modargs_new("foo='foobar=waldo foo2=\"lj\\\"dhflh\" foo3=\"kjlskj\\'\"'", x);
    fail_unless(ma != NULL);
    a = pa_proplist_new();
    fail_unless(a != NULL);

    fail_unless(pa_modargs_get_proplist(ma, "foo", a, PA_UPDATE_REPLACE) >= 0);

    pa_log_debug("%s", v = pa_proplist_to_string(a));
    pa_xfree(v);

    pa_proplist_free(a);
    pa_modargs_free(ma);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    s = suite_create("Property List");
    tc = tcase_create("propertylist");
    tcase_add_test(tc, proplist_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
