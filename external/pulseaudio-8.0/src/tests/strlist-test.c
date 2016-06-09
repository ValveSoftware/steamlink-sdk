#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <check.h>

#include <pulse/xmalloc.h>

#include <pulsecore/strlist.h>
#include <pulsecore/core-util.h>

START_TEST (strlist_test) {
    char *t, *u;
    pa_strlist *l = NULL;

    l = pa_strlist_prepend(l, "e");
    l = pa_strlist_prepend(l, "d");
    l = pa_strlist_prepend(l, "c");
    l = pa_strlist_prepend(l, "b");
    l = pa_strlist_prepend(l, "a");

    t = pa_strlist_to_string(l);
    pa_strlist_free(l);

    fprintf(stderr, "1: %s\n", t);
    fail_unless(pa_streq(t, "a b c d e"));

    l = pa_strlist_parse(t);
    pa_xfree(t);

    t = pa_strlist_to_string(l);
    fprintf(stderr, "2: %s\n", t);
    fail_unless(pa_streq(t, "a b c d e"));
    pa_xfree(t);

    l = pa_strlist_pop(l, &u);
    fprintf(stderr, "3: %s\n", u);
    fail_unless(pa_streq(u, "a"));
    pa_xfree(u);

    l = pa_strlist_remove(l, "c");

    t = pa_strlist_to_string(l);
    fprintf(stderr, "4: %s\n", t);
    fail_unless(pa_streq(t, "b d e"));
    pa_xfree(t);

    pa_strlist_free(l);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("StrList");
    tc = tcase_create("strlist");
    tcase_add_test(tc, strlist_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
