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

#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include <gnutls_errors.h>

int _gnutls_send_handshake(gnutls_session_t session, mbuffer_st * bufel,
			   gnutls_handshake_description_t type);
int _gnutls_recv_hello_request(gnutls_session_t session, void *data,
			       uint32_t data_size);
int _gnutls_recv_handshake(gnutls_session_t session,
			   gnutls_handshake_description_t type,
			   unsigned int optional, gnutls_buffer_st * buf);
int _gnutls_generate_session_id(uint8_t * session_id, uint8_t * len);
int _gnutls_set_server_random(gnutls_session_t session, uint8_t * rnd);
int _gnutls_set_client_random(gnutls_session_t session, uint8_t * rnd);

int _gnutls_find_pk_algos_in_ciphersuites(uint8_t * data, int datalen);
int _gnutls_server_select_suite(gnutls_session_t session, uint8_t * data,
				unsigned int datalen);

int _gnutls_negotiate_version(gnutls_session_t session,
			      gnutls_protocol_t adv_version);
int _gnutls_user_hello_func(gnutls_session_t session,
			    gnutls_protocol_t adv_version);

void _gnutls_handshake_hash_buffers_clear(gnutls_session_t session);

#define STATE session->internals.handshake_state
#define FINAL_STATE session->internals.handshake_final_state
/* This returns true if we have got there
 * before (and not finished due to an interrupt).
 */
#define AGAIN(target) (STATE==target?1:0)
#define FAGAIN(target) (FINAL_STATE==target?1:0)
#define AGAIN2(state, target) (state==target?1:0)

inline static int handshake_remaining_time(gnutls_session_t session)
{
	if (session->internals.handshake_endtime) {
		struct timespec now;
		gettime(&now);

		if (now.tv_sec < session->internals.handshake_endtime)
			return (session->internals.handshake_endtime -
				now.tv_sec) * 1000;
		else
			return gnutls_assert_val(GNUTLS_E_TIMEDOUT);
	}
	return 0;
}

#endif
