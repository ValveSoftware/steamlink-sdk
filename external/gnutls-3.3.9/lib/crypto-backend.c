/*
 * Copyright (C) 2008-2012 Free Software Foundation, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
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

#include <gnutls_errors.h>
#include <gnutls_int.h>
#include <gnutls/crypto.h>
#include <crypto-backend.h>
#include <crypto.h>
#include <gnutls_mpi.h>
#include <gnutls_pk.h>
#include <random.h>
#include <gnutls_cipher_int.h>

/* default values for priorities */
int crypto_mac_prio = INT_MAX;
int crypto_digest_prio = INT_MAX;
int crypto_cipher_prio = INT_MAX;

typedef struct algo_list {
	int algorithm;
	int priority;
	const void *alg_data;
	struct algo_list *next;
} algo_list;

#define cipher_list algo_list
#define mac_list algo_list
#define digest_list algo_list

static int
_algo_register(algo_list * al, int algorithm, int priority, const void *s)
{
	algo_list *cl;
	algo_list *last_cl = al;

	if (al == NULL)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	/* look if there is any cipher with lowest priority. In that case do not add.
	 */
	cl = al;
	while (cl && cl->alg_data) {
		if (cl->algorithm == algorithm) {
			if (cl->priority < priority) {
				gnutls_assert();
				return GNUTLS_E_CRYPTO_ALREADY_REGISTERED;
			} else {
				/* the current has higher priority -> overwrite */
				cl->algorithm = algorithm;
				cl->priority = priority;
				cl->alg_data = s;
				return 0;
			}
		}
		cl = cl->next;
		if (cl)
			last_cl = cl;
	}

	cl = gnutls_calloc(1, sizeof(cipher_list));

	if (cl == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	last_cl->algorithm = algorithm;
	last_cl->priority = priority;
	last_cl->alg_data = s;
	last_cl->next = cl;

	return 0;

}

static const void *_get_algo(algo_list * al, int algo)
{
	cipher_list *cl;

	/* look if there is any cipher with lowest priority. In that case do not add.
	 */
	cl = al;
	while (cl && cl->alg_data) {
		if (cl->algorithm == algo) {
			return cl->alg_data;
		}
		cl = cl->next;
	}

	return NULL;
}

static cipher_list glob_cl = { GNUTLS_CIPHER_NULL, 0, NULL, NULL };
static mac_list glob_ml = { GNUTLS_MAC_NULL, 0, NULL, NULL };
static digest_list glob_dl = { GNUTLS_MAC_NULL, 0, NULL, NULL };

static void _deregister(algo_list * cl)
{
	algo_list *next;

	next = cl->next;
	cl->next = NULL;
	cl = next;

	while (cl) {
		next = cl->next;
		gnutls_free(cl);
		cl = next;
	}
}

void _gnutls_crypto_deregister(void)
{
	_deregister(&glob_cl);
	_deregister(&glob_ml);
	_deregister(&glob_dl);
}

/*-
 * gnutls_crypto_single_cipher_register:
 * @algorithm: is the gnutls algorithm identifier
 * @priority: is the priority of the algorithm
 * @s: is a structure holding new cipher's data
 *
 * This function will register a cipher algorithm to be used by
 * gnutls.  Any algorithm registered will override the included
 * algorithms and by convention kernel implemented algorithms have
 * priority of 90 and CPU-assisted of 80.  The algorithm with the lowest priority will be
 * used by gnutls.
 *
 * This function should be called before gnutls_global_init().
 *
 * For simplicity you can use the convenience
 * gnutls_crypto_single_cipher_register() macro.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, otherwise a negative error code.
 *
 * Since: 2.6.0
 -*/
int
gnutls_crypto_single_cipher_register(gnutls_cipher_algorithm_t algorithm,
				     int priority,
				     const gnutls_crypto_cipher_st * s)
{
	return _algo_register(&glob_cl, algorithm, priority, s);
}

const gnutls_crypto_cipher_st
    *_gnutls_get_crypto_cipher(gnutls_cipher_algorithm_t algo)
{
	return _get_algo(&glob_cl, algo);
}

/*-
 * gnutls_crypto_rnd_register:
 * @priority: is the priority of the generator
 * @s: is a structure holding new generator's data
 *
 * This function will register a random generator to be used by
 * gnutls.  Any generator registered will override the included
 * generator and by convention kernel implemented generators have
 * priority of 90 and CPU-assisted of 80. The generator with the lowest priority will be
 * used by gnutls.
 *
 * This function should be called before gnutls_global_init().
 *
 * For simplicity you can use the convenience
 * gnutls_crypto_rnd_register() macro.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, otherwise a negative error code.
 *
 * Since: 2.6.0
 -*/
int
gnutls_crypto_rnd_register(int priority, const gnutls_crypto_rnd_st * s)
{
	if (crypto_rnd_prio > priority) {
		memcpy(&_gnutls_rnd_ops, s, sizeof(*s));
		crypto_rnd_prio = priority;
		return 0;
	}

	return GNUTLS_E_CRYPTO_ALREADY_REGISTERED;
}

/*-
 * gnutls_crypto_single_mac_register:
 * @algorithm: is the gnutls algorithm identifier
 * @priority: is the priority of the algorithm
 * @s: is a structure holding new algorithms's data
 *
 * This function will register a MAC algorithm to be used by gnutls.
 * Any algorithm registered will override the included algorithms and
 * by convention kernel implemented algorithms have priority of 90
 *  and CPU-assisted of 80.
 * The algorithm with the lowest priority will be used by gnutls.
 *
 * This function should be called before gnutls_global_init().
 *
 * For simplicity you can use the convenience
 * gnutls_crypto_single_mac_register() macro.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, otherwise a negative error code.
 *
 * Since: 2.6.0
 -*/
int
gnutls_crypto_single_mac_register(gnutls_mac_algorithm_t algorithm,
				  int priority,
				  const gnutls_crypto_mac_st * s)
{
	return _algo_register(&glob_ml, algorithm, priority, s);
}

const gnutls_crypto_mac_st *_gnutls_get_crypto_mac(gnutls_mac_algorithm_t
						   algo)
{
	return _get_algo(&glob_ml, algo);
}

/*-
 * gnutls_crypto_single_digest_register:
 * @algorithm: is the gnutls algorithm identifier
 * @priority: is the priority of the algorithm
 * @s: is a structure holding new algorithms's data
 *
 * This function will register a digest (hash) algorithm to be used by
 * gnutls.  Any algorithm registered will override the included
 * algorithms and by convention kernel implemented algorithms have
 * priority of 90  and CPU-assisted of 80.  The algorithm with the lowest priority will be
 * used by gnutls.
 *
 * This function should be called before gnutls_global_init().
 *
 * For simplicity you can use the convenience
 * gnutls_crypto_single_digest_register() macro.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, otherwise a negative error code.
 *
 * Since: 2.6.0
 -*/
int
gnutls_crypto_single_digest_register(gnutls_digest_algorithm_t algorithm,
				     int priority,
				     const gnutls_crypto_digest_st * s)
{
	return _algo_register(&glob_dl, algorithm, priority, s);
}

const gnutls_crypto_digest_st
    *_gnutls_get_crypto_digest(gnutls_digest_algorithm_t algo)
{
	return _get_algo(&glob_dl, algo);
}

/*-
 * gnutls_crypto_bigint_register:
 * @priority: is the priority of the interface
 * @s: is a structure holding new interface's data
 *
 * This function will register an interface for gnutls to operate
 * on big integers. Any interface registered will override
 * the included interface. The interface with the lowest
 * priority will be used by gnutls.
 *
 * Note that the bigint interface must interoperate with the public
 * key interface. Thus if this interface is updated the
 * gnutls_crypto_pk_register() should also be used.
 *
 * This function should be called before gnutls_global_init().
 *
 * For simplicity you can use the convenience gnutls_crypto_bigint_register()
 * macro.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, otherwise a negative error code.
 *
 * Since: 2.6.0
 -*/
int
gnutls_crypto_bigint_register(int priority,
			      const gnutls_crypto_bigint_st * s)
{
	if (crypto_bigint_prio > priority) {
		memcpy(&_gnutls_mpi_ops, s, sizeof(*s));
		crypto_bigint_prio = priority;
		return 0;
	}

	return GNUTLS_E_CRYPTO_ALREADY_REGISTERED;
}

/*-
 * gnutls_crypto_pk_register:
 * @priority: is the priority of the interface
 * @s: is a structure holding new interface's data
 *
 * This function will register an interface for gnutls to operate
 * on public key operations. Any interface registered will override
 * the included interface. The interface with the lowest
 * priority will be used by gnutls.
 *
 * Note that the bigint interface must interoperate with the bigint
 * interface. Thus if this interface is updated the
 * gnutls_crypto_bigint_register() should also be used.
 *
 * This function should be called before gnutls_global_init().
 *
 * For simplicity you can use the convenience gnutls_crypto_pk_register()
 * macro.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, otherwise a negative error code.
 *
 * Since: 2.6.0
 -*/
int gnutls_crypto_pk_register(int priority, const gnutls_crypto_pk_st * s)
{
	if (crypto_pk_prio > priority) {
		memcpy(&_gnutls_pk_ops, s, sizeof(*s));
		crypto_pk_prio = priority;
		return 0;
	}

	return GNUTLS_E_CRYPTO_ALREADY_REGISTERED;
}

/*-
 * gnutls_crypto_cipher_register:
 * @priority: is the priority of the cipher interface
 * @s: is a structure holding new interface's data
 *
 * This function will register a cipher interface to be used by
 * gnutls. Any interface registered will override the included engine
 * and by convention kernel implemented interfaces should have
 * priority of 90  and CPU-assisted of 80. The interface with the lowest priority will be used
 * by gnutls.
 *
 * This function should be called before gnutls_global_init().
 *
 * For simplicity you can use the convenience
 * gnutls_crypto_cipher_register() macro.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, otherwise a negative error code.
 *
 * Since: 2.6.0
 -*/
int
gnutls_crypto_cipher_register(int priority,
			      const gnutls_crypto_cipher_st * s)
{
	if (crypto_cipher_prio > priority) {
		memcpy(&_gnutls_cipher_ops, s, sizeof(*s));
		crypto_cipher_prio = priority;
		return 0;
	}

	return GNUTLS_E_CRYPTO_ALREADY_REGISTERED;
}

/*-
 * gnutls_crypto_mac_register:
 * @priority: is the priority of the mac interface
 * @s: is a structure holding new interface's data
 *
 * This function will register a mac interface to be used by
 * gnutls. Any interface registered will override the included engine
 * and by convention kernel implemented interfaces should have
 * priority of 90  and CPU-assisted of 80. The interface with the lowest priority will be used
 * by gnutls.
 *
 * This function should be called before gnutls_global_init().
 *
 * For simplicity you can use the convenience
 * gnutls_crypto_digest_register() macro.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, otherwise a negative error code.
 *
 * Since: 2.6.0
 -*/
int
gnutls_crypto_mac_register(int priority, const gnutls_crypto_mac_st * s)
{
	if (crypto_mac_prio > priority) {
		memcpy(&_gnutls_mac_ops, s, sizeof(*s));
		crypto_mac_prio = priority;
		return 0;
	}

	return GNUTLS_E_CRYPTO_ALREADY_REGISTERED;
}

/*-
 * gnutls_crypto_digest_register:
 * @priority: is the priority of the digest interface
 * @s: is a structure holding new interface's data
 *
 * This function will register a digest interface to be used by
 * gnutls. Any interface registered will override the included engine
 * and by convention kernel implemented interfaces should have
 * priority of 90  and CPU-assisted of 80. The interface with the lowest priority will be used
 * by gnutls.
 *
 * This function should be called before gnutls_global_init().
 *
 * For simplicity you can use the convenience
 * gnutls_crypto_digest_register() macro.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, otherwise a negative error code.
 *
 * Since: 2.6.0
 -*/
int
gnutls_crypto_digest_register(int priority,
			      const gnutls_crypto_digest_st * s)
{
	if (crypto_digest_prio > priority) {
		memcpy(&_gnutls_digest_ops, s, sizeof(*s));
		crypto_digest_prio = priority;
		return 0;
	}

	return GNUTLS_E_CRYPTO_ALREADY_REGISTERED;
}
