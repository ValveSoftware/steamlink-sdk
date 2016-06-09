#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <assert.h>
#include <check.h>

#include <pulse/utf8.h>
#include <pulse/xmalloc.h>
#include <pulsecore/core-util.h>

START_TEST (utf8_valid) {
    fail_unless(pa_utf8_valid("hallo") != NULL);
    fail_unless(pa_utf8_valid("hallo\n") != NULL);
    fail_unless(pa_utf8_valid("hüpfburg\n") == NULL);
    fail_unless(pa_utf8_valid("hallo\n") != NULL);
    fail_unless(pa_utf8_valid("hÃ¼pfburg\n") != NULL);
}
END_TEST

START_TEST (utf8_filter) {
    char *c;

    {
        char res1[] = { 0x68, 0x5f, 0x70, 0x66, 0x62, 0x75, 0x72, 0x67, '\0' };
        c = pa_utf8_filter("hüpfburg");
        pa_log_debug("%s %s\n", res1, c);
        fail_unless(pa_streq(c, res1));
        pa_xfree(c);
    }

    {
        char res2[] = { 0x68, 0xc3, 0xbc, 0x70, 0x66, 0x62, 0x75, 0x72, 0x67, '\0' };
        c = pa_utf8_filter("hÃ¼pfburg");
        fail_unless(pa_streq(c, res2));
        pa_log_debug("%s %s\n", res2, c);
        pa_xfree(c);
    }

    {
        char res3[] = { 0x5f, 0x78, 0x6b, 0x6e, 0x5f, 0x72, 0x7a, 0x6d, 0x5f, 0x72, 0x7a, 0x65, 0x6c, 0x74, 0x5f, 0x72, 0x73, 0x7a, 0xdf, 0xb3, 0x5f, 0x64, 0x73, 0x6a, 0x6b, 0x66, 0x68, '\0' };
        c = pa_utf8_filter("üxknärzmörzeltörszß³§dsjkfh");
        pa_log_debug("%s %s\n", res3, c);
        fail_unless(pa_streq(c, res3));
        pa_xfree(c);
    }
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    s = suite_create("UTF8");
    tc = tcase_create("utf8");
    tcase_add_test(tc, utf8_valid);
    tcase_add_test(tc, utf8_filter);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
