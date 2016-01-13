/*
 * Copyright (C) 2009-2012 Free Software Foundation, Inc.
 *
 * Author: Daiki Ueno
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

#include <gnutls_int.h>
#include <gnutls_errors.h>
#include <gnutls_datum.h>
#include <algorithms.h>
#include <gnutls_handshake.h>
#include <gnutls_num.h>
#include <gnutls_constate.h>
#include <gnutls_session_pack.h>
#include <random.h>
#include <ext/session_ticket.h>
#include <gnutls_mbuffers.h>
#include <gnutls_extensions.h>
#include <gnutls_constate.h>
#include <gnutls_dtls.h>

#ifdef ENABLE_SESSION_TICKETS

#define KEY_NAME_SIZE SESSION_TICKET_KEY_NAME_SIZE
#define KEY_SIZE SESSION_TICKET_KEY_SIZE
#define IV_SIZE 12 /* GCM */

#define TAG_SIZE 16 /* GCM */

static int session_ticket_recv_params(gnutls_session_t session,
				      const uint8_t * data,
				      size_t data_size);
static int session_ticket_send_params(gnutls_session_t session,
				      gnutls_buffer_st * extdata);
static int session_ticket_unpack(gnutls_buffer_st * ps,
				 extension_priv_data_t * _priv);
static int session_ticket_pack(extension_priv_data_t _priv,
			       gnutls_buffer_st * ps);
static void session_ticket_deinit_data(extension_priv_data_t priv);

extension_entry_st ext_mod_session_ticket = {
	.name = "SESSION TICKET",
	.type = GNUTLS_EXTENSION_SESSION_TICKET,
	.parse_type = GNUTLS_EXT_MANDATORY,

	.recv_func = session_ticket_recv_params,
	.send_func = session_ticket_send_params,
	.pack_func = session_ticket_pack,
	.unpack_func = session_ticket_unpack,
	.deinit_func = session_ticket_deinit_data,
};

#define SESSION_KEY_SIZE (SESSION_TICKET_KEY_NAME_SIZE+SESSION_TICKET_KEY_SIZE)
#define NAME_POS (0)
#define KEY_POS (SESSION_TICKET_KEY_NAME_SIZE)

typedef struct {
	int session_ticket_enable;
	int session_ticket_renew;

	uint8_t *session_ticket;
	int session_ticket_len;

	uint8_t key[SESSION_KEY_SIZE];
} session_ticket_ext_st;

struct ticket_st {
	uint8_t key_name[KEY_NAME_SIZE];
	uint8_t IV[IV_SIZE];
	uint8_t *encrypted_state;
	uint16_t encrypted_state_len;
	uint8_t tag[TAG_SIZE];
};

static int
decrypt_ticket(gnutls_session_t session, session_ticket_ext_st * priv,
	       struct ticket_st *ticket)
{
	cipher_hd_st cipher_hd;
	gnutls_datum_t key, IV, state;
	uint8_t final[TAG_SIZE];
	time_t timestamp = gnutls_time(0);
	int ret;

	/* Decrypt encrypted_state using 128-bit AES in CBC mode. */
	key.data = (void *) &priv->key[KEY_POS];
	key.size = KEY_SIZE;
	IV.data = ticket->IV;
	IV.size = IV_SIZE;
	ret =
	    _gnutls_cipher_init(&cipher_hd,
				cipher_to_entry(GNUTLS_CIPHER_AES_128_GCM),
				&key, &IV, 0);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}
	ret = _gnutls_cipher_decrypt(&cipher_hd, ticket->encrypted_state,
				     ticket->encrypted_state_len);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}
	
	_gnutls_cipher_tag(&cipher_hd, final, TAG_SIZE);
	if (memcmp(ticket->tag, final, TAG_SIZE) != 0) {
		gnutls_assert();
		ret = GNUTLS_E_DECRYPTION_FAILED;
		goto cleanup;
	}

	/* Unpack security parameters. */
	state.data = ticket->encrypted_state;
	state.size = ticket->encrypted_state_len;
	ret = _gnutls_session_unpack(session, &state);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	if (timestamp -
	    session->internals.resumed_security_parameters.timestamp >
	    session->internals.expire_time
	    || session->internals.resumed_security_parameters.timestamp >
	    timestamp) {
		gnutls_assert();
		ret = GNUTLS_E_EXPIRED;
		goto cleanup;
	}

	session->internals.resumed = RESUME_TRUE;

	ret = 0;
cleanup:
        _gnutls_cipher_deinit(&cipher_hd);
        
        return ret;

}

static int
encrypt_ticket(gnutls_session_t session, session_ticket_ext_st * priv,
	       struct ticket_st *ticket)
{
	cipher_hd_st cipher_hd;
	gnutls_datum_t key, IV;
	gnutls_datum_t state = {NULL,0}, encrypted_state = {NULL,0};
	uint8_t iv[IV_SIZE];
	uint32_t t;
	int ret;

	/* Pack security parameters. */
	ret = _gnutls_session_pack(session, &state);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	encrypted_state.size = state.size;
	encrypted_state.data = gnutls_malloc(encrypted_state.size);
	if (!encrypted_state.data) {
		gnutls_assert();
		ret = GNUTLS_E_MEMORY_ERROR;
		goto cleanup;
	}
	memset(encrypted_state.data, 0, encrypted_state.size);
	memcpy(encrypted_state.data, state.data, state.size);

	/* Encrypt state using 128-bit AES in GCM mode. */
	key.data = (void *) &priv->key[KEY_POS];
	key.size = KEY_SIZE;
	IV.data = iv;
	IV.size = IV_SIZE;
	
	t = gnutls_time(0);
	memcpy(iv, &t, 4);
	ret = _gnutls_rnd(GNUTLS_RND_NONCE, iv+4, IV_SIZE-4);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret =
	    _gnutls_cipher_init(&cipher_hd,
				cipher_to_entry(GNUTLS_CIPHER_AES_128_GCM),
				&key, &IV, 1);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = _gnutls_cipher_encrypt(&cipher_hd, encrypted_state.data,
				     encrypted_state.size);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup2;
	}
	
	_gnutls_cipher_tag(&cipher_hd, ticket->tag, TAG_SIZE);

	/* Fill the ticket structure to compute MAC. */
	memcpy(ticket->key_name, &priv->key[NAME_POS], KEY_NAME_SIZE);
	memcpy(ticket->IV, IV.data, IV.size);
	ticket->encrypted_state_len = encrypted_state.size;
	ticket->encrypted_state = encrypted_state.data;
	
	encrypted_state.data = NULL;

	ret = 0;

cleanup2:
	_gnutls_cipher_deinit(&cipher_hd);

cleanup:
	_gnutls_free_datum(&state);
	_gnutls_free_datum(&encrypted_state);

	return ret;
}

static int
session_ticket_recv_params(gnutls_session_t session,
			   const uint8_t * data, size_t _data_size)
{
	ssize_t data_size = _data_size;
	session_ticket_ext_st *priv = NULL;
	extension_priv_data_t epriv;
	int ret;

	ret =
	    _gnutls_ext_get_session_data(session,
					 GNUTLS_EXTENSION_SESSION_TICKET,
					 &epriv);
	if (ret < 0) {
		return 0;
	}
	priv = epriv.ptr;

	if (!priv->session_ticket_enable)
		return 0;

	if (session->security_parameters.entity == GNUTLS_SERVER) {
		struct ticket_st ticket;
		const uint8_t *encrypted_state;
		int ret;

		/* The client requested a new session ticket. */
		if (data_size == 0) {
			priv->session_ticket_renew = 1;
			return 0;
		}

		DECR_LEN(data_size, KEY_NAME_SIZE);
		memcpy(ticket.key_name, data, KEY_NAME_SIZE);
		data += KEY_NAME_SIZE;

		/* If the key name of the ticket does not match the one that we
		   hold, issue a new ticket. */
		if (memcmp
		    (ticket.key_name, &priv->key[NAME_POS],
		     KEY_NAME_SIZE)) {
			priv->session_ticket_renew = 1;
			return 0;
		}

		DECR_LEN(data_size, IV_SIZE);
		memcpy(ticket.IV, data, IV_SIZE);
		data += IV_SIZE;

		DECR_LEN(data_size, 2);
		ticket.encrypted_state_len = _gnutls_read_uint16(data);
		data += 2;

		encrypted_state = data;

		DECR_LEN(data_size, ticket.encrypted_state_len);
		data += ticket.encrypted_state_len;

		DECR_LEN(data_size, TAG_SIZE);
		memcpy(ticket.tag, data, TAG_SIZE);

		ticket.encrypted_state =
		    gnutls_malloc(ticket.encrypted_state_len);
		if (!ticket.encrypted_state) {
			gnutls_assert();
			return GNUTLS_E_MEMORY_ERROR;
		}
		memcpy(ticket.encrypted_state, encrypted_state,
		       ticket.encrypted_state_len);

		ret = decrypt_ticket(session, priv, &ticket);
		gnutls_free(ticket.encrypted_state);
		if (ret < 0) {
			priv->session_ticket_renew = 1;
			return 0;
		}
	} else {		/* Client */

		if (data_size == 0) {
			priv->session_ticket_renew = 1;
			return 0;
		}
	}

	return 0;
}

/* returns a positive number if we send the extension data, (0) if we
   do not want to send it, and a negative number on failure.
 */
static int
session_ticket_send_params(gnutls_session_t session,
			   gnutls_buffer_st * extdata)
{
	session_ticket_ext_st *priv = NULL;
	extension_priv_data_t epriv;
	int ret;

	ret =
	    _gnutls_ext_get_session_data(session,
					 GNUTLS_EXTENSION_SESSION_TICKET,
					 &epriv);
	if (ret >= 0)
		priv = epriv.ptr;

	if (priv == NULL || !priv->session_ticket_enable)
		return 0;

	if (session->security_parameters.entity == GNUTLS_SERVER) {
		if (priv && priv->session_ticket_renew) {
			return GNUTLS_E_INT_RET_0;
		}
	} else {
		ret =
		    _gnutls_ext_get_resumed_session_data(session,
							 GNUTLS_EXTENSION_SESSION_TICKET,
							 &epriv);
		if (ret >= 0)
			priv = epriv.ptr;

		/* no previous data. Just advertize it */
		if (ret < 0)
			return GNUTLS_E_INT_RET_0;

		/* previous data had session tickets disabled. Don't advertize. Ignore. */
		if (!priv->session_ticket_enable)
			return 0;

		if (priv->session_ticket_len > 0) {
			ret =
			    _gnutls_buffer_append_data(extdata,
						       priv->
						       session_ticket,
						       priv->
						       session_ticket_len);
			if (ret < 0)
				return gnutls_assert_val(ret);

			return priv->session_ticket_len;
		}
	}
	return 0;
}


static void session_ticket_deinit_data(extension_priv_data_t epriv)
{
	session_ticket_ext_st *priv = epriv.ptr;

	gnutls_free(priv->session_ticket);
	gnutls_free(priv);
}

static int
session_ticket_pack(extension_priv_data_t epriv, gnutls_buffer_st * ps)
{
	session_ticket_ext_st *priv = epriv.ptr;
	int ret;

	BUFFER_APPEND_PFX4(ps, priv->session_ticket,
			   priv->session_ticket_len);
	BUFFER_APPEND_NUM(ps, priv->session_ticket_enable);

	return 0;
}

static int
session_ticket_unpack(gnutls_buffer_st * ps, extension_priv_data_t * _priv)
{
	session_ticket_ext_st *priv = NULL;
	int ret;
	extension_priv_data_t epriv;
	gnutls_datum_t ticket;

	priv = gnutls_calloc(1, sizeof(*priv));
	if (priv == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	BUFFER_POP_DATUM(ps, &ticket);
	priv->session_ticket = ticket.data;
	priv->session_ticket_len = ticket.size;
	BUFFER_POP_NUM(ps, priv->session_ticket_enable);

	epriv.ptr = priv;
	*_priv = epriv;

	return 0;

      error:
	gnutls_free(priv);
	return ret;
}



/**
 * gnutls_session_ticket_key_generate:
 * @key: is a pointer to a #gnutls_datum_t which will contain a newly
 * created key.
 *
 * Generate a random key to encrypt security parameters within
 * SessionTicket.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, or an
 * error code.
 *
 * Since: 2.10.0
 **/
int gnutls_session_ticket_key_generate(gnutls_datum_t * key)
{
	return gnutls_key_generate(key, SESSION_KEY_SIZE);
}

/**
 * gnutls_session_ticket_enable_client:
 * @session: is a #gnutls_session_t structure.
 *
 * Request that the client should attempt session resumption using
 * SessionTicket.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, or an
 * error code.
 *
 * Since: 2.10.0
 **/
int gnutls_session_ticket_enable_client(gnutls_session_t session)
{
	session_ticket_ext_st *priv = NULL;
	extension_priv_data_t epriv;

	if (!session) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	priv = gnutls_calloc(1, sizeof(*priv));
	if (priv == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}
	priv->session_ticket_enable = 1;
	epriv.ptr = priv;

	_gnutls_ext_set_session_data(session,
				     GNUTLS_EXTENSION_SESSION_TICKET,
				     epriv);

	return 0;
}

/**
 * gnutls_session_ticket_enable_server:
 * @session: is a #gnutls_session_t structure.
 * @key: key to encrypt session parameters.
 *
 * Request that the server should attempt session resumption using
 * SessionTicket.  @key must be initialized with
 * gnutls_session_ticket_key_generate().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, or an
 * error code.
 *
 * Since: 2.10.0
 **/
int
gnutls_session_ticket_enable_server(gnutls_session_t session,
				    const gnutls_datum_t * key)
{
	session_ticket_ext_st *priv = NULL;
	extension_priv_data_t epriv;

	if (!session || !key || key->size != SESSION_KEY_SIZE) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	priv = gnutls_calloc(1, sizeof(*priv));
	if (priv == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}
	epriv.ptr = priv;

	memcpy(&priv->key, key->data, key->size);
	priv->session_ticket_enable = 1;

	_gnutls_ext_set_session_data(session,
				     GNUTLS_EXTENSION_SESSION_TICKET,
				     epriv);

	return 0;
}

int _gnutls_send_new_session_ticket(gnutls_session_t session, int again)
{
	mbuffer_st *bufel = NULL;
	uint8_t *data = NULL, *p;
	int data_size = 0;
	int ret;
	struct ticket_st ticket;
	uint16_t ticket_len;
	session_ticket_ext_st *priv = NULL;
	extension_priv_data_t epriv;
	uint16_t epoch_saved = session->security_parameters.epoch_write;

	if (again == 0) {
		ret =
		    _gnutls_ext_get_session_data(session,
						 GNUTLS_EXTENSION_SESSION_TICKET,
						 &epriv);
		if (ret < 0)
			return 0;
		priv = epriv.ptr;

		if (!priv->session_ticket_renew)
			return 0;

		/* XXX: Temporarily set write algorithms to be used.
		   _gnutls_write_connection_state_init() does this job, but it also
		   triggers encryption, while NewSessionTicket should not be
		   encrypted in the record layer. */
		ret =
		    _gnutls_epoch_set_keys(session,
					   session->security_parameters.
					   epoch_next);
		if (ret < 0) {
			gnutls_assert();
			return ret;
		}

		session->security_parameters.epoch_write =
		    session->security_parameters.epoch_next;

		ret = encrypt_ticket(session, priv, &ticket);
		session->security_parameters.epoch_write = epoch_saved;
		if (ret < 0) {
			gnutls_assert();
			return ret;
		}

		ticket_len =
		    KEY_NAME_SIZE + IV_SIZE + 2 +
		    ticket.encrypted_state_len + TAG_SIZE;

		bufel =
		    _gnutls_handshake_alloc(session, 
					    4 + 2 + ticket_len);
		if (!bufel) {
			gnutls_assert();
			gnutls_free(ticket.encrypted_state);
			return GNUTLS_E_MEMORY_ERROR;
		}

		data = _mbuffer_get_udata_ptr(bufel);
		p = data;

		_gnutls_write_uint32(session->internals.expire_time, p);
		p += 4;

		_gnutls_write_uint16(ticket_len, p);
		p += 2;

		memcpy(p, ticket.key_name, KEY_NAME_SIZE);
		p += KEY_NAME_SIZE;

		memcpy(p, ticket.IV, IV_SIZE);
		p += IV_SIZE;

		_gnutls_write_uint16(ticket.encrypted_state_len, p);
		p += 2;

		memcpy(p, ticket.encrypted_state, ticket.encrypted_state_len);
		gnutls_free(ticket.encrypted_state);
		p += ticket.encrypted_state_len;

		memcpy(p, ticket.tag, TAG_SIZE);
		p += TAG_SIZE;

		data_size = p - data;

		session->internals.ticket_sent = 1;
	}
	return _gnutls_send_handshake(session, data_size ? bufel : NULL,
				      GNUTLS_HANDSHAKE_NEW_SESSION_TICKET);
}

int _gnutls_recv_new_session_ticket(gnutls_session_t session)
{
	uint8_t *p;
	int data_size;
	gnutls_buffer_st buf;
	uint16_t ticket_len;
	int ret;
	session_ticket_ext_st *priv = NULL;
	extension_priv_data_t epriv;

	ret =
	    _gnutls_ext_get_session_data(session,
					 GNUTLS_EXTENSION_SESSION_TICKET,
					 &epriv);
	if (ret < 0) {
		gnutls_assert();
		return 0;
	}
	priv = epriv.ptr;

	if (!priv->session_ticket_renew)
		return 0;

	/* This is the last flight and peer cannot be sure
	 * we have received it unless we notify him. So we
	 * wait for a message and retransmit if needed. */
	if (IS_DTLS(session) && !_dtls_is_async(session) &&
	    (gnutls_record_check_pending(session) +
	     record_check_unprocessed(session)) == 0) {
		ret = _dtls_wait_and_retransmit(session);
		if (ret < 0)
			return gnutls_assert_val(ret);
	}

	ret = _gnutls_recv_handshake(session,
				     GNUTLS_HANDSHAKE_NEW_SESSION_TICKET,
				     0, &buf);
	if (ret < 0)
		return gnutls_assert_val_fatal(ret);

	p = buf.data;
	data_size = buf.length;

	DECR_LENGTH_COM(data_size, 4, ret =
			GNUTLS_E_UNEXPECTED_PACKET_LENGTH;
			goto error);
	/* skip over lifetime hint */
	p += 4;

	DECR_LENGTH_COM(data_size, 2, ret =
			GNUTLS_E_UNEXPECTED_PACKET_LENGTH;
			goto error);
	ticket_len = _gnutls_read_uint16(p);
	p += 2;

	DECR_LENGTH_COM(data_size, ticket_len, ret =
			GNUTLS_E_UNEXPECTED_PACKET_LENGTH;
			goto error);
	priv->session_ticket =
	    gnutls_realloc_fast(priv->session_ticket, ticket_len);
	if (!priv->session_ticket) {
		gnutls_assert();
		ret = GNUTLS_E_MEMORY_ERROR;
		goto error;
	}
	memcpy(priv->session_ticket, p, ticket_len);
	priv->session_ticket_len = ticket_len;

	/* Discard the current session ID.  (RFC5077 3.4) */
	ret =
	    _gnutls_generate_session_id(session->security_parameters.
					session_id,
					&session->security_parameters.
					session_id_size);
	if (ret < 0) {
		gnutls_assert();
		gnutls_free(priv->session_ticket);
		priv->session_ticket = NULL;
		ret = GNUTLS_E_INTERNAL_ERROR;
		goto error;
	}
	ret = 0;

      error:
	_gnutls_buffer_clear(&buf);

	return ret;
}

#endif
