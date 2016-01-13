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

#include <pulse/sample.h>

#include <pulsecore/resampler.h>
#include <pulsecore/macro.h>
#include <pulsecore/memblock.h>

int main(int argc, char *argv[]) {

    static const pa_channel_map maps[] = {
        { 1, { PA_CHANNEL_POSITION_MONO } },
        { 2, { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT } },
        { 3, { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT, PA_CHANNEL_POSITION_CENTER } },
        { 3, { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT, PA_CHANNEL_POSITION_LFE } },
        { 3, { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT, PA_CHANNEL_POSITION_REAR_CENTER } },
        { 4, { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT, PA_CHANNEL_POSITION_CENTER, PA_CHANNEL_POSITION_LFE } },
        { 4, { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT, PA_CHANNEL_POSITION_CENTER, PA_CHANNEL_POSITION_REAR_CENTER } },
        { 4, { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT } },
        { 5, { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT, PA_CHANNEL_POSITION_CENTER } },
        { 5, { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT, PA_CHANNEL_POSITION_LFE } },
        { 6, { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT, PA_CHANNEL_POSITION_LFE, PA_CHANNEL_POSITION_CENTER } },
        { 8, { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT, PA_CHANNEL_POSITION_LFE, PA_CHANNEL_POSITION_CENTER, PA_CHANNEL_POSITION_SIDE_LEFT, PA_CHANNEL_POSITION_SIDE_RIGHT } },
        { 0, { 0 } }
    };

    unsigned i, j;
    pa_mempool *pool;

    pa_log_set_level(PA_LOG_DEBUG);

    pa_assert_se(pool = pa_mempool_new(false, 0));

    for (i = 0; maps[i].channels > 0; i++)
        for (j = 0; maps[j].channels > 0; j++) {
            char a[PA_CHANNEL_MAP_SNPRINT_MAX], b[PA_CHANNEL_MAP_SNPRINT_MAX];
            pa_resampler *r;
            pa_sample_spec ss1, ss2;

            pa_log_info("Converting from '%s' to '%s'.\n", pa_channel_map_snprint(a, sizeof(a), &maps[i]), pa_channel_map_snprint(b, sizeof(b), &maps[j]));

            ss1.channels = maps[i].channels;
            ss2.channels = maps[j].channels;

            ss1.rate = ss2.rate = 44100;
            ss1.format = ss2.format = PA_SAMPLE_S16NE;

            r = pa_resampler_new(pool, &ss1, &maps[i], &ss2, &maps[j], PA_RESAMPLER_AUTO, 0);

            /* We don't really care for the resampler. We just want to
             * see the remixing debug output. */

            pa_resampler_free(r);
        }

    pa_mempool_free(pool);

    return 0;
}
