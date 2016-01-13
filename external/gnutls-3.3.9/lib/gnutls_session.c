/*
 * Copyright (C) 2000-2012 Free Software Foundation, Inc.
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
#include "gnutls_errors.h"
#include "debug.h"
#include <gnutls_session_pack.h>
#include <gnutls_datum.h>

/**
 * gnutls_session_get_data:
 * @session: is a #gnutls_session_t structure.
 * @session_data: is a pointer to space to hold the session.
 * @session_data_size: is the session_data's size, or it will be set by the function.
 *
 * Returns all session parameters needed to be stored to support resumption.
 * The client should call this, and store the returned session data. A session
 * may be resumed later by calling gnutls_session_set_data().  
 * This function must be called after a successful handshake. 
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 **/
int
gnutls_session_get_data(gnutls_session_t session,
			void *session_data, size_t * session_data_size)
{

	gnutls_datum_t psession;
	int ret;

	if (session->internals.resumable == RESUME_FALSE)
		return GNUTLS_E_INVALID_SESSION;

	psession.data = session_data;

	ret = _gnutls_session_pack(session, &psession);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	if (psession.size > *session_data_size) {
		*session_data_size = psession.size;
		ret = GNUTLS_E_SHORT_MEMORY_BUFFER;
		goto error;
	}
	*session_data_size = psession.size;

	if (session_data != NULL)
		memcpy(session_data, psession.data, psession.size);

	ret = 0;

      error:
	_gnutls_free_datum(&psession);
	return ret;
}

/**
 * gnutls_session_get_data2:
 * @session: is a #gnutls_session_t structure.
 * @data: is a pointer to a datum that will hold the session.
 *
 * Returns all session parameters needed to be stored to support resumption.
 * The client should call this, and store the returned session data. A session
 * may be resumed later by calling gnutls_session_set_data().  
 * This function must be called after a successful handshake. 
 * The returned @data are allocated and must be released using gnutls_free().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 **/
int
gnutls_session_get_data2(gnutls_session_t session, gnutls_datum_t * data)
{

	int ret;

	if (data == NULL) {
		return GNUTLS_E_INVALID_REQUEST;
	}

	if (session->internals.resumable == RESUME_FALSE)
		return GNUTLS_E_INVALID_SESSION;

	ret = _gnutls_session_pack(session, data);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return 0;
}


/**
 * gnutls_session_get_id:
 * @session: is a #gnutls_session_t structure.
 * @session_id: is a pointer to space to hold the session id.
 * @session_id_size: initially should contain the maximum @session_id size and will be updated.
 *
 * Returns the current session ID. This can be used if you want to
 * check if the next session you tried to resume was actually
 * resumed.  That is because resumed sessions share the same session ID
 * with the original session.
 *
 * The session ID is selected by the server, that identify the
 * current session.  In TLS 1.0 and SSL 3.0 session id is always less
 * than 32 bytes.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 **/
int
gnutls_session_get_id(gnutls_session_t session,
		      void *session_id, size_t * session_id_size)
{
	size_t given_session_id_size = *session_id_size;

	*session_id_size = session->security_parameters.session_id_size;

	/* just return the session size */
	if (session_id == NULL) {
		return 0;
	}

	if (given_session_id_size <
	    session->security_parameters.session_id_size) {
		return GNUTLS_E_SHORT_MEMORY_BUFFER;
	}

	memcpy(session_id, &session->security_parameters.session_id,
	       *session_id_size);

	return 0;
}

/**
 * gnutls_session_get_id2:
 * @session: is a #gnutls_session_t structure.
 * @session_id: will point to the session ID.
 *
 * Returns the current session ID. The returned data should be
 * treated as constant.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 *
 * Since: 3.1.4
 **/
int
gnutls_session_get_id2(gnutls_session_t session,
		       gnutls_datum_t * session_id)
{
	session_id->size = session->security_parameters.session_id_size;
	session_id->data = session->security_parameters.session_id;

	return 0;
}

/**
 * gnutls_session_set_data:
 * @session: is a #gnutls_session_t structure.
 * @session_data: is a pointer to space to hold the session.
 * @session_data_size: is the session's size
 *
 * Sets all session parameters, in order to resume a previously
 * established session.  The session data given must be the one
 * returned by gnutls_session_get_data().  This function should be
 * called before gnutls_handshake().
 *
 * Keep in mind that session resuming is advisory. The server may
 * choose not to resume the session, thus a full handshake will be
 * performed.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 **/
int
gnutls_session_set_data(gnutls_session_t session,
			const void *session_data, size_t session_data_size)
{
	int ret;
	gnutls_datum_t psession;

	psession.data = (uint8_t *) session_data;
	psession.size = session_data_size;

	if (session_data == NULL || session_data_size == 0) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}
	ret = _gnutls_session_unpack(session, &psession);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	session->internals.resumption_requested = 1;

	return 0;
}

/**
 * gnutls_session_force_valid:
 * @session: is a #gnutls_session_t structure.
 *
 * Clears the invalid flag in a session. That means
 * that sessions were corrupt or invalid data were received 
 * can be re-used. Use only when debugging or experimenting
 * with the TLS protocol. Should not be used in typical
 * applications.
 *
 **/
void gnutls_session_force_valid(gnutls_session_t session)
{
	session->internals.invalid_connection = 0;
}
