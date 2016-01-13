#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <check.h>

#include <pulsecore/hook-list.h>
#include <pulsecore/log.h>

static pa_hook_result_t func1(const char *hook_data, const char *call_data, const char *slot_data) {
    pa_log("(func1) hook=%s call=%s slot=%s", hook_data, call_data, slot_data);
    /* succeed when it runs to here */
    fail_unless(1);
    return PA_HOOK_OK;
}

static pa_hook_result_t func2(const char *hook_data, const char *call_data, const char *slot_data) {
    pa_log("(func2) hook=%s call=%s slot=%s", hook_data, call_data, slot_data);
    /* succeed when it runs to here */
    fail_unless(1);
    return PA_HOOK_OK;
}

START_TEST (hooklist_test) {
    pa_hook hook;
    pa_hook_slot *slot;

    pa_hook_init(&hook, (void*) "hook");

    pa_hook_connect(&hook, PA_HOOK_LATE, (pa_hook_cb_t) func1, (void*) "slot1");
    slot = pa_hook_connect(&hook, PA_HOOK_NORMAL, (pa_hook_cb_t) func2, (void*) "slot2");
    pa_hook_connect(&hook, PA_HOOK_NORMAL, (pa_hook_cb_t) func1, (void*) "slot3");

    pa_hook_fire(&hook, (void*) "call1");

    pa_hook_slot_free(slot);

    pa_hook_fire(&hook, (void*) "call2");

    pa_hook_done(&hook);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Hook List");
    tc = tcase_create("hooklist");
    tcase_add_test(tc, hooklist_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
