#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <check.h>

#include <pulse/channelmap.h>

START_TEST (channelmap_test) {
    char cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    pa_channel_map map, map2;

    pa_channel_map_init_auto(&map, 6, PA_CHANNEL_MAP_AIFF);

    fprintf(stderr, "map: <%s>\n", pa_channel_map_snprint(cm, sizeof(cm), &map));

    pa_channel_map_init_auto(&map, 6, PA_CHANNEL_MAP_AUX);

    fprintf(stderr, "map: <%s>\n", pa_channel_map_snprint(cm, sizeof(cm), &map));

    pa_channel_map_init_auto(&map, 6, PA_CHANNEL_MAP_ALSA);

    fprintf(stderr, "map: <%s>\n", pa_channel_map_snprint(cm, sizeof(cm), &map));

    pa_channel_map_init_extend(&map, 14, PA_CHANNEL_MAP_ALSA);

    fprintf(stderr, "map: <%s>\n", pa_channel_map_snprint(cm, sizeof(cm), &map));

    pa_channel_map_parse(&map2, cm);

    fail_unless(pa_channel_map_equal(&map, &map2));

    pa_channel_map_parse(&map2, "left,test");
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Channel Map");
    tc = tcase_create("channelmap");
    tcase_add_test(tc, channelmap_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
