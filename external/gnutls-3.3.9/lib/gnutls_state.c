/*
 * Copyright (C) 2002-2013 Free Software Foundation, Inc.
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

/* Functions to manipulate the session (gnutls_int.h), and some other stuff
 * are included here. The file's name is traditionally gnutls_state even if the
 * state has been renamed to session.
 */

#include <gnutls_int.h>
#include <gnutls_errors.h>
#include <gnutls_auth.h>
#include <gnutls_num.h>
#include <gnutls_datum.h>
#include <gnutls_db.h>
#include <gnutls_record.h>
#include <gnutls_handshake.h>
#include <gnutls_dh.h>
#include <gnutls_buffers.h>
#include <gnutls_mbuffers.h>
#include <gnutls_state.h>
#include <gnutls_constate.h>
#include <auth/cert.h>
#include <auth/anon.h>
#include <auth/psk.h>
#include <algorithms.h>
#include <gnutls_extensions.h>
#include <system.h>
#include <random.h>
#include <fips.h>
#include <gnutls/dtls.h>

/* These should really be static, but src/tests.c calls them.  Make
   them public functions?  */
void
_gnutls_rsa_pms_set_version(gnutls_session_t session,
			    unsigned char major, unsigned char minor);

void
_gnutls_session_cert_type_set(gnutls_session_t session,
			      gnutls_certificate_type_t ct)
{
	_gnutls_handshake_log
	    ("HSK[%p]: Selected certificate type %s (%d)\n", session,
	     gnutls_certificate_type_get_name(ct), ct);
	session->security_parameters.cert_type = ct;
}

void
_gnutls_session_ecc_curve_set(gnutls_session_t session,
			      gnutls_ecc_curve_t c)
{
	_gnutls_handshake_log("HSK[%p]: Selected ECC curve %s (%d)\n",
			      session, gnutls_ecc_curve_get_name(c), c);
	session->security_parameters.ecc_curve = c;
}

/**
 * gnutls_cipher_get:
 * @session: is a #gnutls_session_t structure.
 *
 * Get currently used cipher.
 *
 * Returns: the currently used cipher, a #gnutls_cipher_algorithm_t
 *   type.
 **/
gnutls_cipher_algorithm_t gnutls_cipher_get(gnutls_session_t session)
{
	record_parameters_st *record_params;
	int ret;

	ret =
	    _gnutls_epoch_get(session, EPOCH_READ_CURRENT, &record_params);
	if (ret < 0)
		return gnutls_assert_val(GNUTLS_CIPHER_NULL);

	return record_params->cipher->id;
}

/**
 * gnutls_certificate_type_get:
 * @session: is a #gnutls_session_t structure.
 *
 * The certificate type is by default X.509, unless it is negotiated
 * as a TLS extension.
 *
 * Returns: the currently used #gnutls_certificate_type_t certificate
 *   type.
 **/
gnutls_certificate_type_t
gnutls_certificate_type_get(gnutls_session_t session)
{
	return session->security_parameters.cert_type;
}

/**
 * gnutls_kx_get:
 * @session: is a #gnutls_session_t structure.
 *
 * Get currently used key exchange algorithm.
 *
 * Returns: the key exchange algorithm used in the last handshake, a
 *   #gnutls_kx_algorithm_t value.
 **/
gnutls_kx_algorithm_t gnutls_kx_get(gnutls_session_t session)
{
	return session->security_parameters.kx_algorithm;
}

/**
 * gnutls_mac_get:
 * @session: is a #gnutls_session_t structure.
 *
 * Get currently used MAC algorithm.
 *
 * Returns: the currently used mac algorithm, a
 *   #gnutls_mac_algorithm_t value.
 **/
gnutls_mac_algorithm_t gnutls_mac_get(gnutls_session_t session)
{
	record_parameters_st *record_params;
	int ret;

	ret =
	    _gnutls_epoch_get(session, EPOCH_READ_CURRENT, &record_params);
	if (ret < 0)
		return gnutls_assert_val(GNUTLS_MAC_NULL);

	return record_params->mac->id;
}

/**
 * gnutls_compression_get:
 * @session: is a #gnutls_session_t structure.
 *
 * Get currently used compression algorithm.
 *
 * Returns: the currently used compression method, a
 *   #gnutls_compression_method_t value.
 **/
gnutls_compression_method_t
gnutls_compression_get(gnutls_session_t session)
{
	record_parameters_st *record_params;
	int ret;

	ret =
	    _gnutls_epoch_get(session, EPOCH_READ_CURRENT, &record_params);
	if (ret < 0)
		return gnutls_assert_val(GNUTLS_COMP_NULL);

	return record_params->compression_algorithm;
}

/* Check if the given certificate type is supported.
 * This means that it is enabled by the priority functions,
 * and a matching certificate exists.
 */
int
_gnutls_session_cert_type_supported(gnutls_session_t session,
				    gnutls_certificate_type_t cert_type)
{
	unsigned i;
	unsigned cert_found = 0;
	gnutls_certificate_credentials_t cred;

	if (session->security_parameters.entity == GNUTLS_SERVER) {
		cred = (gnutls_certificate_credentials_t)
		    _gnutls_get_cred(session, GNUTLS_CRD_CERTIFICATE);

		if (cred == NULL)
			return GNUTLS_E_UNSUPPORTED_CERTIFICATE_TYPE;

		if (cred->server_get_cert_callback == NULL
		    && cred->get_cert_callback == NULL) {
			for (i = 0; i < cred->ncerts; i++) {
				if (cred->certs[i].cert_list[0].type ==
				    cert_type) {
					cert_found = 1;
					break;
				}
			}

			if (cert_found == 0)
				/* no certificate is of that type.
				 */
				return
				    GNUTLS_E_UNSUPPORTED_CERTIFICATE_TYPE;
		}
	}

	if (session->internals.priorities.cert_type.algorithms == 0
	    && cert_type == DEFAULT_CERT_TYPE)
		return 0;

	for (i = 0; i < session->internals.priorities.cert_type.algorithms;
	     i++) {
		if (session->internals.priorities.cert_type.priority[i] ==
		    cert_type) {
			return 0;	/* ok */
		}
	}

	return GNUTLS_E_UNSUPPORTED_CERTIFICATE_TYPE;
}


/* this function deinitializes all the internal parameters stored
 * in a session struct.
 */
inline static void deinit_internal_params(gnutls_session_t session)
{
#if defined(ENABLE_DHE) || defined(ENABLE_ANON)
	if (session->internals.params.free_dh_params)
		gnutls_dh_params_deinit(session->internals.params.
					dh_params);
#endif

	_gnutls_handshake_hash_buffers_clear(session);

	memset(&session->internals.params, 0,
	       sizeof(session->internals.params));
}

/* This function will clear all the variables in internals
 * structure within the session, which depend on the current handshake.
 * This is used to allow further handshakes.
 */
static void _gnutls_handshake_internal_state_init(gnutls_session_t session)
{
	session->internals.extensions_sent_size = 0;

	/* by default no selected certificate */
	session->internals.adv_version_major = 0;
	session->internals.adv_version_minor = 0;
	session->internals.direction = 0;

	/* use out of band data for the last
	 * handshake messages received.
	 */
	session->internals.last_handshake_in = -1;
	session->internals.last_handshake_out = -1;

	session->internals.resumable = RESUME_TRUE;

	session->internals.handshake_large_loops = 0;
	session->internals.dtls.hsk_read_seq = 0;
	session->internals.dtls.hsk_write_seq = 0;
}

void _gnutls_handshake_internal_state_clear(gnutls_session_t session)
{
	_gnutls_handshake_internal_state_init(session);

	deinit_internal_params(session);

	_gnutls_epoch_gc(session);

	session->internals.handshake_endtime = 0;
	session->internals.handshake_in_progress = 0;
}

/**
 * gnutls_init:
 * @session: is a pointer to a #gnutls_session_t structure.
 * @flags: indicate if this session is to be used for server or client.
 *
 * This function initializes the current session to null. Every
 * session must be initialized before use, so internal structures can
 * be allocated.  This function allocates structures which can only
 * be free'd by calling gnutls_deinit().  Returns %GNUTLS_E_SUCCESS (0) on success.
 *
 * @flags can be one of %GNUTLS_CLIENT and %GNUTLS_SERVER. For a DTLS
 * entity, the flags %GNUTLS_DATAGRAM and  %GNUTLS_NONBLOCK are
 * also available. The latter flag will enable a non-blocking
 * operation of the DTLS timers. 
 *
 * The flag %GNUTLS_NO_REPLAY_PROTECTION will disable any 
 * replay protection in DTLS mode. That must only used when 
 * replay protection is achieved using other means.
 *
 * Note that since version 3.1.2 this function enables some common
 * TLS extensions such as session tickets and OCSP certificate status
 * request in client side by default. To prevent that use the %GNUTLS_NO_EXTENSIONS
 * flag.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int gnutls_init(gnutls_session_t * session, unsigned int flags)
{
	int ret;
	record_parameters_st *epoch;
	
	FAIL_IF_LIB_ERROR;

	*session = gnutls_calloc(1, sizeof(struct gnutls_session_int));
	if (*session == NULL)
		return GNUTLS_E_MEMORY_ERROR;

	ret = _gnutls_epoch_alloc(*session, 0, &epoch);
	if (ret < 0) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	/* Set all NULL algos on epoch 0 */
	_gnutls_epoch_set_null_algos(*session, epoch);

	(*session)->security_parameters.epoch_next = 1;

	(*session)->security_parameters.entity =
	    (flags & GNUTLS_SERVER ? GNUTLS_SERVER : GNUTLS_CLIENT);

	/* the default certificate type for TLS */
	(*session)->security_parameters.cert_type = DEFAULT_CERT_TYPE;

	/* Initialize buffers */
	_gnutls_buffer_init(&(*session)->internals.handshake_hash_buffer);
	_gnutls_buffer_init(&(*session)->internals.hb_remote_data);
	_gnutls_buffer_init(&(*session)->internals.hb_local_data);
	_gnutls_buffer_init(&(*session)->internals.record_presend_buffer);

	_mbuffer_head_init(&(*session)->internals.record_buffer);
	_mbuffer_head_init(&(*session)->internals.record_send_buffer);
	_mbuffer_head_init(&(*session)->internals.record_recv_buffer);

	_mbuffer_head_init(&(*session)->internals.handshake_send_buffer);
	_gnutls_handshake_recv_buffer_init(*session);

	(*session)->internals.expire_time = DEFAULT_EXPIRE_TIME;	/* one hour default */

	gnutls_handshake_set_max_packet_length((*session),
					       MAX_HANDSHAKE_PACKET_SIZE);

	/* set the socket pointers to -1;
	 */
	(*session)->internals.transport_recv_ptr =
	    (gnutls_transport_ptr_t) - 1;
	(*session)->internals.transport_send_ptr =
	    (gnutls_transport_ptr_t) - 1;

	/* set the default maximum record size for TLS
	 */
	(*session)->security_parameters.max_record_recv_size =
	    DEFAULT_MAX_RECORD_SIZE;
	(*session)->security_parameters.max_record_send_size =
	    DEFAULT_MAX_RECORD_SIZE;

	/* everything else not initialized here is initialized
	 * as NULL or 0. This is why calloc is used.
	 */

	_gnutls_handshake_internal_state_init(*session);

	/* emulate old gnutls behavior for old applications that do not use the priority_*
	 * functions.
	 */
	(*session)->internals.priorities.sr = SR_PARTIAL;

#ifdef HAVE_WRITEV
	gnutls_transport_set_vec_push_function(*session, system_writev);
#else
	gnutls_transport_set_push_function(*session, system_write);
#endif
	gnutls_transport_set_pull_function(*session, system_read);
	gnutls_transport_set_errno_function(*session, system_errno);
	gnutls_transport_set_pull_timeout_function(*session,
						   system_recv_timeout);

	(*session)->internals.hb_retrans_timeout_ms = 1000;
	(*session)->internals.hb_total_timeout_ms = 60000;

	if (flags & GNUTLS_DATAGRAM) {
		(*session)->internals.dtls.mtu = DTLS_DEFAULT_MTU;
		(*session)->internals.transport = GNUTLS_DGRAM;

		(*session)->internals.dtls.retrans_timeout_ms = 1000;
		(*session)->internals.dtls.total_timeout_ms = 60000;
	} else
		(*session)->internals.transport = GNUTLS_STREAM;

	if (flags & GNUTLS_NONBLOCK)
		(*session)->internals.dtls.blocking = 0;
	else
		(*session)->internals.dtls.blocking = 1;

	/* Enable useful extensions */
	if ((flags & GNUTLS_CLIENT) && !(flags & GNUTLS_NO_EXTENSIONS)) {
#ifdef ENABLE_SESSION_TICKETS
		gnutls_session_ticket_enable_client(*session);
#endif
#ifdef ENABLE_OCSP
		gnutls_ocsp_status_request_enable_client(*session, NULL, 0,
							 NULL);
#endif
	}

	if (flags & GNUTLS_NO_REPLAY_PROTECTION)
		(*session)->internals.no_replay_protection = 1;

	return 0;
}

/* returns RESUME_FALSE or RESUME_TRUE.
 */
int _gnutls_session_is_resumable(gnutls_session_t session)
{
	return session->internals.resumable;
}


/**
 * gnutls_deinit:
 * @session: is a #gnutls_session_t structure.
 *
 * This function clears all buffers associated with the @session.
 * This function will also remove session data from the session
 * database if the session was terminated abnormally.
 **/
void gnutls_deinit(gnutls_session_t session)
{
	unsigned int i;

	if (session == NULL)
		return;

	_gnutls_rnd_refresh();

	/* remove auth info firstly */
	_gnutls_free_auth_info(session);

	_gnutls_handshake_internal_state_clear(session);
	_gnutls_handshake_io_buffer_clear(session);
	_gnutls_ext_free_session_data(session);

	for (i = 0; i < MAX_EPOCH_INDEX; i++)
		if (session->record_parameters[i] != NULL) {
			_gnutls_epoch_free(session,
					   session->record_parameters[i]);
			session->record_parameters[i] = NULL;
		}

	_gnutls_buffer_clear(&session->internals.handshake_hash_buffer);
	_gnutls_buffer_clear(&session->internals.hb_remote_data);
	_gnutls_buffer_clear(&session->internals.hb_local_data);
	_gnutls_buffer_clear(&session->internals.record_presend_buffer);

	_mbuffer_head_clear(&session->internals.record_buffer);
	_mbuffer_head_clear(&session->internals.record_recv_buffer);
	_mbuffer_head_clear(&session->internals.record_send_buffer);

	gnutls_credentials_clear(session);
	_gnutls_selected_certs_deinit(session);

	gnutls_pk_params_release(&session->key.ecdh_params);
	gnutls_pk_params_release(&session->key.dh_params);
	zrelease_temp_mpi_key(&session->key.ecdh_x);
	zrelease_temp_mpi_key(&session->key.ecdh_y);

	zrelease_temp_mpi_key(&session->key.client_Y);

	zrelease_temp_mpi_key(&session->key.srp_p);
	zrelease_temp_mpi_key(&session->key.srp_g);
	zrelease_temp_mpi_key(&session->key.srp_key);

	zrelease_temp_mpi_key(&session->key.u);
	zrelease_temp_mpi_key(&session->key.a);
	zrelease_temp_mpi_key(&session->key.x);
	zrelease_temp_mpi_key(&session->key.A);
	zrelease_temp_mpi_key(&session->key.B);
	zrelease_temp_mpi_key(&session->key.b);

	/* RSA */
	zrelease_temp_mpi_key(&session->key.rsa[0]);
	zrelease_temp_mpi_key(&session->key.rsa[1]);

	_gnutls_free_temp_key_datum(&session->key.key);

	gnutls_free(session);
}

/* Returns the minimum prime bits that are acceptable.
 */
int _gnutls_dh_set_peer_public(gnutls_session_t session, bigint_t public)
{
	dh_info_st *dh;
	int ret;

	switch (gnutls_auth_get_type(session)) {
	case GNUTLS_CRD_ANON:
		{
			anon_auth_info_t info;
			info = _gnutls_get_auth_info(session, GNUTLS_CRD_ANON);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			dh = &info->dh;
			break;
		}
	case GNUTLS_CRD_PSK:
		{
			psk_auth_info_t info;
			info = _gnutls_get_auth_info(session, GNUTLS_CRD_PSK);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			dh = &info->dh;
			break;
		}
	case GNUTLS_CRD_CERTIFICATE:
		{
			cert_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_CERTIFICATE);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			dh = &info->dh;
			break;
		}
	default:
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	if (dh->public_key.data)
		_gnutls_free_datum(&dh->public_key);

	ret = _gnutls_mpi_dprint_lz(public, &dh->public_key);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return 0;
}

int _gnutls_dh_set_secret_bits(gnutls_session_t session, unsigned bits)
{
	switch (gnutls_auth_get_type(session)) {
	case GNUTLS_CRD_ANON:
		{
			anon_auth_info_t info;
			info = _gnutls_get_auth_info(session, GNUTLS_CRD_ANON);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;
			info->dh.secret_bits = bits;
			break;
		}
	case GNUTLS_CRD_PSK:
		{
			psk_auth_info_t info;
			info = _gnutls_get_auth_info(session, GNUTLS_CRD_PSK);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;
			info->dh.secret_bits = bits;
			break;
		}
	case GNUTLS_CRD_CERTIFICATE:
		{
			cert_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_CERTIFICATE);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			info->dh.secret_bits = bits;
			break;
	default:
			gnutls_assert();
			return GNUTLS_E_INTERNAL_ERROR;
		}
	}

	return 0;
}

/* Sets the prime and the generator in the auth info structure.
 */
int
_gnutls_dh_set_group(gnutls_session_t session, bigint_t gen,
		     bigint_t prime)
{
	dh_info_st *dh;
	int ret;

	switch (gnutls_auth_get_type(session)) {
	case GNUTLS_CRD_ANON:
		{
			anon_auth_info_t info;
			info = _gnutls_get_auth_info(session, GNUTLS_CRD_ANON);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			dh = &info->dh;
			break;
		}
	case GNUTLS_CRD_PSK:
		{
			psk_auth_info_t info;
			info = _gnutls_get_auth_info(session, GNUTLS_CRD_PSK);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			dh = &info->dh;
			break;
		}
	case GNUTLS_CRD_CERTIFICATE:
		{
			cert_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_CERTIFICATE);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			dh = &info->dh;
			break;
		}
	default:
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	if (dh->prime.data)
		_gnutls_free_datum(&dh->prime);

	if (dh->generator.data)
		_gnutls_free_datum(&dh->generator);

	/* prime
	 */
	ret = _gnutls_mpi_dprint_lz(prime, &dh->prime);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	/* generator
	 */
	ret = _gnutls_mpi_dprint_lz(gen, &dh->generator);
	if (ret < 0) {
		gnutls_assert();
		_gnutls_free_datum(&dh->prime);
		return ret;
	}

	return 0;
}

#ifdef ENABLE_OPENPGP
/**
 * gnutls_openpgp_send_cert:
 * @session: is a pointer to a #gnutls_session_t structure.
 * @status: is one of GNUTLS_OPENPGP_CERT, or GNUTLS_OPENPGP_CERT_FINGERPRINT
 *
 * This function will order gnutls to send the key fingerprint
 * instead of the key in the initial handshake procedure. This should
 * be used with care and only when there is indication or knowledge
 * that the server can obtain the client's key.
 **/
void
gnutls_openpgp_send_cert(gnutls_session_t session,
			 gnutls_openpgp_crt_status_t status)
{
	session->internals.pgp_fingerprint = status;
}
#endif

/**
 * gnutls_certificate_send_x509_rdn_sequence:
 * @session: is a pointer to a #gnutls_session_t structure.
 * @status: is 0 or 1
 *
 * If status is non zero, this function will order gnutls not to send
 * the rdnSequence in the certificate request message. That is the
 * server will not advertise its trusted CAs to the peer. If status
 * is zero then the default behaviour will take effect, which is to
 * advertise the server's trusted CAs.
 *
 * This function has no effect in clients, and in authentication
 * methods other than certificate with X.509 certificates.
 **/
void
gnutls_certificate_send_x509_rdn_sequence(gnutls_session_t session,
					  int status)
{
	session->internals.ignore_rdn_sequence = status;
}

#ifdef ENABLE_OPENPGP
int _gnutls_openpgp_send_fingerprint(gnutls_session_t session)
{
	return session->internals.pgp_fingerprint;
}
#endif

/*-
 * _gnutls_record_set_default_version - Used to set the default version for the first record packet
 * @session: is a #gnutls_session_t structure.
 * @major: is a tls major version
 * @minor: is a tls minor version
 *
 * This function sets the default version that we will use in the first
 * record packet (client hello). This function is only useful to people
 * that know TLS internals and want to debug other implementations.
 -*/
void
_gnutls_record_set_default_version(gnutls_session_t session,
				   unsigned char major,
				   unsigned char minor)
{
	session->internals.default_record_version[0] = major;
	session->internals.default_record_version[1] = minor;
}

/**
 * gnutls_handshake_set_private_extensions:
 * @session: is a #gnutls_session_t structure.
 * @allow: is an integer (0 or 1)
 *
 * This function will enable or disable the use of private cipher
 * suites (the ones that start with 0xFF).  By default or if @allow
 * is 0 then these cipher suites will not be advertised nor used.
 *
 * Currently GnuTLS does not include such cipher-suites or
 * compression algorithms.
 *
 * Enabling the private ciphersuites when talking to other than
 * gnutls servers and clients may cause interoperability problems.
 **/
void
gnutls_handshake_set_private_extensions(gnutls_session_t session,
					int allow)
{
	session->internals.enable_private = allow;
}

inline static int
_gnutls_cal_PRF_A(const mac_entry_st * me,
		  const void *secret, int secret_size,
		  const void *seed, int seed_size, void *result)
{
	int ret;

	ret =
	    _gnutls_mac_fast(me->id, secret, secret_size, seed, seed_size,
			     result);
	if (ret < 0)
		return gnutls_assert_val(ret);

	return 0;
}

#define MAX_SEED_SIZE 200

/* Produces "total_bytes" bytes using the hash algorithm specified.
 * (used in the PRF function)
 */
static int
P_hash(gnutls_mac_algorithm_t algorithm,
       const uint8_t * secret, int secret_size,
       const uint8_t * seed, int seed_size, int total_bytes, uint8_t * ret)
{

	mac_hd_st td2;
	int i, times, how, blocksize, A_size;
	uint8_t final[MAX_HASH_SIZE], Atmp[MAX_SEED_SIZE];
	int output_bytes, result;
	const mac_entry_st *me = mac_to_entry(algorithm);

	blocksize = _gnutls_mac_get_algo_len(me);

	if (seed_size > MAX_SEED_SIZE || total_bytes <= 0 || blocksize == 0) {
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	output_bytes = 0;
	do {
		output_bytes += blocksize;
	}
	while (output_bytes < total_bytes);

	/* calculate A(0) */

	memcpy(Atmp, seed, seed_size);
	A_size = seed_size;

	times = output_bytes / blocksize;

	for (i = 0; i < times; i++) {
		result = _gnutls_mac_init(&td2, me, secret, secret_size);
		if (result < 0) {
			gnutls_assert();
			return result;
		}

		/* here we calculate A(i+1) */
		if ((result =
		     _gnutls_cal_PRF_A(me, secret, secret_size, Atmp,
				       A_size, Atmp)) < 0) {
			gnutls_assert();
			_gnutls_mac_deinit(&td2, final);
			return result;
		}

		A_size = blocksize;

		_gnutls_mac(&td2, Atmp, A_size);
		_gnutls_mac(&td2, seed, seed_size);
		_gnutls_mac_deinit(&td2, final);

		if ((1 + i) * blocksize < total_bytes) {
			how = blocksize;
		} else {
			how = total_bytes - (i) * blocksize;
		}

		if (how > 0) {
			memcpy(&ret[i * blocksize], final, how);
		}
	}

	return 0;
}

#define MAX_PRF_BYTES 200

/* This function operates as _gnutls_PRF(), but does not require
 * a pointer to the current session. It takes the @mac algorithm
 * explicitly. For legacy TLS/SSL sessions before TLS 1.2 the MAC
 * must be set to %GNUTLS_MAC_UNKNOWN.
 */
static int
_gnutls_PRF_raw(gnutls_mac_algorithm_t mac,
	    	const uint8_t * secret, unsigned int secret_size,
	    	const char *label, int label_size, const uint8_t * seed,
	    	int seed_size, int total_bytes, void *ret)
{
	int l_s, s_seed_size;
	const uint8_t *s1, *s2;
	uint8_t s_seed[MAX_SEED_SIZE];
	uint8_t o1[MAX_PRF_BYTES], o2[MAX_PRF_BYTES];
	int result;

	if (total_bytes > MAX_PRF_BYTES) {
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}
	/* label+seed = s_seed */
	s_seed_size = seed_size + label_size;

	if (s_seed_size > MAX_SEED_SIZE) {
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	memcpy(s_seed, label, label_size);
	memcpy(&s_seed[label_size], seed, seed_size);

	if (mac != GNUTLS_MAC_UNKNOWN) {
		result =
		    P_hash(mac, secret, secret_size,
		    	   s_seed, s_seed_size,
			   total_bytes, ret);
		if (result < 0) {
			gnutls_assert();
			return result;
		}
	} else {
		l_s = secret_size / 2;

		s1 = &secret[0];
		s2 = &secret[l_s];

		if (secret_size % 2 != 0) {
			l_s++;
		}

		result =
		    P_hash(GNUTLS_MAC_MD5, s1, l_s, s_seed, s_seed_size,
			   total_bytes, o1);
		if (result < 0) {
			gnutls_assert();
			return result;
		}

		result =
		    P_hash(GNUTLS_MAC_SHA1, s2, l_s, s_seed, s_seed_size,
			   total_bytes, o2);
		if (result < 0) {
			gnutls_assert();
			return result;
		}

		memxor(o1, o2, total_bytes);

		memcpy(ret, o1, total_bytes);
	}

	return 0;		/* ok */
}

/* The PRF function expands a given secret 
 * needed by the TLS specification. ret must have a least total_bytes
 * available.
 */
int
_gnutls_PRF(gnutls_session_t session,
	    const uint8_t * secret, unsigned int secret_size,
	    const char *label, int label_size, const uint8_t * seed,
	    int seed_size, int total_bytes, void *ret)
{
	const version_entry_st *ver = get_version(session);

	if (_gnutls_version_has_selectable_prf(ver)) {
		return _gnutls_PRF_raw(
			_gnutls_cipher_suite_get_prf(session->security_parameters.cipher_suite),
			secret, secret_size,
			label, label_size,
			seed, seed_size,
			total_bytes,
			ret);
	} else {
		return _gnutls_PRF_raw(
			GNUTLS_MAC_UNKNOWN,
			secret, secret_size,
			label, label_size,
			seed, seed_size,
			total_bytes,
			ret);
	}
}

#ifdef ENABLE_FIPS140
int
_gnutls_prf_raw(gnutls_mac_algorithm_t mac,
	        size_t master_size, const void *master,
	        size_t label_size, const char *label,
	        size_t seed_size, const char *seed, size_t outsize,
	        char *out);

/*-
 * _gnutls_prf_raw:
 * @mac: the MAC algorithm to use, set to %GNUTLS_MAC_UNKNOWN for the TLS1.0 mac
 * @master_size: length of the @master variable.
 * @master: the master secret used in PRF computation
 * @label_size: length of the @label variable.
 * @label: label used in PRF computation, typically a short string.
 * @seed_size: length of the @seed variable.
 * @seed: optional extra data to seed the PRF with.
 * @outsize: size of pre-allocated output buffer to hold the output.
 * @out: pre-allocated buffer to hold the generated data.
 *
 * Apply the TLS Pseudo-Random-Function (PRF) on the master secret
 * and the provided data.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 -*/
int
_gnutls_prf_raw(gnutls_mac_algorithm_t mac,
	        size_t master_size, const void *master,
	        size_t label_size, const char *label,
	        size_t seed_size, const char *seed, size_t outsize,
	        char *out)
{
	return _gnutls_PRF_raw(mac,
			  master, master_size,
			  label, label_size,
			  (uint8_t *) seed, seed_size,
			  outsize, out);

}
#endif

/**
 * gnutls_prf_raw:
 * @session: is a #gnutls_session_t structure.
 * @label_size: length of the @label variable.
 * @label: label used in PRF computation, typically a short string.
 * @seed_size: length of the @seed variable.
 * @seed: optional extra data to seed the PRF with.
 * @outsize: size of pre-allocated output buffer to hold the output.
 * @out: pre-allocated buffer to hold the generated data.
 *
 * Apply the TLS Pseudo-Random-Function (PRF) on the master secret
 * and the provided data.
 *
 * The @label variable usually contains a string denoting the purpose
 * for the generated data.  The @seed usually contains data such as the
 * client and server random, perhaps together with some additional
 * data that is added to guarantee uniqueness of the output for a
 * particular purpose.
 *
 * Because the output is not guaranteed to be unique for a particular
 * session unless @seed includes the client random and server random
 * fields (the PRF would output the same data on another connection
 * resumed from the first one), it is not recommended to use this
 * function directly.  The gnutls_prf() function seeds the PRF with the
 * client and server random fields directly, and is recommended if you
 * want to generate pseudo random data unique for each session.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_prf_raw(gnutls_session_t session,
	       size_t label_size,
	       const char *label,
	       size_t seed_size, const char *seed, size_t outsize,
	       char *out)
{
	int ret;

	ret = _gnutls_PRF(session,
			  session->security_parameters.master_secret,
			  GNUTLS_MASTER_SIZE,
			  label,
			  label_size, (uint8_t *) seed, seed_size, outsize,
			  out);

	return ret;
}

/**
 * gnutls_prf:
 * @session: is a #gnutls_session_t structure.
 * @label_size: length of the @label variable.
 * @label: label used in PRF computation, typically a short string.
 * @server_random_first: non-zero if server random field should be first in seed
 * @extra_size: length of the @extra variable.
 * @extra: optional extra data to seed the PRF with.
 * @outsize: size of pre-allocated output buffer to hold the output.
 * @out: pre-allocated buffer to hold the generated data.
 *
 * Applies the TLS Pseudo-Random-Function (PRF) on the master secret
 * and the provided data, seeded with the client and server random fields,
 * as specified in RFC5705.
 *
 * The @label variable usually contains a string denoting the purpose
 * for the generated data.  The @server_random_first indicates whether
 * the client random field or the server random field should be first
 * in the seed.  Non-zero indicates that the server random field is first,
 * 0 that the client random field is first.
 *
 * The @extra variable can be used to add more data to the seed, after
 * the random variables.  It can be used to make sure the
 * generated output is strongly connected to some additional data
 * (e.g., a string used in user authentication).
 *
 * The output is placed in @out, which must be pre-allocated.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_prf(gnutls_session_t session,
	   size_t label_size,
	   const char *label,
	   int server_random_first,
	   size_t extra_size, const char *extra, size_t outsize, char *out)
{
	int ret;
	uint8_t *seed;
	size_t seedsize = 2 * GNUTLS_RANDOM_SIZE + extra_size;

	seed = gnutls_malloc(seedsize);
	if (!seed) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	memcpy(seed, server_random_first ?
	       session->security_parameters.server_random :
	       session->security_parameters.client_random,
	       GNUTLS_RANDOM_SIZE);
	memcpy(seed + GNUTLS_RANDOM_SIZE,
	       server_random_first ? session->security_parameters.
	       client_random : session->security_parameters.server_random,
	       GNUTLS_RANDOM_SIZE);

	memcpy(seed + 2 * GNUTLS_RANDOM_SIZE, extra, extra_size);

	ret =
	    _gnutls_PRF(session,
			session->security_parameters.master_secret,
			GNUTLS_MASTER_SIZE, label, label_size, seed,
			seedsize, outsize, out);

	gnutls_free(seed);

	return ret;
}

/**
 * gnutls_session_is_resumed:
 * @session: is a #gnutls_session_t structure.
 *
 * Check whether session is resumed or not.
 *
 * Returns: non zero if this session is resumed, or a zero if this is
 *   a new session.
 **/
int gnutls_session_is_resumed(gnutls_session_t session)
{
	if (session->security_parameters.entity == GNUTLS_CLIENT) {
		if (session->security_parameters.session_id_size > 0 &&
		    session->security_parameters.session_id_size ==
		    session->internals.resumed_security_parameters.
		    session_id_size
		    && memcmp(session->security_parameters.session_id,
			      session->
			      internals.resumed_security_parameters.
			      session_id,
			      session->security_parameters.
			      session_id_size) == 0)
			return 1;
	} else {
		if (session->internals.resumed != RESUME_FALSE)
			return 1;
	}

	return 0;
}

/**
 * gnutls_session_resumption_requested:
 * @session: is a #gnutls_session_t structure.
 *
 * Check whether the client has asked for session resumption.
 * This function is valid only on server side.
 *
 * Returns: non zero if session resumption was asked, or a zero if not.
 **/
int gnutls_session_resumption_requested(gnutls_session_t session)
{
	if (session->security_parameters.entity == GNUTLS_CLIENT) {
		return 0;
	} else {
		return session->internals.resumption_requested;
	}
}

/*-
 * _gnutls_session_is_psk - Used to check whether this session uses PSK kx
 * @session: is a #gnutls_session_t structure.
 *
 * This function will return non zero if this session uses a PSK key
 * exchange algorithm.
 -*/
int _gnutls_session_is_psk(gnutls_session_t session)
{
	gnutls_kx_algorithm_t kx;

	kx = _gnutls_cipher_suite_get_kx_algo(session->security_parameters.
					      cipher_suite);
	if (kx == GNUTLS_KX_PSK || kx == GNUTLS_KX_DHE_PSK
	    || kx == GNUTLS_KX_RSA_PSK)
		return 1;

	return 0;
}

/*-
 * _gnutls_session_is_ecc - Used to check whether this session uses ECC kx
 * @session: is a #gnutls_session_t structure.
 *
 * This function will return non zero if this session uses an elliptic
 * curves key exchange exchange algorithm.
 -*/
int _gnutls_session_is_ecc(gnutls_session_t session)
{
	gnutls_kx_algorithm_t kx;

	/* We get the key exchange algorithm through the ciphersuite because
	 * the negotiated key exchange might not have been set yet.
	 */
	kx = _gnutls_cipher_suite_get_kx_algo(session->security_parameters.
					      cipher_suite);

	return _gnutls_kx_is_ecc(kx);
}

/**
 * gnutls_session_get_ptr:
 * @session: is a #gnutls_session_t structure.
 *
 * Get user pointer for session.  Useful in callbacks.  This is the
 *   pointer set with gnutls_session_set_ptr().
 *
 * Returns: the user given pointer from the session structure, or
 *   %NULL if it was never set.
 **/
void *gnutls_session_get_ptr(gnutls_session_t session)
{
	return session->internals.user_ptr;
}

/**
 * gnutls_session_set_ptr:
 * @session: is a #gnutls_session_t structure.
 * @ptr: is the user pointer
 *
 * This function will set (associate) the user given pointer @ptr to
 * the session structure.  This pointer can be accessed with
 * gnutls_session_get_ptr().
 **/
void gnutls_session_set_ptr(gnutls_session_t session, void *ptr)
{
	session->internals.user_ptr = ptr;
}


/**
 * gnutls_record_get_direction:
 * @session: is a #gnutls_session_t structure.
 *
 * This function provides information about the internals of the
 * record protocol and is only useful if a prior gnutls function call
 * (e.g.  gnutls_handshake()) was interrupted for some reason, that
 * is, if a function returned %GNUTLS_E_INTERRUPTED or
 * %GNUTLS_E_AGAIN.  In such a case, you might want to call select()
 * or poll() before calling the interrupted gnutls function again.  To
 * tell you whether a file descriptor should be selected for either
 * reading or writing, gnutls_record_get_direction() returns 0 if the
 * interrupted function was trying to read data, and 1 if it was
 * trying to write data.
 *
 * Returns: 0 if trying to read data, 1 if trying to write data.
 **/
int gnutls_record_get_direction(gnutls_session_t session)
{
	return session->internals.direction;
}

/*-
 * _gnutls_rsa_pms_set_version - Sets a version to be used at the RSA PMS
 * @session: is a #gnutls_session_t structure.
 * @major: is the major version to use
 * @minor: is the minor version to use
 *
 * This function will set the given version number to be used at the
 * RSA PMS secret. This is only useful to clients, which want to
 * test server's capabilities.
 -*/
void
_gnutls_rsa_pms_set_version(gnutls_session_t session,
			    unsigned char major, unsigned char minor)
{
	session->internals.rsa_pms_version[0] = major;
	session->internals.rsa_pms_version[1] = minor;
}

/**
 * gnutls_handshake_set_post_client_hello_function:
 * @session: is a #gnutls_session_t structure.
 * @func: is the function to be called
 *
 * This function will set a callback to be called after the client
 * hello has been received (callback valid in server side only). This
 * allows the server to adjust settings based on received extensions.
 *
 * Those settings could be ciphersuites, requesting certificate, or
 * anything else except for version negotiation (this is done before
 * the hello message is parsed).
 *
 * This callback must return 0 on success or a gnutls error code to
 * terminate the handshake.
 *
 * Since GnuTLS 3.3.5 the callback is
 * allowed to return %GNUTLS_E_AGAIN or %GNUTLS_E_INTERRUPTED to
 * put the handshake on hold. In that case gnutls_handshake()
 * will return %GNUTLS_E_INTERRUPTED and can be resumed when needed.
 *
 * Warning: You should not use this function to terminate the
 * handshake based on client input unless you know what you are
 * doing. Before the handshake is finished there is no way to know if
 * there is a man-in-the-middle attack being performed.
 **/
void
gnutls_handshake_set_post_client_hello_function(gnutls_session_t session,
						gnutls_handshake_post_client_hello_func
						func)
{
	session->internals.user_hello_func = func;
}


/**
 * gnutls_session_enable_compatibility_mode:
 * @session: is a #gnutls_session_t structure.
 *
 * This function can be used to disable certain (security) features in
 * TLS in order to maintain maximum compatibility with buggy
 * clients. Because several trade-offs with security are enabled,
 * if required they will be reported through the audit subsystem.
 *
 * Normally only servers that require maximum compatibility with
 * everything out there, need to call this function.
 *
 * Note that this function must be called after any call to gnutls_priority
 * functions.
 **/
void gnutls_session_enable_compatibility_mode(gnutls_session_t session)
{
	ENABLE_COMPAT(&session->internals.priorities);
}

/**
 * gnutls_session_channel_binding:
 * @session: is a #gnutls_session_t structure.
 * @cbtype: an #gnutls_channel_binding_t enumeration type
 * @cb: output buffer array with data
 *
 * Extract given channel binding data of the @cbtype (e.g.,
 * %GNUTLS_CB_TLS_UNIQUE) type.
 *
 * Returns: %GNUTLS_E_SUCCESS on success,
 * %GNUTLS_E_UNIMPLEMENTED_FEATURE if the @cbtype is unsupported,
 * %GNUTLS_E_CHANNEL_BINDING_NOT_AVAILABLE if the data is not
 * currently available, or an error code.
 *
 * Since: 2.12.0
 **/
int
gnutls_session_channel_binding(gnutls_session_t session,
			       gnutls_channel_binding_t cbtype,
			       gnutls_datum_t * cb)
{
	if (cbtype != GNUTLS_CB_TLS_UNIQUE)
		return GNUTLS_E_UNIMPLEMENTED_FEATURE;

	if (!session->internals.initial_negotiation_completed)
		return GNUTLS_E_CHANNEL_BINDING_NOT_AVAILABLE;

	cb->size = session->internals.cb_tls_unique_len;
	cb->data = gnutls_malloc(cb->size);
	if (cb->data == NULL)
		return GNUTLS_E_MEMORY_ERROR;

	memcpy(cb->data, session->internals.cb_tls_unique, cb->size);

	return 0;
}

/**
 * gnutls_ecc_curve_get:
 * @session: is a #gnutls_session_t structure.
 *
 * Returns the currently used elliptic curve. Only valid
 * when using an elliptic curve ciphersuite.
 *
 * Returns: the currently used curve, a #gnutls_ecc_curve_t
 *   type.
 *
 * Since: 3.0
 **/
gnutls_ecc_curve_t gnutls_ecc_curve_get(gnutls_session_t session)
{
	return _gnutls_session_ecc_curve_get(session);
}

/**
 * gnutls_protocol_get_version:
 * @session: is a #gnutls_session_t structure.
 *
 * Get TLS version, a #gnutls_protocol_t value.
 *
 * Returns: The version of the currently used protocol.
 **/
gnutls_protocol_t gnutls_protocol_get_version(gnutls_session_t session)
{
	return get_num_version(session);
}

/**
 * gnutls_session_get_random:
 * @session: is a #gnutls_session_t structure.
 * @client: the client part of the random
 * @server: the server part of the random
 *
 * This function returns pointers to the client and server
 * random fields used in the TLS handshake. The pointers are
 * not to be modified or deallocated.
 *
 * If a client random value has not yet been established, the output
 * will be garbage.
 *
 * Since: 3.0
 **/
void
gnutls_session_get_random(gnutls_session_t session,
			  gnutls_datum_t * client, gnutls_datum_t * server)
{
	if (client) {
		client->data = session->security_parameters.client_random;
		client->size =
		    sizeof(session->security_parameters.client_random);
	}

	if (server) {
		server->data = session->security_parameters.server_random;
		server->size =
		    sizeof(session->security_parameters.server_random);
	}
}

unsigned int timespec_sub_ms(struct timespec *a, struct timespec *b)
{
	return (a->tv_sec * 1000 + a->tv_nsec / (1000 * 1000) -
		(b->tv_sec * 1000 + b->tv_nsec / (1000 * 1000)));
}

/**
 * gnutls_handshake_set_random:
 * @session: is a #gnutls_session_t structure.
 * @random: a random value of 32-bytes
 *
 * This function will explicitly set the server or client hello 
 * random value in the subsequent TLS handshake. The random value 
 * should be a 32-byte value.
 *
 * Note that this function should not normally be used as gnutls
 * will select automatically a random value for the handshake.
 *
 * This function should not be used when resuming a session.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 *
 * Since 3.1.9
 **/
int
gnutls_handshake_set_random(gnutls_session_t session,
			    const gnutls_datum_t * random)
{
	if (random->size != GNUTLS_RANDOM_SIZE)
		return GNUTLS_E_INVALID_REQUEST;

	session->internals.sc_random_set = 1;
	if (session->security_parameters.entity == GNUTLS_CLIENT)
		memcpy(session->internals.resumed_security_parameters.
		       client_random, random->data, random->size);
	else
		memcpy(session->internals.resumed_security_parameters.
		       server_random, random->data, random->size);

	return 0;
}

/**
 * gnutls_handshake_set_hook_function:
 * @session: is a #gnutls_session_t structure
 * @htype: the %gnutls_handshake_description_t of the message to hook at
 * @post: %GNUTLS_HOOK_* depending on when the hook function should be called
 * @func: is the function to be called
 *
 * This function will set a callback to be called after or before the specified
 * handshake message has been received or generated. This is a
 * generalization of gnutls_handshake_set_post_client_hello_function().
 *
 * To call the hook function prior to the message being sent/generated use
 * %GNUTLS_HOOK_PRE as @post parameter, %GNUTLS_HOOK_POST to call
 * after, and %GNUTLS_HOOK_BOTH for both cases.
 *
 * This callback must return 0 on success or a gnutls error code to
 * terminate the handshake.
 *
 * Note to hook at all handshake messages use an @htype of %GNUTLS_HANDSHAKE_ANY.
 *
 * Warning: You should not use this function to terminate the
 * handshake based on client input unless you know what you are
 * doing. Before the handshake is finished there is no way to know if
 * there is a man-in-the-middle attack being performed.
 **/
void
gnutls_handshake_set_hook_function(gnutls_session_t session,
				   unsigned int htype,
				   int post,
				   gnutls_handshake_hook_func func)
{
	session->internals.h_hook = func;
	session->internals.h_type = htype;
	session->internals.h_post = post;
}
