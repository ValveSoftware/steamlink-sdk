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
#include <unistd.h>

#include <check.h>

#include <pulse/xmalloc.h>

#include <pulsecore/log.h>
#include <pulsecore/memblock.h>
#include <pulsecore/macro.h>

static void release_cb(pa_memimport *i, uint32_t block_id, void *userdata) {
    pa_log("%s: Imported block %u is released.", (char*) userdata, block_id);
}

static void revoke_cb(pa_memexport *e, uint32_t block_id, void *userdata) {
    pa_log("%s: Exported block %u is revoked.", (char*) userdata, block_id);
}

static void print_stats(pa_mempool *p, const char *text) {
    const pa_mempool_stat*s = pa_mempool_get_stat(p);

    pa_log_debug("%s = {\n"
                 "\tn_allocated = %u\n"
                 "\tn_accumulated = %u\n"
                 "\tn_imported = %u\n"
                 "\tn_exported = %u\n"
                 "\tallocated_size = %u\n"
                 "\taccumulated_size = %u\n"
                 "\timported_size = %u\n"
                 "\texported_size = %u\n"
                 "\tn_too_large_for_pool = %u\n"
                 "\tn_pool_full = %u\n"
                 "}",
           text,
           (unsigned) pa_atomic_load(&s->n_allocated),
           (unsigned) pa_atomic_load(&s->n_accumulated),
           (unsigned) pa_atomic_load(&s->n_imported),
           (unsigned) pa_atomic_load(&s->n_exported),
           (unsigned) pa_atomic_load(&s->allocated_size),
           (unsigned) pa_atomic_load(&s->accumulated_size),
           (unsigned) pa_atomic_load(&s->imported_size),
           (unsigned) pa_atomic_load(&s->exported_size),
           (unsigned) pa_atomic_load(&s->n_too_large_for_pool),
           (unsigned) pa_atomic_load(&s->n_pool_full));
}

START_TEST (memblock_test) {
    pa_mempool *pool_a, *pool_b, *pool_c;
    unsigned id_a, id_b, id_c;
    pa_memexport *export_a, *export_b;
    pa_memimport *import_b, *import_c;
    pa_memblock *mb_a, *mb_b, *mb_c;
    int r, i;
    pa_memblock* blocks[5];
    uint32_t id, shm_id;
    size_t offset, size;
    char *x;

    const char txt[] = "This is a test!";

    pool_a = pa_mempool_new(true, 0);
    fail_unless(pool_a != NULL);
    pool_b = pa_mempool_new(true, 0);
    fail_unless(pool_b != NULL);
    pool_c = pa_mempool_new(true, 0);
    fail_unless(pool_c != NULL);

    pa_mempool_get_shm_id(pool_a, &id_a);
    pa_mempool_get_shm_id(pool_b, &id_b);
    pa_mempool_get_shm_id(pool_c, &id_c);

    blocks[0] = pa_memblock_new_fixed(pool_a, (void*) txt, sizeof(txt), 1);

    blocks[1] = pa_memblock_new(pool_a, sizeof(txt));
    x = pa_memblock_acquire(blocks[1]);
    snprintf(x, pa_memblock_get_length(blocks[1]), "%s", txt);
    pa_memblock_release(blocks[1]);

    blocks[2] = pa_memblock_new_pool(pool_a, sizeof(txt));
    x = pa_memblock_acquire(blocks[2]);
    snprintf(x, pa_memblock_get_length(blocks[2]), "%s", txt);
    pa_memblock_release(blocks[2]);

    blocks[3] = pa_memblock_new_malloced(pool_a, pa_xstrdup(txt), sizeof(txt));
    blocks[4] = NULL;

    for (i = 0; blocks[i]; i++) {
        pa_log("Memory block %u", i);

        mb_a = blocks[i];
        fail_unless(mb_a != NULL);

        export_a = pa_memexport_new(pool_a, revoke_cb, (void*) "A");
        fail_unless(export_a != NULL);
        export_b = pa_memexport_new(pool_b, revoke_cb, (void*) "B");
        fail_unless(export_b != NULL);

        import_b = pa_memimport_new(pool_b, release_cb, (void*) "B");
        fail_unless(import_b != NULL);
        import_c = pa_memimport_new(pool_c, release_cb, (void*) "C");
        fail_unless(import_b != NULL);

        r = pa_memexport_put(export_a, mb_a, &id, &shm_id, &offset, &size);
        fail_unless(r >= 0);
        fail_unless(shm_id == id_a);

        pa_log("A: Memory block exported as %u", id);

        mb_b = pa_memimport_get(import_b, id, shm_id, offset, size, false);
        fail_unless(mb_b != NULL);
        r = pa_memexport_put(export_b, mb_b, &id, &shm_id, &offset, &size);
        fail_unless(r >= 0);
        fail_unless(shm_id == id_a || shm_id == id_b);
        pa_memblock_unref(mb_b);

        pa_log("B: Memory block exported as %u", id);

        mb_c = pa_memimport_get(import_c, id, shm_id, offset, size, false);
        fail_unless(mb_c != NULL);
        x = pa_memblock_acquire(mb_c);
        pa_log_debug("1 data=%s", x);
        pa_memblock_release(mb_c);

        print_stats(pool_a, "A");
        print_stats(pool_b, "B");
        print_stats(pool_c, "C");

        pa_memexport_free(export_b);
        x = pa_memblock_acquire(mb_c);
        pa_log_debug("2 data=%s", x);
        pa_memblock_release(mb_c);
        pa_memblock_unref(mb_c);

        pa_memimport_free(import_b);

        pa_memblock_unref(mb_a);

        pa_memimport_free(import_c);
        pa_memexport_free(export_a);
    }

    pa_log("vacuuming...");

    pa_mempool_vacuum(pool_a);
    pa_mempool_vacuum(pool_b);
    pa_mempool_vacuum(pool_c);

    pa_log("vacuuming done...");

    pa_mempool_free(pool_a);
    pa_mempool_free(pool_b);
    pa_mempool_free(pool_c);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    s = suite_create("Memblock");
    tc = tcase_create("memblock");
    tcase_add_test(tc, memblock_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
