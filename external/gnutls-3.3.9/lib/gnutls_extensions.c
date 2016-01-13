/*
 * Copyright (C) 2001-2012 Free Software Foundation, Inc.
 *
 * Author: Nikos Mavrogiannopoulos, Simon Josefsson
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

/* Functions that relate to the TLS hello extension parsing.
 * Hello extensions are packets appended in the TLS hello packet, and
 * allow for extra functionality.
 */

#include "gnutls_int.h"
#include "gnutls_extensions.h"
#include "gnutls_errors.h"
#include "ext/max_record.h"
#include <ext/cert_type.h>
#include <ext/server_name.h>
#include <ext/srp.h>
#include <ext/heartbeat.h>
#include <ext/session_ticket.h>
#include <ext/safe_renegotiation.h>
#include <ext/signature.h>
#include <ext/safe_renegotiation.h>
#include <ext/ecc.h>
#include <ext/status_request.h>
#include <ext/srtp.h>
#include <ext/alpn.h>
#include <ext/dumbfw.h>
#include <gnutls_num.h>


static void _gnutls_ext_unset_resumed_session_data(gnutls_session_t
						   session, uint16_t type);


static size_t extfunc_size = 0;
static extension_entry_st *extfunc = NULL;

static gnutls_ext_parse_type_t _gnutls_ext_parse_type(uint16_t type)
{
	size_t i;

	for (i = 0; i < extfunc_size; i++) {
		if (extfunc[i].type == type)
			return extfunc[i].parse_type;
	}

	return GNUTLS_EXT_NONE;
}

static gnutls_ext_recv_func
_gnutls_ext_func_recv(uint16_t type, gnutls_ext_parse_type_t parse_type)
{
	size_t i;

	for (i = 0; i < extfunc_size; i++)
		if (extfunc[i].type == type)
			if (parse_type == GNUTLS_EXT_ANY
			    || extfunc[i].parse_type == parse_type)
				return extfunc[i].recv_func;

	return NULL;
}

static gnutls_ext_deinit_data_func _gnutls_ext_func_deinit(uint16_t type)
{
	size_t i;

	for (i = 0; i < extfunc_size; i++)
		if (extfunc[i].type == type)
			return extfunc[i].deinit_func;

	return NULL;
}

static gnutls_ext_unpack_func _gnutls_ext_func_unpack(uint16_t type)
{
	size_t i;

	for (i = 0; i < extfunc_size; i++)
		if (extfunc[i].type == type)
			return extfunc[i].unpack_func;

	return NULL;
}


static const char *_gnutls_extension_get_name(uint16_t type)
{
	size_t i;

	for (i = 0; i < extfunc_size; i++)
		if (extfunc[i].type == type)
			return extfunc[i].name;

	return NULL;
}

/* Checks if the extension we just received is one of the 
 * requested ones. Otherwise it's a fatal error.
 */
static int
_gnutls_extension_list_check(gnutls_session_t session, uint16_t type)
{
	if (session->security_parameters.entity == GNUTLS_CLIENT) {
		int i;

		for (i = 0; i < session->internals.extensions_sent_size;
		     i++) {
			if (type == session->internals.extensions_sent[i])
				return 0;	/* ok found */
		}

		return GNUTLS_E_RECEIVED_ILLEGAL_EXTENSION;
	}

	return 0;
}

int
_gnutls_parse_extensions(gnutls_session_t session,
			 gnutls_ext_parse_type_t parse_type,
			 const uint8_t * data, int data_size)
{
	int next, ret;
	int pos = 0;
	uint16_t type;
	const uint8_t *sdata;
	gnutls_ext_recv_func ext_recv;
	uint16_t size;

#ifdef DEBUG
	int i;

	if (session->security_parameters.entity == GNUTLS_CLIENT)
		for (i = 0; i < session->internals.extensions_sent_size;
		     i++) {
			_gnutls_handshake_log
			    ("EXT[%d]: expecting extension '%s'\n",
			     session,
			     _gnutls_extension_get_name(session->internals.
							extensions_sent
							[i]));
		}
#endif

	DECR_LENGTH_RET(data_size, 2, 0);
	next = _gnutls_read_uint16(data);
	pos += 2;

	DECR_LENGTH_RET(data_size, next, 0);

	do {
		DECR_LENGTH_RET(next, 2, 0);
		type = _gnutls_read_uint16(&data[pos]);
		pos += 2;

		if ((ret =
		     _gnutls_extension_list_check(session, type)) < 0) {
			gnutls_assert();
			return ret;
		}

		DECR_LENGTH_RET(next, 2, 0);
		size = _gnutls_read_uint16(&data[pos]);
		pos += 2;

		DECR_LENGTH_RET(next, size, 0);
		sdata = &data[pos];
		pos += size;

		ext_recv = _gnutls_ext_func_recv(type, parse_type);
		if (ext_recv == NULL) {
			_gnutls_handshake_log
			    ("EXT[%p]: Found extension '%s/%d'\n", session,
			     _gnutls_extension_get_name(type), type);

			continue;
		}

		_gnutls_handshake_log
		    ("EXT[%p]: Parsing extension '%s/%d' (%d bytes)\n",
		     session, _gnutls_extension_get_name(type), type,
		     size);

		if ((ret = ext_recv(session, sdata, size)) < 0) {
			gnutls_assert();
			return ret;
		}

	}
	while (next > 2);

	return 0;

}

/* Adds the extension we want to send in the extensions list.
 * This list is used to check whether the (later) received
 * extensions are the ones we requested.
 */
void _gnutls_extension_list_add(gnutls_session_t session, uint16_t type)
{

	if (session->security_parameters.entity == GNUTLS_CLIENT) {
		if (session->internals.extensions_sent_size <
		    MAX_EXT_TYPES) {
			session->internals.extensions_sent[session->
							   internals.extensions_sent_size]
			    = type;
			session->internals.extensions_sent_size++;
		} else {
			_gnutls_handshake_log
			    ("extensions: Increase MAX_EXT_TYPES\n");
		}
	}
}

int
_gnutls_gen_extensions(gnutls_session_t session,
		       gnutls_buffer_st * extdata,
		       gnutls_ext_parse_type_t parse_type)
{
	int size;
	int pos, size_pos, ret;
	size_t i, init_size = extdata->length;

	pos = extdata->length;	/* we will store length later on */

	ret = _gnutls_buffer_append_prefix(extdata, 16, 0);
	if (ret < 0)
		return gnutls_assert_val(ret);

	for (i = 0; i < extfunc_size; i++) {
		extension_entry_st *p = &extfunc[i];

		if (p->send_func == NULL)
			continue;

		if (parse_type != GNUTLS_EXT_ANY
		    && p->parse_type != parse_type)
			continue;

		ret = _gnutls_buffer_append_prefix(extdata, 16, p->type);
		if (ret < 0)
			return gnutls_assert_val(ret);

		size_pos = extdata->length;
		ret = _gnutls_buffer_append_prefix(extdata, 16, 0);
		if (ret < 0)
			return gnutls_assert_val(ret);

		size = p->send_func(session, extdata);
		/* returning GNUTLS_E_INT_RET_0 means to send an empty
		 * extension of this type.
		 */
		if (size > 0 || size == GNUTLS_E_INT_RET_0) {
			if (size == GNUTLS_E_INT_RET_0)
				size = 0;

			/* write the real size */
			_gnutls_write_uint16(size,
					     &extdata->data[size_pos]);

			/* add this extension to the extension list
			 */
			_gnutls_extension_list_add(session, p->type);

			_gnutls_handshake_log
			    ("EXT[%p]: Sending extension %s (%d bytes)\n",
			     session, p->name, size);
		} else if (size < 0) {
			gnutls_assert();
			return size;
		} else if (size == 0)
			extdata->length -= 4;	/* reset type and size */
	}

	/* remove any initial data, and the size of the header */
	size = extdata->length - init_size - 2;

	if (size > 0)
		_gnutls_write_uint16(size, &extdata->data[pos]);
	else if (size == 0)
		extdata->length -= 2;	/* the length bytes */

	return size;
}

int _gnutls_ext_init(void)
{
	int ret;

	ret = _gnutls_ext_register(&ext_mod_max_record_size);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;

#ifdef ENABLE_OCSP
	ret = _gnutls_ext_register(&ext_mod_status_request);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;
#endif

#ifdef ENABLE_OPENPGP
	ret = _gnutls_ext_register(&ext_mod_cert_type);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;
#endif

	ret = _gnutls_ext_register(&ext_mod_server_name);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;

	ret = _gnutls_ext_register(&ext_mod_sr);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;

#ifdef ENABLE_SRP
	ret = _gnutls_ext_register(&ext_mod_srp);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;
#endif

#ifdef ENABLE_HEARTBEAT
	ret = _gnutls_ext_register(&ext_mod_heartbeat);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;
#endif

#ifdef ENABLE_SESSION_TICKETS
	ret = _gnutls_ext_register(&ext_mod_session_ticket);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;
#endif

	ret = _gnutls_ext_register(&ext_mod_supported_ecc);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;

	ret = _gnutls_ext_register(&ext_mod_supported_ecc_pf);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;

	ret = _gnutls_ext_register(&ext_mod_sig);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;

#ifdef ENABLE_DTLS_SRTP
	ret = _gnutls_ext_register(&ext_mod_srtp);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;
#endif

#ifdef ENABLE_ALPN
	ret = _gnutls_ext_register(&ext_mod_alpn);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;
#endif

	/* This must be the last extension registered.
	 */
	ret = _gnutls_ext_register(&ext_mod_dumbfw);
	if (ret != GNUTLS_E_SUCCESS)
		return ret;

	return GNUTLS_E_SUCCESS;
}

void _gnutls_ext_deinit(void)
{
	gnutls_free(extfunc);
	extfunc = NULL;
	extfunc_size = 0;
}

int _gnutls_ext_register(extension_entry_st * mod)
{
	extension_entry_st *p;

	p = gnutls_realloc_fast(extfunc,
				sizeof(*extfunc) * (extfunc_size + 1));
	if (!p) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	extfunc = p;

	memcpy(&extfunc[extfunc_size], mod, sizeof(*mod));

	extfunc_size++;

	return GNUTLS_E_SUCCESS;
}

int _gnutls_ext_before_epoch_change(gnutls_session_t session)
{
	unsigned int i;
	int ret;

	for (i = 0; i < extfunc_size; i++) {
		if (extfunc[i].epoch_func != NULL) {
			ret = extfunc[i].epoch_func(session);
			if (ret < 0)
				return gnutls_assert_val(ret);
		}
	}

	return 0;
}


int _gnutls_ext_pack(gnutls_session_t session, gnutls_buffer_st * packed)
{
	unsigned int i;
	int ret;
	extension_priv_data_t data;
	int cur_size;
	int size_offset;
	int total_exts_pos;
	int exts = 0;

	total_exts_pos = packed->length;
	BUFFER_APPEND_NUM(packed, 0);

	for (i = 0; i < extfunc_size; i++) {
		ret =
		    _gnutls_ext_get_session_data(session, extfunc[i].type,
						 &data);
		if (ret >= 0 && extfunc[i].pack_func != NULL) {
			BUFFER_APPEND_NUM(packed, extfunc[i].type);

			size_offset = packed->length;
			BUFFER_APPEND_NUM(packed, 0);

			cur_size = packed->length;

			ret = extfunc[i].pack_func(data, packed);
			if (ret < 0) {
				gnutls_assert();
				return ret;
			}

			exts++;
			/* write the actual size */
			_gnutls_write_uint32(packed->length - cur_size,
					     packed->data + size_offset);
		}
	}

	_gnutls_write_uint32(exts, packed->data + total_exts_pos);

	return 0;
}

void _gnutls_ext_restore_resumed_session(gnutls_session_t session)
{
	int i;


	/* clear everything except MANDATORY extensions */
	for (i = 0; i < MAX_EXT_TYPES; i++) {
		if (session->internals.extension_int_data[i].set != 0 &&
		    _gnutls_ext_parse_type(session->
					   internals.extension_int_data[i].
					   type) != GNUTLS_EXT_MANDATORY) {
			_gnutls_ext_unset_session_data(session,
						       session->internals.
						       extension_int_data
						       [i].type);
		}
	}

	/* copy resumed to main */
	for (i = 0; i < MAX_EXT_TYPES; i++) {
		if (session->internals.resumed_extension_int_data[i].set !=
		    0
		    && _gnutls_ext_parse_type(session->internals.
					      resumed_extension_int_data
					      [i].type) !=
		    GNUTLS_EXT_MANDATORY) {
			_gnutls_ext_set_session_data(session,
						     session->internals.
						     resumed_extension_int_data
						     [i].type,
						     session->internals.
						     resumed_extension_int_data
						     [i].priv);
			session->internals.resumed_extension_int_data[i].
			    set = 0;
		}
	}

}


static void
_gnutls_ext_set_resumed_session_data(gnutls_session_t session,
				     uint16_t type,
				     extension_priv_data_t data)
{
	int i;

	for (i = 0; i < MAX_EXT_TYPES; i++) {
		if (session->internals.resumed_extension_int_data[i].
		    type == type
		    || session->internals.resumed_extension_int_data[i].
		    set == 0) {

			if (session->internals.
			    resumed_extension_int_data[i].set != 0)
				_gnutls_ext_unset_resumed_session_data
				    (session, type);

			session->internals.resumed_extension_int_data[i].
			    type = type;
			session->internals.resumed_extension_int_data[i].
			    priv = data;
			session->internals.resumed_extension_int_data[i].
			    set = 1;
			return;
		}
	}
}

int _gnutls_ext_unpack(gnutls_session_t session, gnutls_buffer_st * packed)
{
	int i, ret;
	extension_priv_data_t data;
	gnutls_ext_unpack_func unpack;
	int max_exts = 0;
	uint16_t type;
	int size_for_type, cur_pos;


	BUFFER_POP_NUM(packed, max_exts);
	for (i = 0; i < max_exts; i++) {
		BUFFER_POP_NUM(packed, type);
		BUFFER_POP_NUM(packed, size_for_type);

		cur_pos = packed->length;

		unpack = _gnutls_ext_func_unpack(type);
		if (unpack == NULL) {
			gnutls_assert();
			return GNUTLS_E_PARSING_ERROR;
		}

		ret = unpack(packed, &data);
		if (ret < 0) {
			gnutls_assert();
			return ret;
		}

		/* verify that unpack read the correct bytes */
		cur_pos = cur_pos - packed->length;
		if (cur_pos /* read length */  != size_for_type) {
			gnutls_assert();
			return GNUTLS_E_PARSING_ERROR;
		}

		_gnutls_ext_set_resumed_session_data(session, type, data);
	}

	return 0;

      error:
	return ret;
}

void
_gnutls_ext_unset_session_data(gnutls_session_t session, uint16_t type)
{
	gnutls_ext_deinit_data_func deinit;
	extension_priv_data_t data;
	int ret, i;

	deinit = _gnutls_ext_func_deinit(type);
	ret = _gnutls_ext_get_session_data(session, type, &data);

	if (ret >= 0 && deinit != NULL) {
		deinit(data);
	}

	for (i = 0; i < MAX_EXT_TYPES; i++) {
		if (session->internals.extension_int_data[i].type == type) {
			session->internals.extension_int_data[i].set = 0;
			return;
		}
	}

}

static void
_gnutls_ext_unset_resumed_session_data(gnutls_session_t session,
				       uint16_t type)
{
	gnutls_ext_deinit_data_func deinit;
	extension_priv_data_t data;
	int ret, i;

	deinit = _gnutls_ext_func_deinit(type);
	ret = _gnutls_ext_get_resumed_session_data(session, type, &data);

	if (ret >= 0 && deinit != NULL) {
		deinit(data);
	}

	for (i = 0; i < MAX_EXT_TYPES; i++) {
		if (session->internals.resumed_extension_int_data[i].
		    type == type) {
			session->internals.resumed_extension_int_data[i].
			    set = 0;
			return;
		}
	}

}

/* Deinitializes all data that are associated with TLS extensions.
 */
void _gnutls_ext_free_session_data(gnutls_session_t session)
{
	unsigned int i;

	for (i = 0; i < extfunc_size; i++) {
		_gnutls_ext_unset_session_data(session, extfunc[i].type);
	}

	for (i = 0; i < extfunc_size; i++) {
		_gnutls_ext_unset_resumed_session_data(session,
						       extfunc[i].type);
	}

}

/* This function allows and extension to store data in the current session
 * and retrieve them later on. We use functions instead of a pointer to a
 * private pointer, to allow API additions by individual extensions.
 */
void
_gnutls_ext_set_session_data(gnutls_session_t session, uint16_t type,
			     extension_priv_data_t data)
{
	unsigned int i;
	gnutls_ext_deinit_data_func deinit;

	deinit = _gnutls_ext_func_deinit(type);

	for (i = 0; i < MAX_EXT_TYPES; i++) {
		if (session->internals.extension_int_data[i].type == type
		    || session->internals.extension_int_data[i].set == 0) {
			if (session->internals.extension_int_data[i].set !=
			    0) {
				if (deinit)
					deinit(session->internals.
					       extension_int_data[i].priv);
			}
			session->internals.extension_int_data[i].type =
			    type;
			session->internals.extension_int_data[i].priv =
			    data;
			session->internals.extension_int_data[i].set = 1;
			return;
		}
	}
}

int
_gnutls_ext_get_session_data(gnutls_session_t session,
			     uint16_t type, extension_priv_data_t * data)
{
	int i;

	for (i = 0; i < MAX_EXT_TYPES; i++) {
		if (session->internals.extension_int_data[i].set != 0 &&
		    session->internals.extension_int_data[i].type == type)
		{
			*data =
			    session->internals.extension_int_data[i].priv;
			return 0;
		}
	}
	return GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;
}

int
_gnutls_ext_get_resumed_session_data(gnutls_session_t session,
				     uint16_t type,
				     extension_priv_data_t * data)
{
	int i;

	for (i = 0; i < MAX_EXT_TYPES; i++) {
		if (session->internals.resumed_extension_int_data[i].set !=
		    0
		    && session->internals.resumed_extension_int_data[i].
		    type == type) {
			*data =
			    session->internals.
			    resumed_extension_int_data[i].priv;
			return 0;
		}
	}
	return GNUTLS_E_INVALID_REQUEST;
}
