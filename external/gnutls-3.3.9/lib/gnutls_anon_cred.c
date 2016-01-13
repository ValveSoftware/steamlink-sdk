/*
 * Copyright (C) 2001-2012 Free Software Foundation, Inc.
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

#include "gnutls_int.h"

#ifdef ENABLE_ANON

#include "gnutls_errors.h"
#include <auth/anon.h>
#include "gnutls_auth.h"
#include "gnutls_dh.h"
#include "gnutls_num.h"
#include "gnutls_mpi.h"

/**
 * gnutls_anon_free_server_credentials:
 * @sc: is a #gnutls_anon_server_credentials_t structure.
 *
 * This structure is complex enough to manipulate directly thus this
 * helper function is provided in order to free (deallocate) it.
 **/
void
gnutls_anon_free_server_credentials(gnutls_anon_server_credentials_t sc)
{

	gnutls_free(sc);
}

/**
 * gnutls_anon_allocate_server_credentials:
 * @sc: is a pointer to a #gnutls_anon_server_credentials_t structure.
 *
 * This structure is complex enough to manipulate directly thus this
 * helper function is provided in order to allocate it.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_anon_allocate_server_credentials(gnutls_anon_server_credentials_t *
					sc)
{

	*sc = gnutls_calloc(1, sizeof(anon_server_credentials_st));

	return 0;
}


/**
 * gnutls_anon_free_client_credentials:
 * @sc: is a #gnutls_anon_client_credentials_t structure.
 *
 * This structure is complex enough to manipulate directly thus this
 * helper function is provided in order to free (deallocate) it.
 **/
void
gnutls_anon_free_client_credentials(gnutls_anon_client_credentials_t sc)
{
}

static struct gnutls_anon_client_credentials_st anon_dummy_struct;
static const gnutls_anon_client_credentials_t anon_dummy =
    &anon_dummy_struct;

/**
 * gnutls_anon_allocate_client_credentials:
 * @sc: is a pointer to a #gnutls_anon_client_credentials_t structure.
 *
 * This structure is complex enough to manipulate directly thus
 * this helper function is provided in order to allocate it.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_anon_allocate_client_credentials(gnutls_anon_client_credentials_t *
					sc)
{
	/* anon_dummy is only there for *sc not to be null.
	 * it is not used at all;
	 */
	*sc = anon_dummy;

	return 0;
}

/**
 * gnutls_anon_set_server_dh_params:
 * @res: is a gnutls_anon_server_credentials_t structure
 * @dh_params: is a structure that holds Diffie-Hellman parameters.
 *
 * This function will set the Diffie-Hellman parameters for an
 * anonymous server to use.  These parameters will be used in
 * Anonymous Diffie-Hellman cipher suites.
 **/
void
gnutls_anon_set_server_dh_params(gnutls_anon_server_credentials_t res,
				 gnutls_dh_params_t dh_params)
{
	res->dh_params = dh_params;
}

/**
 * gnutls_anon_set_server_params_function:
 * @res: is a gnutls_certificate_credentials_t structure
 * @func: is the function to be called
 *
 * This function will set a callback in order for the server to get
 * the Diffie-Hellman parameters for anonymous authentication.  The
 * callback should return %GNUTLS_E_SUCCESS (0) on success.
 **/
void
gnutls_anon_set_server_params_function(gnutls_anon_server_credentials_t
				       res, gnutls_params_function * func)
{
	res->params_func = func;
}

#endif
