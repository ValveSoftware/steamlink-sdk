/*
 * Copyright (C) 2000-2013 Free Software Foundation, Inc.
 * Copyright (C) 2012,2013 Nikos Mavrogiannopoulos
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

/* Functions that are record layer specific, are included in this file.
 */

/* allocate this many bytes more when encrypting or decrypting, to
 * compensate for broken backends such as cryptodev.
 */
#define CIPHER_SLACK_SIZE 32

#include "gnutls_int.h"
#include "gnutls_errors.h"
#include "debug.h"
#include "gnutls_compress.h"
#include "gnutls_cipher.h"
#include "gnutls_buffers.h"
#include "gnutls_mbuffers.h"
#include "gnutls_handshake.h"
#include "gnutls_hash_int.h"
#include "gnutls_cipher_int.h"
#include "algorithms.h"
#include "gnutls_db.h"
#include "gnutls_auth.h"
#include "gnutls_num.h"
#include "gnutls_record.h"
#include "gnutls_datum.h"
#include "gnutls_constate.h"
#include "ext/max_record.h"
#include <ext/heartbeat.h>
#include <gnutls_state.h>
#include <gnutls_dtls.h>
#include <gnutls_dh.h>
#include <random.h>

struct tls_record_st {
	uint16_t header_size;
	uint8_t version[2];
	uint64 sequence;	/* DTLS */
	uint16_t length;
	uint16_t packet_size;	/* header_size + length */
	content_type_t type;
	uint16_t epoch;		/* valid in DTLS only */
	unsigned v2:1;		/* whether an SSLv2 client hello */
	/* the data */
};

/**
 * gnutls_record_disable_padding:  
 * @session: is a #gnutls_session_t structure.
 *
 * Used to disabled padding in TLS 1.0 and above.  Normally you do not
 * need to use this function, but there are buggy clients that
 * complain if a server pads the encrypted data.  This of course will
 * disable protection against statistical attacks on the data.
 *
 * This functions is defunt since 3.1.7. Random padding is disabled
 * by default unless requested using gnutls_range_send_message().
 *
 **/
void gnutls_record_disable_padding(gnutls_session_t session)
{
	return;
}

/**
 * gnutls_record_set_max_empty_records:
 * @session: is a #gnutls_session_t structure.
 * @i: is the desired value of maximum empty records that can be accepted in a row.
 *
 * Used to set the maximum number of empty fragments that can be accepted
 * in a row. Accepting many empty fragments is useful for receiving length-hidden
 * content, where empty fragments filled with pad are sent to hide the real
 * length of a message. However, a malicious peer could send empty fragments to
 * mount a DoS attack, so as a safety measure, a maximum number of empty fragments
 * is accepted by default. If you know your application must accept a given number
 * of empty fragments in a row, you can use this function to set the desired value.
 **/
void
gnutls_record_set_max_empty_records(gnutls_session_t session,
				    const unsigned int i)
{
	session->internals.priorities.max_empty_records = i;
}

/**
 * gnutls_transport_set_ptr:
 * @session: is a #gnutls_session_t structure.
 * @ptr: is the value.
 *
 * Used to set the first argument of the transport function (for push
 * and pull callbacks). In berkeley style sockets this function will set the
 * connection descriptor.
 * 
 **/
void
gnutls_transport_set_ptr(gnutls_session_t session,
			 gnutls_transport_ptr_t ptr)
{
	session->internals.transport_recv_ptr = ptr;
	session->internals.transport_send_ptr = ptr;
}

/**
 * gnutls_transport_set_ptr2:
 * @session: is a #gnutls_session_t structure.
 * @recv_ptr: is the value for the pull function
 * @send_ptr: is the value for the push function
 *
 * Used to set the first argument of the transport function (for push
 * and pull callbacks). In berkeley style sockets this function will set the
 * connection descriptor.  With this function you can use two different
 * pointers for receiving and sending.
 **/
void
gnutls_transport_set_ptr2(gnutls_session_t session,
			  gnutls_transport_ptr_t recv_ptr,
			  gnutls_transport_ptr_t send_ptr)
{
	session->internals.transport_send_ptr = send_ptr;
	session->internals.transport_recv_ptr = recv_ptr;
}

/**
 * gnutls_transport_set_int2:
 * @session: is a #gnutls_session_t structure.
 * @recv_int: is the value for the pull function
 * @send_int: is the value for the push function
 *
 * Used to set the first argument of the transport function (for push
 * and pull callbacks), when using the berkeley style sockets. 
 * With this function you can set two different
 * pointers for receiving and sending.
 *
 * Since: 3.1.9
 **/
void
gnutls_transport_set_int2(gnutls_session_t session,
			  int recv_int, int send_int)
{
	session->internals.transport_send_ptr =
	    (gnutls_transport_ptr_t) (long) send_int;
	session->internals.transport_recv_ptr =
	    (gnutls_transport_ptr_t) (long) recv_int;
}

#if 0
/* this will be a macro */
/**
 * gnutls_transport_set_int:
 * @session: is a #gnutls_session_t structure.
 * @i: is the value.
 *
 * Used to set the first argument of the transport function (for push
 * and pull callbacks) for berkeley style sockets.
 *
 * Since: 3.1.9
 * 
 **/
void gnutls_transport_set_int(gnutls_session_t session, int i)
{
	session->internals.transport_recv_ptr =
	    (gnutls_transport_ptr_t) (long) i;
	session->internals.transport_send_ptr =
	    (gnutls_transport_ptr_t) (long) i;
}
#endif

/**
 * gnutls_transport_get_ptr:
 * @session: is a #gnutls_session_t structure.
 *
 * Used to get the first argument of the transport function (like
 * PUSH and PULL).  This must have been set using
 * gnutls_transport_set_ptr().
 *
 * Returns: The first argument of the transport function.
 **/
gnutls_transport_ptr_t gnutls_transport_get_ptr(gnutls_session_t session)
{
	return session->internals.transport_recv_ptr;
}

/**
 * gnutls_transport_get_ptr2:
 * @session: is a #gnutls_session_t structure.
 * @recv_ptr: will hold the value for the pull function
 * @send_ptr: will hold the value for the push function
 *
 * Used to get the arguments of the transport functions (like PUSH
 * and PULL).  These should have been set using
 * gnutls_transport_set_ptr2().
 **/
void
gnutls_transport_get_ptr2(gnutls_session_t session,
			  gnutls_transport_ptr_t * recv_ptr,
			  gnutls_transport_ptr_t * send_ptr)
{

	*recv_ptr = session->internals.transport_recv_ptr;
	*send_ptr = session->internals.transport_send_ptr;
}

/**
 * gnutls_transport_get_int2:
 * @session: is a #gnutls_session_t structure.
 * @recv_int: will hold the value for the pull function
 * @send_int: will hold the value for the push function
 *
 * Used to get the arguments of the transport functions (like PUSH
 * and PULL).  These should have been set using
 * gnutls_transport_set_int2().
 *
 * Since: 3.1.9
 **/
void
gnutls_transport_get_int2(gnutls_session_t session,
			  int *recv_int, int *send_int)
{

	*recv_int = (long) session->internals.transport_recv_ptr;
	*send_int = (long) session->internals.transport_send_ptr;
}

/**
 * gnutls_transport_get_int:
 * @session: is a #gnutls_session_t structure.
 *
 * Used to get the first argument of the transport function (like
 * PUSH and PULL).  This must have been set using
 * gnutls_transport_set_int().
 *
 * Returns: The first argument of the transport function.
 *
 * Since: 3.1.9
 **/
int gnutls_transport_get_int(gnutls_session_t session)
{
	return (long) session->internals.transport_recv_ptr;
}

/**
 * gnutls_bye:
 * @session: is a #gnutls_session_t structure.
 * @how: is an integer
 *
 * Terminates the current TLS/SSL connection. The connection should
 * have been initiated using gnutls_handshake().  @how should be one
 * of %GNUTLS_SHUT_RDWR, %GNUTLS_SHUT_WR.
 *
 * In case of %GNUTLS_SHUT_RDWR the TLS session gets
 * terminated and further receives and sends will be disallowed.  If
 * the return value is zero you may continue using the underlying
 * transport layer. %GNUTLS_SHUT_RDWR sends an alert containing a close
 * request and waits for the peer to reply with the same message.
 *
 * In case of %GNUTLS_SHUT_WR the TLS session gets terminated
 * and further sends will be disallowed. In order to reuse the
 * connection you should wait for an EOF from the peer.
 * %GNUTLS_SHUT_WR sends an alert containing a close request.
 *
 * Note that not all implementations will properly terminate a TLS
 * connection.  Some of them, usually for performance reasons, will
 * terminate only the underlying transport layer, and thus not
 * distinguishing between a malicious party prematurely terminating 
 * the connection and normal termination. 
 *
 * This function may also return %GNUTLS_E_AGAIN or
 * %GNUTLS_E_INTERRUPTED; cf.  gnutls_record_get_direction().
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code, see
 *   function documentation for entire semantics.
 **/
int gnutls_bye(gnutls_session_t session, gnutls_close_request_t how)
{
	int ret = 0;

	switch (STATE) {
	case STATE0:
	case STATE60:
		ret = _gnutls_io_write_flush(session);
		STATE = STATE60;
		if (ret < 0) {
			gnutls_assert();
			return ret;
		}
		/* fallthrough */
	case STATE61:
		ret =
		    gnutls_alert_send(session, GNUTLS_AL_WARNING,
				      GNUTLS_A_CLOSE_NOTIFY);
		STATE = STATE61;
		if (ret < 0) {
			gnutls_assert();
			return ret;
		}

	case STATE62:
		STATE = STATE62;
		if (how == GNUTLS_SHUT_RDWR) {
			do {
				ret =
				    _gnutls_recv_int(session, GNUTLS_ALERT,
						     -1, NULL, NULL, 0, NULL,
						     session->internals.
						     record_timeout_ms);
			}
			while (ret == GNUTLS_E_GOT_APPLICATION_DATA);

			if (ret >= 0)
				session->internals.may_not_read = 1;

			if (ret < 0) {
				gnutls_assert();
				return ret;
			}
		}
		STATE = STATE62;

		break;
	default:
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	STATE = STATE0;

	session->internals.may_not_write = 1;
	return 0;
}

inline static void session_unresumable(gnutls_session_t session)
{
	session->internals.resumable = RESUME_FALSE;
}

/* returns 0 if session is valid
 */
inline static int session_is_valid(gnutls_session_t session)
{
	if (session->internals.invalid_connection != 0)
		return GNUTLS_E_INVALID_SESSION;

	return 0;
}

/* Copies the record version into the headers. The 
 * version must have 2 bytes at least.
 */
inline static void
copy_record_version(gnutls_session_t session,
		    gnutls_handshake_description_t htype,
		    uint8_t version[2])
{
	const version_entry_st *lver;

	if (session->internals.initial_negotiation_completed
	    || htype != GNUTLS_HANDSHAKE_CLIENT_HELLO
	    || session->internals.default_record_version[0] == 0) {
		lver = get_version(session);

		version[0] = lver->major;
		version[1] = lver->minor;
	} else {
		version[0] = session->internals.default_record_version[0];
		version[1] = session->internals.default_record_version[1];
	}
}

/* Increments the sequence value
 */
inline static int
sequence_increment(gnutls_session_t session, uint64 * value)
{
	if (IS_DTLS(session)) {
		return _gnutls_uint48pp(value);
	} else {
		return _gnutls_uint64pp(value);
	}
}

/* This function behaves exactly like write(). The only difference is
 * that it accepts, the gnutls_session_t and the content_type_t of data to
 * send (if called by the user the Content is specific)
 * It is intended to transfer data, under the current session.    
 *
 * @type: The content type to send
 * @htype: If this is a handshake message then the handshake type
 * @epoch_rel: %EPOCH_READ_* or %EPOCH_WRITE_*
 * @data: the data to be sent
 * @data_size: the size of the @data
 * @min_pad: the minimum required padding
 * @mflags: zero or %MBUFFER_FLUSH
 *
 * Oct 30 2001: Removed capability to send data more than MAX_RECORD_SIZE.
 * This makes the function much easier to read, and more error resistant
 * (there were cases were the old function could mess everything up).
 * --nmav
 *
 * This function may accept a NULL pointer for data, and 0 for size, if
 * and only if the previous send was interrupted for some reason.
 *
 */
ssize_t
_gnutls_send_tlen_int(gnutls_session_t session, content_type_t type,
		      gnutls_handshake_description_t htype,
		      unsigned int epoch_rel, const void *_data,
		      size_t data_size, size_t min_pad,
		      unsigned int mflags)
{
	mbuffer_st *bufel;
	ssize_t cipher_size;
	int retval, ret;
	int send_data_size;
	uint8_t *headers;
	int header_size;
	const uint8_t *data = _data;
	record_parameters_st *record_params;
	size_t max_send_size;
	record_state_st *record_state;

	ret = _gnutls_epoch_get(session, epoch_rel, &record_params);
	if (ret < 0)
		return gnutls_assert_val(ret);

	/* Safeguard against processing data with an incomplete cipher state. */
	if (!record_params->initialized)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	record_state = &record_params->write;

	/* Do not allow null pointer if the send buffer is empty.
	 * If the previous send was interrupted then a null pointer is
	 * ok, and means to resume.
	 */
	if (session->internals.record_send_buffer.byte_length == 0 &&
	    (data_size == 0 && _data == NULL)) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	if (type != GNUTLS_ALERT)	/* alert messages are sent anyway */
		if (session_is_valid(session)
		    || session->internals.may_not_write != 0) {
			gnutls_assert();
			return GNUTLS_E_INVALID_SESSION;
		}

	max_send_size = max_user_send_size(session, record_params);

	if (data_size > max_send_size) {
		if (IS_DTLS(session))
			return gnutls_assert_val(GNUTLS_E_LARGE_PACKET);

		send_data_size = max_send_size;
	} else
		send_data_size = data_size;

	/* Only encrypt if we don't have data to send 
	 * from the previous run. - probably interrupted.
	 */
	if (mflags != 0
	    && session->internals.record_send_buffer.byte_length > 0) {
		ret = _gnutls_io_write_flush(session);
		if (ret > 0)
			cipher_size = ret;
		else
			cipher_size = 0;

		retval = session->internals.record_send_buffer_user_size;
	} else {
		if (unlikely((send_data_size == 0 && min_pad == 0)))
			return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);

		/* now proceed to packet encryption
		 */
		cipher_size = MAX_RECORD_SEND_SIZE(session);

		bufel = _mbuffer_alloc_align16(cipher_size + CIPHER_SLACK_SIZE, 
			get_total_headers2(session, record_params));
		if (bufel == NULL)
			return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

		headers = _mbuffer_get_uhead_ptr(bufel);
		headers[0] = type;
		/* Use the default record version, if it is
		 * set. */
		copy_record_version(session, htype, &headers[1]);
		/* Adjust header length and add sequence for DTLS */
		if (IS_DTLS(session))
			memcpy(&headers[3],
			       &record_state->sequence_number.i, 8);

		_gnutls_record_log
		    ("REC[%p]: Preparing Packet %s(%d) with length: %d and min pad: %d\n",
		     session, _gnutls_packet2str(type), type,
		     (int) data_size, (int) min_pad);

		header_size = RECORD_HEADER_SIZE(session);
		_mbuffer_set_udata_size(bufel, cipher_size);
		_mbuffer_set_uhead_size(bufel, header_size);

		ret =
		    _gnutls_encrypt(session,
				    data, send_data_size, min_pad,
				    bufel, type, record_params);
		if (ret <= 0) {
			gnutls_assert();
			if (ret == 0)
				ret = GNUTLS_E_ENCRYPTION_FAILED;
			gnutls_free(bufel);
			return ret;	/* error */
		}

		cipher_size = _mbuffer_get_udata_size(bufel);
		retval = send_data_size;
		session->internals.record_send_buffer_user_size =
		    send_data_size;

		/* increase sequence number
		 */
		if (sequence_increment
		    (session, &record_state->sequence_number) != 0) {
			session_invalidate(session);
			gnutls_free(bufel);
			return
			    gnutls_assert_val
			    (GNUTLS_E_RECORD_LIMIT_REACHED);
		}

		ret = _gnutls_io_write_buffered(session, bufel, mflags);
	}

	if (ret != cipher_size) {
		/* If we have sent any data then just return
		 * the error value. Do not invalidate the session.
		 */
		if (ret < 0 && gnutls_error_is_fatal(ret) == 0)
			return gnutls_assert_val(ret);

		if (ret > 0)
			ret = gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);

		session_unresumable(session);
		session->internals.may_not_write = 1;
		return gnutls_assert_val(ret);
	}

	session->internals.record_send_buffer_user_size = 0;

	_gnutls_record_log
	    ("REC[%p]: Sent Packet[%d] %s(%d) in epoch %d and length: %d\n",
	     session, (unsigned int)
	     _gnutls_uint64touint32(&record_state->sequence_number),
	     _gnutls_packet2str(type), type, (int) record_params->epoch,
	     (int) cipher_size);

	return retval;
}

inline static int
check_recv_type(gnutls_session_t session, content_type_t recv_type)
{
	switch (recv_type) {
	case GNUTLS_CHANGE_CIPHER_SPEC:
	case GNUTLS_ALERT:
	case GNUTLS_HANDSHAKE:
	case GNUTLS_HEARTBEAT:
	case GNUTLS_APPLICATION_DATA:
		return 0;
	default:
		gnutls_assert();
		_gnutls_audit_log(session,
				  "Received record packet of unknown type %u\n",
				  (unsigned int) recv_type);
		return GNUTLS_E_UNEXPECTED_PACKET;
	}

}


/* Checks if there are pending data in the record buffers. If there are
 * then it copies the data.
 */
static int
check_buffers(gnutls_session_t session, content_type_t type,
	      uint8_t * data, int data_size, void *seq)
{
	if ((type == GNUTLS_APPLICATION_DATA ||
	     type == GNUTLS_HANDSHAKE || type == GNUTLS_CHANGE_CIPHER_SPEC)
	    && _gnutls_record_buffer_get_size(session) > 0) {
		int ret;
		ret =
		    _gnutls_record_buffer_get(type, session, data,
					      data_size, seq);
		if (ret < 0) {
			if (IS_DTLS(session)) {
				if (ret == GNUTLS_E_UNEXPECTED_PACKET) {
					ret = GNUTLS_E_AGAIN;
				}
			}
			gnutls_assert();
			return ret;
		}

		return ret;
	}

	return 0;
}

/* Checks and retrieves any pending data in the application data record buffers.
 */
static int
check_packet_buffers(gnutls_session_t session, content_type_t type,
		     gnutls_packet_t *packet)
{
	if (_gnutls_record_buffer_get_size(session) > 0) {
		int ret;
		ret =
		    _gnutls_record_buffer_get_packet(type, session,
					      	     packet);
		if (ret < 0) {
			if (IS_DTLS(session)) {
				if (ret == GNUTLS_E_UNEXPECTED_PACKET) {
					ret = GNUTLS_E_AGAIN;
				}
			}
			gnutls_assert();
			return ret;
		}

		return ret;
	}

	*packet = NULL;
	return 0;
}



/* Here we check if the advertized version is the one we
 * negotiated in the handshake.
 */
inline static int
record_check_version(gnutls_session_t session,
		     gnutls_handshake_description_t htype,
		     uint8_t version[2])
{
	const version_entry_st *vers = get_version(session);
	int diff = 0;

	if (vers->major != version[0] || vers->minor != version[1])
		diff = 1;

	if (!IS_DTLS(session)) {
		if (htype == GNUTLS_HANDSHAKE_CLIENT_HELLO ||
		    htype == GNUTLS_HANDSHAKE_SERVER_HELLO) {
			if (version[0] != 3) {
				gnutls_assert();
				_gnutls_record_log
				    ("REC[%p]: INVALID VERSION PACKET: (%d) %d.%d\n",
				     session, htype, version[0],
				     version[1]);
				return GNUTLS_E_UNSUPPORTED_VERSION_PACKET;
			}
		} else if (diff != 0) {
			/* Reject record packets that have a different version than the
			 * one negotiated. Note that this version is not protected by any
			 * mac. I don't really think that this check serves any purpose.
			 */
			gnutls_assert();
			_gnutls_record_log
			    ("REC[%p]: INVALID VERSION PACKET: (%d) %d.%d\n",
			     session, htype, version[0], version[1]);

			return GNUTLS_E_UNSUPPORTED_VERSION_PACKET;
		}
	} else {		/* DTLS */

		/* In DTLS the only information we have here is whether we
		 * expect a handshake message or not.
		 */
		if (htype == (gnutls_handshake_description_t) - 1) {
			if (diff) {
				/* Reject record packets that have a different version than the
				 * one negotiated. Note that this version is not protected by any
				 * mac. I don't really think that this check serves any purpose.
				 */
				gnutls_assert();
				_gnutls_record_log
				    ("REC[%p]: INVALID VERSION PACKET: (%d) %d.%d\n",
				     session, htype, version[0],
				     version[1]);

				return GNUTLS_E_UNSUPPORTED_VERSION_PACKET;
			}
		} else if (vers->id > GNUTLS_DTLS1_0 && version[0] > 254) {
			gnutls_assert();
			_gnutls_record_log
			    ("REC[%p]: INVALID DTLS VERSION PACKET: (%d) %d.%d\n",
			     session, htype, version[0], version[1]);
			return GNUTLS_E_UNSUPPORTED_VERSION_PACKET;
		} else if (vers->id == GNUTLS_DTLS0_9 && version[0] > 1) {
			gnutls_assert();
			_gnutls_record_log
			    ("REC[%p]: INVALID DTLS VERSION PACKET: (%d) %d.%d\n",
			     session, htype, version[0], version[1]);
			return GNUTLS_E_UNSUPPORTED_VERSION_PACKET;
		}
	}

	return 0;
}

/* This function will check if the received record type is
 * the one we actually expect and adds it to the proper
 * buffer. The bufel will be deinitialized after calling
 * this function, even if it fails.
 */
static int
record_add_to_buffers(gnutls_session_t session,
		      struct tls_record_st *recv, content_type_t type,
		      gnutls_handshake_description_t htype,
		      uint64 * seq, mbuffer_st * bufel)
{

	int ret;

	if ((recv->type == type)
	    && (type == GNUTLS_APPLICATION_DATA ||
		type == GNUTLS_CHANGE_CIPHER_SPEC ||
		type == GNUTLS_HANDSHAKE)) {
		_gnutls_record_buffer_put(session, type, seq, bufel);

		/* if we received application data as expected then we
		 * deactivate the async timer */
		_dtls_async_timer_delete(session);
	} else {
		/* if the expected type is different than the received 
		 */
		switch (recv->type) {
		case GNUTLS_ALERT:
			if (bufel->msg.size < 2) {
				ret =
				    gnutls_assert_val
				    (GNUTLS_E_UNEXPECTED_PACKET_LENGTH);
				goto unexpected_packet;
			}

			_gnutls_record_log
			    ("REC[%p]: Alert[%d|%d] - %s - was received\n",
			     session, bufel->msg.data[0],
			     bufel->msg.data[1],
			     gnutls_alert_get_name((int) bufel->msg.
						   data[1]));

			session->internals.last_alert = bufel->msg.data[1];

			/* if close notify is received and
			 * the alert is not fatal
			 */
			if (bufel->msg.data[1] == GNUTLS_A_CLOSE_NOTIFY
			    && bufel->msg.data[0] != GNUTLS_AL_FATAL) {
				/* If we have been expecting for an alert do 
				 */
				session->internals.read_eof = 1;
				ret = GNUTLS_E_SESSION_EOF;
				goto cleanup;
			} else {
				/* if the alert is FATAL or WARNING
				 * return the apropriate message
				 */

				gnutls_assert();
				ret = GNUTLS_E_WARNING_ALERT_RECEIVED;
				if (bufel->msg.data[0] == GNUTLS_AL_FATAL) {
					session_unresumable(session);
					session_invalidate(session);
					ret =
					    gnutls_assert_val
					    (GNUTLS_E_FATAL_ALERT_RECEIVED);
				}
				goto cleanup;
			}
			break;

		case GNUTLS_CHANGE_CIPHER_SPEC:
			if (!(IS_DTLS(session))) {
				ret = gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET);
				goto cleanup;
			}

			_gnutls_record_buffer_put(session, recv->type, seq,
						  bufel);

			break;

#ifdef ENABLE_HEARTBEAT
		case GNUTLS_HEARTBEAT:
			ret = _gnutls_heartbeat_handle(session, bufel);
			goto cleanup;
#endif

		case GNUTLS_APPLICATION_DATA:
			if (session->internals.
			    initial_negotiation_completed == 0) {
				ret =
				    gnutls_assert_val
				    (GNUTLS_E_UNEXPECTED_PACKET);
				goto unexpected_packet;
			}


			/* the got_application data is only returned
			 * if expecting client hello (for rehandshake
			 * reasons). Otherwise it is an unexpected packet
			 */
			if (type == GNUTLS_ALERT
			    || (htype == GNUTLS_HANDSHAKE_CLIENT_HELLO
				&& type == GNUTLS_HANDSHAKE)) {
				/* even if data is unexpected put it into the buffer */
				_gnutls_record_buffer_put(session, recv->type,
							  seq, bufel);
				return
				    gnutls_assert_val
				    (GNUTLS_E_GOT_APPLICATION_DATA);
			} else {
				ret =
				    gnutls_assert_val
				    (GNUTLS_E_UNEXPECTED_PACKET);
				goto unexpected_packet;
			}

			break;

		case GNUTLS_HANDSHAKE:
			/* In DTLS we might receive a handshake replay from the peer to indicate
			 * the our last TLS handshake messages were not received.
			 */
			if (IS_DTLS(session)) {
				if (type == GNUTLS_CHANGE_CIPHER_SPEC) {
					ret =
					    gnutls_assert_val
					    (GNUTLS_E_UNEXPECTED_PACKET);
					goto unexpected_packet;
				}

				if (_dtls_is_async(session)
				    && _dtls_async_timer_active(session)) {
					if (session->security_parameters.
					    entity == GNUTLS_SERVER
					    && bufel->htype ==
					    GNUTLS_HANDSHAKE_CLIENT_HELLO)
					{
						/* client requested rehandshake. Delete the timer */
						_dtls_async_timer_delete
						    (session);
					} else {
						session->internals.
						    recv_state =
						    RECV_STATE_DTLS_RETRANSMIT;
						ret =
						    _dtls_retransmit
						    (session);
						if (ret == 0) {
							session->internals.
							    recv_state =
							    RECV_STATE_0;
							ret =
							    gnutls_assert_val
							    (GNUTLS_E_AGAIN);
							goto unexpected_packet;
						}
						goto cleanup;
					}
				}
			}

			/* This is legal if HELLO_REQUEST is received - and we are a client.
			 * If we are a server, a client may initiate a renegotiation at any time.
			 */
			if (session->security_parameters.entity ==
			    GNUTLS_SERVER
			    && bufel->htype ==
			    GNUTLS_HANDSHAKE_CLIENT_HELLO) {
				gnutls_assert();
				_gnutls_record_buffer_put(session,
							      recv->type,
							      seq, bufel);
				return GNUTLS_E_REHANDSHAKE;
			}

			/* If we are already in a handshake then a Hello
			 * Request is illegal. But here we don't really care
			 * since this message will never make it up here.
			 */

			/* So we accept it, if it is a Hello. If not, this will
			 * fail and trigger flight retransmissions after some time. */
			ret =
			    _gnutls_recv_hello_request(session,
						       bufel->msg.data,
						       bufel->msg.size);
			goto unexpected_packet;

			break;
		default:

			_gnutls_record_log
			    ("REC[%p]: Received unexpected packet %d (%s) expecting %d (%s)\n",
			     session, recv->type,
			     _gnutls_packet2str(recv->type), type,
			     _gnutls_packet2str(type));

			gnutls_assert();
			ret = GNUTLS_E_UNEXPECTED_PACKET;
			goto unexpected_packet;
		}
	}

	return 0;

      unexpected_packet:
	if (IS_DTLS(session) && ret != GNUTLS_E_REHANDSHAKE) {
		_mbuffer_xfree(&bufel);
		RETURN_DTLS_EAGAIN_OR_TIMEOUT(session, ret);
	}

      cleanup:
	_mbuffer_xfree(&bufel);
	return ret;

}


/* Checks the record headers and returns the length, version and
 * content type.
 */
static void
record_read_headers(gnutls_session_t session,
		    uint8_t headers[MAX_RECORD_HEADER_SIZE],
		    content_type_t type,
		    gnutls_handshake_description_t htype,
		    struct tls_record_st *record)
{

	/* Read the first two bytes to determine if this is a 
	 * version 2 message 
	 */

	if (htype == GNUTLS_HANDSHAKE_CLIENT_HELLO
	    && type == GNUTLS_HANDSHAKE && headers[0] > 127
	    && !(IS_DTLS(session))) {

		/* if msb set and expecting handshake message
		 * it should be SSL 2 hello 
		 */
		record->version[0] = 3;	/* assume SSL 3.0 */
		record->version[1] = 0;

		record->length = (((headers[0] & 0x7f) << 8)) | headers[1];

		/* SSL 2.0 headers */
		record->header_size = record->packet_size = 2;
		record->type = GNUTLS_HANDSHAKE;	/* we accept only v2 client hello
							 */

		/* in order to assist the handshake protocol.
		 * V2 compatibility is a mess.
		 */
		record->v2 = 1;
		record->epoch = 0;
		memset(&record->sequence, 0, sizeof(record->sequence));

		_gnutls_record_log
		    ("REC[%p]: SSL 2.0 %s packet received. Length: %d\n",
		     session, _gnutls_packet2str(record->type),
		     record->length);

	} else {
		/* dtls version 1.0 and TLS version 1.x */
		record->v2 = 0;

		record->type = headers[0];
		record->version[0] = headers[1];
		record->version[1] = headers[2];

		if (IS_DTLS(session)) {
			memcpy(record->sequence.i, &headers[3], 8);
			record->length = _gnutls_read_uint16(&headers[11]);
			record->epoch =
			    _gnutls_read_uint16(record->sequence.i);
		} else {
			memset(&record->sequence, 0,
			       sizeof(record->sequence));
			record->length = _gnutls_read_uint16(&headers[3]);
			record->epoch = 0;
		}

		_gnutls_record_log
		    ("REC[%p]: SSL %d.%d %s packet received. Epoch %d, length: %d\n",
		     session, (int) record->version[0],
		     (int) record->version[1],
		     _gnutls_packet2str(record->type), (int) record->epoch,
		     record->length);

	}

	record->packet_size += record->length;
}


static int recv_headers(gnutls_session_t session, 
			record_parameters_st *record_params,
			content_type_t type,
			gnutls_handshake_description_t htype,
			struct tls_record_st *record, unsigned int *ms)
{
	int ret;
	gnutls_datum_t raw;	/* raw headers */
	/* Read the headers.
	 */
	record->header_size = record->packet_size =
	    RECORD_HEADER_SIZE(session);

	ret =
	    _gnutls_io_read_buffered(session, record->header_size, -1, ms);
	if (ret != record->header_size) {
		if (ret < 0 && gnutls_error_is_fatal(ret) == 0)
			return ret;

		if (ret > 0)
			ret = GNUTLS_E_UNEXPECTED_PACKET_LENGTH;
		else if (ret == 0)
			ret = GNUTLS_E_PREMATURE_TERMINATION;

		return gnutls_assert_val(ret);
	}

	ret = _mbuffer_linearize_align16(&session->internals.record_recv_buffer, 
		get_total_headers2(session, record_params));
	if (ret < 0)
		return gnutls_assert_val(ret);

	_mbuffer_head_get_first(&session->internals.record_recv_buffer,
				&raw);
	if (raw.size < RECORD_HEADER_SIZE(session))
		return
		    gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET_LENGTH);

	record_read_headers(session, raw.data, type, htype, record);

	/* Check if the DTLS epoch is valid */
	if (IS_DTLS(session)) {
		if (_gnutls_epoch_is_valid(session, record->epoch) == 0) {
			_gnutls_audit_log(session,
					  "Discarded message[%u] with invalid epoch %u.\n",
					  (unsigned int)
					  _gnutls_uint64touint32(&record->
								 sequence),
					  (unsigned int) record->sequence.
					  i[0] * 256 +
					  (unsigned int) record->sequence.
					  i[1]);
			gnutls_assert();
			/* doesn't matter, just a fatal error */
			return GNUTLS_E_UNEXPECTED_PACKET_LENGTH;
		}
	}

	/* Here we check if the Type of the received packet is
	 * ok. 
	 */
	if ((ret = check_recv_type(session, record->type)) < 0)
		return gnutls_assert_val(ret);

	/* Here we check if the advertized version is the one we
	 * negotiated in the handshake.
	 */
	if ((ret =
	     record_check_version(session, htype, record->version)) < 0)
		return gnutls_assert_val(ret);

	if (record->length > max_record_recv_size(session)) {
		_gnutls_audit_log
		    (session, "Received packet with illegal length: %u\n",
		     (unsigned int) record->length);
		return
		    gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET_LENGTH);
	}

	_gnutls_record_log
	    ("REC[%p]: Expected Packet %s(%d)\n", session,
	     _gnutls_packet2str(type), type);
	_gnutls_record_log
	    ("REC[%p]: Received Packet %s(%d) with length: %d\n", session,
	     _gnutls_packet2str(record->type), record->type,
	     record->length);


	return 0;
}

/* @ms: is the number of milliseconds to wait for data. Use zero for indefinite.
 *
 * This will receive record layer packets and add them to 
 * application_data_buffer and handshake_data_buffer.
 *
 * If the htype is not -1 then handshake timeouts
 * will be enforced.
 */
ssize_t
_gnutls_recv_in_buffers(gnutls_session_t session, content_type_t type,
			gnutls_handshake_description_t htype,
			unsigned int ms)
{
	uint64 *packet_sequence;
	gnutls_datum_t ciphertext;
	mbuffer_st *bufel = NULL, *decrypted = NULL;
	gnutls_datum_t t;
	int ret;
	unsigned int empty_fragments = 0;
	record_parameters_st *record_params;
	record_state_st *record_state;
	struct tls_record_st record;

      begin:

	if (empty_fragments >
	    session->internals.priorities.max_empty_records) {
		gnutls_assert();
		return GNUTLS_E_TOO_MANY_EMPTY_PACKETS;
	}

	if (session->internals.read_eof != 0) {
		/* if we have already read an EOF
		 */
		return 0;
	} else if (session_is_valid(session) != 0
		   || session->internals.may_not_read != 0)
		return gnutls_assert_val(GNUTLS_E_INVALID_SESSION);

	/* get the record state parameters */
	ret =
	    _gnutls_epoch_get(session, EPOCH_READ_CURRENT, &record_params);
	if (ret < 0)
		return gnutls_assert_val(ret);

	/* Safeguard against processing data with an incomplete cipher state. */
	if (!record_params->initialized)
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);

	record_state = &record_params->read;

	/* receive headers */
	ret = recv_headers(session, record_params, type, htype, &record, &ms);
	if (ret < 0) {
		ret = gnutls_assert_val_fatal(ret);
		goto recv_error;
	}

	if (IS_DTLS(session))
		packet_sequence = &record.sequence;
	else
		packet_sequence = &record_state->sequence_number;

	/* Read the packet data and insert it to record_recv_buffer.
	 */
	ret =
	    _gnutls_io_read_buffered(session, record.packet_size,
				     record.type, &ms);
	if (ret != record.packet_size) {
		gnutls_assert();
		goto recv_error;
	}

	/* ok now we are sure that we have read all the data - so
	 * move on !
	 */
	ret = _mbuffer_linearize_align16(&session->internals.record_recv_buffer, 
		get_total_headers2(session, record_params));
	if (ret < 0)
		return gnutls_assert_val(ret);

	bufel =
	    _mbuffer_head_get_first(&session->internals.record_recv_buffer,
				    NULL);
	if (bufel == NULL)
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);

	/* We allocate the maximum possible to allow few compressed bytes to expand to a
	 * full record. Moreover we add space for any pad and the MAC (in case
	 * they are encrypted).
	 */
	ret = max_decrypted_size(session) + MAX_PAD_SIZE + MAX_HASH_SIZE;
	decrypted = _mbuffer_alloc_align16(ret, 0);
	if (decrypted == NULL)
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

	_mbuffer_set_udata_size(decrypted, ret);
	ciphertext.data =
	    (uint8_t *) _mbuffer_get_udata_ptr(bufel) + record.header_size;
	ciphertext.size = record.length;

	/* decrypt the data we got. 
	 */
	t.data = _mbuffer_get_udata_ptr(decrypted);
	t.size = _mbuffer_get_udata_size(decrypted);
	ret =
	    _gnutls_decrypt(session, &ciphertext, &t,
			    record.type, record_params, packet_sequence);
	if (ret >= 0)
		_mbuffer_set_udata_size(decrypted, ret);

	_mbuffer_head_remove_bytes(&session->internals.record_recv_buffer,
				   record.header_size + record.length);
	if (ret < 0) {
		gnutls_assert();
		_gnutls_audit_log(session,
				  "Discarded message[%u] due to invalid decryption\n",
				  (unsigned int)
				  _gnutls_uint64touint32(packet_sequence));
		goto sanity_check_error;
	}

	/* check for duplicates. We check after the message
	 * is processed and authenticated to avoid someone
	 * messing with our windows.
	 */
	if (IS_DTLS(session)
	    && session->internals.no_replay_protection == 0) {
		ret = _dtls_record_check(record_params, packet_sequence);
		if (ret < 0) {
			_gnutls_record_log
			    ("REC[%p]: Discarded duplicate message[%u.%u]: %s\n",
			     session,
			     (unsigned int) record.sequence.i[0] * 256 +
			     (unsigned int) record.sequence.i[1],
			     (unsigned int)
			     _gnutls_uint64touint32(packet_sequence),
			     _gnutls_packet2str(record.type));
			goto sanity_check_error;
		}
		_gnutls_record_log
		    ("REC[%p]: Decrypted Packet[%u.%u] %s(%d) with length: %d\n",
		     session,
		     (unsigned int) record.sequence.i[0] * 256 +
		     (unsigned int) record.sequence.i[1],
		     (unsigned int)
		     _gnutls_uint64touint32(packet_sequence),
		     _gnutls_packet2str(record.type), record.type,
		     (int) _mbuffer_get_udata_size(decrypted));
	} else {
		_gnutls_record_log
		    ("REC[%p]: Decrypted Packet[%u] %s(%d) with length: %d\n",
		     session,
		     (unsigned int)
		     _gnutls_uint64touint32(packet_sequence),
		     _gnutls_packet2str(record.type), record.type,
		     (int) _mbuffer_get_udata_size(decrypted));
	}

	/* increase sequence number 
	 */
	if (!IS_DTLS(session)
	    && sequence_increment(session,
				  &record_state->sequence_number) != 0) {
		session_invalidate(session);
		gnutls_assert();
		ret = GNUTLS_E_RECORD_LIMIT_REACHED;
		goto sanity_check_error;
	}

/* (originally for) TLS 1.0 CBC protection. 
 * Actually this code is called if we just received
 * an empty packet. An empty TLS packet is usually
 * sent to protect some vulnerabilities in the CBC mode.
 * In that case we go to the beginning and start reading
 * the next packet.
 */
	if (_mbuffer_get_udata_size(decrypted) == 0) {
		_mbuffer_xfree(&decrypted);
		empty_fragments++;
		goto begin;
	}

	if (record.v2) {
		decrypted->htype = GNUTLS_HANDSHAKE_CLIENT_HELLO_V2;
	} else {
		uint8_t *p = _mbuffer_get_udata_ptr(decrypted);
		decrypted->htype = p[0];
	}

	ret =
	    record_add_to_buffers(session, &record, type, htype,
				  packet_sequence, decrypted);

	/* decrypted is now either deinitialized or buffered somewhere else */

	if (ret < 0)
		return gnutls_assert_val(ret);

	return ret;

      discard:
	session->internals.dtls.packets_dropped++;

	/* discard the whole received fragment. */
	bufel =
	    _mbuffer_head_pop_first(&session->internals.
				    record_recv_buffer);
	_mbuffer_xfree(&bufel);
	return gnutls_assert_val(GNUTLS_E_AGAIN);

      sanity_check_error:
	if (IS_DTLS(session)) {
		session->internals.dtls.packets_dropped++;
		ret = gnutls_assert_val(GNUTLS_E_AGAIN);
		goto cleanup;
	}

	session_unresumable(session);
	session_invalidate(session);

      cleanup:
	_mbuffer_xfree(&decrypted);
	return ret;

      recv_error:
	if (ret < 0
	    && (gnutls_error_is_fatal(ret) == 0
		|| ret == GNUTLS_E_TIMEDOUT))
		return ret;

	if (type == GNUTLS_ALERT) {	/* we were expecting close notify */
		session_invalidate(session);
		gnutls_assert();
		return 0;
	}

	if (IS_DTLS(session) && (ret == GNUTLS_E_DECRYPTION_FAILED ||
		ret == GNUTLS_E_UNSUPPORTED_VERSION_PACKET ||
		ret == GNUTLS_E_UNEXPECTED_PACKET_LENGTH ||
		ret == GNUTLS_E_UNEXPECTED_PACKET ||
		ret == GNUTLS_E_ERROR_IN_FINISHED_PACKET ||
		ret == GNUTLS_E_UNEXPECTED_HANDSHAKE_PACKET)) {
		goto discard;
	}

	session_invalidate(session);
	session_unresumable(session);

	if (ret == 0)
		return GNUTLS_E_UNEXPECTED_PACKET_LENGTH;
	else
		return ret;
}

/* This function behaves exactly like read(). The only difference is
 * that it accepts the gnutls_session_t and the content_type_t of data to
 * receive (if called by the user the Content is Userdata only)
 * It is intended to receive data, under the current session.
 *
 * The gnutls_handshake_description_t was introduced to support SSL V2.0 client hellos.
 */
ssize_t
_gnutls_recv_int(gnutls_session_t session, content_type_t type,
		 gnutls_handshake_description_t htype,
		 gnutls_packet_t *packet,
		 uint8_t * data, size_t data_size, void *seq,
		 unsigned int ms)
{
	int ret;

	if (packet == NULL && (type != GNUTLS_ALERT && type != GNUTLS_HEARTBEAT)
	    && (data_size == 0 || data == NULL))
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	if (session->internals.read_eof != 0) {
		/* if we have already read an EOF
		 */
		return 0;
	} else if (session_is_valid(session) != 0
		   || session->internals.may_not_read != 0) {
		gnutls_assert();
		return GNUTLS_E_INVALID_SESSION;
	}

	switch (session->internals.recv_state) {
	case RECV_STATE_DTLS_RETRANSMIT:
		ret = _dtls_retransmit(session);
		if (ret < 0)
			return gnutls_assert_val(ret);

		session->internals.recv_state = RECV_STATE_0;
	case RECV_STATE_0:

		_dtls_async_timer_check(session);

		if (packet == NULL) {
			/* If we have enough data in the cache do not bother receiving
			 * a new packet. (in order to flush the cache)
			 */
			ret = check_buffers(session, type, data, data_size, seq);
			if (ret != 0)
				return ret;

			ret = _gnutls_recv_in_buffers(session, type, htype, ms);
			if (ret < 0 && ret != GNUTLS_E_SESSION_EOF)
				return gnutls_assert_val(ret);

			return check_buffers(session, type, data, data_size, seq);
		} else {
			ret = check_packet_buffers(session, type, packet);
			if (ret != 0)
				return ret;

			ret = _gnutls_recv_in_buffers(session, type, -1, ms);
			if (ret < 0 && ret != GNUTLS_E_SESSION_EOF)
				return gnutls_assert_val(ret);

			return check_packet_buffers(session, type, packet);
		}
	default:
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
	}
}

/**
 * gnutls_packet_get:
 * @packet: is a #gnutls_packet_t structure.
 * @data: will contain the data present in the @packet structure (may be %NULL)
 * @sequence: the 8-bytes of the packet sequence number (may be %NULL)
 *
 * This function returns the data and sequence number associated with
 * the received packet.
 *
 * Since: 3.3.5
 **/

void gnutls_packet_get(gnutls_packet_t packet, gnutls_datum_t *data, unsigned char *sequence)
{
	if (unlikely(packet == NULL)) {
		gnutls_assert();
		if (data) {
			data->data = NULL;
			data->size = 0;
		}
	}

	if (sequence) {
		memcpy(sequence, packet->record_sequence.i, 8);
	}

	if (data) {
		data->size = packet->msg.size - packet->mark;
		data->data = packet->msg.data + packet->mark;
	}
}

/**
 * gnutls_packet_deinit:
 * @packet: is a pointer to a #gnutls_packet_st structure.
 *
 * This function will deinitialize all data associated with
 * the received packet.
 *
 * Since: 3.3.5
 **/
void gnutls_packet_deinit(gnutls_packet_t packet)
{
	gnutls_free(packet);
}

/**
 * gnutls_record_recv_packet:
 * @session: is a #gnutls_session_t structure.
 * @packet: the structure that will hold the packet data
 *
 * This is a lower-level function thatn gnutls_record_recv() and allows
 * to directly receive the whole decrypted packet. That avoids a
 * memory copy, and is mostly applicable to applications seeking high
 * performance.
 *
 * The received packet is accessed using gnutls_packet_get() and 
 * must be deinitialized using gnutls_packet_deinit(). The returned
 * packet will be %NULL if the return value is zero (EOF).
 *
 * Returns: The number of bytes received and zero on EOF (for stream
 * connections).  A negative error code is returned in case of an error.  
 *
 * Since: 3.3.5
 **/
ssize_t
gnutls_record_recv_packet(gnutls_session_t session, 
		   	  gnutls_packet_t *packet)
{
	return _gnutls_recv_int(session, GNUTLS_APPLICATION_DATA, -1, packet,
				NULL, 0, NULL,
				session->internals.record_timeout_ms);
}

/**
 * gnutls_record_send:
 * @session: is a #gnutls_session_t structure.
 * @data: contains the data to send
 * @data_size: is the length of the data
 *
 * This function has the similar semantics with send().  The only
 * difference is that it accepts a GnuTLS session, and uses different
 * error codes.
 * Note that if the send buffer is full, send() will block this
 * function.  See the send() documentation for more information.  
 *
 * You can replace the default push function which is send(), by using
 * gnutls_transport_set_push_function().
 *
 * If the EINTR is returned by the internal push function 
 * then %GNUTLS_E_INTERRUPTED will be returned. If
 * %GNUTLS_E_INTERRUPTED or %GNUTLS_E_AGAIN is returned, you must
 * call this function again, with the exact same parameters; alternatively
 * you could provide a %NULL pointer for data, and 0 for
 * size. cf. gnutls_record_get_direction(). 
 *
 * Note that in DTLS this function will return the %GNUTLS_E_LARGE_PACKET
 * error code if the send data exceed the data MTU value - as returned
 * by gnutls_dtls_get_data_mtu(). The errno value EMSGSIZE
 * also maps to %GNUTLS_E_LARGE_PACKET. 
 * Note that since 3.2.13 this function can be called under cork in DTLS
 * mode, and will refuse to send data over the MTU size by returning
 * %GNUTLS_E_LARGE_PACKET.
 *
 * Returns: The number of bytes sent, or a negative error code.  The
 *   number of bytes sent might be less than @data_size.  The maximum
 *   number of bytes this function can send in a single call depends
 *   on the negotiated maximum record size.
 **/
ssize_t
gnutls_record_send(gnutls_session_t session, const void *data,
		   size_t data_size)
{
	if (session->internals.record_flush_mode == RECORD_FLUSH) {
		return _gnutls_send_int(session, GNUTLS_APPLICATION_DATA,
					-1, EPOCH_WRITE_CURRENT, data,
					data_size, MBUFFER_FLUSH);
	} else {		/* GNUTLS_CORKED */

		int ret;

		if (IS_DTLS(session)) {
			if (data_size + session->internals.record_presend_buffer.length >
				gnutls_dtls_get_data_mtu(session)) {
				return gnutls_assert_val(GNUTLS_E_LARGE_PACKET);
			}
		}

		ret =
		    _gnutls_buffer_append_data(&session->internals.
					       record_presend_buffer, data,
					       data_size);
		if (ret < 0)
			return gnutls_assert_val(ret);

		return data_size;
	}
}

/**
 * gnutls_cork:
 * @session: is a #gnutls_session_t structure.
 *
 * If called gnutls_record_send() will no longer send partial records.
 * All queued records will be sent when gnutls_uncork() is called, or
 * when the maximum record size is reached.
 *
 * This function is safe to use with DTLS after GnuTLS 3.3.0.
 *
 * Since: 3.1.9
 **/
void gnutls_record_cork(gnutls_session_t session)
{
	session->internals.record_flush_mode = RECORD_CORKED;
}

/**
 * gnutls_uncork:
 * @session: is a #gnutls_session_t structure.
 * @flags: Could be zero or %GNUTLS_RECORD_WAIT
 *
 * This resets the effect of gnutls_cork(), and flushes any pending
 * data. If the %GNUTLS_RECORD_WAIT flag is specified then this
 * function will block until the data is sent or a fatal error
 * occurs (i.e., the function will retry on %GNUTLS_E_AGAIN and
 * %GNUTLS_E_INTERRUPTED).
 *
 * If the flag %GNUTLS_RECORD_WAIT is not specified and the function
 * is interrupted then the %GNUTLS_E_AGAIN or %GNUTLS_E_INTERRUPTED
 * errors will be returned. To obtain the data left in the corked
 * buffer use gnutls_record_check_corked().
 *
 * Returns: On success the number of transmitted data is returned, or 
 * otherwise a negative error code. 
 *
 * Since: 3.1.9
 **/
int gnutls_record_uncork(gnutls_session_t session, unsigned int flags)
{
	int ret;
	ssize_t total = 0;

	if (session->internals.record_flush_mode == RECORD_FLUSH)
		return 0;	/* nothing to be done */

	session->internals.record_flush_mode = RECORD_FLUSH;

	while (session->internals.record_presend_buffer.length > 0) {
		if (flags == GNUTLS_RECORD_WAIT) {
			do {
				ret =
				    gnutls_record_send(session,
						       session->internals.
						       record_presend_buffer.
						       data,
						       session->internals.
						       record_presend_buffer.
						       length);
			}
			while (ret < 0 && (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED));
		} else {
			ret =
			    gnutls_record_send(session,
					       session->internals.
					       record_presend_buffer.data,
					       session->internals.
					       record_presend_buffer.
					       length);
		}
		if (ret < 0)
			goto fail;

		session->internals.record_presend_buffer.data += ret;
		session->internals.record_presend_buffer.length -= ret;
		total += ret;
	}

	return total;

      fail:
	session->internals.record_flush_mode = RECORD_CORKED;
	return ret;
}

/**
 * gnutls_record_recv:
 * @session: is a #gnutls_session_t structure.
 * @data: the buffer that the data will be read into
 * @data_size: the number of requested bytes
 *
 * This function has the similar semantics with recv().  The only
 * difference is that it accepts a GnuTLS session, and uses different
 * error codes.
 * In the special case that a server requests a renegotiation, the
 * client may receive an error code of %GNUTLS_E_REHANDSHAKE.  This
 * message may be simply ignored, replied with an alert
 * %GNUTLS_A_NO_RENEGOTIATION, or replied with a new handshake,
 * depending on the client's will.
 * If %EINTR is returned by the internal push function (the default
 * is recv()) then %GNUTLS_E_INTERRUPTED will be returned.  If
 * %GNUTLS_E_INTERRUPTED or %GNUTLS_E_AGAIN is returned, you must
 * call this function again to get the data.  See also
 * gnutls_record_get_direction().
 * A server may also receive %GNUTLS_E_REHANDSHAKE when a client has
 * initiated a handshake. In that case the server can only initiate a
 * handshake or terminate the connection.
 *
 * Returns: The number of bytes received and zero on EOF (for stream
 * connections).  A negative error code is returned in case of an error.  
 * The number of bytes received might be less than the requested @data_size.
 **/
ssize_t
gnutls_record_recv(gnutls_session_t session, void *data, size_t data_size)
{
	return _gnutls_recv_int(session, GNUTLS_APPLICATION_DATA, -1, NULL,
				data, data_size, NULL,
				session->internals.record_timeout_ms);
}

/**
 * gnutls_record_recv_seq:
 * @session: is a #gnutls_session_t structure.
 * @data: the buffer that the data will be read into
 * @data_size: the number of requested bytes
 * @seq: is the packet's 64-bit sequence number. Should have space for 8 bytes.
 *
 * This function is the same as gnutls_record_recv(), except that
 * it returns in addition to data, the sequence number of the data.
 * This is useful in DTLS where record packets might be received
 * out-of-order. The returned 8-byte sequence number is an
 * integer in big-endian format and should be
 * treated as a unique message identification. 
 *
 * Returns: The number of bytes received and zero on EOF.  A negative
 *   error code is returned in case of an error.  The number of bytes
 *   received might be less than @data_size.
 *
 * Since: 3.0
 **/
ssize_t
gnutls_record_recv_seq(gnutls_session_t session, void *data,
		       size_t data_size, unsigned char *seq)
{
	return _gnutls_recv_int(session, GNUTLS_APPLICATION_DATA, -1, NULL,
				data, data_size, seq,
				session->internals.record_timeout_ms);
}

/**
 * gnutls_record_set_timeout:
 * @session: is a #gnutls_session_t structure.
 * @ms: is a timeout value in milliseconds
 *
 * This function sets the receive timeout for the record layer
 * to the provided value. Use an @ms value of zero to disable
 * timeout (the default).
 *
 * Since: 3.1.7
 **/
void gnutls_record_set_timeout(gnutls_session_t session, unsigned int ms)
{
	session->internals.record_timeout_ms = ms;
}
