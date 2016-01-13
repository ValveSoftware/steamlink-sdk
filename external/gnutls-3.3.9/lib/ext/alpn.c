/*
 * Copyright (C) 2013 Nikos Mavrogiannopoulos
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
#include "gnutls_auth.h"
#include "gnutls_errors.h"
#include "gnutls_num.h"
#include <ext/alpn.h>

static int _gnutls_alpn_recv_params(gnutls_session_t session,
				    const uint8_t * data,
				    size_t data_size);
static int _gnutls_alpn_send_params(gnutls_session_t session,
				    gnutls_buffer_st * extdata);

static int _gnutls_alpn_unpack(gnutls_buffer_st * ps,
			       extension_priv_data_t * _priv);
static int _gnutls_alpn_pack(extension_priv_data_t _priv,
			     gnutls_buffer_st * ps);
static void _gnutls_alpn_deinit_data(extension_priv_data_t priv);


extension_entry_st ext_mod_alpn = {
	.name = "ALPN",
	.type = GNUTLS_EXTENSION_ALPN,
	.parse_type = GNUTLS_EXT_APPLICATION,

	.recv_func = _gnutls_alpn_recv_params,
	.send_func = _gnutls_alpn_send_params,
	.pack_func = _gnutls_alpn_pack,
	.unpack_func = _gnutls_alpn_unpack,
	.deinit_func = _gnutls_alpn_deinit_data,
};

static int
_gnutls_alpn_recv_params(gnutls_session_t session,
			 const uint8_t * data, size_t _data_size)
{
	unsigned int i;
	int ret;
	const uint8_t *p = data;
	unsigned len1, len;
	ssize_t data_size = _data_size;
	alpn_ext_st *priv;
	extension_priv_data_t epriv;

	ret =
	    _gnutls_ext_get_session_data(session, GNUTLS_EXTENSION_ALPN,
					 &epriv);
	if (ret < 0)
		return 0;

	priv = epriv.ptr;

	DECR_LENGTH_RET(data_size, 2, 0);
	len = _gnutls_read_uint16(p);
	p += 2;

	if (len > data_size)
		return
		    gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET_LENGTH);

	if (session->security_parameters.entity == GNUTLS_SERVER) {
		while (data_size > 0) {
			DECR_LENGTH_RET(data_size, 1, 0);
			len1 = *p;
			p += 1;
			DECR_LENGTH_RET(data_size, len1, 0);

			for (i = 0; i < priv->size; i++)
				if (priv->protocol_size[i] == len1
				    && memcmp(p, priv->protocols[i],
					      len1) == 0) {
					priv->selected_protocol =
					    priv->protocols[i];
					priv->selected_protocol_size =
					    priv->protocol_size[i];
					break;
				}
			p += len1;
		}
	} else {
		DECR_LENGTH_RET(data_size, 1, 0);
		len1 = *p;
		p += 1;
		DECR_LENGTH_RET(data_size, len1, 0);

		for (i = 0; i < priv->size; i++)
			if (priv->protocol_size[i] == len1
			    && memcmp(p, priv->protocols[i], len1) == 0) {
				priv->selected_protocol =
				    priv->protocols[i];
				priv->selected_protocol_size =
				    priv->protocol_size[i];
				break;
			}
		p += len1;
	}

	if (priv->selected_protocol == NULL
	    && (priv->flags & GNUTLS_ALPN_MAND))
		return gnutls_assert_val(GNUTLS_E_NO_APPLICATION_PROTOCOL);

	return 0;
}

static int
_gnutls_alpn_send_params(gnutls_session_t session,
			 gnutls_buffer_st * extdata)
{
	unsigned i;
	int total_size = 0, ret;
	alpn_ext_st *priv;
	extension_priv_data_t epriv;

	ret =
	    _gnutls_ext_get_session_data(session, GNUTLS_EXTENSION_ALPN,
					 &epriv);
	if (ret < 0)
		return 0;

	priv = epriv.ptr;

	if (priv->size == 0)
		return 0;

	if (session->security_parameters.entity == GNUTLS_SERVER) {
		if (priv->selected_protocol_size == 0)
			return 0;

		ret =
		    _gnutls_buffer_append_prefix(extdata, 16,
						 priv->
						 selected_protocol_size +
						 1);
		if (ret < 0)
			return gnutls_assert_val(ret);

		total_size += 2;

		ret =
		    _gnutls_buffer_append_data_prefix(extdata, 8,
						      priv->
						      selected_protocol,
						      priv->
						      selected_protocol_size);
		if (ret < 0)
			return gnutls_assert_val(ret);

		total_size += 1 + priv->selected_protocol_size;
	} else {
		int t = 0;
		for (i = 0; i < priv->size; i++)
			t += priv->protocol_size[i] + 1;

		ret = _gnutls_buffer_append_prefix(extdata, 16, t);
		if (ret < 0)
			return gnutls_assert_val(ret);

		total_size += 2;

		for (i = 0; i < priv->size; i++) {
			ret =
			    _gnutls_buffer_append_data_prefix(extdata, 8,
							      priv->
							      protocols[i],
							      priv->
							      protocol_size
							      [i]);
			if (ret < 0)
				return gnutls_assert_val(ret);

			total_size += 1 + priv->protocol_size[i];
		}
	}

	return total_size;
}

/**
 * gnutls_alpn_get_selected_protocol:
 * @session: is a #gnutls_session_t structure.
 * @protocol: will hold the protocol name
 *
 * This function allows you to get the negotiated protocol name. The
 * returned protocol should be treated as opaque, constant value and
 * only valid during the session life.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned,
 *   otherwise a negative error code is returned.
 *
 * Since 3.2.0
 **/
int
gnutls_alpn_get_selected_protocol(gnutls_session_t session,
				  gnutls_datum_t * protocol)
{
	alpn_ext_st *priv;
	int ret;
	extension_priv_data_t epriv;

	ret =
	    _gnutls_ext_get_session_data(session, GNUTLS_EXTENSION_ALPN,
					 &epriv);
	if (ret < 0) {
		gnutls_assert();
		return GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;
	}

	priv = epriv.ptr;

	if (priv->selected_protocol_size == 0)
		return
		    gnutls_assert_val
		    (GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE);

	protocol->data = priv->selected_protocol;
	protocol->size = priv->selected_protocol_size;

	return 0;
}

/**
 * gnutls_alpn_set_protocols:
 * @session: is a #gnutls_session_t structure.
 * @protocols: is the protocol names to add.
 * @protocols_size: the number of protocols to add.
 * @flags: zero or %GNUTLS_ALPN_*
 *
 * This function is to be used by both clients and servers, to declare
 * the supported ALPN protocols, which are used during peer negotiation.
 *
 * If %GNUTLS_ALPN_MAND is specified the connection will be aborted
 * if no matching ALPN protocol is found.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned,
 *   otherwise a negative error code is returned.
 *
 * Since 3.2.0
 **/
int
gnutls_alpn_set_protocols(gnutls_session_t session,
			  const gnutls_datum_t * protocols,
			  unsigned protocols_size, unsigned int flags)
{
	int ret;
	alpn_ext_st *priv;
	extension_priv_data_t epriv;
	unsigned i;

	ret =
	    _gnutls_ext_get_session_data(session, GNUTLS_EXTENSION_ALPN,
					 &epriv);
	if (ret < 0) {
		priv = gnutls_calloc(1, sizeof(*priv));
		if (priv == NULL) {
			gnutls_assert();
			return GNUTLS_E_MEMORY_ERROR;
		}
		epriv.ptr = priv;
		_gnutls_ext_set_session_data(session,
					     GNUTLS_EXTENSION_ALPN, epriv);
	} else
		priv = epriv.ptr;

	if (protocols_size > MAX_ALPN_PROTOCOLS)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	for (i = 0; i < protocols_size; i++) {
		if (protocols[i].size >= MAX_ALPN_PROTOCOL_NAME)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		memcpy(priv->protocols[i], protocols[i].data,
		       protocols[i].size);
		priv->protocol_size[i] = protocols[i].size;
		priv->size++;
	}
	priv->flags = flags;

	return 0;
}


static void _gnutls_alpn_deinit_data(extension_priv_data_t priv)
{
	gnutls_free(priv.ptr);
}

static int
_gnutls_alpn_pack(extension_priv_data_t epriv, gnutls_buffer_st * ps)
{
	alpn_ext_st *priv = epriv.ptr;
	int ret;

	BUFFER_APPEND_PFX4(ps, priv->selected_protocol,
			   priv->selected_protocol_size);

	return 0;
}

static int
_gnutls_alpn_unpack(gnutls_buffer_st * ps, extension_priv_data_t * _priv)
{
	alpn_ext_st *priv;
	int ret;
	extension_priv_data_t epriv;

	priv = gnutls_calloc(1, sizeof(*priv));
	if (priv == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	BUFFER_POP_NUM(ps, priv->protocol_size[0]);
	BUFFER_POP(ps, &priv->protocols[0], priv->protocol_size[0]);
	priv->size++;
	priv->selected_protocol_size = priv->protocol_size[0];
	priv->selected_protocol = priv->protocols[0];

	epriv.ptr = priv;
	*_priv = epriv;

	return 0;

      error:
	gnutls_free(priv);
	return ret;
}
