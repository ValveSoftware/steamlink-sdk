// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/nss_ssl_util.h"

#include <nss.h>
#include <secerr.h>
#include <ssl.h>
#include <sslerr.h>
#include <sslproto.h>

#include <string>

#include "base/bind.h"
#include "base/cpu.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/nss_memio.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

// CiphersRemove takes a zero-terminated array of cipher suite ids in
// |to_remove| and sets every instance of them in |ciphers| to zero. It returns
// true if it found and removed every element of |to_remove|. It assumes that
// there are no duplicates in |ciphers| nor in |to_remove|.
bool CiphersRemove(const uint16* to_remove, uint16* ciphers, size_t num) {
  size_t i, found = 0;

  for (i = 0; ; i++) {
    if (to_remove[i] == 0)
      break;

    for (size_t j = 0; j < num; j++) {
      if (to_remove[i] == ciphers[j]) {
        ciphers[j] = 0;
        found++;
        break;
      }
    }
  }

  return found == i;
}

// CiphersCompact takes an array of cipher suite ids in |ciphers|, where some
// entries are zero, and moves the entries so that all the non-zero elements
// are compacted at the end of the array.
void CiphersCompact(uint16* ciphers, size_t num) {
  size_t j = num - 1;

  for (size_t i = num - 1; i < num; i--) {
    if (ciphers[i] == 0)
      continue;
    ciphers[j--] = ciphers[i];
  }
}

// CiphersCopy copies the zero-terminated array |in| to |out|. It returns the
// number of cipher suite ids copied.
size_t CiphersCopy(const uint16* in, uint16* out) {
  for (size_t i = 0; ; i++) {
    if (in[i] == 0)
      return i;
    out[i] = in[i];
  }
}

}  // anonymous namespace

namespace net {

class NSSSSLInitSingleton {
 public:
  NSSSSLInitSingleton() : model_fd_(NULL) {
    crypto::EnsureNSSInit();

    NSS_SetDomesticPolicy();

    const PRUint16* const ssl_ciphers = SSL_GetImplementedCiphers();
    const PRUint16 num_ciphers = SSL_GetNumImplementedCiphers();

    // Disable ECDSA cipher suites on platforms that do not support ECDSA
    // signed certificates, as servers may use the presence of such
    // ciphersuites as a hint to send an ECDSA certificate.
    bool disableECDSA = false;
#if defined(OS_WIN)
    if (base::win::GetVersion() < base::win::VERSION_VISTA)
      disableECDSA = true;
#endif

    // Explicitly enable exactly those ciphers with keys of at least 80 bits
    for (int i = 0; i < num_ciphers; i++) {
      SSLCipherSuiteInfo info;
      if (SSL_GetCipherSuiteInfo(ssl_ciphers[i], &info,
                                 sizeof(info)) == SECSuccess) {
        bool enabled = info.effectiveKeyBits >= 80;
        if (info.authAlgorithm == ssl_auth_ecdsa && disableECDSA)
          enabled = false;

        // Trim the list of cipher suites in order to keep the size of the
        // ClientHello down. DSS, ECDH, CAMELLIA, SEED, ECC+3DES, and
        // HMAC-SHA256 cipher suites are disabled.
        if (info.symCipher == ssl_calg_camellia ||
            info.symCipher == ssl_calg_seed ||
            (info.symCipher == ssl_calg_3des && info.keaType != ssl_kea_rsa) ||
            info.authAlgorithm == ssl_auth_dsa ||
            info.macAlgorithm == ssl_hmac_sha256 ||
            info.nonStandard ||
            strcmp(info.keaTypeName, "ECDH") == 0) {
          enabled = false;
        }

        if (ssl_ciphers[i] == TLS_DHE_DSS_WITH_AES_128_CBC_SHA) {
          // Enabled to allow servers with only a DSA certificate to function.
          enabled = true;
        }
        SSL_CipherPrefSetDefault(ssl_ciphers[i], enabled);
      }
    }

    // Enable SSL.
    SSL_OptionSetDefault(SSL_SECURITY, PR_TRUE);

    // Calculate the order of ciphers that we'll use for NSS sockets. (Note
    // that, even if a cipher is specified in the ordering, it must still be
    // enabled in order to be included in a ClientHello.)
    //
    // Our top preference cipher suites are either forward-secret AES-GCM or
    // forward-secret ChaCha20-Poly1305. If the local machine has AES-NI then
    // we prefer AES-GCM, otherwise ChaCha20. The remainder of the cipher suite
    // preference is inheriented from NSS. */
    static const uint16 chacha_ciphers[] = {
      TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305,
      TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305,
      0,
    };
    static const uint16 aes_gcm_ciphers[] = {
      TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
      TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
      TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
      0,
    };
    scoped_ptr<uint16[]> ciphers(new uint16[num_ciphers]);
    memcpy(ciphers.get(), ssl_ciphers, sizeof(uint16)*num_ciphers);

    if (CiphersRemove(chacha_ciphers, ciphers.get(), num_ciphers) &&
        CiphersRemove(aes_gcm_ciphers, ciphers.get(), num_ciphers)) {
      CiphersCompact(ciphers.get(), num_ciphers);

      const uint16* preference_ciphers = chacha_ciphers;
      const uint16* other_ciphers = aes_gcm_ciphers;
      base::CPU cpu;

      if (cpu.has_aesni() && cpu.has_avx()) {
        preference_ciphers = aes_gcm_ciphers;
        other_ciphers = chacha_ciphers;
      }
      unsigned i = CiphersCopy(preference_ciphers, ciphers.get());
      CiphersCopy(other_ciphers, &ciphers[i]);

      if ((model_fd_ = memio_CreateIOLayer(1, 1)) == NULL ||
          SSL_ImportFD(NULL, model_fd_) == NULL ||
          SECSuccess !=
              SSL_CipherOrderSet(model_fd_, ciphers.get(), num_ciphers)) {
        NOTREACHED();
        if (model_fd_) {
          PR_Close(model_fd_);
          model_fd_ = NULL;
        }
      }
    }

    // All other SSL options are set per-session by SSLClientSocket and
    // SSLServerSocket.
  }

  PRFileDesc* GetModelSocket() {
    return model_fd_;
  }

  ~NSSSSLInitSingleton() {
    // Have to clear the cache, or NSS_Shutdown fails with SEC_ERROR_BUSY.
    SSL_ClearSessionCache();
    if (model_fd_)
      PR_Close(model_fd_);
  }

 private:
  PRFileDesc* model_fd_;
};

static base::LazyInstance<NSSSSLInitSingleton>::Leaky g_nss_ssl_init_singleton =
    LAZY_INSTANCE_INITIALIZER;

// Initialize the NSS SSL library if it isn't already initialized.  This must
// be called before any other NSS SSL functions.  This function is
// thread-safe, and the NSS SSL library will only ever be initialized once.
// The NSS SSL library will be properly shut down on program exit.
void EnsureNSSSSLInit() {
  // Initializing SSL causes us to do blocking IO.
  // Temporarily allow it until we fix
  //   http://code.google.com/p/chromium/issues/detail?id=59847
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  g_nss_ssl_init_singleton.Get();
}

PRFileDesc* GetNSSModelSocket() {
  return g_nss_ssl_init_singleton.Get().GetModelSocket();
}

// Map a Chromium net error code to an NSS error code.
// See _MD_unix_map_default_error in the NSS source
// tree for inspiration.
PRErrorCode MapErrorToNSS(int result) {
  if (result >=0)
    return result;

  switch (result) {
    case ERR_IO_PENDING:
      return PR_WOULD_BLOCK_ERROR;
    case ERR_ACCESS_DENIED:
    case ERR_NETWORK_ACCESS_DENIED:
      // For connect, this could be mapped to PR_ADDRESS_NOT_SUPPORTED_ERROR.
      return PR_NO_ACCESS_RIGHTS_ERROR;
    case ERR_NOT_IMPLEMENTED:
      return PR_NOT_IMPLEMENTED_ERROR;
    case ERR_SOCKET_NOT_CONNECTED:
      return PR_NOT_CONNECTED_ERROR;
    case ERR_INTERNET_DISCONNECTED:  // Equivalent to ENETDOWN.
      return PR_NETWORK_UNREACHABLE_ERROR;  // Best approximation.
    case ERR_CONNECTION_TIMED_OUT:
    case ERR_TIMED_OUT:
      return PR_IO_TIMEOUT_ERROR;
    case ERR_CONNECTION_RESET:
      return PR_CONNECT_RESET_ERROR;
    case ERR_CONNECTION_ABORTED:
      return PR_CONNECT_ABORTED_ERROR;
    case ERR_CONNECTION_REFUSED:
      return PR_CONNECT_REFUSED_ERROR;
    case ERR_ADDRESS_UNREACHABLE:
      return PR_HOST_UNREACHABLE_ERROR;  // Also PR_NETWORK_UNREACHABLE_ERROR.
    case ERR_ADDRESS_INVALID:
      return PR_ADDRESS_NOT_AVAILABLE_ERROR;
    case ERR_NAME_NOT_RESOLVED:
      return PR_DIRECTORY_LOOKUP_ERROR;
    default:
      LOG(WARNING) << "MapErrorToNSS " << result
                   << " mapped to PR_UNKNOWN_ERROR";
      return PR_UNKNOWN_ERROR;
  }
}

// The default error mapping function.
// Maps an NSS error code to a network error code.
int MapNSSError(PRErrorCode err) {
  // TODO(port): fill this out as we learn what's important
  switch (err) {
    case PR_WOULD_BLOCK_ERROR:
      return ERR_IO_PENDING;
    case PR_ADDRESS_NOT_SUPPORTED_ERROR:  // For connect.
    case PR_NO_ACCESS_RIGHTS_ERROR:
      return ERR_ACCESS_DENIED;
    case PR_IO_TIMEOUT_ERROR:
      return ERR_TIMED_OUT;
    case PR_CONNECT_RESET_ERROR:
      return ERR_CONNECTION_RESET;
    case PR_CONNECT_ABORTED_ERROR:
      return ERR_CONNECTION_ABORTED;
    case PR_CONNECT_REFUSED_ERROR:
      return ERR_CONNECTION_REFUSED;
    case PR_NOT_CONNECTED_ERROR:
      return ERR_SOCKET_NOT_CONNECTED;
    case PR_HOST_UNREACHABLE_ERROR:
    case PR_NETWORK_UNREACHABLE_ERROR:
      return ERR_ADDRESS_UNREACHABLE;
    case PR_ADDRESS_NOT_AVAILABLE_ERROR:
      return ERR_ADDRESS_INVALID;
    case PR_INVALID_ARGUMENT_ERROR:
      return ERR_INVALID_ARGUMENT;
    case PR_END_OF_FILE_ERROR:
      return ERR_CONNECTION_CLOSED;
    case PR_NOT_IMPLEMENTED_ERROR:
      return ERR_NOT_IMPLEMENTED;

    case SEC_ERROR_LIBRARY_FAILURE:
      return ERR_UNEXPECTED;
    case SEC_ERROR_INVALID_ARGS:
      return ERR_INVALID_ARGUMENT;
    case SEC_ERROR_NO_MEMORY:
      return ERR_OUT_OF_MEMORY;
    case SEC_ERROR_NO_KEY:
      return ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY;
    case SEC_ERROR_INVALID_KEY:
    case SSL_ERROR_SIGN_HASHES_FAILURE:
      LOG(ERROR) << "ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED: NSS error " << err
                 << ", OS error " << PR_GetOSError();
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    // A handshake (initial or renegotiation) may fail because some signature
    // (for example, the signature in the ServerKeyExchange message for an
    // ephemeral Diffie-Hellman cipher suite) is invalid.
    case SEC_ERROR_BAD_SIGNATURE:
      return ERR_SSL_PROTOCOL_ERROR;

    case SSL_ERROR_SSL_DISABLED:
      return ERR_NO_SSL_VERSIONS_ENABLED;
    case SSL_ERROR_NO_CYPHER_OVERLAP:
    case SSL_ERROR_PROTOCOL_VERSION_ALERT:
    case SSL_ERROR_UNSUPPORTED_VERSION:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SSL_ERROR_HANDSHAKE_FAILURE_ALERT:
    case SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT:
    case SSL_ERROR_ILLEGAL_PARAMETER_ALERT:
      return ERR_SSL_PROTOCOL_ERROR;
    case SSL_ERROR_DECOMPRESSION_FAILURE_ALERT:
      return ERR_SSL_DECOMPRESSION_FAILURE_ALERT;
    case SSL_ERROR_BAD_MAC_ALERT:
      return ERR_SSL_BAD_RECORD_MAC_ALERT;
    case SSL_ERROR_DECRYPT_ERROR_ALERT:
      return ERR_SSL_DECRYPT_ERROR_ALERT;
    case SSL_ERROR_UNRECOGNIZED_NAME_ALERT:
      return ERR_SSL_UNRECOGNIZED_NAME_ALERT;
    case SSL_ERROR_UNSAFE_NEGOTIATION:
      return ERR_SSL_UNSAFE_NEGOTIATION;
    case SSL_ERROR_WEAK_SERVER_EPHEMERAL_DH_KEY:
      return ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY;
    case SSL_ERROR_HANDSHAKE_NOT_COMPLETED:
      return ERR_SSL_HANDSHAKE_NOT_COMPLETED;
    case SEC_ERROR_BAD_KEY:
    case SSL_ERROR_EXTRACT_PUBLIC_KEY_FAILURE:
    // TODO(wtc): the following errors may also occur in contexts unrelated
    // to the peer's public key.  We should add new error codes for them, or
    // map them to ERR_SSL_BAD_PEER_PUBLIC_KEY only in the right context.
    // General unsupported/unknown key algorithm error.
    case SEC_ERROR_UNSUPPORTED_KEYALG:
    // General DER decoding errors.
    case SEC_ERROR_BAD_DER:
    case SEC_ERROR_EXTRA_INPUT:
      return ERR_SSL_BAD_PEER_PUBLIC_KEY;
    // During renegotiation, the server presented a different certificate than
    // was used earlier.
    case SSL_ERROR_WRONG_CERTIFICATE:
      return ERR_SSL_SERVER_CERT_CHANGED;
    case SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT:
      return ERR_SSL_INAPPROPRIATE_FALLBACK;

    default: {
      const char* err_name = PR_ErrorToName(err);
      if (err_name == NULL)
        err_name = "";
      if (IS_SSL_ERROR(err)) {
        LOG(WARNING) << "Unknown SSL error " << err << " (" << err_name << ")"
                     << " mapped to net::ERR_SSL_PROTOCOL_ERROR";
        return ERR_SSL_PROTOCOL_ERROR;
      }
      LOG(WARNING) << "Unknown error " << err << " (" << err_name << ")"
                   << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
    }
  }
}

// Returns parameters to attach to the NetLog when we receive an error in
// response to a call to an NSS function.  Used instead of
// NetLogSSLErrorCallback with events of type TYPE_SSL_NSS_ERROR.
base::Value* NetLogSSLFailedNSSFunctionCallback(
    const char* function,
    const char* param,
    int ssl_lib_error,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("function", function);
  if (param[0] != '\0')
    dict->SetString("param", param);
  dict->SetInteger("ssl_lib_error", ssl_lib_error);
  return dict;
}

void LogFailedNSSFunction(const BoundNetLog& net_log,
                          const char* function,
                          const char* param) {
  DCHECK(function);
  DCHECK(param);
  net_log.AddEvent(
      NetLog::TYPE_SSL_NSS_ERROR,
      base::Bind(&NetLogSSLFailedNSSFunctionCallback,
                 function, param, PR_GetError()));
}

}  // namespace net
