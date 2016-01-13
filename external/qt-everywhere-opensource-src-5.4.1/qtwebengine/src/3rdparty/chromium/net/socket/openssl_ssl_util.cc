// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/openssl_ssl_util.h"

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "base/logging.h"
#include "crypto/openssl_util.h"
#include "net/base/net_errors.h"

namespace net {

SslSetClearMask::SslSetClearMask()
    : set_mask(0),
      clear_mask(0) {
}

void SslSetClearMask::ConfigureFlag(long flag, bool state) {
  (state ? set_mask : clear_mask) |= flag;
  // Make sure we haven't got any intersection in the set & clear options.
  DCHECK_EQ(0, set_mask & clear_mask) << flag << ":" << state;
}

namespace {

int MapOpenSSLErrorSSL() {
  // Walk down the error stack to find the SSLerr generated reason.
  unsigned long error_code;
  do {
    error_code = ERR_get_error();
    if (error_code == 0)
      return ERR_SSL_PROTOCOL_ERROR;
  } while (ERR_GET_LIB(error_code) != ERR_LIB_SSL);

  DVLOG(1) << "OpenSSL SSL error, reason: " << ERR_GET_REASON(error_code)
           << ", name: " << ERR_error_string(error_code, NULL);
  switch (ERR_GET_REASON(error_code)) {
    case SSL_R_READ_TIMEOUT_EXPIRED:
      return ERR_TIMED_OUT;
    case SSL_R_BAD_RESPONSE_ARGUMENT:
      return ERR_INVALID_ARGUMENT;
    case SSL_R_UNKNOWN_CERTIFICATE_TYPE:
    case SSL_R_UNKNOWN_CIPHER_TYPE:
    case SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE:
    case SSL_R_UNKNOWN_PKEY_TYPE:
    case SSL_R_UNKNOWN_REMOTE_ERROR_TYPE:
    case SSL_R_UNKNOWN_SSL_VERSION:
      return ERR_NOT_IMPLEMENTED;
    case SSL_R_UNSUPPORTED_SSL_VERSION:
    case SSL_R_NO_CIPHER_MATCH:
    case SSL_R_NO_SHARED_CIPHER:
    case SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY:
    case SSL_R_TLSV1_ALERT_PROTOCOL_VERSION:
    case SSL_R_UNSUPPORTED_PROTOCOL:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SSL_R_SSLV3_ALERT_BAD_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN:
    case SSL_R_TLSV1_ALERT_ACCESS_DENIED:
    case SSL_R_TLSV1_ALERT_UNKNOWN_CA:
      return ERR_BAD_SSL_CLIENT_AUTH_CERT;
    case SSL_R_BAD_DECOMPRESSION:
    case SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE:
      return ERR_SSL_DECOMPRESSION_FAILURE_ALERT;
    case SSL_R_SSLV3_ALERT_BAD_RECORD_MAC:
      return ERR_SSL_BAD_RECORD_MAC_ALERT;
    case SSL_R_TLSV1_ALERT_DECRYPT_ERROR:
      return ERR_SSL_DECRYPT_ERROR_ALERT;
    case SSL_R_TLSV1_UNRECOGNIZED_NAME:
      return ERR_SSL_UNRECOGNIZED_NAME_ALERT;
    case SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED:
      return ERR_SSL_UNSAFE_NEGOTIATION;
    case SSL_R_WRONG_NUMBER_OF_KEY_BITS:
      return ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY;
    // SSL_R_UNKNOWN_PROTOCOL is reported if premature application data is
    // received (see http://crbug.com/42538), and also if all the protocol
    // versions supported by the server were disabled in this socket instance.
    // Mapped to ERR_SSL_PROTOCOL_ERROR for compatibility with other SSL sockets
    // in the former scenario.
    case SSL_R_UNKNOWN_PROTOCOL:
    case SSL_R_SSL_HANDSHAKE_FAILURE:
    case SSL_R_DECRYPTION_FAILED:
    case SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC:
    case SSL_R_DH_PUBLIC_VALUE_LENGTH_IS_WRONG:
    case SSL_R_DIGEST_CHECK_FAILED:
    case SSL_R_DUPLICATE_COMPRESSION_ID:
    case SSL_R_ECGROUP_TOO_LARGE_FOR_CIPHER:
    case SSL_R_ENCRYPTED_LENGTH_TOO_LONG:
    case SSL_R_ERROR_IN_RECEIVED_CIPHER_LIST:
    case SSL_R_EXCESSIVE_MESSAGE_SIZE:
    case SSL_R_EXTRA_DATA_IN_MESSAGE:
    case SSL_R_GOT_A_FIN_BEFORE_A_CCS:
    case SSL_R_ILLEGAL_PADDING:
    case SSL_R_INVALID_CHALLENGE_LENGTH:
    case SSL_R_INVALID_COMMAND:
    case SSL_R_INVALID_PURPOSE:
    case SSL_R_INVALID_STATUS_RESPONSE:
    case SSL_R_INVALID_TICKET_KEYS_LENGTH:
    case SSL_R_KEY_ARG_TOO_LONG:
    case SSL_R_READ_WRONG_PACKET_TYPE:
    // SSL_do_handshake reports this error when the server responds to a
    // ClientHello with a fatal close_notify alert.
    case SSL_AD_REASON_OFFSET + SSL_AD_CLOSE_NOTIFY:
    case SSL_R_SSLV3_ALERT_UNEXPECTED_MESSAGE:
    // TODO(joth): SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE may be returned from the
    // server after receiving ClientHello if there's no common supported cipher.
    // Ideally we'd map that specific case to ERR_SSL_VERSION_OR_CIPHER_MISMATCH
    // to match the NSS implementation. See also http://goo.gl/oMtZW
    case SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE:
    case SSL_R_SSLV3_ALERT_NO_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_ILLEGAL_PARAMETER:
    case SSL_R_TLSV1_ALERT_DECODE_ERROR:
    case SSL_R_TLSV1_ALERT_DECRYPTION_FAILED:
    case SSL_R_TLSV1_ALERT_EXPORT_RESTRICTION:
    case SSL_R_TLSV1_ALERT_INTERNAL_ERROR:
    case SSL_R_TLSV1_ALERT_NO_RENEGOTIATION:
    case SSL_R_TLSV1_ALERT_RECORD_OVERFLOW:
    case SSL_R_TLSV1_ALERT_USER_CANCELLED:
      return ERR_SSL_PROTOCOL_ERROR;
    case SSL_R_CERTIFICATE_VERIFY_FAILED:
      // The only way that the certificate verify callback can fail is if
      // the leaf certificate changed during a renegotiation.
      return ERR_SSL_SERVER_CERT_CHANGED;
    default:
      LOG(WARNING) << "Unmapped error reason: " << ERR_GET_REASON(error_code);
      return ERR_FAILED;
  }
}

}  // namespace

int MapOpenSSLError(int err, const crypto::OpenSSLErrStackTracer& tracer) {
  switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      return ERR_IO_PENDING;
    case SSL_ERROR_SYSCALL:
      LOG(ERROR) << "OpenSSL SYSCALL error, earliest error code in "
                    "error queue: " << ERR_peek_error() << ", errno: "
                 << errno;
      return ERR_SSL_PROTOCOL_ERROR;
    case SSL_ERROR_SSL:
      return MapOpenSSLErrorSSL();
    default:
      // TODO(joth): Implement full mapping.
      LOG(WARNING) << "Unknown OpenSSL error " << err;
      return ERR_SSL_PROTOCOL_ERROR;
  }
}

}  // namespace net
