/***
  This file is part of PulseAudio.

  Copyright 2009 Lennart Poettering

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <sys/mman.h>

#include <check.h>

#include <pulsecore/memtrap.h>
#include <pulsecore/core-util.h>

START_TEST (sigbus_test) {
    void *p;
    int fd;
    pa_memtrap *m;

    pa_log_set_level(PA_LOG_DEBUG);
    pa_memtrap_install();

    /* Create the memory map */
    fail_unless((fd = open("sigbus-test-map", O_RDWR|O_TRUNC|O_CREAT, 0660)) >= 0);
    fail_unless(unlink("sigbus-test-map") == 0);
    fail_unless(ftruncate(fd, PA_PAGE_SIZE) >= 0);
    fail_unless((p = mmap(NULL, PA_PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)) != MAP_FAILED);

    /* Register memory map */
    m = pa_memtrap_add(p, PA_PAGE_SIZE);

    /* Use memory map */
    pa_snprintf(p, PA_PAGE_SIZE, "This is a test that should work fine.");

    /* Verify memory map */
    pa_log("Let's see if this worked: %s", (char*) p);
    pa_log("And memtrap says it is good: %s", pa_yes_no(pa_memtrap_is_good(m)));
    fail_unless(pa_memtrap_is_good(m) == true);

    /* Invalidate mapping */
    fail_unless(ftruncate(fd, 0) >= 0);

    /* Use memory map */
    pa_snprintf(p, PA_PAGE_SIZE, "This is a test that should fail but get caught.");

    /* Verify memory map */
    pa_log("Let's see if this worked: %s", (char*) p);
    pa_log("And memtrap says it is good: %s", pa_yes_no(pa_memtrap_is_good(m)));
    fail_unless(pa_memtrap_is_good(m) == false);

    pa_memtrap_remove(m);
    munmap(p, PA_PAGE_SIZE);
    close(fd);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Sig Bus");
    tc = tcase_create("sigbus");
    tcase_add_test(tc, sigbus_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
