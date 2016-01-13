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

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <pulse/util.h>
#include <pulse/xmalloc.h>
#include <pulsecore/flist.h>
#include <pulsecore/thread.h>
#include <pulsecore/log.h>
#include <pulsecore/core-util.h>

#define THREADS_MAX 20

static pa_flist *flist;
static int quit = 0;

static void spin(void) {
    int k;

    /* Spin a little */
    k = rand() % 10000;
    for (; k > 0; k--)
        pa_thread_yield();
}

static void thread_func(void *data) {
    char *s = data;
    int n = 0;
    int b = 1;

    while (!quit) {
        char *text;

        /* Allocate some memory, if possible take it from the flist */
        if (b && (text = pa_flist_pop(flist)))
            pa_log("%s: popped '%s'", s, text);
        else {
            text = pa_sprintf_malloc("Block %i, allocated by %s", n++, s);
            pa_log("%s: allocated '%s'", s, text);
        }

        b = !b;

        spin();

        /* Give it back to the flist if possible */
        if (pa_flist_push(flist, text) < 0) {
            pa_log("%s: failed to push back '%s'", s, text);
            pa_xfree(text);
        } else
            pa_log("%s: pushed", s);

        spin();
    }

    if (pa_flist_push(flist, s) < 0)
        pa_xfree(s);
}

int main(int argc, char* argv[]) {
    pa_thread *threads[THREADS_MAX];
    int i;

    flist = pa_flist_new(0);

    for (i = 0; i < THREADS_MAX; i++) {
        threads[i] = pa_thread_new("test", thread_func, pa_sprintf_malloc("Thread #%i", i+1));
        pa_assert(threads[i]);
    }

    pa_msleep(60000);
    quit = 1;

    for (i = 0; i < THREADS_MAX; i++)
        pa_thread_free(threads[i]);

    pa_flist_free(flist, pa_xfree);

    return 0;
}
