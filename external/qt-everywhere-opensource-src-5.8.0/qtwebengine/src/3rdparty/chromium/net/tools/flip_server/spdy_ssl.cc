// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/spdy_ssl.h"

#include "base/logging.h"
#include "openssl/err.h"
#include "openssl/ssl.h"

namespace net {

// Each element consists of <the length of the string><string> .
#define NEXT_PROTO_STRING \
  "\x08spdy/4a2" \
  "\x06spdy/3" \
  "\x06spdy/2" \
  "\x08http/1.1" \
  "\x08http/1.0"
#define SSL_CIPHER_LIST "!aNULL:!ADH:!eNull:!LOW:!EXP:RC4+RSA:MEDIUM:HIGH"

int ssl_set_npn_callback(SSL* s,
                         const unsigned char** data,
                         unsigned int* len,
                         void* arg) {
  VLOG(1) << "SSL NPN callback: advertising protocols.";
  *data = (const unsigned char*)NEXT_PROTO_STRING;
  *len = strlen(NEXT_PROTO_STRING);
  return SSL_TLSEXT_ERR_OK;
}

void InitSSL(SSLState* state,
             std::string ssl_cert_name,
             std::string ssl_key_name,
             bool use_npn,
             int session_expiration_time,
             bool disable_ssl_compression) {
  SSL_library_init();
  PrintSslError();

  SSL_load_error_strings();
  PrintSslError();

  state->ssl_method = SSLv23_method();
  state->ssl_ctx = SSL_CTX_new(state->ssl_method);
  if (!state->ssl_ctx) {
    PrintSslError();
    LOG(FATAL) << "Unable to create SSL context";
  }
  // Disable SSLv2 support.
  SSL_CTX_set_options(state->ssl_ctx,
                      SSL_OP_NO_SSLv2 | SSL_OP_CIPHER_SERVER_PREFERENCE);
  if (SSL_CTX_use_certificate_chain_file(state->ssl_ctx,
                                         ssl_cert_name.c_str()) <= 0) {
    PrintSslError();
    LOG(FATAL) << "Unable to use cert.pem as SSL cert.";
  }
  if (SSL_CTX_use_PrivateKey_file(
          state->ssl_ctx, ssl_key_name.c_str(), SSL_FILETYPE_PEM) <= 0) {
    PrintSslError();
    LOG(FATAL) << "Unable to use key.pem as SSL key.";
  }
  if (!SSL_CTX_check_private_key(state->ssl_ctx)) {
    PrintSslError();
    LOG(FATAL) << "The cert.pem and key.pem files don't match";
  }
  if (use_npn) {
    SSL_CTX_set_next_protos_advertised_cb(
        state->ssl_ctx, ssl_set_npn_callback, NULL);
  }
  VLOG(1) << "SSL CTX default cipher list: " << SSL_CIPHER_LIST;
  SSL_CTX_set_cipher_list(state->ssl_ctx, SSL_CIPHER_LIST);

  VLOG(1) << "SSL CTX session expiry: " << session_expiration_time
          << " seconds";
  SSL_CTX_set_timeout(state->ssl_ctx, session_expiration_time);

#ifdef SSL_MODE_RELEASE_BUFFERS
  VLOG(1) << "SSL CTX: Setting Release Buffers mode.";
  SSL_CTX_set_mode(state->ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
#endif

#if !defined(OPENSSL_IS_BORINGSSL)
  // Proper methods to disable compression don't exist until 0.9.9+. For now
  // we must manipulate the stack of compression methods directly.
  if (disable_ssl_compression) {
    STACK_OF(SSL_COMP)* ssl_comp_methods = SSL_COMP_get_compression_methods();
    int num_methods = sk_SSL_COMP_num(ssl_comp_methods);
    int i;
    for (i = 0; i < num_methods; i++) {
      static_cast<void>(sk_SSL_COMP_delete(ssl_comp_methods, i));
    }
  }
#endif
}

SSL* CreateSSLContext(SSL_CTX* ssl_ctx) {
  SSL* ssl = SSL_new(ssl_ctx);
  SSL_set_accept_state(ssl);
  PrintSslError();
  return ssl;
}

void PrintSslError() {
  char buf[128];  // this buffer must be at least 120 chars long.
  uint32_t error_num = ERR_get_error();
  while (error_num != 0) {
    ERR_error_string_n(error_num, buf, sizeof(buf));
    LOG(ERROR) << buf;
    error_num = ERR_get_error();
  }
}

}  // namespace net
