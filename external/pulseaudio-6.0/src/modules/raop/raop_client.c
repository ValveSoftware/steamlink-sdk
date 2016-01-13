/***
  This file is part of PulseAudio.

  Copyright 2008 Colin Guthrie

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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

/* TODO: Replace OpenSSL with NSS */
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>

#include <pulse/xmalloc.h>

#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>
#include <pulsecore/iochannel.h>
#include <pulsecore/socket-util.h>
#include <pulsecore/log.h>
#include <pulsecore/parseaddr.h>
#include <pulsecore/macro.h>
#include <pulsecore/memchunk.h>
#include <pulsecore/random.h>

#include "raop_client.h"
#include "rtsp_client.h"
#include "base64.h"

#define AES_CHUNKSIZE 16

#define JACK_STATUS_DISCONNECTED 0
#define JACK_STATUS_CONNECTED 1

#define JACK_TYPE_ANALOG 0
#define JACK_TYPE_DIGITAL 1

#define VOLUME_DEF -30
#define VOLUME_MIN -144
#define VOLUME_MAX 0

#define RAOP_PORT 5000

struct pa_raop_client {
    pa_core *core;
    char *host;
    uint16_t port;
    char *sid;
    pa_rtsp_client *rtsp;

    uint8_t jack_type;
    uint8_t jack_status;

    /* Encryption Related bits */
    AES_KEY aes;
    uint8_t aes_iv[AES_CHUNKSIZE]; /* initialization vector for aes-cbc */
    uint8_t aes_nv[AES_CHUNKSIZE]; /* next vector for aes-cbc */
    uint8_t aes_key[AES_CHUNKSIZE]; /* key for aes-cbc */

    pa_socket_client *sc;
    int fd;

    uint16_t seq;
    uint32_t rtptime;

    pa_raop_client_cb_t callback;
    void* userdata;
    pa_raop_client_closed_cb_t closed_callback;
    void* closed_userdata;
};

/**
 * Function to write bits into a buffer.
 * @param buffer Handle to the buffer. It will be incremented if new data requires it.
 * @param bit_pos A pointer to a position buffer to keep track the current write location (0 for MSB, 7 for LSB)
 * @param size A pointer to the byte size currently written. This allows the calling function to do simple buffer overflow checks
 * @param data The data to write
 * @param data_bit_len The number of bits from data to write
 */
static inline void bit_writer(uint8_t **buffer, uint8_t *bit_pos, int *size, uint8_t data, uint8_t data_bit_len) {
    int bits_left, bit_overflow;
    uint8_t bit_data;

    if (!data_bit_len)
        return;

    /* If bit pos is zero, we will definately use at least one bit from the current byte so size increments. */
    if (!*bit_pos)
        *size += 1;

    /* Calc the number of bits left in the current byte of buffer */
    bits_left = 7 - *bit_pos  + 1;
    /* Calc the overflow of bits in relation to how much space we have left... */
    bit_overflow = bits_left - data_bit_len;
    if (bit_overflow >= 0) {
        /* We can fit the new data in our current byte */
        /* As we write from MSB->LSB we need to left shift by the overflow amount */
        bit_data = data << bit_overflow;
        if (*bit_pos)
            **buffer |= bit_data;
        else
            **buffer = bit_data;
        /* If our data fits exactly into the current byte, we need to increment our pointer */
        if (0 == bit_overflow) {
            /* Do not increment size as it will be incremented on next call as bit_pos is zero */
            *buffer += 1;
            *bit_pos = 0;
        } else {
            *bit_pos += data_bit_len;
        }
    } else {
        /* bit_overflow is negative, there for we will need a new byte from our buffer */
        /* Firstly fill up what's left in the current byte */
        bit_data = data >> -bit_overflow;
        **buffer |= bit_data;
        /* Increment our buffer pointer and size counter*/
        *buffer += 1;
        *size += 1;
        **buffer = data << (8 + bit_overflow);
        *bit_pos = -bit_overflow;
    }
}

static int rsa_encrypt(uint8_t *text, int len, uint8_t *res) {
    const char n[] =
        "59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUtwC"
        "5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDR"
        "KSKv6kDqnw4UwPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuB"
        "OitnZ/bDzPHrTOZz0Dew0uowxf/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJ"
        "Q+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/UAaHqn9JdsBWLUEpVviYnh"
        "imNVvYFZeCXg/IdTQ+x4IRdiXNv5hEew==";
    const char e[] = "AQAB";
    uint8_t modules[256];
    uint8_t exponent[8];
    int size;
    RSA *rsa;

    rsa = RSA_new();
    size = pa_base64_decode(n, modules);
    rsa->n = BN_bin2bn(modules, size, NULL);
    size = pa_base64_decode(e, exponent);
    rsa->e = BN_bin2bn(exponent, size, NULL);

    size = RSA_public_encrypt(len, text, res, rsa, RSA_PKCS1_OAEP_PADDING);
    RSA_free(rsa);
    return size;
}

static int aes_encrypt(pa_raop_client* c, uint8_t *data, int size) {
    uint8_t *buf;
    int i=0, j;

    pa_assert(c);

    memcpy(c->aes_nv, c->aes_iv, AES_CHUNKSIZE);
    while (i+AES_CHUNKSIZE <= size) {
        buf = data + i;
        for (j=0; j<AES_CHUNKSIZE; ++j)
            buf[j] ^= c->aes_nv[j];

        AES_encrypt(buf, buf, &c->aes);
        memcpy(c->aes_nv, buf, AES_CHUNKSIZE);
        i += AES_CHUNKSIZE;
    }
    return i;
}

static inline void rtrimchar(char *str, char rc) {
    char *sp = str + strlen(str) - 1;
    while (sp >= str && *sp == rc) {
        *sp = '\0';
        sp -= 1;
    }
}

static void on_connection(pa_socket_client *sc, pa_iochannel *io, void *userdata) {
    pa_raop_client *c = userdata;

    pa_assert(sc);
    pa_assert(c);
    pa_assert(c->sc == sc);
    pa_assert(c->fd < 0);
    pa_assert(c->callback);

    pa_socket_client_unref(c->sc);
    c->sc = NULL;

    if (!io) {
        pa_log("Connection failed: %s", pa_cstrerror(errno));
        return;
    }

    c->fd = pa_iochannel_get_send_fd(io);

    pa_iochannel_set_noclose(io, true);
    pa_iochannel_free(io);

    pa_make_tcp_socket_low_delay(c->fd);

    pa_log_debug("Connection established");
    c->callback(c->fd, c->userdata);
}

static void rtsp_cb(pa_rtsp_client *rtsp, pa_rtsp_state state, pa_headerlist* headers, void *userdata) {
    pa_raop_client* c = userdata;
    pa_assert(c);
    pa_assert(rtsp);
    pa_assert(rtsp == c->rtsp);

    switch (state) {
        case STATE_CONNECT: {
            int i;
            uint8_t rsakey[512];
            char *key, *iv, *sac, *sdp;
            uint16_t rand_data;
            const char *ip;
            char *url;

            pa_log_debug("RAOP: CONNECTED");
            ip = pa_rtsp_localip(c->rtsp);
            /* First of all set the url properly */
            url = pa_sprintf_malloc("rtsp://%s/%s", ip, c->sid);
            pa_rtsp_set_url(c->rtsp, url);
            pa_xfree(url);

            /* Now encrypt our aes_public key to send to the device */
            i = rsa_encrypt(c->aes_key, AES_CHUNKSIZE, rsakey);
            pa_base64_encode(rsakey, i, &key);
            rtrimchar(key, '=');
            pa_base64_encode(c->aes_iv, AES_CHUNKSIZE, &iv);
            rtrimchar(iv, '=');

            pa_random(&rand_data, sizeof(rand_data));
            pa_base64_encode(&rand_data, AES_CHUNKSIZE, &sac);
            rtrimchar(sac, '=');
            pa_rtsp_add_header(c->rtsp, "Apple-Challenge", sac);
            sdp = pa_sprintf_malloc(
                "v=0\r\n"
                "o=iTunes %s 0 IN IP4 %s\r\n"
                "s=iTunes\r\n"
                "c=IN IP4 %s\r\n"
                "t=0 0\r\n"
                "m=audio 0 RTP/AVP 96\r\n"
                "a=rtpmap:96 AppleLossless\r\n"
                "a=fmtp:96 4096 0 16 40 10 14 2 255 0 0 44100\r\n"
                "a=rsaaeskey:%s\r\n"
                "a=aesiv:%s\r\n",
                c->sid, ip, c->host, key, iv);
            pa_rtsp_announce(c->rtsp, sdp);
            pa_xfree(key);
            pa_xfree(iv);
            pa_xfree(sac);
            pa_xfree(sdp);
            break;
        }

        case STATE_ANNOUNCE:
            pa_log_debug("RAOP: ANNOUNCED");
            pa_rtsp_remove_header(c->rtsp, "Apple-Challenge");
            pa_rtsp_setup(c->rtsp);
            break;

        case STATE_SETUP: {
            char *aj = pa_xstrdup(pa_headerlist_gets(headers, "Audio-Jack-Status"));
            pa_log_debug("RAOP: SETUP");
            if (aj) {
                char *token, *pc;
                char delimiters[] = ";";
                const char* token_state = NULL;
                c->jack_type = JACK_TYPE_ANALOG;
                c->jack_status = JACK_STATUS_DISCONNECTED;

                while ((token = pa_split(aj, delimiters, &token_state))) {
                    if ((pc = strstr(token, "="))) {
                      *pc = 0;
                      if (pa_streq(token, "type") && pa_streq(pc+1, "digital")) {
                          c->jack_type = JACK_TYPE_DIGITAL;
                      }
                    } else {
                        if (pa_streq(token, "connected"))
                            c->jack_status = JACK_STATUS_CONNECTED;
                    }
                    pa_xfree(token);
                }
                pa_xfree(aj);
            } else {
                pa_log_warn("Audio Jack Status missing");
            }
            pa_rtsp_record(c->rtsp, &c->seq, &c->rtptime);
            break;
        }

        case STATE_RECORD: {
            uint32_t port = pa_rtsp_serverport(c->rtsp);
            pa_log_debug("RAOP: RECORDED");

            if (!(c->sc = pa_socket_client_new_string(c->core->mainloop, true, c->host, port))) {
                pa_log("failed to connect to server '%s:%d'", c->host, port);
                return;
            }
            pa_socket_client_set_callback(c->sc, on_connection, c);
            break;
        }

        case STATE_FLUSH:
            pa_log_debug("RAOP: FLUSHED");
            break;

        case STATE_TEARDOWN:
            pa_log_debug("RAOP: TEARDOWN");
            break;

        case STATE_SET_PARAMETER:
            pa_log_debug("RAOP: SET_PARAMETER");
            break;

        case STATE_DISCONNECTED:
            pa_assert(c->closed_callback);
            pa_assert(c->rtsp);

            pa_log_debug("RTSP control channel closed");
            pa_rtsp_client_free(c->rtsp);
            c->rtsp = NULL;
            if (c->fd > 0) {
                /* We do not close the fd, we leave it to the closed callback to do that */
                c->fd = -1;
            }
            if (c->sc) {
                pa_socket_client_unref(c->sc);
                c->sc = NULL;
            }
            pa_xfree(c->sid);
            c->sid = NULL;
            c->closed_callback(c->closed_userdata);
            break;
    }
}

pa_raop_client* pa_raop_client_new(pa_core *core, const char* host) {
    pa_parsed_address a;
    pa_raop_client* c;

    pa_assert(core);
    pa_assert(host);

    if (pa_parse_address(host, &a) < 0)
        return NULL;

    if (a.type == PA_PARSED_ADDRESS_UNIX) {
        pa_xfree(a.path_or_host);
        return NULL;
    }

    c = pa_xnew0(pa_raop_client, 1);
    c->core = core;
    c->fd = -1;

    c->host = a.path_or_host;
    if (a.port)
        c->port = a.port;
    else
        c->port = RAOP_PORT;

    if (pa_raop_connect(c)) {
        pa_raop_client_free(c);
        return NULL;
    }
    return c;
}

void pa_raop_client_free(pa_raop_client* c) {
    pa_assert(c);

    if (c->rtsp)
        pa_rtsp_client_free(c->rtsp);
    if (c->sid)
        pa_xfree(c->sid);
    pa_xfree(c->host);
    pa_xfree(c);
}

int pa_raop_connect(pa_raop_client* c) {
    char *sci;
    struct {
        uint32_t a;
        uint32_t b;
        uint32_t c;
    } rand_data;

    pa_assert(c);

    if (c->rtsp) {
        pa_log_debug("Connection already in progress");
        return 0;
    }

    c->rtsp = pa_rtsp_client_new(c->core->mainloop, c->host, c->port, "iTunes/4.6 (Macintosh; U; PPC Mac OS X 10.3)");

    /* Initialise the AES encryption system */
    pa_random(c->aes_iv, sizeof(c->aes_iv));
    pa_random(c->aes_key, sizeof(c->aes_key));
    memcpy(c->aes_nv, c->aes_iv, sizeof(c->aes_nv));
    AES_set_encrypt_key(c->aes_key, 128, &c->aes);

    /* Generate random instance id */
    pa_random(&rand_data, sizeof(rand_data));
    c->sid = pa_sprintf_malloc("%u", rand_data.a);
    sci = pa_sprintf_malloc("%08x%08x",rand_data.b, rand_data.c);
    pa_rtsp_add_header(c->rtsp, "Client-Instance", sci);
    pa_xfree(sci);
    pa_rtsp_set_callback(c->rtsp, rtsp_cb, c);
    return pa_rtsp_connect(c->rtsp);
}

int pa_raop_flush(pa_raop_client* c) {
    pa_assert(c);

    pa_rtsp_flush(c->rtsp, c->seq, c->rtptime);
    return 0;
}

int pa_raop_client_set_volume(pa_raop_client* c, pa_volume_t volume) {
    int rv;
    double db;
    char *param;

    pa_assert(c);

    db = pa_sw_volume_to_dB(volume);
    if (db < VOLUME_MIN)
        db = VOLUME_MIN;
    else if (db > VOLUME_MAX)
        db = VOLUME_MAX;

    param = pa_sprintf_malloc("volume: %0.6f\r\n",  db);

    /* We just hit and hope, cannot wait for the callback */
    rv = pa_rtsp_setparameter(c->rtsp, param);
    pa_xfree(param);
    return rv;
}

int pa_raop_client_encode_sample(pa_raop_client* c, pa_memchunk* raw, pa_memchunk* encoded) {
    uint16_t len;
    size_t bufmax;
    uint8_t *bp, bpos;
    uint8_t *ibp, *maxibp;
    int size;
    uint8_t *b, *p;
    uint32_t bsize;
    size_t length;
    static uint8_t header[] = {
        0x24, 0x00, 0x00, 0x00,
        0xF0, 0xFF, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };
    int header_size = sizeof(header);

    pa_assert(c);
    pa_assert(c->fd > 0);
    pa_assert(raw);
    pa_assert(raw->memblock);
    pa_assert(raw->length > 0);
    pa_assert(encoded);

    /* We have to send 4 byte chunks */
    bsize = (int)(raw->length / 4);
    length = bsize * 4;

    /* Leave 16 bytes extra to allow for the ALAC header which is about 55 bits */
    bufmax = length + header_size + 16;
    pa_memchunk_reset(encoded);
    encoded->memblock = pa_memblock_new(c->core->mempool, bufmax);
    b = pa_memblock_acquire(encoded->memblock);
    memcpy(b, header, header_size);

    /* Now write the actual samples */
    bp = b + header_size;
    size = bpos = 0;
    bit_writer(&bp,&bpos,&size,1,3); /* channel=1, stereo */
    bit_writer(&bp,&bpos,&size,0,4); /* unknown */
    bit_writer(&bp,&bpos,&size,0,8); /* unknown */
    bit_writer(&bp,&bpos,&size,0,4); /* unknown */
    bit_writer(&bp,&bpos,&size,1,1); /* hassize */
    bit_writer(&bp,&bpos,&size,0,2); /* unused */
    bit_writer(&bp,&bpos,&size,1,1); /* is-not-compressed */

    /* size of data, integer, big endian */
    bit_writer(&bp,&bpos,&size,(bsize>>24)&0xff,8);
    bit_writer(&bp,&bpos,&size,(bsize>>16)&0xff,8);
    bit_writer(&bp,&bpos,&size,(bsize>>8)&0xff,8);
    bit_writer(&bp,&bpos,&size,(bsize)&0xff,8);

    ibp = p = pa_memblock_acquire(raw->memblock);
    maxibp = p + raw->length - 4;
    while (ibp <= maxibp) {
        /* Byte swap stereo data */
        bit_writer(&bp,&bpos,&size,*(ibp+1),8);
        bit_writer(&bp,&bpos,&size,*(ibp+0),8);
        bit_writer(&bp,&bpos,&size,*(ibp+3),8);
        bit_writer(&bp,&bpos,&size,*(ibp+2),8);
        ibp += 4;
        raw->index += 4;
        raw->length -= 4;
    }
    pa_memblock_release(raw->memblock);
    encoded->length = header_size + size;

    /* store the length (endian swapped: make this better) */
    len = size + header_size - 4;
    *(b + 2) = len >> 8;
    *(b + 3) = len & 0xff;

    /* encrypt our data */
    aes_encrypt(c, (b + header_size), size);

    /* We're done with the chunk */
    pa_memblock_release(encoded->memblock);

    return 0;
}

void pa_raop_client_set_callback(pa_raop_client* c, pa_raop_client_cb_t callback, void *userdata) {
    pa_assert(c);

    c->callback = callback;
    c->userdata = userdata;
}

void pa_raop_client_set_closed_callback(pa_raop_client* c, pa_raop_client_closed_cb_t callback, void *userdata) {
    pa_assert(c);

    c->closed_callback = callback;
    c->closed_userdata = userdata;
}
