/***
  This file is part of PulseAudio.

  Copyright 2014 David Henningsson, Canonical Ltd.

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

#include <unistd.h>
#include <check.h>

#include <pulse/mainloop.h>
#include <pulsecore/packet.h>
#include <pulsecore/pstream.h>
#include <pulsecore/iochannel.h>
#include <pulsecore/memblock.h>

static unsigned packets_received;
static unsigned packets_checksum;
static size_t packets_length;

static void packet_received(pa_pstream *p, pa_packet *packet, const pa_cmsg_ancil_data *ancil_data, void *userdata) {
    unsigned i;
    fail_unless(packets_length == packet->length);
    packets_received++;
    for (i = 0; i < packet->length; i++)
        packets_checksum += packet->data[i];
}

static void packet_test(unsigned npackets, size_t plength, pa_mainloop *ml, pa_pstream *p1, pa_pstream *p2) {
    pa_packet *packet = pa_packet_new(plength);
    unsigned i;
    unsigned psum = 0, totalsum = 0;
    pa_log_info("Sending %d packets of length %zd", npackets, plength);
    packets_received = 0;
    packets_checksum = 0;
    packets_length = plength;
    pa_pstream_set_receive_packet_callback(p2, packet_received, NULL);

    for (i = 0; i < plength; i++) {
        packet->data[i] = i;
        psum += packet->data[i];
    }

    for (i = 0; i < npackets; i++) {
        pa_pstream_send_packet(p1, packet, NULL);
        totalsum += psum;
        pa_mainloop_iterate(ml, 0, NULL);
    }

    while (packets_received < npackets)
        pa_mainloop_iterate(ml, 1, NULL);

    fail_unless(packets_checksum == totalsum);
    pa_log_debug("Correct checksum received (%d)", packets_checksum);
    pa_packet_unref(packet);
}

START_TEST (srbchannel_test) {

    int pipefd[4];

    pa_mainloop *ml = pa_mainloop_new();
    pa_mempool *mp = pa_mempool_new(true, 0);
    pa_iochannel *io1, *io2;
    pa_pstream *p1, *p2;
    pa_srbchannel *sr1, *sr2;
    pa_srbchannel_template srt;

    fail_unless(pipe(pipefd) == 0);
    fail_unless(pipe(&pipefd[2]) == 0);
    io1 = pa_iochannel_new(pa_mainloop_get_api(ml), pipefd[2], pipefd[1]);
    io2 = pa_iochannel_new(pa_mainloop_get_api(ml), pipefd[0], pipefd[3]);
    p1 = pa_pstream_new(pa_mainloop_get_api(ml), io1, mp);
    p2 = pa_pstream_new(pa_mainloop_get_api(ml), io2, mp);

    pa_log_debug("Pipes: fd %d -> %d, %d -> %d", pipefd[1], pipefd[0], pipefd[3], pipefd[2]);

    packet_test(250, 5, ml, p1, p2);
    packet_test(10, 1234567, ml, p1, p2);

    pa_log_debug("And now the same thing with srbchannel...");

    sr1 = pa_srbchannel_new(pa_mainloop_get_api(ml), mp);
    pa_srbchannel_export(sr1, &srt);
    pa_pstream_set_srbchannel(p1, sr1);
    sr2 = pa_srbchannel_new_from_template(pa_mainloop_get_api(ml), &srt);
    pa_pstream_set_srbchannel(p2, sr2);

    packet_test(250, 5, ml, p1, p2);
    packet_test(10, 1234567, ml, p1, p2);

    pa_pstream_unref(p1);
    pa_pstream_unref(p2);
    pa_mempool_free(mp);
    pa_mainloop_free(ml);
}
END_TEST


int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    s = suite_create("srbchannel");
    tc = tcase_create("srbchannel");
    tcase_add_test(tc, srbchannel_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
