/*
 * Copyright (C) 2010-2012 Free Software Foundation, Inc.
 * Copyright (C) 2000, 2001, 2008 Niels Möller
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GNUTLS.
 *
 * The GNUTLS library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

/* Here is the random generator layer. This code was based on the LSH 
 * random generator (the trivia and device source functions for POSIX)
 * and modified to fit gnutls' needs. Relicenced with permission. 
 * Original author Niels Möller.
 */

#include <gnutls_int.h>
#include <gnutls_errors.h>
#include <locks.h>
#include <gnutls_num.h>
#include <nettle/yarrow.h>
#include <nettle/salsa20.h>
#ifdef HAVE_GETPID
#include <unistd.h>		/* getpid */
#endif
#include <rnd-common.h>
#include <errno.h>

#define SOURCES 2

#define RND_LOCK(ctx) if (gnutls_mutex_lock(&((ctx)->mutex))!=0) abort()
#define RND_UNLOCK(ctx) if (gnutls_mutex_unlock(&((ctx)->mutex))!=0) abort()

enum {
	RANDOM_SOURCE_TRIVIA = 0,
	RANDOM_SOURCE_DEVICE,
};


struct nonce_ctx_st {
	struct salsa20_ctx ctx;
	unsigned int counter;
	void *mutex;
#ifdef HAVE_GETPID
	pid_t pid;		/* detect fork() */
#endif
};

struct rnd_ctx_st {
	struct yarrow256_ctx yctx;
	struct yarrow_source ysources[SOURCES];
	struct timespec device_last_read;
	time_t trivia_previous_time;
	time_t trivia_time_count;
	void *mutex;
#ifdef HAVE_GETPID
	pid_t pid;		/* detect fork() */
#endif
};

static struct rnd_ctx_st rnd_ctx;

/* after this number of bytes salsa20 will rekey */
#define NONCE_RESEED_BYTES (1048576)
static struct nonce_ctx_st nonce_ctx;

inline static unsigned int
timespec_sub_sec(struct timespec *a, struct timespec *b)
{
	return (a->tv_sec - b->tv_sec);
}

#define DEVICE_READ_INTERVAL (10800)
/* universal functions */

static int do_trivia_source(struct rnd_ctx_st *ctx, int init, struct event_st *event)
{
	unsigned entropy = 0;

	if (init) {
		ctx->trivia_time_count = 0;
	} else {
		ctx->trivia_time_count++;

		if (event->now.tv_sec != ctx->trivia_previous_time) {
			/* Count one bit of entropy if we either have more than two
			 * invocations in one second, or more than two seconds
			 * between invocations. */
			if ((ctx->trivia_time_count > 2)
			    || ((event->now.tv_sec - ctx->trivia_previous_time) > 2))
				entropy++;

			ctx->trivia_time_count = 0;
		}
	}
	ctx->trivia_previous_time = event->now.tv_sec;

	return yarrow256_update(&ctx->yctx, RANDOM_SOURCE_TRIVIA, entropy,
				sizeof(*event), (void *) event);
}

#define DEVICE_READ_SIZE 16
#define DEVICE_READ_SIZE_MAX 32

static int do_device_source(struct rnd_ctx_st *ctx, int init, struct event_st *event)
{
	unsigned int read_size = DEVICE_READ_SIZE;
	int ret;

	if (init) {
#ifdef HAVE_GETPID
		ctx->pid = event->pid;
#endif

		memcpy(&ctx->device_last_read, &event->now,
		       sizeof(ctx->device_last_read));

		read_size = DEVICE_READ_SIZE_MAX;	/* initially read more data */
	}

	if ((init
	     || (timespec_sub_sec(&event->now, &ctx->device_last_read) >
		 DEVICE_READ_INTERVAL))) {
		/* More than 20 minutes since we last read the device */
		uint8_t buf[DEVICE_READ_SIZE_MAX];

		ret = _rnd_get_system_entropy(buf, read_size);
		if (ret < 0)
			return gnutls_assert_val(ret);

		memcpy(&ctx->device_last_read, &event->now,
		       sizeof(ctx->device_last_read));

		return yarrow256_update(&ctx->yctx, RANDOM_SOURCE_DEVICE,
					read_size * 8 /
					2 /* we trust the RNG */ ,
					read_size, buf);
	}
	return 0;
}

static void wrap_nettle_rnd_deinit(void *ctx)
{
	_rnd_system_entropy_deinit();

	gnutls_mutex_deinit(&nonce_ctx.mutex);
	nonce_ctx.mutex = NULL;
	gnutls_mutex_deinit(&rnd_ctx.mutex);
	rnd_ctx.mutex = NULL;
}

static int nonce_rng_init(struct nonce_ctx_st *ctx, unsigned init)
{
	uint8_t buffer[SALSA20_KEY_SIZE];
	uint8_t iv[8];
	int ret;

	/* Get a key from the system randomness source.  */
	ret = _rnd_get_system_entropy(buffer, sizeof(buffer));
	if (ret < 0)
		return gnutls_assert_val(ret);

	if (init == 0) {
		/* use the previous key to generate IV as well */
		memset(iv, 0, sizeof(iv)); /* to prevent valgrind from whinning */
		salsa20r12_crypt(&ctx->ctx, sizeof(iv), iv, iv);

		/* Add key continuity by XORing the new key with data generated
		 * from the old key */
		salsa20r12_crypt(&ctx->ctx, sizeof(buffer), buffer, buffer);
	} else {
		/* when initializing read the IV from the system randomness source */
		ret = _rnd_get_system_entropy(iv, sizeof(iv));
		if (ret < 0)
			return gnutls_assert_val(ret);
	}

	salsa20_set_key(&ctx->ctx, sizeof(buffer), buffer);
	salsa20_set_iv(&ctx->ctx, iv);

	zeroize_key(buffer, sizeof(buffer));

	ctx->counter = 0;
#ifdef HAVE_GETPID
	ctx->pid = getpid();
#endif

	return 0;
}

/* API functions */

static int wrap_nettle_rnd_init(void **ctx)
{
	int ret;
	struct event_st event;

	memset(&rnd_ctx, 0, sizeof(rnd_ctx));

	ret = gnutls_mutex_init(&nonce_ctx.mutex);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	ret = gnutls_mutex_init(&rnd_ctx.mutex);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	ret = _rnd_system_entropy_init();
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	/* initialize the main RNG */
	yarrow256_init(&rnd_ctx.yctx, SOURCES, rnd_ctx.ysources);

	_rnd_get_event(&event);

	ret = do_device_source(&rnd_ctx, 1, &event);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	ret = do_trivia_source(&rnd_ctx, 1, &event);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	yarrow256_slow_reseed(&rnd_ctx.yctx);

	/* initialize the nonce RNG */
	ret = nonce_rng_init(&nonce_ctx, 1);
	if (ret < 0)
		return gnutls_assert_val(ret);

	return 0;
}

static int
wrap_nettle_rnd_nonce(void *_ctx, void *data, size_t datasize)
{
	int ret, reseed = 0;
#ifdef HAVE_GETPID
	pid_t tpid = getpid();
#endif

	/* we don't really need memset here, but otherwise we
	 * get filled with valgrind warnings */
	memset(data, 0, datasize);

	RND_LOCK(&nonce_ctx);

#ifdef HAVE_GETPID
	if (tpid != nonce_ctx.pid) {	/* fork() detected */
		reseed = 1;
	}
#endif

	if (reseed != 0 || nonce_ctx.counter > NONCE_RESEED_BYTES) {
		/* reseed nonce */
		ret = nonce_rng_init(&nonce_ctx, 0);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
	}

	salsa20r12_crypt(&nonce_ctx.ctx, datasize, data, data);
	nonce_ctx.counter += datasize;

	ret = 0;

cleanup:
	RND_UNLOCK(&nonce_ctx);
	return ret;
}



static int
wrap_nettle_rnd(void *_ctx, int level, void *data, size_t datasize)
{
	int ret, reseed = 0;
	struct event_st event;

	if (level == GNUTLS_RND_NONCE)
		return wrap_nettle_rnd_nonce(_ctx, data, datasize);

	_rnd_get_event(&event);

	RND_LOCK(&rnd_ctx);

#ifdef HAVE_GETPID
	if (event.pid != rnd_ctx.pid) {	/* fork() detected */
		memset(&rnd_ctx.device_last_read, 0, sizeof(rnd_ctx.device_last_read));
		rnd_ctx.pid = event.pid;
		reseed = 1;
	}
#endif

	/* reseed main */
	ret = do_trivia_source(&rnd_ctx, 0, &event);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = do_device_source(&rnd_ctx, 0, &event);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	if (reseed != 0)
		yarrow256_slow_reseed(&rnd_ctx.yctx);

	yarrow256_random(&rnd_ctx.yctx, datasize, data);
	ret = 0;

cleanup:
	RND_UNLOCK(&rnd_ctx);
	return ret;
}

static void wrap_nettle_rnd_refresh(void *_ctx)
{
	struct event_st event;

	_rnd_get_event(&event);

	RND_LOCK(&rnd_ctx);
	do_trivia_source(&rnd_ctx, 0, &event);
	do_device_source(&rnd_ctx, 0, &event);
	RND_UNLOCK(&rnd_ctx);

	RND_LOCK(&nonce_ctx);
	nonce_rng_init(&nonce_ctx, 0);
	RND_UNLOCK(&nonce_ctx);

	return;
}

int crypto_rnd_prio = INT_MAX;

gnutls_crypto_rnd_st _gnutls_rnd_ops = {
	.init = wrap_nettle_rnd_init,
	.deinit = wrap_nettle_rnd_deinit,
	.rnd = wrap_nettle_rnd,
	.rnd_refresh = wrap_nettle_rnd_refresh,
	.self_test = NULL,
};
