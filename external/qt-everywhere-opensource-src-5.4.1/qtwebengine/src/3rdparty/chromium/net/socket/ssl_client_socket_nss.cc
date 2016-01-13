// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file includes code SSLClientSocketNSS::DoVerifyCertComplete() derived
// from AuthCertificateCallback() in
// mozilla/security/manager/ssl/src/nsNSSCallbacks.cpp.

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
 *   Kai Engert <kengert@redhat.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/socket/ssl_client_socket_nss.h"

#include <certdb.h>
#include <hasht.h>
#include <keyhi.h>
#include <nspr.h>
#include <nss.h>
#include <ocsp.h>
#include <pk11pub.h>
#include <secerr.h>
#include <sechash.h>
#include <ssl.h>
#include <sslerr.h>
#include <sslproto.h>

#include <algorithm>
#include <limits>
#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "crypto/ec_private_key.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/address_list.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/dns_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_objects_extractor.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/ct_verify_result.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/single_request_cert_verifier.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/cert/x509_util.h"
#include "net/http/transport_security_state.h"
#include "net/ocsp/nss_ocsp.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/nss_ssl_util.h"
#include "net/socket/ssl_error_params.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>

#include "base/win/windows_version.h"
#elif defined(OS_MACOSX)
#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>

#include "base/mac/mac_logging.h"
#include "base/synchronization/lock.h"
#include "crypto/mac_security_services_lock.h"
#elif defined(USE_NSS)
#include <dlfcn.h>
#endif

namespace net {

// State machines are easier to debug if you log state transitions.
// Enable these if you want to see what's going on.
#if 1
#define EnterFunction(x)
#define LeaveFunction(x)
#define GotoState(s) next_handshake_state_ = s
#else
#define EnterFunction(x)\
    VLOG(1) << (void *)this << " " << __FUNCTION__ << " enter " << x\
            << "; next_handshake_state " << next_handshake_state_
#define LeaveFunction(x)\
    VLOG(1) << (void *)this << " " << __FUNCTION__ << " leave " << x\
            << "; next_handshake_state " << next_handshake_state_
#define GotoState(s)\
    do {\
      VLOG(1) << (void *)this << " " << __FUNCTION__ << " jump to state " << s;\
      next_handshake_state_ = s;\
    } while (0)
#endif

namespace {

// SSL plaintext fragments are shorter than 16KB. Although the record layer
// overhead is allowed to be 2K + 5 bytes, in practice the overhead is much
// smaller than 1KB. So a 17KB buffer should be large enough to hold an
// entire SSL record.
const int kRecvBufferSize = 17 * 1024;
const int kSendBufferSize = 17 * 1024;

// Used by SSLClientSocketNSS::Core to indicate there is no read result
// obtained by a previous operation waiting to be returned to the caller.
// This constant can be any non-negative/non-zero value (eg: it does not
// overlap with any value of the net::Error range, including net::OK).
const int kNoPendingReadResult = 1;

#if defined(OS_WIN)
// CERT_OCSP_RESPONSE_PROP_ID is only implemented on Vista+, but it can be
// set on Windows XP without error. There is some overhead from the server
// sending the OCSP response if it supports the extension, for the subset of
// XP clients who will request it but be unable to use it, but this is an
// acceptable trade-off for simplicity of implementation.
bool IsOCSPStaplingSupported() {
  return true;
}
#elif defined(USE_NSS)
typedef SECStatus
(*CacheOCSPResponseFromSideChannelFunction)(
    CERTCertDBHandle *handle, CERTCertificate *cert, PRTime time,
    SECItem *encodedResponse, void *pwArg);

// On Linux, we dynamically link against the system version of libnss3.so. In
// order to continue working on systems without up-to-date versions of NSS we
// lookup CERT_CacheOCSPResponseFromSideChannel with dlsym.

// RuntimeLibNSSFunctionPointers is a singleton which caches the results of any
// runtime symbol resolution that we need.
class RuntimeLibNSSFunctionPointers {
 public:
  CacheOCSPResponseFromSideChannelFunction
  GetCacheOCSPResponseFromSideChannelFunction() {
    return cache_ocsp_response_from_side_channel_;
  }

  static RuntimeLibNSSFunctionPointers* GetInstance() {
    return Singleton<RuntimeLibNSSFunctionPointers>::get();
  }

 private:
  friend struct DefaultSingletonTraits<RuntimeLibNSSFunctionPointers>;

  RuntimeLibNSSFunctionPointers() {
    cache_ocsp_response_from_side_channel_ =
        (CacheOCSPResponseFromSideChannelFunction)
        dlsym(RTLD_DEFAULT, "CERT_CacheOCSPResponseFromSideChannel");
  }

  CacheOCSPResponseFromSideChannelFunction
      cache_ocsp_response_from_side_channel_;
};

CacheOCSPResponseFromSideChannelFunction
GetCacheOCSPResponseFromSideChannelFunction() {
  return RuntimeLibNSSFunctionPointers::GetInstance()
    ->GetCacheOCSPResponseFromSideChannelFunction();
}

bool IsOCSPStaplingSupported() {
  return GetCacheOCSPResponseFromSideChannelFunction() != NULL;
}
#else
// TODO(agl): Figure out if we can plumb the OCSP response into Mac's system
// certificate validation functions.
bool IsOCSPStaplingSupported() {
  return false;
}
#endif

#if defined(OS_WIN)

// This callback is intended to be used with CertFindChainInStore. In addition
// to filtering by extended/enhanced key usage, we do not show expired
// certificates and require digital signature usage in the key usage
// extension.
//
// This matches our behavior on Mac OS X and that of NSS. It also matches the
// default behavior of IE8. See http://support.microsoft.com/kb/890326 and
// http://blogs.msdn.com/b/askie/archive/2009/06/09/my-expired-client-certificates-no-longer-display-when-connecting-to-my-web-server-using-ie8.aspx
BOOL WINAPI ClientCertFindCallback(PCCERT_CONTEXT cert_context,
                                   void* find_arg) {
  VLOG(1) << "Calling ClientCertFindCallback from _nss";
  // Verify the certificate's KU is good.
  BYTE key_usage;
  if (CertGetIntendedKeyUsage(X509_ASN_ENCODING, cert_context->pCertInfo,
                              &key_usage, 1)) {
    if (!(key_usage & CERT_DIGITAL_SIGNATURE_KEY_USAGE))
      return FALSE;
  } else {
    DWORD err = GetLastError();
    // If |err| is non-zero, it's an actual error. Otherwise the extension
    // just isn't present, and we treat it as if everything was allowed.
    if (err) {
      DLOG(ERROR) << "CertGetIntendedKeyUsage failed: " << err;
      return FALSE;
    }
  }

  // Verify the current time is within the certificate's validity period.
  if (CertVerifyTimeValidity(NULL, cert_context->pCertInfo) != 0)
    return FALSE;

  // Verify private key metadata is associated with this certificate.
  DWORD size = 0;
  if (!CertGetCertificateContextProperty(
          cert_context, CERT_KEY_PROV_INFO_PROP_ID, NULL, &size)) {
    return FALSE;
  }

  return TRUE;
}

#endif

// Helper functions to make it possible to log events from within the
// SSLClientSocketNSS::Core.
void AddLogEvent(const base::WeakPtr<BoundNetLog>& net_log,
                 NetLog::EventType event_type) {
  if (!net_log)
    return;
  net_log->AddEvent(event_type);
}

// Helper function to make it possible to log events from within the
// SSLClientSocketNSS::Core.
void AddLogEventWithCallback(const base::WeakPtr<BoundNetLog>& net_log,
                             NetLog::EventType event_type,
                             const NetLog::ParametersCallback& callback) {
  if (!net_log)
    return;
  net_log->AddEvent(event_type, callback);
}

// Helper function to make it easier to call BoundNetLog::AddByteTransferEvent
// from within the SSLClientSocketNSS::Core.
// AddByteTransferEvent expects to receive a const char*, which within the
// Core is backed by an IOBuffer. If the "const char*" is bound via
// base::Bind and posted to another thread, and the IOBuffer that backs that
// pointer then goes out of scope on the origin thread, this would result in
// an invalid read of a stale pointer.
// Instead, provide a signature that accepts an IOBuffer*, so that a reference
// to the owning IOBuffer can be bound to the Callback. This ensures that the
// IOBuffer will stay alive long enough to cross threads if needed.
void LogByteTransferEvent(
    const base::WeakPtr<BoundNetLog>& net_log, NetLog::EventType event_type,
    int len, IOBuffer* buffer) {
  if (!net_log)
    return;
  net_log->AddByteTransferEvent(event_type, len, buffer->data());
}

// PeerCertificateChain is a helper object which extracts the certificate
// chain, as given by the server, from an NSS socket and performs the needed
// resource management. The first element of the chain is the leaf certificate
// and the other elements are in the order given by the server.
class PeerCertificateChain {
 public:
  PeerCertificateChain() {}
  PeerCertificateChain(const PeerCertificateChain& other);
  ~PeerCertificateChain();
  PeerCertificateChain& operator=(const PeerCertificateChain& other);

  // Resets the current chain, freeing any resources, and updates the current
  // chain to be a copy of the chain stored in |nss_fd|.
  // If |nss_fd| is NULL, then the current certificate chain will be freed.
  void Reset(PRFileDesc* nss_fd);

  // Returns the current certificate chain as a vector of DER-encoded
  // base::StringPieces. The returned vector remains valid until Reset is
  // called.
  std::vector<base::StringPiece> AsStringPieceVector() const;

  bool empty() const { return certs_.empty(); }

  CERTCertificate* operator[](size_t index) const {
    DCHECK_LT(index, certs_.size());
    return certs_[index];
  }

 private:
  std::vector<CERTCertificate*> certs_;
};

PeerCertificateChain::PeerCertificateChain(
    const PeerCertificateChain& other) {
  *this = other;
}

PeerCertificateChain::~PeerCertificateChain() {
  Reset(NULL);
}

PeerCertificateChain& PeerCertificateChain::operator=(
    const PeerCertificateChain& other) {
  if (this == &other)
    return *this;

  Reset(NULL);
  certs_.reserve(other.certs_.size());
  for (size_t i = 0; i < other.certs_.size(); ++i)
    certs_.push_back(CERT_DupCertificate(other.certs_[i]));

  return *this;
}

void PeerCertificateChain::Reset(PRFileDesc* nss_fd) {
  for (size_t i = 0; i < certs_.size(); ++i)
    CERT_DestroyCertificate(certs_[i]);
  certs_.clear();

  if (nss_fd == NULL)
    return;

  CERTCertList* list = SSL_PeerCertificateChain(nss_fd);
  // The handshake on |nss_fd| may not have completed.
  if (list == NULL)
    return;

  for (CERTCertListNode* node = CERT_LIST_HEAD(list);
       !CERT_LIST_END(node, list); node = CERT_LIST_NEXT(node)) {
    certs_.push_back(CERT_DupCertificate(node->cert));
  }
  CERT_DestroyCertList(list);
}

std::vector<base::StringPiece>
PeerCertificateChain::AsStringPieceVector() const {
  std::vector<base::StringPiece> v(certs_.size());
  for (unsigned i = 0; i < certs_.size(); i++) {
    v[i] = base::StringPiece(
        reinterpret_cast<const char*>(certs_[i]->derCert.data),
        certs_[i]->derCert.len);
  }

  return v;
}

// HandshakeState is a helper struct used to pass handshake state between
// the NSS task runner and the network task runner.
//
// It contains members that may be read or written on the NSS task runner,
// but which also need to be read from the network task runner. The NSS task
// runner will notify the network task runner whenever this state changes, so
// that the network task runner can safely make a copy, which avoids the need
// for locking.
struct HandshakeState {
  HandshakeState() { Reset(); }

  void Reset() {
    next_proto_status = SSLClientSocket::kNextProtoUnsupported;
    next_proto.clear();
    server_protos.clear();
    channel_id_sent = false;
    server_cert_chain.Reset(NULL);
    server_cert = NULL;
    sct_list_from_tls_extension.clear();
    stapled_ocsp_response.clear();
    resumed_handshake = false;
    ssl_connection_status = 0;
  }

  // Set to kNextProtoNegotiated if NPN was successfully negotiated, with the
  // negotiated protocol stored in |next_proto|.
  SSLClientSocket::NextProtoStatus next_proto_status;
  std::string next_proto;
  // If the server supports NPN, the protocols supported by the server.
  std::string server_protos;

  // True if a channel ID was sent.
  bool channel_id_sent;

  // List of DER-encoded X.509 DistinguishedName of certificate authorities
  // allowed by the server.
  std::vector<std::string> cert_authorities;

  // Set when the handshake fully completes.
  //
  // The server certificate is first received from NSS as an NSS certificate
  // chain (|server_cert_chain|) and then converted into a platform-specific
  // X509Certificate object (|server_cert|). It's possible for some
  // certificates to be successfully parsed by NSS, and not by the platform
  // libraries (i.e.: when running within a sandbox, different parsing
  // algorithms, etc), so it's not safe to assume that |server_cert| will
  // always be non-NULL.
  PeerCertificateChain server_cert_chain;
  scoped_refptr<X509Certificate> server_cert;
  // SignedCertificateTimestampList received via TLS extension (RFC 6962).
  std::string sct_list_from_tls_extension;
  // Stapled OCSP response received.
  std::string stapled_ocsp_response;

  // True if the current handshake was the result of TLS session resumption.
  bool resumed_handshake;

  // The negotiated security parameters (TLS version, cipher, extensions) of
  // the SSL connection.
  int ssl_connection_status;
};

// Client-side error mapping functions.

// Map NSS error code to network error code.
int MapNSSClientError(PRErrorCode err) {
  switch (err) {
    case SSL_ERROR_BAD_CERT_ALERT:
    case SSL_ERROR_UNSUPPORTED_CERT_ALERT:
    case SSL_ERROR_REVOKED_CERT_ALERT:
    case SSL_ERROR_EXPIRED_CERT_ALERT:
    case SSL_ERROR_CERTIFICATE_UNKNOWN_ALERT:
    case SSL_ERROR_UNKNOWN_CA_ALERT:
    case SSL_ERROR_ACCESS_DENIED_ALERT:
      return ERR_BAD_SSL_CLIENT_AUTH_CERT;
    default:
      return MapNSSError(err);
  }
}

// Map NSS error code from the first SSL handshake to network error code.
int MapNSSClientHandshakeError(PRErrorCode err) {
  switch (err) {
    // If the server closed on us, it is a protocol error.
    // Some TLS-intolerant servers do this when we request TLS.
    case PR_END_OF_FILE_ERROR:
      return ERR_SSL_PROTOCOL_ERROR;
    default:
      return MapNSSClientError(err);
  }
}

}  // namespace

// SSLClientSocketNSS::Core provides a thread-safe, ref-counted core that is
// able to marshal data between NSS functions and an underlying transport
// socket.
//
// All public functions are meant to be called from the network task runner,
// and any callbacks supplied will be invoked there as well, provided that
// Detach() has not been called yet.
//
/////////////////////////////////////////////////////////////////////////////
//
// Threading within SSLClientSocketNSS and SSLClientSocketNSS::Core:
//
// Because NSS may block on either hardware or user input during operations
// such as signing, creating certificates, or locating private keys, the Core
// handles all of the interactions with the underlying NSS SSL socket, so
// that these blocking calls can be executed on a dedicated task runner.
//
// Note that the network task runner and the NSS task runner may be executing
// on the same thread. If that happens, then it's more performant to try to
// complete as much work as possible synchronously, even if it might block,
// rather than continually PostTask-ing to the same thread.
//
// Because NSS functions should only be called on the NSS task runner, while
// I/O resources should only be accessed on the network task runner, most
// public functions are implemented via three methods, each with different
// task runner affinities.
//
// In the single-threaded mode (where the network and NSS task runners run on
// the same thread), these are all attempted synchronously, while in the
// multi-threaded mode, message passing is used.
//
// 1) NSS Task Runner: Execute NSS function (DoPayloadRead, DoPayloadWrite,
//    DoHandshake)
// 2) NSS Task Runner: Prepare data to go from NSS to an IO function:
//    (BufferRecv, BufferSend)
// 3) Network Task Runner: Perform IO on that data (DoBufferRecv,
//    DoBufferSend, DoGetDomainBoundCert, OnGetDomainBoundCertComplete)
// 4) Both Task Runners: Callback for asynchronous completion or to marshal
//    data from the network task runner back to NSS (BufferRecvComplete,
//    BufferSendComplete, OnHandshakeIOComplete)
//
/////////////////////////////////////////////////////////////////////////////
// Single-threaded example
//
// |--------------------------Network Task Runner--------------------------|
//  SSLClientSocketNSS              Core               (Transport Socket)
//       Read()
//         |-------------------------V
//                                 Read()
//                                   |
//                            DoPayloadRead()
//                                   |
//                               BufferRecv()
//                                   |
//                              DoBufferRecv()
//                                   |-------------------------V
//                                                           Read()
//                                   V-------------------------|
//                          BufferRecvComplete()
//                                   |
//                           PostOrRunCallback()
//         V-------------------------|
//    (Read Callback)
//
/////////////////////////////////////////////////////////////////////////////
// Multi-threaded example:
//
// |--------------------Network Task Runner-------------|--NSS Task Runner--|
//  SSLClientSocketNSS          Core            Socket         Core
//       Read()
//         |---------------------V
//                             Read()
//                               |-------------------------------V
//                                                             Read()
//                                                               |
//                                                         DoPayloadRead()
//                                                               |
//                                                          BufferRecv
//                               V-------------------------------|
//                          DoBufferRecv
//                               |----------------V
//                                              Read()
//                               V----------------|
//                        BufferRecvComplete()
//                               |-------------------------------V
//                                                      BufferRecvComplete()
//                                                               |
//                                                       PostOrRunCallback()
//                               V-------------------------------|
//                        PostOrRunCallback()
//         V---------------------|
//    (Read Callback)
//
/////////////////////////////////////////////////////////////////////////////
class SSLClientSocketNSS::Core : public base::RefCountedThreadSafe<Core> {
 public:
  // Creates a new Core.
  //
  // Any calls to NSS are executed on the |nss_task_runner|, while any calls
  // that need to operate on the underlying transport, net log, or server
  // bound certificate fetching will happen on the |network_task_runner|, so
  // that their lifetimes match that of the owning SSLClientSocketNSS.
  //
  // The caller retains ownership of |transport|, |net_log|, and
  // |server_bound_cert_service|, and they will not be accessed once Detach()
  // has been called.
  Core(base::SequencedTaskRunner* network_task_runner,
       base::SequencedTaskRunner* nss_task_runner,
       ClientSocketHandle* transport,
       const HostPortPair& host_and_port,
       const SSLConfig& ssl_config,
       BoundNetLog* net_log,
       ServerBoundCertService* server_bound_cert_service);

  // Called on the network task runner.
  // Transfers ownership of |socket|, an NSS SSL socket, and |buffers|, the
  // underlying memio implementation, to the Core. Returns true if the Core
  // was successfully registered with the socket.
  bool Init(PRFileDesc* socket, memio_Private* buffers);

  // Called on the network task runner.
  //
  // Attempts to perform an SSL handshake. If the handshake cannot be
  // completed synchronously, returns ERR_IO_PENDING, invoking |callback| on
  // the network task runner once the handshake has completed. Otherwise,
  // returns OK on success or a network error code on failure.
  int Connect(const CompletionCallback& callback);

  // Called on the network task runner.
  // Signals that the resources owned by the network task runner are going
  // away. No further callbacks will be invoked on the network task runner.
  // May be called at any time.
  void Detach();

  // Called on the network task runner.
  // Returns the current state of the underlying SSL socket. May be called at
  // any time.
  const HandshakeState& state() const { return network_handshake_state_; }

  // Called on the network task runner.
  // Read() and Write() mirror the net::Socket functions of the same name.
  // If ERR_IO_PENDING is returned, |callback| will be invoked on the network
  // task runner at a later point, unless the caller calls Detach().
  int Read(IOBuffer* buf, int buf_len, const CompletionCallback& callback);
  int Write(IOBuffer* buf, int buf_len, const CompletionCallback& callback);

  // Called on the network task runner.
  bool IsConnected() const;
  bool HasPendingAsyncOperation() const;
  bool HasUnhandledReceivedData() const;
  bool WasEverUsed() const;

  // Called on the network task runner.
  // Causes the associated SSL/TLS session ID to be added to NSS's session
  // cache, but only if the connection has not been False Started.
  //
  // This should only be called after the server's certificate has been
  // verified, and may not be called within an NSS callback.
  void CacheSessionIfNecessary();

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core();

  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
    STATE_GET_DOMAIN_BOUND_CERT_COMPLETE,
  };

  bool OnNSSTaskRunner() const;
  bool OnNetworkTaskRunner() const;

  ////////////////////////////////////////////////////////////////////////////
  // Methods that are ONLY called on the NSS task runner:
  ////////////////////////////////////////////////////////////////////////////

  // Called by NSS during full handshakes to allow the application to
  // verify the certificate. Instead of verifying the certificate in the midst
  // of the handshake, SECSuccess is always returned and the peer's certificate
  // is verified afterwards.
  // This behaviour is an artifact of the original SSLClientSocketWin
  // implementation, which could not verify the peer's certificate until after
  // the handshake had completed, as well as bugs in NSS that prevent
  // SSL_RestartHandshakeAfterCertReq from working.
  static SECStatus OwnAuthCertHandler(void* arg,
                                      PRFileDesc* socket,
                                      PRBool checksig,
                                      PRBool is_server);

  // Callbacks called by NSS when the peer requests client certificate
  // authentication.
  // See the documentation in third_party/nss/ssl/ssl.h for the meanings of
  // the arguments.
#if defined(NSS_PLATFORM_CLIENT_AUTH)
  // When NSS has been integrated with awareness of the underlying system
  // cryptographic libraries, this callback allows the caller to supply a
  // native platform certificate and key for use by NSS. At most, one of
  // either (result_certs, result_private_key) or (result_nss_certificate,
  // result_nss_private_key) should be set.
  // |arg| contains a pointer to the current SSLClientSocketNSS::Core.
  static SECStatus PlatformClientAuthHandler(
      void* arg,
      PRFileDesc* socket,
      CERTDistNames* ca_names,
      CERTCertList** result_certs,
      void** result_private_key,
      CERTCertificate** result_nss_certificate,
      SECKEYPrivateKey** result_nss_private_key);
#else
  static SECStatus ClientAuthHandler(void* arg,
                                     PRFileDesc* socket,
                                     CERTDistNames* ca_names,
                                     CERTCertificate** result_certificate,
                                     SECKEYPrivateKey** result_private_key);
#endif

  // Called by NSS to determine if we can False Start.
  // |arg| contains a pointer to the current SSLClientSocketNSS::Core.
  static SECStatus CanFalseStartCallback(PRFileDesc* socket,
                                         void* arg,
                                         PRBool* can_false_start);

  // Called by NSS once the handshake has completed.
  // |arg| contains a pointer to the current SSLClientSocketNSS::Core.
  static void HandshakeCallback(PRFileDesc* socket, void* arg);

  // Called once the handshake has succeeded.
  void HandshakeSucceeded();

  // Handles an NSS error generated while handshaking or performing IO.
  // Returns a network error code mapped from the original NSS error.
  int HandleNSSError(PRErrorCode error, bool handshake_error);

  int DoHandshakeLoop(int last_io_result);
  int DoReadLoop(int result);
  int DoWriteLoop(int result);

  int DoHandshake();
  int DoGetDBCertComplete(int result);

  int DoPayloadRead();
  int DoPayloadWrite();

  bool DoTransportIO();
  int BufferRecv();
  int BufferSend();

  void OnRecvComplete(int result);
  void OnSendComplete(int result);

  void DoConnectCallback(int result);
  void DoReadCallback(int result);
  void DoWriteCallback(int result);

  // Client channel ID handler.
  static SECStatus ClientChannelIDHandler(
      void* arg,
      PRFileDesc* socket,
      SECKEYPublicKey **out_public_key,
      SECKEYPrivateKey **out_private_key);

  // ImportChannelIDKeys is a helper function for turning a DER-encoded cert and
  // key into a SECKEYPublicKey and SECKEYPrivateKey. Returns OK upon success
  // and an error code otherwise.
  // Requires |domain_bound_private_key_| and |domain_bound_cert_| to have been
  // set by a call to ServerBoundCertService->GetDomainBoundCert. The caller
  // takes ownership of the |*cert| and |*key|.
  int ImportChannelIDKeys(SECKEYPublicKey** public_key, SECKEYPrivateKey** key);

  // Updates the NSS and platform specific certificates.
  void UpdateServerCert();
  // Update the nss_handshake_state_ with the SignedCertificateTimestampList
  // received in the handshake via a TLS extension.
  void UpdateSignedCertTimestamps();
  // Update the OCSP response cache with the stapled response received in the
  // handshake, and update nss_handshake_state_ with
  // the SignedCertificateTimestampList received in the stapled OCSP response.
  void UpdateStapledOCSPResponse();
  // Updates the nss_handshake_state_ with the negotiated security parameters.
  void UpdateConnectionStatus();
  // Record histograms for channel id support during full handshakes - resumed
  // handshakes are ignored.
  void RecordChannelIDSupportOnNSSTaskRunner();
  // UpdateNextProto gets any application-layer protocol that may have been
  // negotiated by the TLS connection.
  void UpdateNextProto();

  ////////////////////////////////////////////////////////////////////////////
  // Methods that are ONLY called on the network task runner:
  ////////////////////////////////////////////////////////////////////////////
  int DoBufferRecv(IOBuffer* buffer, int len);
  int DoBufferSend(IOBuffer* buffer, int len);
  int DoGetDomainBoundCert(const std::string& host);

  void OnGetDomainBoundCertComplete(int result);
  void OnHandshakeStateUpdated(const HandshakeState& state);
  void OnNSSBufferUpdated(int amount_in_read_buffer);
  void DidNSSRead(int result);
  void DidNSSWrite(int result);
  void RecordChannelIDSupportOnNetworkTaskRunner(
      bool negotiated_channel_id,
      bool channel_id_enabled,
      bool supports_ecc) const;

  ////////////////////////////////////////////////////////////////////////////
  // Methods that are called on both the network task runner and the NSS
  // task runner.
  ////////////////////////////////////////////////////////////////////////////
  void OnHandshakeIOComplete(int result);
  void BufferRecvComplete(IOBuffer* buffer, int result);
  void BufferSendComplete(int result);

  // PostOrRunCallback is a helper function to ensure that |callback| is
  // invoked on the network task runner, but only if Detach() has not yet
  // been called.
  void PostOrRunCallback(const tracked_objects::Location& location,
                         const base::Closure& callback);

  // Uses PostOrRunCallback and |weak_net_log_| to try and log a
  // SSL_CLIENT_CERT_PROVIDED event, with the indicated count.
  void AddCertProvidedEvent(int cert_count);

  // Sets the handshake state |channel_id_sent| flag and logs the
  // SSL_CHANNEL_ID_PROVIDED event.
  void SetChannelIDProvided();

  ////////////////////////////////////////////////////////////////////////////
  // Members that are ONLY accessed on the network task runner:
  ////////////////////////////////////////////////////////////////////////////

  // True if the owning SSLClientSocketNSS has called Detach(). No further
  // callbacks will be invoked nor access to members owned by the network
  // task runner.
  bool detached_;

  // The underlying transport to use for network IO.
  ClientSocketHandle* transport_;
  base::WeakPtrFactory<BoundNetLog> weak_net_log_factory_;

  // The current handshake state. Mirrors |nss_handshake_state_|.
  HandshakeState network_handshake_state_;

  // The service for retrieving Channel ID keys.  May be NULL.
  ServerBoundCertService* server_bound_cert_service_;
  ServerBoundCertService::RequestHandle domain_bound_cert_request_handle_;

  // The information about NSS task runner.
  int unhandled_buffer_size_;
  bool nss_waiting_read_;
  bool nss_waiting_write_;
  bool nss_is_closed_;

  // Set when Read() or Write() successfully reads or writes data to or from the
  // network.
  bool was_ever_used_;

  ////////////////////////////////////////////////////////////////////////////
  // Members that are ONLY accessed on the NSS task runner:
  ////////////////////////////////////////////////////////////////////////////
  HostPortPair host_and_port_;
  SSLConfig ssl_config_;

  // NSS SSL socket.
  PRFileDesc* nss_fd_;

  // Buffers for the network end of the SSL state machine
  memio_Private* nss_bufs_;

  // Used by DoPayloadRead() when attempting to fill the caller's buffer with
  // as much data as possible, without blocking.
  // If DoPayloadRead() encounters an error after having read some data, stores
  // the results to return on the *next* call to DoPayloadRead(). A value of
  // kNoPendingReadResult indicates there is no pending result, otherwise 0
  // indicates EOF and < 0 indicates an error.
  int pending_read_result_;
  // Contains the previously observed NSS error. Only valid when
  // pending_read_result_ != kNoPendingReadResult.
  PRErrorCode pending_read_nss_error_;

  // The certificate chain, in DER form, that is expected to be received from
  // the server.
  std::vector<std::string> predicted_certs_;

  State next_handshake_state_;

  // True if channel ID extension was negotiated.
  bool channel_id_xtn_negotiated_;
  // True if the handshake state machine was interrupted for channel ID.
  bool channel_id_needed_;
  // True if the handshake state machine was interrupted for client auth.
  bool client_auth_cert_needed_;
  // True if NSS has False Started.
  bool false_started_;
  // True if NSS has called HandshakeCallback.
  bool handshake_callback_called_;

  HandshakeState nss_handshake_state_;

  bool transport_recv_busy_;
  bool transport_recv_eof_;
  bool transport_send_busy_;

  // Used by Read function.
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // Used by Write function.
  scoped_refptr<IOBuffer> user_write_buf_;
  int user_write_buf_len_;

  CompletionCallback user_connect_callback_;
  CompletionCallback user_read_callback_;
  CompletionCallback user_write_callback_;

  ////////////////////////////////////////////////////////////////////////////
  // Members that are accessed on both the network task runner and the NSS
  // task runner.
  ////////////////////////////////////////////////////////////////////////////
  scoped_refptr<base::SequencedTaskRunner> network_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> nss_task_runner_;

  // Dereferenced only on the network task runner, but bound to tasks destined
  // for the network task runner from the NSS task runner.
  base::WeakPtr<BoundNetLog> weak_net_log_;

  // Written on the network task runner by the |server_bound_cert_service_|,
  // prior to invoking OnHandshakeIOComplete.
  // Read on the NSS task runner when once OnHandshakeIOComplete is invoked
  // on the NSS task runner.
  std::string domain_bound_private_key_;
  std::string domain_bound_cert_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

SSLClientSocketNSS::Core::Core(
    base::SequencedTaskRunner* network_task_runner,
    base::SequencedTaskRunner* nss_task_runner,
    ClientSocketHandle* transport,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    BoundNetLog* net_log,
    ServerBoundCertService* server_bound_cert_service)
    : detached_(false),
      transport_(transport),
      weak_net_log_factory_(net_log),
      server_bound_cert_service_(server_bound_cert_service),
      unhandled_buffer_size_(0),
      nss_waiting_read_(false),
      nss_waiting_write_(false),
      nss_is_closed_(false),
      was_ever_used_(false),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      nss_fd_(NULL),
      nss_bufs_(NULL),
      pending_read_result_(kNoPendingReadResult),
      pending_read_nss_error_(0),
      next_handshake_state_(STATE_NONE),
      channel_id_xtn_negotiated_(false),
      channel_id_needed_(false),
      client_auth_cert_needed_(false),
      false_started_(false),
      handshake_callback_called_(false),
      transport_recv_busy_(false),
      transport_recv_eof_(false),
      transport_send_busy_(false),
      user_read_buf_len_(0),
      user_write_buf_len_(0),
      network_task_runner_(network_task_runner),
      nss_task_runner_(nss_task_runner),
      weak_net_log_(weak_net_log_factory_.GetWeakPtr()) {
}

SSLClientSocketNSS::Core::~Core() {
  // TODO(wtc): Send SSL close_notify alert.
  if (nss_fd_ != NULL) {
    PR_Close(nss_fd_);
    nss_fd_ = NULL;
  }
}

bool SSLClientSocketNSS::Core::Init(PRFileDesc* socket,
                                    memio_Private* buffers) {
  DCHECK(OnNetworkTaskRunner());
  DCHECK(!nss_fd_);
  DCHECK(!nss_bufs_);

  nss_fd_ = socket;
  nss_bufs_ = buffers;

  SECStatus rv = SECSuccess;

  if (!ssl_config_.next_protos.empty()) {
    size_t wire_length = 0;
    for (std::vector<std::string>::const_iterator
         i = ssl_config_.next_protos.begin();
         i != ssl_config_.next_protos.end(); ++i) {
      if (i->size() > 255) {
        LOG(WARNING) << "Ignoring overlong NPN/ALPN protocol: " << *i;
        continue;
      }
      wire_length += i->size();
      wire_length++;
    }
    scoped_ptr<uint8[]> wire_protos(new uint8[wire_length]);
    uint8* dst = wire_protos.get();
    for (std::vector<std::string>::const_iterator
         i = ssl_config_.next_protos.begin();
         i != ssl_config_.next_protos.end(); i++) {
      if (i->size() > 255)
        continue;
      *dst++ = i->size();
      memcpy(dst, i->data(), i->size());
      dst += i->size();
    }
    DCHECK_EQ(dst, wire_protos.get() + wire_length);
    rv = SSL_SetNextProtoNego(nss_fd_, wire_protos.get(), wire_length);
    if (rv != SECSuccess)
      LogFailedNSSFunction(*weak_net_log_, "SSL_SetNextProtoNego", "");
    rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_ALPN, PR_TRUE);
    if (rv != SECSuccess)
      LogFailedNSSFunction(*weak_net_log_, "SSL_OptionSet", "SSL_ENABLE_ALPN");
    rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_NPN, PR_TRUE);
    if (rv != SECSuccess)
      LogFailedNSSFunction(*weak_net_log_, "SSL_OptionSet", "SSL_ENABLE_NPN");
  }

  rv = SSL_AuthCertificateHook(
      nss_fd_, SSLClientSocketNSS::Core::OwnAuthCertHandler, this);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(*weak_net_log_, "SSL_AuthCertificateHook", "");
    return false;
  }

#if defined(NSS_PLATFORM_CLIENT_AUTH)
  rv = SSL_GetPlatformClientAuthDataHook(
      nss_fd_, SSLClientSocketNSS::Core::PlatformClientAuthHandler,
      this);
#else
  rv = SSL_GetClientAuthDataHook(
      nss_fd_, SSLClientSocketNSS::Core::ClientAuthHandler, this);
#endif
  if (rv != SECSuccess) {
    LogFailedNSSFunction(*weak_net_log_, "SSL_GetClientAuthDataHook", "");
    return false;
  }

  if (IsChannelIDEnabled(ssl_config_, server_bound_cert_service_)) {
    rv = SSL_SetClientChannelIDCallback(
        nss_fd_, SSLClientSocketNSS::Core::ClientChannelIDHandler, this);
    if (rv != SECSuccess) {
      LogFailedNSSFunction(
          *weak_net_log_, "SSL_SetClientChannelIDCallback", "");
    }
  }

  rv = SSL_SetCanFalseStartCallback(
      nss_fd_, SSLClientSocketNSS::Core::CanFalseStartCallback, this);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(*weak_net_log_, "SSL_SetCanFalseStartCallback", "");
    return false;
  }

  rv = SSL_HandshakeCallback(
      nss_fd_, SSLClientSocketNSS::Core::HandshakeCallback, this);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(*weak_net_log_, "SSL_HandshakeCallback", "");
    return false;
  }

  return true;
}

int SSLClientSocketNSS::Core::Connect(const CompletionCallback& callback) {
  if (!OnNSSTaskRunner()) {
    DCHECK(!detached_);
    bool posted = nss_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(IgnoreResult(&Core::Connect), this, callback));
    return posted ? ERR_IO_PENDING : ERR_ABORTED;
  }

  DCHECK(OnNSSTaskRunner());
  DCHECK_EQ(STATE_NONE, next_handshake_state_);
  DCHECK(user_read_callback_.is_null());
  DCHECK(user_write_callback_.is_null());
  DCHECK(user_connect_callback_.is_null());
  DCHECK(!user_read_buf_.get());
  DCHECK(!user_write_buf_.get());

  next_handshake_state_ = STATE_HANDSHAKE;
  int rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = callback;
  } else if (rv > OK) {
    rv = OK;
  }
  if (rv != ERR_IO_PENDING && !OnNetworkTaskRunner()) {
    PostOrRunCallback(FROM_HERE, base::Bind(callback, rv));
    return ERR_IO_PENDING;
  }

  return rv;
}

void SSLClientSocketNSS::Core::Detach() {
  DCHECK(OnNetworkTaskRunner());

  detached_ = true;
  transport_ = NULL;
  weak_net_log_factory_.InvalidateWeakPtrs();

  network_handshake_state_.Reset();

  domain_bound_cert_request_handle_.Cancel();
}

int SSLClientSocketNSS::Core::Read(IOBuffer* buf, int buf_len,
                                   const CompletionCallback& callback) {
  if (!OnNSSTaskRunner()) {
    DCHECK(OnNetworkTaskRunner());
    DCHECK(!detached_);
    DCHECK(transport_);
    DCHECK(!nss_waiting_read_);

    nss_waiting_read_ = true;
    bool posted = nss_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(IgnoreResult(&Core::Read), this, make_scoped_refptr(buf),
                   buf_len, callback));
    if (!posted) {
      nss_is_closed_ = true;
      nss_waiting_read_ = false;
    }
    return posted ? ERR_IO_PENDING : ERR_ABORTED;
  }

  DCHECK(OnNSSTaskRunner());
  DCHECK(false_started_ || handshake_callback_called_);
  DCHECK_EQ(STATE_NONE, next_handshake_state_);
  DCHECK(user_read_callback_.is_null());
  DCHECK(user_connect_callback_.is_null());
  DCHECK(!user_read_buf_.get());
  DCHECK(nss_bufs_);

  user_read_buf_ = buf;
  user_read_buf_len_ = buf_len;

  int rv = DoReadLoop(OK);
  if (rv == ERR_IO_PENDING) {
    if (OnNetworkTaskRunner())
      nss_waiting_read_ = true;
    user_read_callback_ = callback;
  } else {
    user_read_buf_ = NULL;
    user_read_buf_len_ = 0;

    if (!OnNetworkTaskRunner()) {
      PostOrRunCallback(FROM_HERE, base::Bind(&Core::DidNSSRead, this, rv));
      PostOrRunCallback(FROM_HERE, base::Bind(callback, rv));
      return ERR_IO_PENDING;
    } else {
      DCHECK(!nss_waiting_read_);
      if (rv <= 0) {
        nss_is_closed_ = true;
      } else {
        was_ever_used_ = true;
      }
    }
  }

  return rv;
}

int SSLClientSocketNSS::Core::Write(IOBuffer* buf, int buf_len,
                                    const CompletionCallback& callback) {
  if (!OnNSSTaskRunner()) {
    DCHECK(OnNetworkTaskRunner());
    DCHECK(!detached_);
    DCHECK(transport_);
    DCHECK(!nss_waiting_write_);

    nss_waiting_write_ = true;
    bool posted = nss_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(IgnoreResult(&Core::Write), this, make_scoped_refptr(buf),
                   buf_len, callback));
    if (!posted) {
      nss_is_closed_ = true;
      nss_waiting_write_ = false;
    }
    return posted ? ERR_IO_PENDING : ERR_ABORTED;
  }

  DCHECK(OnNSSTaskRunner());
  DCHECK(false_started_ || handshake_callback_called_);
  DCHECK_EQ(STATE_NONE, next_handshake_state_);
  DCHECK(user_write_callback_.is_null());
  DCHECK(user_connect_callback_.is_null());
  DCHECK(!user_write_buf_.get());
  DCHECK(nss_bufs_);

  user_write_buf_ = buf;
  user_write_buf_len_ = buf_len;

  int rv = DoWriteLoop(OK);
  if (rv == ERR_IO_PENDING) {
    if (OnNetworkTaskRunner())
      nss_waiting_write_ = true;
    user_write_callback_ = callback;
  } else {
    user_write_buf_ = NULL;
    user_write_buf_len_ = 0;

    if (!OnNetworkTaskRunner()) {
      PostOrRunCallback(FROM_HERE, base::Bind(&Core::DidNSSWrite, this, rv));
      PostOrRunCallback(FROM_HERE, base::Bind(callback, rv));
      return ERR_IO_PENDING;
    } else {
      DCHECK(!nss_waiting_write_);
      if (rv < 0) {
        nss_is_closed_ = true;
      } else if (rv > 0) {
        was_ever_used_ = true;
      }
    }
  }

  return rv;
}

bool SSLClientSocketNSS::Core::IsConnected() const {
  DCHECK(OnNetworkTaskRunner());
  return !nss_is_closed_;
}

bool SSLClientSocketNSS::Core::HasPendingAsyncOperation() const {
  DCHECK(OnNetworkTaskRunner());
  return nss_waiting_read_ || nss_waiting_write_;
}

bool SSLClientSocketNSS::Core::HasUnhandledReceivedData() const {
  DCHECK(OnNetworkTaskRunner());
  return unhandled_buffer_size_ != 0;
}

bool SSLClientSocketNSS::Core::WasEverUsed() const {
  DCHECK(OnNetworkTaskRunner());
  return was_ever_used_;
}

void SSLClientSocketNSS::Core::CacheSessionIfNecessary() {
  // TODO(rsleevi): This should occur on the NSS task runner, due to the use of
  // nss_fd_. However, it happens on the network task runner in order to match
  // the buggy behavior of ExportKeyingMaterial.
  //
  // Once http://crbug.com/330360 is fixed, this should be moved to an
  // implementation that exclusively does this work on the NSS TaskRunner. This
  // is "safe" because it is only called during the certificate verification
  // state machine of the main socket, which is safe because no underlying
  // transport IO will be occuring in that state, and NSS will not be blocking
  // on any PKCS#11 related locks that might block the Network TaskRunner.
  DCHECK(OnNetworkTaskRunner());

  // Only cache the session if the connection was not False Started, because
  // sessions should only be cached *after* the peer's Finished message is
  // processed.
  // In the case of False Start, the session will be cached once the
  // HandshakeCallback is called, which signals the receipt and processing of
  // the Finished message, and which will happen during a call to
  // PR_Read/PR_Write.
  if (!false_started_)
    SSL_CacheSession(nss_fd_);
}

bool SSLClientSocketNSS::Core::OnNSSTaskRunner() const {
  return nss_task_runner_->RunsTasksOnCurrentThread();
}

bool SSLClientSocketNSS::Core::OnNetworkTaskRunner() const {
  return network_task_runner_->RunsTasksOnCurrentThread();
}

// static
SECStatus SSLClientSocketNSS::Core::OwnAuthCertHandler(
    void* arg,
    PRFileDesc* socket,
    PRBool checksig,
    PRBool is_server) {
  Core* core = reinterpret_cast<Core*>(arg);
  if (core->handshake_callback_called_) {
    // Disallow the server certificate to change in a renegotiation.
    CERTCertificate* old_cert = core->nss_handshake_state_.server_cert_chain[0];
    ScopedCERTCertificate new_cert(SSL_PeerCertificate(socket));
    if (new_cert->derCert.len != old_cert->derCert.len ||
        memcmp(new_cert->derCert.data, old_cert->derCert.data,
               new_cert->derCert.len) != 0) {
      // NSS doesn't have an error code that indicates the server certificate
      // changed. Borrow SSL_ERROR_WRONG_CERTIFICATE (which NSS isn't using)
      // for this purpose.
      PORT_SetError(SSL_ERROR_WRONG_CERTIFICATE);
      return SECFailure;
    }
  }

  // Tell NSS to not verify the certificate.
  return SECSuccess;
}

#if defined(NSS_PLATFORM_CLIENT_AUTH)
// static
SECStatus SSLClientSocketNSS::Core::PlatformClientAuthHandler(
    void* arg,
    PRFileDesc* socket,
    CERTDistNames* ca_names,
    CERTCertList** result_certs,
    void** result_private_key,
    CERTCertificate** result_nss_certificate,
    SECKEYPrivateKey** result_nss_private_key) {
  Core* core = reinterpret_cast<Core*>(arg);
  DCHECK(core->OnNSSTaskRunner());

  core->PostOrRunCallback(
      FROM_HERE,
      base::Bind(&AddLogEvent, core->weak_net_log_,
                 NetLog::TYPE_SSL_CLIENT_CERT_REQUESTED));

  core->client_auth_cert_needed_ = !core->ssl_config_.send_client_cert;
#if defined(OS_WIN)
  if (core->ssl_config_.send_client_cert) {
    if (core->ssl_config_.client_cert) {
      PCCERT_CONTEXT cert_context =
          core->ssl_config_.client_cert->os_cert_handle();

      HCRYPTPROV_OR_NCRYPT_KEY_HANDLE crypt_prov = 0;
      DWORD key_spec = 0;
      BOOL must_free = FALSE;
      DWORD flags = 0;
      if (base::win::GetVersion() >= base::win::VERSION_VISTA)
        flags |= CRYPT_ACQUIRE_PREFER_NCRYPT_KEY_FLAG;

      BOOL acquired_key = CryptAcquireCertificatePrivateKey(
          cert_context, flags, NULL, &crypt_prov, &key_spec, &must_free);

      if (acquired_key) {
        // Should never get a cached handle back - ownership must always be
        // transferred.
        CHECK_EQ(must_free, TRUE);

        SECItem der_cert;
        der_cert.type = siDERCertBuffer;
        der_cert.data = cert_context->pbCertEncoded;
        der_cert.len  = cert_context->cbCertEncoded;

        // TODO(rsleevi): Error checking for NSS allocation errors.
        CERTCertDBHandle* db_handle = CERT_GetDefaultCertDB();
        CERTCertificate* user_cert = CERT_NewTempCertificate(
            db_handle, &der_cert, NULL, PR_FALSE, PR_TRUE);
        if (!user_cert) {
          // Importing the certificate can fail for reasons including a serial
          // number collision. See crbug.com/97355.
          core->AddCertProvidedEvent(0);
          return SECFailure;
        }
        CERTCertList* cert_chain = CERT_NewCertList();
        CERT_AddCertToListTail(cert_chain, user_cert);

        // Add the intermediates.
        X509Certificate::OSCertHandles intermediates =
            core->ssl_config_.client_cert->GetIntermediateCertificates();
        for (X509Certificate::OSCertHandles::const_iterator it =
            intermediates.begin(); it != intermediates.end(); ++it) {
          der_cert.data = (*it)->pbCertEncoded;
          der_cert.len = (*it)->cbCertEncoded;

          CERTCertificate* intermediate = CERT_NewTempCertificate(
              db_handle, &der_cert, NULL, PR_FALSE, PR_TRUE);
          if (!intermediate) {
            CERT_DestroyCertList(cert_chain);
            core->AddCertProvidedEvent(0);
            return SECFailure;
          }
          CERT_AddCertToListTail(cert_chain, intermediate);
        }
        PCERT_KEY_CONTEXT key_context = reinterpret_cast<PCERT_KEY_CONTEXT>(
            PORT_ZAlloc(sizeof(CERT_KEY_CONTEXT)));
        key_context->cbSize = sizeof(*key_context);
        // NSS will free this context when no longer in use.
        key_context->hCryptProv = crypt_prov;
        key_context->dwKeySpec = key_spec;
        *result_private_key = key_context;
        *result_certs = cert_chain;

        int cert_count = 1 + intermediates.size();
        core->AddCertProvidedEvent(cert_count);
        return SECSuccess;
      }
      LOG(WARNING) << "Client cert found without private key";
    }

    // Send no client certificate.
    core->AddCertProvidedEvent(0);
    return SECFailure;
  }

  core->nss_handshake_state_.cert_authorities.clear();

  std::vector<CERT_NAME_BLOB> issuer_list(ca_names->nnames);
  for (int i = 0; i < ca_names->nnames; ++i) {
    issuer_list[i].cbData = ca_names->names[i].len;
    issuer_list[i].pbData = ca_names->names[i].data;
    core->nss_handshake_state_.cert_authorities.push_back(std::string(
        reinterpret_cast<const char*>(ca_names->names[i].data),
        static_cast<size_t>(ca_names->names[i].len)));
  }

  // Update the network task runner's view of the handshake state now that
  // server certificate request has been recorded.
  core->PostOrRunCallback(
      FROM_HERE, base::Bind(&Core::OnHandshakeStateUpdated, core,
                            core->nss_handshake_state_));

  // Tell NSS to suspend the client authentication.  We will then abort the
  // handshake by returning ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  return SECWouldBlock;
#elif defined(OS_MACOSX)
  if (core->ssl_config_.send_client_cert) {
    if (core->ssl_config_.client_cert.get()) {
      OSStatus os_error = noErr;
      SecIdentityRef identity = NULL;
      SecKeyRef private_key = NULL;
      X509Certificate::OSCertHandles chain;
      {
        base::AutoLock lock(crypto::GetMacSecurityServicesLock());
        os_error = SecIdentityCreateWithCertificate(
            NULL, core->ssl_config_.client_cert->os_cert_handle(), &identity);
      }
      if (os_error == noErr) {
        os_error = SecIdentityCopyPrivateKey(identity, &private_key);
        CFRelease(identity);
      }

      if (os_error == noErr) {
        // TODO(rsleevi): Error checking for NSS allocation errors.
        *result_certs = CERT_NewCertList();
        *result_private_key = private_key;

        chain.push_back(core->ssl_config_.client_cert->os_cert_handle());
        const X509Certificate::OSCertHandles& intermediates =
            core->ssl_config_.client_cert->GetIntermediateCertificates();
        if (!intermediates.empty())
          chain.insert(chain.end(), intermediates.begin(), intermediates.end());

        for (size_t i = 0, chain_count = chain.size(); i < chain_count; ++i) {
          CSSM_DATA cert_data;
          SecCertificateRef cert_ref = chain[i];
          os_error = SecCertificateGetData(cert_ref, &cert_data);
          if (os_error != noErr)
            break;

          SECItem der_cert;
          der_cert.type = siDERCertBuffer;
          der_cert.data = cert_data.Data;
          der_cert.len = cert_data.Length;
          CERTCertificate* nss_cert = CERT_NewTempCertificate(
              CERT_GetDefaultCertDB(), &der_cert, NULL, PR_FALSE, PR_TRUE);
          if (!nss_cert) {
            // In the event of an NSS error, make up an OS error and reuse
            // the error handling below.
            os_error = errSecCreateChainFailed;
            break;
          }
          CERT_AddCertToListTail(*result_certs, nss_cert);
        }
      }

      if (os_error == noErr) {
        core->AddCertProvidedEvent(chain.size());
        return SECSuccess;
      }

      OSSTATUS_LOG(WARNING, os_error)
          << "Client cert found, but could not be used";
      if (*result_certs) {
        CERT_DestroyCertList(*result_certs);
        *result_certs = NULL;
      }
      if (*result_private_key)
        *result_private_key = NULL;
      if (private_key)
        CFRelease(private_key);
    }

    // Send no client certificate.
    core->AddCertProvidedEvent(0);
    return SECFailure;
  }

  core->nss_handshake_state_.cert_authorities.clear();

  // Retrieve the cert issuers accepted by the server.
  std::vector<CertPrincipal> valid_issuers;
  int n = ca_names->nnames;
  for (int i = 0; i < n; i++) {
    core->nss_handshake_state_.cert_authorities.push_back(std::string(
        reinterpret_cast<const char*>(ca_names->names[i].data),
        static_cast<size_t>(ca_names->names[i].len)));
  }

  // Update the network task runner's view of the handshake state now that
  // server certificate request has been recorded.
  core->PostOrRunCallback(
      FROM_HERE, base::Bind(&Core::OnHandshakeStateUpdated, core,
                            core->nss_handshake_state_));

  // Tell NSS to suspend the client authentication.  We will then abort the
  // handshake by returning ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  return SECWouldBlock;
#else
  return SECFailure;
#endif
}

#elif defined(OS_IOS)

SECStatus SSLClientSocketNSS::Core::ClientAuthHandler(
    void* arg,
    PRFileDesc* socket,
    CERTDistNames* ca_names,
    CERTCertificate** result_certificate,
    SECKEYPrivateKey** result_private_key) {
  Core* core = reinterpret_cast<Core*>(arg);
  DCHECK(core->OnNSSTaskRunner());

  core->PostOrRunCallback(
      FROM_HERE,
      base::Bind(&AddLogEvent, core->weak_net_log_,
                 NetLog::TYPE_SSL_CLIENT_CERT_REQUESTED));

  // TODO(droger): Support client auth on iOS. See http://crbug.com/145954).
  LOG(WARNING) << "Client auth is not supported";

  // Never send a certificate.
  core->AddCertProvidedEvent(0);
  return SECFailure;
}

#else  // NSS_PLATFORM_CLIENT_AUTH

// static
// Based on Mozilla's NSS_GetClientAuthData.
SECStatus SSLClientSocketNSS::Core::ClientAuthHandler(
    void* arg,
    PRFileDesc* socket,
    CERTDistNames* ca_names,
    CERTCertificate** result_certificate,
    SECKEYPrivateKey** result_private_key) {
  Core* core = reinterpret_cast<Core*>(arg);
  DCHECK(core->OnNSSTaskRunner());

  core->PostOrRunCallback(
      FROM_HERE,
      base::Bind(&AddLogEvent, core->weak_net_log_,
                 NetLog::TYPE_SSL_CLIENT_CERT_REQUESTED));

  // Regular client certificate requested.
  core->client_auth_cert_needed_ = !core->ssl_config_.send_client_cert;
  void* wincx  = SSL_RevealPinArg(socket);

  if (core->ssl_config_.send_client_cert) {
    // Second pass: a client certificate should have been selected.
    if (core->ssl_config_.client_cert.get()) {
      CERTCertificate* cert =
          CERT_DupCertificate(core->ssl_config_.client_cert->os_cert_handle());
      SECKEYPrivateKey* privkey = PK11_FindKeyByAnyCert(cert, wincx);
      if (privkey) {
        // TODO(jsorianopastor): We should wait for server certificate
        // verification before sending our credentials.  See
        // http://crbug.com/13934.
        *result_certificate = cert;
        *result_private_key = privkey;
        // A cert_count of -1 means the number of certificates is unknown.
        // NSS will construct the certificate chain.
        core->AddCertProvidedEvent(-1);

        return SECSuccess;
      }
      LOG(WARNING) << "Client cert found without private key";
    }
    // Send no client certificate.
    core->AddCertProvidedEvent(0);
    return SECFailure;
  }

  // First pass: client certificate is needed.
  core->nss_handshake_state_.cert_authorities.clear();

  // Retrieve the DER-encoded DistinguishedName of the cert issuers accepted by
  // the server and save them in |cert_authorities|.
  for (int i = 0; i < ca_names->nnames; i++) {
    core->nss_handshake_state_.cert_authorities.push_back(std::string(
        reinterpret_cast<const char*>(ca_names->names[i].data),
        static_cast<size_t>(ca_names->names[i].len)));
  }

  // Update the network task runner's view of the handshake state now that
  // server certificate request has been recorded.
  core->PostOrRunCallback(
      FROM_HERE, base::Bind(&Core::OnHandshakeStateUpdated, core,
                            core->nss_handshake_state_));

  // Tell NSS to suspend the client authentication.  We will then abort the
  // handshake by returning ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  return SECWouldBlock;
}
#endif  // NSS_PLATFORM_CLIENT_AUTH

// static
SECStatus SSLClientSocketNSS::Core::CanFalseStartCallback(
    PRFileDesc* socket,
    void* arg,
    PRBool* can_false_start) {
  // If the server doesn't support NPN or ALPN, then we don't do False
  // Start with it.
  PRBool negotiated_extension;
  SECStatus rv = SSL_HandshakeNegotiatedExtension(socket,
                                                  ssl_app_layer_protocol_xtn,
                                                  &negotiated_extension);
  if (rv != SECSuccess || !negotiated_extension) {
    rv = SSL_HandshakeNegotiatedExtension(socket,
                                          ssl_next_proto_nego_xtn,
                                          &negotiated_extension);
  }
  if (rv != SECSuccess || !negotiated_extension) {
    *can_false_start = PR_FALSE;
    return SECSuccess;
  }

  return SSL_RecommendedCanFalseStart(socket, can_false_start);
}

// static
void SSLClientSocketNSS::Core::HandshakeCallback(
    PRFileDesc* socket,
    void* arg) {
  Core* core = reinterpret_cast<Core*>(arg);
  DCHECK(core->OnNSSTaskRunner());

  core->handshake_callback_called_ = true;
  if (core->false_started_) {
    core->false_started_ = false;
    // If the connection was False Started, then at the time of this callback,
    // the peer's certificate will have been verified or the caller will have
    // accepted the error.
    // This is guaranteed when using False Start because this callback will
    // not be invoked until processing the peer's Finished message, which
    // will only happen in a PR_Read/PR_Write call, which can only happen
    // after the peer's certificate is verified.
    SSL_CacheSessionUnlocked(socket);

    // Additionally, when False Starting, DoHandshake() will have already
    // called HandshakeSucceeded(), so return now.
    return;
  }
  core->HandshakeSucceeded();
}

void SSLClientSocketNSS::Core::HandshakeSucceeded() {
  DCHECK(OnNSSTaskRunner());

  PRBool last_handshake_resumed;
  SECStatus rv = SSL_HandshakeResumedSession(nss_fd_, &last_handshake_resumed);
  if (rv == SECSuccess && last_handshake_resumed) {
    nss_handshake_state_.resumed_handshake = true;
  } else {
    nss_handshake_state_.resumed_handshake = false;
  }

  RecordChannelIDSupportOnNSSTaskRunner();
  UpdateServerCert();
  UpdateSignedCertTimestamps();
  UpdateStapledOCSPResponse();
  UpdateConnectionStatus();
  UpdateNextProto();

  // Update the network task runners view of the handshake state whenever
  // a handshake has completed.
  PostOrRunCallback(
      FROM_HERE, base::Bind(&Core::OnHandshakeStateUpdated, this,
                            nss_handshake_state_));
}

int SSLClientSocketNSS::Core::HandleNSSError(PRErrorCode nss_error,
                                             bool handshake_error) {
  DCHECK(OnNSSTaskRunner());

  int net_error = handshake_error ? MapNSSClientHandshakeError(nss_error) :
                                    MapNSSClientError(nss_error);

#if defined(OS_WIN)
  // On Windows, a handle to the HCRYPTPROV is cached in the X509Certificate
  // os_cert_handle() as an optimization. However, if the certificate
  // private key is stored on a smart card, and the smart card is removed,
  // the cached HCRYPTPROV will not be able to obtain the HCRYPTKEY again,
  // preventing client certificate authentication. Because the
  // X509Certificate may outlive the individual SSLClientSocketNSS, due to
  // caching in X509Certificate, this failure ends up preventing client
  // certificate authentication with the same certificate for all future
  // attempts, even after the smart card has been re-inserted. By setting
  // the CERT_KEY_PROV_HANDLE_PROP_ID to NULL, the cached HCRYPTPROV will
  // typically be freed. This allows a new HCRYPTPROV to be obtained from
  // the certificate on the next attempt, which should succeed if the smart
  // card has been re-inserted, or will typically prompt the user to
  // re-insert the smart card if not.
  if ((net_error == ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY ||
       net_error == ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED) &&
      ssl_config_.send_client_cert && ssl_config_.client_cert) {
    CertSetCertificateContextProperty(
        ssl_config_.client_cert->os_cert_handle(),
        CERT_KEY_PROV_HANDLE_PROP_ID, 0, NULL);
  }
#endif

  return net_error;
}

int SSLClientSocketNSS::Core::DoHandshakeLoop(int last_io_result) {
  DCHECK(OnNSSTaskRunner());

  int rv = last_io_result;
  do {
    // Default to STATE_NONE for next state.
    State state = next_handshake_state_;
    GotoState(STATE_NONE);

    switch (state) {
      case STATE_HANDSHAKE:
        rv = DoHandshake();
        break;
      case STATE_GET_DOMAIN_BOUND_CERT_COMPLETE:
        rv = DoGetDBCertComplete(rv);
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state " << state;
        break;
    }

    // Do the actual network I/O
    bool network_moved = DoTransportIO();
    if (network_moved && next_handshake_state_ == STATE_HANDSHAKE) {
      // In general we exit the loop if rv is ERR_IO_PENDING.  In this
      // special case we keep looping even if rv is ERR_IO_PENDING because
      // the transport IO may allow DoHandshake to make progress.
      DCHECK(rv == OK || rv == ERR_IO_PENDING);
      rv = OK;  // This causes us to stay in the loop.
    }
  } while (rv != ERR_IO_PENDING && next_handshake_state_ != STATE_NONE);
  return rv;
}

int SSLClientSocketNSS::Core::DoReadLoop(int result) {
  DCHECK(OnNSSTaskRunner());
  DCHECK(false_started_ || handshake_callback_called_);
  DCHECK_EQ(STATE_NONE, next_handshake_state_);

  if (result < 0)
    return result;

  if (!nss_bufs_) {
    LOG(DFATAL) << "!nss_bufs_";
    int rv = ERR_UNEXPECTED;
    PostOrRunCallback(
        FROM_HERE,
        base::Bind(&AddLogEventWithCallback, weak_net_log_,
                   NetLog::TYPE_SSL_READ_ERROR,
                   CreateNetLogSSLErrorCallback(rv, 0)));
    return rv;
  }

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadRead();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  return rv;
}

int SSLClientSocketNSS::Core::DoWriteLoop(int result) {
  DCHECK(OnNSSTaskRunner());
  DCHECK(false_started_ || handshake_callback_called_);
  DCHECK_EQ(STATE_NONE, next_handshake_state_);

  if (result < 0)
    return result;

  if (!nss_bufs_) {
    LOG(DFATAL) << "!nss_bufs_";
    int rv = ERR_UNEXPECTED;
    PostOrRunCallback(
        FROM_HERE,
        base::Bind(&AddLogEventWithCallback, weak_net_log_,
                   NetLog::TYPE_SSL_READ_ERROR,
                   CreateNetLogSSLErrorCallback(rv, 0)));
    return rv;
  }

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadWrite();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  LeaveFunction(rv);
  return rv;
}

int SSLClientSocketNSS::Core::DoHandshake() {
  DCHECK(OnNSSTaskRunner());

  int net_error = net::OK;
  SECStatus rv = SSL_ForceHandshake(nss_fd_);

  // Note: this function may be called multiple times during the handshake, so
  // even though channel id and client auth are separate else cases, they can
  // both be used during a single SSL handshake.
  if (channel_id_needed_) {
    GotoState(STATE_GET_DOMAIN_BOUND_CERT_COMPLETE);
    net_error = ERR_IO_PENDING;
  } else if (client_auth_cert_needed_) {
    net_error = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    PostOrRunCallback(
        FROM_HERE,
        base::Bind(&AddLogEventWithCallback, weak_net_log_,
                   NetLog::TYPE_SSL_HANDSHAKE_ERROR,
                   CreateNetLogSSLErrorCallback(net_error, 0)));

    // If the handshake already succeeded (because the server requests but
    // doesn't require a client cert), we need to invalidate the SSL session
    // so that we won't try to resume the non-client-authenticated session in
    // the next handshake.  This will cause the server to ask for a client
    // cert again.
    if (rv == SECSuccess && SSL_InvalidateSession(nss_fd_) != SECSuccess)
      LOG(WARNING) << "Couldn't invalidate SSL session: " << PR_GetError();
  } else if (rv == SECSuccess) {
    if (!handshake_callback_called_) {
      false_started_ = true;
      HandshakeSucceeded();
    }
  } else {
    PRErrorCode prerr = PR_GetError();
    net_error = HandleNSSError(prerr, true);

    // Some network devices that inspect application-layer packets seem to
    // inject TCP reset packets to break the connections when they see
    // TLS 1.1 in ClientHello or ServerHello. See http://crbug.com/130293.
    //
    // Only allow ERR_CONNECTION_RESET to trigger a fallback from TLS 1.1 or
    // 1.2. We don't lose much in this fallback because the explicit IV for CBC
    // mode in TLS 1.1 is approximated by record splitting in TLS 1.0. The
    // fallback will be more painful for TLS 1.2 when we have GCM support.
    //
    // ERR_CONNECTION_RESET is a common network error, so we don't want it
    // to trigger a version fallback in general, especially the TLS 1.0 ->
    // SSL 3.0 fallback, which would drop TLS extensions.
    if (prerr == PR_CONNECT_RESET_ERROR &&
        ssl_config_.version_max >= SSL_PROTOCOL_VERSION_TLS1_1) {
      net_error = ERR_SSL_PROTOCOL_ERROR;
    }

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE);
    } else {
      PostOrRunCallback(
          FROM_HERE,
          base::Bind(&AddLogEventWithCallback, weak_net_log_,
                     NetLog::TYPE_SSL_HANDSHAKE_ERROR,
                     CreateNetLogSSLErrorCallback(net_error, prerr)));
    }
  }

  return net_error;
}

int SSLClientSocketNSS::Core::DoGetDBCertComplete(int result) {
  SECStatus rv;
  PostOrRunCallback(
      FROM_HERE,
      base::Bind(&BoundNetLog::EndEventWithNetErrorCode, weak_net_log_,
                 NetLog::TYPE_SSL_GET_DOMAIN_BOUND_CERT, result));

  channel_id_needed_ = false;

  if (result != OK)
    return result;

  SECKEYPublicKey* public_key;
  SECKEYPrivateKey* private_key;
  int error = ImportChannelIDKeys(&public_key, &private_key);
  if (error != OK)
    return error;

  rv = SSL_RestartHandshakeAfterChannelIDReq(nss_fd_, public_key, private_key);
  if (rv != SECSuccess)
    return MapNSSError(PORT_GetError());

  SetChannelIDProvided();
  GotoState(STATE_HANDSHAKE);
  return OK;
}

int SSLClientSocketNSS::Core::DoPayloadRead() {
  DCHECK(OnNSSTaskRunner());
  DCHECK(user_read_buf_.get());
  DCHECK_GT(user_read_buf_len_, 0);

  int rv;
  // If a previous greedy read resulted in an error that was not consumed (eg:
  // due to the caller having read some data successfully), then return that
  // pending error now.
  if (pending_read_result_ != kNoPendingReadResult) {
    rv = pending_read_result_;
    PRErrorCode prerr = pending_read_nss_error_;
    pending_read_result_ = kNoPendingReadResult;
    pending_read_nss_error_ = 0;

    if (rv == 0) {
      PostOrRunCallback(
          FROM_HERE,
          base::Bind(&LogByteTransferEvent, weak_net_log_,
                     NetLog::TYPE_SSL_SOCKET_BYTES_RECEIVED, rv,
                     scoped_refptr<IOBuffer>(user_read_buf_)));
    } else {
      PostOrRunCallback(
          FROM_HERE,
          base::Bind(&AddLogEventWithCallback, weak_net_log_,
                     NetLog::TYPE_SSL_READ_ERROR,
                     CreateNetLogSSLErrorCallback(rv, prerr)));
    }
    return rv;
  }

  // Perform a greedy read, attempting to read as much as the caller has
  // requested. In the current NSS implementation, PR_Read will return
  // exactly one SSL application data record's worth of data per invocation.
  // The record size is dictated by the server, and may be noticeably smaller
  // than the caller's buffer. This may be as little as a single byte, if the
  // server is performing 1/n-1 record splitting.
  //
  // However, this greedy read may result in renegotiations/re-handshakes
  // happening or may lead to some data being read, followed by an EOF (such as
  // a TLS close-notify). If at least some data was read, then that result
  // should be deferred until the next call to DoPayloadRead(). Otherwise, if no
  // data was read, it's safe to return the error or EOF immediately.
  int total_bytes_read = 0;
  do {
    rv = PR_Read(nss_fd_, user_read_buf_->data() + total_bytes_read,
                 user_read_buf_len_ - total_bytes_read);
    if (rv > 0)
      total_bytes_read += rv;
  } while (total_bytes_read < user_read_buf_len_ && rv > 0);
  int amount_in_read_buffer = memio_GetReadableBufferSize(nss_bufs_);
  PostOrRunCallback(FROM_HERE, base::Bind(&Core::OnNSSBufferUpdated, this,
                                          amount_in_read_buffer));

  if (total_bytes_read == user_read_buf_len_) {
    // The caller's entire request was satisfied without error. No further
    // processing needed.
    rv = total_bytes_read;
  } else {
    // Otherwise, an error occurred (rv <= 0). The error needs to be handled
    // immediately, while the NSPR/NSS errors are still available in
    // thread-local storage. However, the handled/remapped error code should
    // only be returned if no application data was already read; if it was, the
    // error code should be deferred until the next call of DoPayloadRead.
    //
    // If no data was read, |*next_result| will point to the return value of
    // this function. If at least some data was read, |*next_result| will point
    // to |pending_read_error_|, to be returned in a future call to
    // DoPayloadRead() (e.g.: after the current data is handled).
    int* next_result = &rv;
    if (total_bytes_read > 0) {
      pending_read_result_ = rv;
      rv = total_bytes_read;
      next_result = &pending_read_result_;
    }

    if (client_auth_cert_needed_) {
      *next_result = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
      pending_read_nss_error_ = 0;
    } else if (*next_result < 0) {
      // If *next_result == 0, then that indicates EOF, and no special error
      // handling is needed.
      pending_read_nss_error_ = PR_GetError();
      *next_result = HandleNSSError(pending_read_nss_error_, false);
      if (rv > 0 && *next_result == ERR_IO_PENDING) {
        // If at least some data was read from PR_Read(), do not treat
        // insufficient data as an error to return in the next call to
        // DoPayloadRead() - instead, let the call fall through to check
        // PR_Read() again. This is because DoTransportIO() may complete
        // in between the next call to DoPayloadRead(), and thus it is
        // important to check PR_Read() on subsequent invocations to see
        // if a complete record may now be read.
        pending_read_nss_error_ = 0;
        pending_read_result_ = kNoPendingReadResult;
      }
    }
  }

  DCHECK_NE(ERR_IO_PENDING, pending_read_result_);

  if (rv >= 0) {
    PostOrRunCallback(
        FROM_HERE,
        base::Bind(&LogByteTransferEvent, weak_net_log_,
                   NetLog::TYPE_SSL_SOCKET_BYTES_RECEIVED, rv,
                   scoped_refptr<IOBuffer>(user_read_buf_)));
  } else if (rv != ERR_IO_PENDING) {
    PostOrRunCallback(
        FROM_HERE,
        base::Bind(&AddLogEventWithCallback, weak_net_log_,
                   NetLog::TYPE_SSL_READ_ERROR,
                   CreateNetLogSSLErrorCallback(rv, pending_read_nss_error_)));
    pending_read_nss_error_ = 0;
  }
  return rv;
}

int SSLClientSocketNSS::Core::DoPayloadWrite() {
  DCHECK(OnNSSTaskRunner());

  DCHECK(user_write_buf_.get());

  int old_amount_in_read_buffer = memio_GetReadableBufferSize(nss_bufs_);
  int rv = PR_Write(nss_fd_, user_write_buf_->data(), user_write_buf_len_);
  int new_amount_in_read_buffer = memio_GetReadableBufferSize(nss_bufs_);
  // PR_Write could potentially consume the unhandled data in the memio read
  // buffer if a renegotiation is in progress. If the buffer is consumed,
  // notify the latest buffer size to NetworkRunner.
  if (old_amount_in_read_buffer != new_amount_in_read_buffer) {
    PostOrRunCallback(
        FROM_HERE,
        base::Bind(&Core::OnNSSBufferUpdated, this, new_amount_in_read_buffer));
  }
  if (rv >= 0) {
    PostOrRunCallback(
        FROM_HERE,
        base::Bind(&LogByteTransferEvent, weak_net_log_,
                   NetLog::TYPE_SSL_SOCKET_BYTES_SENT, rv,
                   scoped_refptr<IOBuffer>(user_write_buf_)));
    return rv;
  }
  PRErrorCode prerr = PR_GetError();
  if (prerr == PR_WOULD_BLOCK_ERROR)
    return ERR_IO_PENDING;

  rv = HandleNSSError(prerr, false);
  PostOrRunCallback(
      FROM_HERE,
      base::Bind(&AddLogEventWithCallback, weak_net_log_,
                 NetLog::TYPE_SSL_WRITE_ERROR,
                 CreateNetLogSSLErrorCallback(rv, prerr)));
  return rv;
}

// Do as much network I/O as possible between the buffer and the
// transport socket. Return true if some I/O performed, false
// otherwise (error or ERR_IO_PENDING).
bool SSLClientSocketNSS::Core::DoTransportIO() {
  DCHECK(OnNSSTaskRunner());

  bool network_moved = false;
  if (nss_bufs_ != NULL) {
    int rv;
    // Read and write as much data as we can. The loop is neccessary
    // because Write() may return synchronously.
    do {
      rv = BufferSend();
      if (rv != ERR_IO_PENDING && rv != 0)
        network_moved = true;
    } while (rv > 0);
    if (!transport_recv_eof_ && BufferRecv() != ERR_IO_PENDING)
      network_moved = true;
  }
  return network_moved;
}

int SSLClientSocketNSS::Core::BufferRecv() {
  DCHECK(OnNSSTaskRunner());

  if (transport_recv_busy_)
    return ERR_IO_PENDING;

  // If NSS is blocked on reading from |nss_bufs_|, because it is empty,
  // determine how much data NSS wants to read. If NSS was not blocked,
  // this will return 0.
  int requested = memio_GetReadRequest(nss_bufs_);
  if (requested == 0) {
    // This is not a perfect match of error codes, as no operation is
    // actually pending. However, returning 0 would be interpreted as a
    // possible sign of EOF, which is also an inappropriate match.
    return ERR_IO_PENDING;
  }

  char* buf;
  int nb = memio_GetReadParams(nss_bufs_, &buf);
  int rv;
  if (!nb) {
    // buffer too full to read into, so no I/O possible at moment
    rv = ERR_IO_PENDING;
  } else {
    scoped_refptr<IOBuffer> read_buffer(new IOBuffer(nb));
    if (OnNetworkTaskRunner()) {
      rv = DoBufferRecv(read_buffer.get(), nb);
    } else {
      bool posted = network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(IgnoreResult(&Core::DoBufferRecv), this, read_buffer,
                     nb));
      rv = posted ? ERR_IO_PENDING : ERR_ABORTED;
    }

    if (rv == ERR_IO_PENDING) {
      transport_recv_busy_ = true;
    } else {
      if (rv > 0) {
        memcpy(buf, read_buffer->data(), rv);
      } else if (rv == 0) {
        transport_recv_eof_ = true;
      }
      memio_PutReadResult(nss_bufs_, MapErrorToNSS(rv));
    }
  }
  return rv;
}

// Return 0 if nss_bufs_ was empty,
// > 0 for bytes transferred immediately,
// < 0 for error (or the non-error ERR_IO_PENDING).
int SSLClientSocketNSS::Core::BufferSend() {
  DCHECK(OnNSSTaskRunner());

  if (transport_send_busy_)
    return ERR_IO_PENDING;

  const char* buf1;
  const char* buf2;
  unsigned int len1, len2;
  if (memio_GetWriteParams(nss_bufs_, &buf1, &len1, &buf2, &len2)) {
    // It is important this return synchronously to prevent spinning infinitely
    // in the off-thread NSS case. The error code itself is ignored, so just
    // return ERR_ABORTED. See https://crbug.com/381160.
    return ERR_ABORTED;
  }
  const unsigned int len = len1 + len2;

  int rv = 0;
  if (len) {
    scoped_refptr<IOBuffer> send_buffer(new IOBuffer(len));
    memcpy(send_buffer->data(), buf1, len1);
    memcpy(send_buffer->data() + len1, buf2, len2);

    if (OnNetworkTaskRunner()) {
      rv = DoBufferSend(send_buffer.get(), len);
    } else {
      bool posted = network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(IgnoreResult(&Core::DoBufferSend), this, send_buffer,
                     len));
      rv = posted ? ERR_IO_PENDING : ERR_ABORTED;
    }

    if (rv == ERR_IO_PENDING) {
      transport_send_busy_ = true;
    } else {
      memio_PutWriteResult(nss_bufs_, MapErrorToNSS(rv));
    }
  }

  return rv;
}

void SSLClientSocketNSS::Core::OnRecvComplete(int result) {
  DCHECK(OnNSSTaskRunner());

  if (next_handshake_state_ == STATE_HANDSHAKE) {
    OnHandshakeIOComplete(result);
    return;
  }

  // Network layer received some data, check if client requested to read
  // decrypted data.
  if (!user_read_buf_.get())
    return;

  int rv = DoReadLoop(result);
  if (rv != ERR_IO_PENDING)
    DoReadCallback(rv);
}

void SSLClientSocketNSS::Core::OnSendComplete(int result) {
  DCHECK(OnNSSTaskRunner());

  if (next_handshake_state_ == STATE_HANDSHAKE) {
    OnHandshakeIOComplete(result);
    return;
  }

  // OnSendComplete may need to call DoPayloadRead while the renegotiation
  // handshake is in progress.
  int rv_read = ERR_IO_PENDING;
  int rv_write = ERR_IO_PENDING;
  bool network_moved;
  do {
    if (user_read_buf_.get())
      rv_read = DoPayloadRead();
    if (user_write_buf_.get())
      rv_write = DoPayloadWrite();
    network_moved = DoTransportIO();
  } while (rv_read == ERR_IO_PENDING && rv_write == ERR_IO_PENDING &&
           (user_read_buf_.get() || user_write_buf_.get()) && network_moved);

  // If the parent SSLClientSocketNSS is deleted during the processing of the
  // Read callback and OnNSSTaskRunner() == OnNetworkTaskRunner(), then the Core
  // will be detached (and possibly deleted). Guard against deletion by taking
  // an extra reference, then check if the Core was detached before invoking the
  // next callback.
  scoped_refptr<Core> guard(this);
  if (user_read_buf_.get() && rv_read != ERR_IO_PENDING)
    DoReadCallback(rv_read);

  if (OnNetworkTaskRunner() && detached_)
    return;

  if (user_write_buf_.get() && rv_write != ERR_IO_PENDING)
    DoWriteCallback(rv_write);
}

// As part of Connect(), the SSLClientSocketNSS object performs an SSL
// handshake. This requires network IO, which in turn calls
// BufferRecvComplete() with a non-zero byte count. This byte count eventually
// winds its way through the state machine and ends up being passed to the
// callback. For Read() and Write(), that's what we want. But for Connect(),
// the caller expects OK (i.e. 0) for success.
void SSLClientSocketNSS::Core::DoConnectCallback(int rv) {
  DCHECK(OnNSSTaskRunner());
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!user_connect_callback_.is_null());

  base::Closure c = base::Bind(
      base::ResetAndReturn(&user_connect_callback_),
      rv > OK ? OK : rv);
  PostOrRunCallback(FROM_HERE, c);
}

void SSLClientSocketNSS::Core::DoReadCallback(int rv) {
  DCHECK(OnNSSTaskRunner());
  DCHECK_NE(ERR_IO_PENDING, rv);
  DCHECK(!user_read_callback_.is_null());

  user_read_buf_ = NULL;
  user_read_buf_len_ = 0;
  int amount_in_read_buffer = memio_GetReadableBufferSize(nss_bufs_);
  // This is used to curry the |amount_int_read_buffer| and |user_cb| back to
  // the network task runner.
  PostOrRunCallback(
      FROM_HERE,
      base::Bind(&Core::OnNSSBufferUpdated, this, amount_in_read_buffer));
  PostOrRunCallback(
      FROM_HERE,
      base::Bind(&Core::DidNSSRead, this, rv));
  PostOrRunCallback(
      FROM_HERE,
      base::Bind(base::ResetAndReturn(&user_read_callback_), rv));
}

void SSLClientSocketNSS::Core::DoWriteCallback(int rv) {
  DCHECK(OnNSSTaskRunner());
  DCHECK_NE(ERR_IO_PENDING, rv);
  DCHECK(!user_write_callback_.is_null());

  // Since Run may result in Write being called, clear |user_write_callback_|
  // up front.
  user_write_buf_ = NULL;
  user_write_buf_len_ = 0;
  // Update buffer status because DoWriteLoop called DoTransportIO which may
  // perform read operations.
  int amount_in_read_buffer = memio_GetReadableBufferSize(nss_bufs_);
  // This is used to curry the |amount_int_read_buffer| and |user_cb| back to
  // the network task runner.
  PostOrRunCallback(
      FROM_HERE,
      base::Bind(&Core::OnNSSBufferUpdated, this, amount_in_read_buffer));
  PostOrRunCallback(
      FROM_HERE,
      base::Bind(&Core::DidNSSWrite, this, rv));
  PostOrRunCallback(
      FROM_HERE,
      base::Bind(base::ResetAndReturn(&user_write_callback_), rv));
}

SECStatus SSLClientSocketNSS::Core::ClientChannelIDHandler(
    void* arg,
    PRFileDesc* socket,
    SECKEYPublicKey **out_public_key,
    SECKEYPrivateKey **out_private_key) {
  Core* core = reinterpret_cast<Core*>(arg);
  DCHECK(core->OnNSSTaskRunner());

  core->PostOrRunCallback(
      FROM_HERE,
      base::Bind(&AddLogEvent, core->weak_net_log_,
                 NetLog::TYPE_SSL_CHANNEL_ID_REQUESTED));

  // We have negotiated the TLS channel ID extension.
  core->channel_id_xtn_negotiated_ = true;
  std::string host = core->host_and_port_.host();
  int error = ERR_UNEXPECTED;
  if (core->OnNetworkTaskRunner()) {
    error = core->DoGetDomainBoundCert(host);
  } else {
    bool posted = core->network_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            IgnoreResult(&Core::DoGetDomainBoundCert),
            core, host));
    error = posted ? ERR_IO_PENDING : ERR_ABORTED;
  }

  if (error == ERR_IO_PENDING) {
    // Asynchronous case.
    core->channel_id_needed_ = true;
    return SECWouldBlock;
  }

  core->PostOrRunCallback(
      FROM_HERE,
      base::Bind(&BoundNetLog::EndEventWithNetErrorCode, core->weak_net_log_,
                 NetLog::TYPE_SSL_GET_DOMAIN_BOUND_CERT, error));
  SECStatus rv = SECSuccess;
  if (error == OK) {
    // Synchronous success.
    int result = core->ImportChannelIDKeys(out_public_key, out_private_key);
    if (result == OK)
      core->SetChannelIDProvided();
    else
      rv = SECFailure;
  } else {
    rv = SECFailure;
  }

  return rv;
}

int SSLClientSocketNSS::Core::ImportChannelIDKeys(SECKEYPublicKey** public_key,
                                                  SECKEYPrivateKey** key) {
  // Set the certificate.
  SECItem cert_item;
  cert_item.data = (unsigned char*) domain_bound_cert_.data();
  cert_item.len = domain_bound_cert_.size();
  ScopedCERTCertificate cert(CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                                     &cert_item,
                                                     NULL,
                                                     PR_FALSE,
                                                     PR_TRUE));
  if (cert == NULL)
    return MapNSSError(PORT_GetError());

  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  // Set the private key.
  if (!crypto::ECPrivateKey::ImportFromEncryptedPrivateKeyInfo(
          slot.get(),
          ServerBoundCertService::kEPKIPassword,
          reinterpret_cast<const unsigned char*>(
              domain_bound_private_key_.data()),
          domain_bound_private_key_.size(),
          &cert->subjectPublicKeyInfo,
          false,
          false,
          key,
          public_key)) {
    int error = MapNSSError(PORT_GetError());
    return error;
  }

  return OK;
}

void SSLClientSocketNSS::Core::UpdateServerCert() {
  nss_handshake_state_.server_cert_chain.Reset(nss_fd_);
  nss_handshake_state_.server_cert = X509Certificate::CreateFromDERCertChain(
      nss_handshake_state_.server_cert_chain.AsStringPieceVector());
  if (nss_handshake_state_.server_cert.get()) {
    // Since this will be called asynchronously on another thread, it needs to
    // own a reference to the certificate.
    NetLog::ParametersCallback net_log_callback =
        base::Bind(&NetLogX509CertificateCallback,
                   nss_handshake_state_.server_cert);
    PostOrRunCallback(
        FROM_HERE,
        base::Bind(&AddLogEventWithCallback, weak_net_log_,
                   NetLog::TYPE_SSL_CERTIFICATES_RECEIVED,
                   net_log_callback));
  }
}

void SSLClientSocketNSS::Core::UpdateSignedCertTimestamps() {
  const SECItem* signed_cert_timestamps =
      SSL_PeerSignedCertTimestamps(nss_fd_);

  if (!signed_cert_timestamps || !signed_cert_timestamps->len)
    return;

  nss_handshake_state_.sct_list_from_tls_extension = std::string(
      reinterpret_cast<char*>(signed_cert_timestamps->data),
      signed_cert_timestamps->len);
}

void SSLClientSocketNSS::Core::UpdateStapledOCSPResponse() {
  PRBool ocsp_requested = PR_FALSE;
  SSL_OptionGet(nss_fd_, SSL_ENABLE_OCSP_STAPLING, &ocsp_requested);
  const SECItemArray* ocsp_responses =
      SSL_PeerStapledOCSPResponses(nss_fd_);
  bool ocsp_responses_present = ocsp_responses && ocsp_responses->len;
  if (ocsp_requested)
    UMA_HISTOGRAM_BOOLEAN("Net.OCSPResponseStapled", ocsp_responses_present);
  if (!ocsp_responses_present)
    return;

  nss_handshake_state_.stapled_ocsp_response = std::string(
      reinterpret_cast<char*>(ocsp_responses->items[0].data),
      ocsp_responses->items[0].len);

  // TODO(agl): figure out how to plumb an OCSP response into the Mac
  // system library and update IsOCSPStaplingSupported for Mac.
  if (IsOCSPStaplingSupported()) {
  #if defined(OS_WIN)
    if (nss_handshake_state_.server_cert) {
      CRYPT_DATA_BLOB ocsp_response_blob;
      ocsp_response_blob.cbData = ocsp_responses->items[0].len;
      ocsp_response_blob.pbData = ocsp_responses->items[0].data;
      BOOL ok = CertSetCertificateContextProperty(
          nss_handshake_state_.server_cert->os_cert_handle(),
          CERT_OCSP_RESPONSE_PROP_ID,
          CERT_SET_PROPERTY_IGNORE_PERSIST_ERROR_FLAG,
          &ocsp_response_blob);
      if (!ok) {
        VLOG(1) << "Failed to set OCSP response property: "
                << GetLastError();
      }
    }
  #elif defined(USE_NSS)
    CacheOCSPResponseFromSideChannelFunction cache_ocsp_response =
        GetCacheOCSPResponseFromSideChannelFunction();

    cache_ocsp_response(
        CERT_GetDefaultCertDB(),
        nss_handshake_state_.server_cert_chain[0], PR_Now(),
        &ocsp_responses->items[0], NULL);
  #endif
  }  // IsOCSPStaplingSupported()
}

void SSLClientSocketNSS::Core::UpdateConnectionStatus() {
  SSLChannelInfo channel_info;
  SECStatus ok = SSL_GetChannelInfo(nss_fd_,
                                    &channel_info, sizeof(channel_info));
  if (ok == SECSuccess &&
      channel_info.length == sizeof(channel_info) &&
      channel_info.cipherSuite) {
    nss_handshake_state_.ssl_connection_status |=
        (static_cast<int>(channel_info.cipherSuite) &
         SSL_CONNECTION_CIPHERSUITE_MASK) <<
        SSL_CONNECTION_CIPHERSUITE_SHIFT;

    nss_handshake_state_.ssl_connection_status |=
        (static_cast<int>(channel_info.compressionMethod) &
         SSL_CONNECTION_COMPRESSION_MASK) <<
        SSL_CONNECTION_COMPRESSION_SHIFT;

    // NSS 3.14.x doesn't have a version macro for TLS 1.2 (because NSS didn't
    // support it yet), so use 0x0303 directly.
    int version = SSL_CONNECTION_VERSION_UNKNOWN;
    if (channel_info.protocolVersion < SSL_LIBRARY_VERSION_3_0) {
      // All versions less than SSL_LIBRARY_VERSION_3_0 are treated as SSL
      // version 2.
      version = SSL_CONNECTION_VERSION_SSL2;
    } else if (channel_info.protocolVersion == SSL_LIBRARY_VERSION_3_0) {
      version = SSL_CONNECTION_VERSION_SSL3;
    } else if (channel_info.protocolVersion == SSL_LIBRARY_VERSION_3_1_TLS) {
      version = SSL_CONNECTION_VERSION_TLS1;
    } else if (channel_info.protocolVersion == SSL_LIBRARY_VERSION_TLS_1_1) {
      version = SSL_CONNECTION_VERSION_TLS1_1;
    } else if (channel_info.protocolVersion == 0x0303) {
      version = SSL_CONNECTION_VERSION_TLS1_2;
    }
    nss_handshake_state_.ssl_connection_status |=
        (version & SSL_CONNECTION_VERSION_MASK) <<
        SSL_CONNECTION_VERSION_SHIFT;
  }

  PRBool peer_supports_renego_ext;
  ok = SSL_HandshakeNegotiatedExtension(nss_fd_, ssl_renegotiation_info_xtn,
                                        &peer_supports_renego_ext);
  if (ok == SECSuccess) {
    if (!peer_supports_renego_ext) {
      nss_handshake_state_.ssl_connection_status |=
          SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION;
      // Log an informational message if the server does not support secure
      // renegotiation (RFC 5746).
      VLOG(1) << "The server " << host_and_port_.ToString()
              << " does not support the TLS renegotiation_info extension.";
    }
    UMA_HISTOGRAM_ENUMERATION("Net.RenegotiationExtensionSupported",
                              peer_supports_renego_ext, 2);

    // We would like to eliminate fallback to SSLv3 for non-buggy servers
    // because of security concerns. For example, Google offers forward
    // secrecy with ECDHE but that requires TLS 1.0. An attacker can block
    // TLSv1 connections and force us to downgrade to SSLv3 and remove forward
    // secrecy.
    //
    // Yngve from Opera has suggested using the renegotiation extension as an
    // indicator that SSLv3 fallback was mistaken:
    // tools.ietf.org/html/draft-pettersen-tls-version-rollback-removal-00 .
    //
    // As a first step, measure how often clients perform version fallback
    // while the server advertises support secure renegotiation.
    if (ssl_config_.version_fallback &&
        channel_info.protocolVersion == SSL_LIBRARY_VERSION_3_0) {
      UMA_HISTOGRAM_BOOLEAN("Net.SSLv3FallbackToRenegoPatchedServer",
                            peer_supports_renego_ext == PR_TRUE);
    }
  }

  if (ssl_config_.version_fallback) {
    nss_handshake_state_.ssl_connection_status |=
        SSL_CONNECTION_VERSION_FALLBACK;
  }
}

void SSLClientSocketNSS::Core::UpdateNextProto() {
  uint8 buf[256];
  SSLNextProtoState state;
  unsigned buf_len;

  SECStatus rv = SSL_GetNextProto(nss_fd_, &state, buf, &buf_len, sizeof(buf));
  if (rv != SECSuccess)
    return;

  nss_handshake_state_.next_proto =
      std::string(reinterpret_cast<char*>(buf), buf_len);
  switch (state) {
    case SSL_NEXT_PROTO_NEGOTIATED:
    case SSL_NEXT_PROTO_SELECTED:
      nss_handshake_state_.next_proto_status = kNextProtoNegotiated;
      break;
    case SSL_NEXT_PROTO_NO_OVERLAP:
      nss_handshake_state_.next_proto_status = kNextProtoNoOverlap;
      break;
    case SSL_NEXT_PROTO_NO_SUPPORT:
      nss_handshake_state_.next_proto_status = kNextProtoUnsupported;
      break;
    default:
      NOTREACHED();
      break;
  }
}

void SSLClientSocketNSS::Core::RecordChannelIDSupportOnNSSTaskRunner() {
  DCHECK(OnNSSTaskRunner());
  if (nss_handshake_state_.resumed_handshake)
    return;

  // Copy the NSS task runner-only state to the network task runner and
  // log histograms from there, since the histograms also need access to the
  // network task runner state.
  PostOrRunCallback(
      FROM_HERE,
      base::Bind(&Core::RecordChannelIDSupportOnNetworkTaskRunner,
                 this,
                 channel_id_xtn_negotiated_,
                 ssl_config_.channel_id_enabled,
                 crypto::ECPrivateKey::IsSupported()));
}

void SSLClientSocketNSS::Core::RecordChannelIDSupportOnNetworkTaskRunner(
    bool negotiated_channel_id,
    bool channel_id_enabled,
    bool supports_ecc) const {
  DCHECK(OnNetworkTaskRunner());

  RecordChannelIDSupport(server_bound_cert_service_,
                         negotiated_channel_id,
                         channel_id_enabled,
                         supports_ecc);
}

int SSLClientSocketNSS::Core::DoBufferRecv(IOBuffer* read_buffer, int len) {
  DCHECK(OnNetworkTaskRunner());
  DCHECK_GT(len, 0);

  if (detached_)
    return ERR_ABORTED;

  int rv = transport_->socket()->Read(
      read_buffer, len,
      base::Bind(&Core::BufferRecvComplete, base::Unretained(this),
                 scoped_refptr<IOBuffer>(read_buffer)));

  if (!OnNSSTaskRunner() && rv != ERR_IO_PENDING) {
    nss_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::BufferRecvComplete, this,
                              scoped_refptr<IOBuffer>(read_buffer), rv));
    return rv;
  }

  return rv;
}

int SSLClientSocketNSS::Core::DoBufferSend(IOBuffer* send_buffer, int len) {
  DCHECK(OnNetworkTaskRunner());
  DCHECK_GT(len, 0);

  if (detached_)
    return ERR_ABORTED;

  int rv = transport_->socket()->Write(
      send_buffer, len,
      base::Bind(&Core::BufferSendComplete,
                 base::Unretained(this)));

  if (!OnNSSTaskRunner() && rv != ERR_IO_PENDING) {
    nss_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Core::BufferSendComplete, this, rv));
    return rv;
  }

  return rv;
}

int SSLClientSocketNSS::Core::DoGetDomainBoundCert(const std::string& host) {
  DCHECK(OnNetworkTaskRunner());

  if (detached_)
    return ERR_FAILED;

  weak_net_log_->BeginEvent(NetLog::TYPE_SSL_GET_DOMAIN_BOUND_CERT);

  int rv = server_bound_cert_service_->GetOrCreateDomainBoundCert(
      host,
      &domain_bound_private_key_,
      &domain_bound_cert_,
      base::Bind(&Core::OnGetDomainBoundCertComplete, base::Unretained(this)),
      &domain_bound_cert_request_handle_);

  if (rv != ERR_IO_PENDING && !OnNSSTaskRunner()) {
    nss_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Core::OnHandshakeIOComplete, this, rv));
    return ERR_IO_PENDING;
  }

  return rv;
}

void SSLClientSocketNSS::Core::OnHandshakeStateUpdated(
    const HandshakeState& state) {
  DCHECK(OnNetworkTaskRunner());
  network_handshake_state_ = state;
}

void SSLClientSocketNSS::Core::OnNSSBufferUpdated(int amount_in_read_buffer) {
  DCHECK(OnNetworkTaskRunner());
  unhandled_buffer_size_ = amount_in_read_buffer;
}

void SSLClientSocketNSS::Core::DidNSSRead(int result) {
  DCHECK(OnNetworkTaskRunner());
  DCHECK(nss_waiting_read_);
  nss_waiting_read_ = false;
  if (result <= 0) {
    nss_is_closed_ = true;
  } else {
    was_ever_used_ = true;
  }
}

void SSLClientSocketNSS::Core::DidNSSWrite(int result) {
  DCHECK(OnNetworkTaskRunner());
  DCHECK(nss_waiting_write_);
  nss_waiting_write_ = false;
  if (result < 0) {
    nss_is_closed_ = true;
  } else if (result > 0) {
    was_ever_used_ = true;
  }
}

void SSLClientSocketNSS::Core::BufferSendComplete(int result) {
  if (!OnNSSTaskRunner()) {
    if (detached_)
      return;

    nss_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::BufferSendComplete, this, result));
    return;
  }

  DCHECK(OnNSSTaskRunner());

  memio_PutWriteResult(nss_bufs_, MapErrorToNSS(result));
  transport_send_busy_ = false;
  OnSendComplete(result);
}

void SSLClientSocketNSS::Core::OnHandshakeIOComplete(int result) {
  if (!OnNSSTaskRunner()) {
    if (detached_)
      return;

    nss_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::OnHandshakeIOComplete, this, result));
    return;
  }

  DCHECK(OnNSSTaskRunner());

  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING)
    DoConnectCallback(rv);
}

void SSLClientSocketNSS::Core::OnGetDomainBoundCertComplete(int result) {
  DVLOG(1) << __FUNCTION__ << " " << result;
  DCHECK(OnNetworkTaskRunner());

  OnHandshakeIOComplete(result);
}

void SSLClientSocketNSS::Core::BufferRecvComplete(
    IOBuffer* read_buffer,
    int result) {
  DCHECK(read_buffer);

  if (!OnNSSTaskRunner()) {
    if (detached_)
      return;

    nss_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::BufferRecvComplete, this,
                              scoped_refptr<IOBuffer>(read_buffer), result));
    return;
  }

  DCHECK(OnNSSTaskRunner());

  if (result > 0) {
    char* buf;
    int nb = memio_GetReadParams(nss_bufs_, &buf);
    CHECK_GE(nb, result);
    memcpy(buf, read_buffer->data(), result);
  } else if (result == 0) {
    transport_recv_eof_ = true;
  }

  memio_PutReadResult(nss_bufs_, MapErrorToNSS(result));
  transport_recv_busy_ = false;
  OnRecvComplete(result);
}

void SSLClientSocketNSS::Core::PostOrRunCallback(
    const tracked_objects::Location& location,
    const base::Closure& task) {
  if (!OnNetworkTaskRunner()) {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Core::PostOrRunCallback, this, location, task));
    return;
  }

  if (detached_ || task.is_null())
    return;
  task.Run();
}

void SSLClientSocketNSS::Core::AddCertProvidedEvent(int cert_count) {
  PostOrRunCallback(
      FROM_HERE,
      base::Bind(&AddLogEventWithCallback, weak_net_log_,
                 NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
                 NetLog::IntegerCallback("cert_count", cert_count)));
}

void SSLClientSocketNSS::Core::SetChannelIDProvided() {
  PostOrRunCallback(
      FROM_HERE, base::Bind(&AddLogEvent, weak_net_log_,
                            NetLog::TYPE_SSL_CHANNEL_ID_PROVIDED));
  nss_handshake_state_.channel_id_sent = true;
  // Update the network task runner's view of the handshake state now that
  // channel id has been sent.
  PostOrRunCallback(
      FROM_HERE, base::Bind(&Core::OnHandshakeStateUpdated, this,
                            nss_handshake_state_));
}

SSLClientSocketNSS::SSLClientSocketNSS(
    base::SequencedTaskRunner* nss_task_runner,
    scoped_ptr<ClientSocketHandle> transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    const SSLClientSocketContext& context)
    : nss_task_runner_(nss_task_runner),
      transport_(transport_socket.Pass()),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      cert_verifier_(context.cert_verifier),
      cert_transparency_verifier_(context.cert_transparency_verifier),
      server_bound_cert_service_(context.server_bound_cert_service),
      ssl_session_cache_shard_(context.ssl_session_cache_shard),
      completed_handshake_(false),
      next_handshake_state_(STATE_NONE),
      nss_fd_(NULL),
      net_log_(transport_->socket()->NetLog()),
      transport_security_state_(context.transport_security_state),
      valid_thread_id_(base::kInvalidThreadId) {
  EnterFunction("");
  InitCore();
  LeaveFunction("");
}

SSLClientSocketNSS::~SSLClientSocketNSS() {
  EnterFunction("");
  Disconnect();
  LeaveFunction("");
}

// static
void SSLClientSocket::ClearSessionCache() {
  // SSL_ClearSessionCache can't be called before NSS is initialized.  Don't
  // bother initializing NSS just to clear an empty SSL session cache.
  if (!NSS_IsInitialized())
    return;

  SSL_ClearSessionCache();
}

bool SSLClientSocketNSS::GetSSLInfo(SSLInfo* ssl_info) {
  EnterFunction("");
  ssl_info->Reset();
  if (core_->state().server_cert_chain.empty() ||
      !core_->state().server_cert_chain[0]) {
    return false;
  }

  ssl_info->cert_status = server_cert_verify_result_.cert_status;
  ssl_info->cert = server_cert_verify_result_.verified_cert;

  AddSCTInfoToSSLInfo(ssl_info);

  ssl_info->connection_status =
      core_->state().ssl_connection_status;
  ssl_info->public_key_hashes = server_cert_verify_result_.public_key_hashes;
  ssl_info->is_issued_by_known_root =
      server_cert_verify_result_.is_issued_by_known_root;
  ssl_info->client_cert_sent =
      ssl_config_.send_client_cert && ssl_config_.client_cert.get();
  ssl_info->channel_id_sent = WasChannelIDSent();
  ssl_info->pinning_failure_log = pinning_failure_log_;

  PRUint16 cipher_suite = SSLConnectionStatusToCipherSuite(
      core_->state().ssl_connection_status);
  SSLCipherSuiteInfo cipher_info;
  SECStatus ok = SSL_GetCipherSuiteInfo(cipher_suite,
                                        &cipher_info, sizeof(cipher_info));
  if (ok == SECSuccess) {
    ssl_info->security_bits = cipher_info.effectiveKeyBits;
  } else {
    ssl_info->security_bits = -1;
    LOG(DFATAL) << "SSL_GetCipherSuiteInfo returned " << PR_GetError()
                << " for cipherSuite " << cipher_suite;
  }

  ssl_info->handshake_type = core_->state().resumed_handshake ?
      SSLInfo::HANDSHAKE_RESUME : SSLInfo::HANDSHAKE_FULL;

  LeaveFunction("");
  return true;
}

void SSLClientSocketNSS::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  EnterFunction("");
  cert_request_info->host_and_port = host_and_port_;
  cert_request_info->cert_authorities = core_->state().cert_authorities;
  LeaveFunction("");
}

int SSLClientSocketNSS::ExportKeyingMaterial(const base::StringPiece& label,
                                             bool has_context,
                                             const base::StringPiece& context,
                                             unsigned char* out,
                                             unsigned int outlen) {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;

  // SSL_ExportKeyingMaterial may block the current thread if |core_| is in
  // the midst of a handshake.
  SECStatus result = SSL_ExportKeyingMaterial(
      nss_fd_, label.data(), label.size(), has_context,
      reinterpret_cast<const unsigned char*>(context.data()),
      context.length(), out, outlen);
  if (result != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_ExportKeyingMaterial", "");
    return MapNSSError(PORT_GetError());
  }
  return OK;
}

int SSLClientSocketNSS::GetTLSUniqueChannelBinding(std::string* out) {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  unsigned char buf[64];
  unsigned int len;
  SECStatus result = SSL_GetChannelBinding(nss_fd_,
                                           SSL_CHANNEL_BINDING_TLS_UNIQUE,
                                           buf, &len, arraysize(buf));
  if (result != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_GetChannelBinding", "");
    return MapNSSError(PORT_GetError());
  }
  out->assign(reinterpret_cast<char*>(buf), len);
  return OK;
}

SSLClientSocket::NextProtoStatus
SSLClientSocketNSS::GetNextProto(std::string* proto,
                                 std::string* server_protos) {
  *proto = core_->state().next_proto;
  *server_protos = core_->state().server_protos;
  return core_->state().next_proto_status;
}

int SSLClientSocketNSS::Connect(const CompletionCallback& callback) {
  EnterFunction("");
  DCHECK(transport_.get());
  // It is an error to create an SSLClientSocket whose context has no
  // TransportSecurityState.
  DCHECK(transport_security_state_);
  DCHECK_EQ(STATE_NONE, next_handshake_state_);
  DCHECK(user_connect_callback_.is_null());
  DCHECK(!callback.is_null());

  EnsureThreadIdAssigned();

  net_log_.BeginEvent(NetLog::TYPE_SSL_CONNECT);

  int rv = Init();
  if (rv != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
    return rv;
  }

  rv = InitializeSSLOptions();
  if (rv != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
    return rv;
  }

  rv = InitializeSSLPeerName();
  if (rv != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
    return rv;
  }

  GotoState(STATE_HANDSHAKE);

  rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = callback;
  } else {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
  }

  LeaveFunction("");
  return rv > OK ? OK : rv;
}

void SSLClientSocketNSS::Disconnect() {
  EnterFunction("");

  CHECK(CalledOnValidThread());

  // Shut down anything that may call us back.
  core_->Detach();
  verifier_.reset();
  transport_->socket()->Disconnect();

  // Reset object state.
  user_connect_callback_.Reset();
  server_cert_verify_result_.Reset();
  completed_handshake_   = false;
  start_cert_verification_time_ = base::TimeTicks();
  InitCore();

  LeaveFunction("");
}

bool SSLClientSocketNSS::IsConnected() const {
  EnterFunction("");
  bool ret = completed_handshake_ &&
             (core_->HasPendingAsyncOperation() ||
              (core_->IsConnected() && core_->HasUnhandledReceivedData()) ||
              transport_->socket()->IsConnected());
  LeaveFunction("");
  return ret;
}

bool SSLClientSocketNSS::IsConnectedAndIdle() const {
  EnterFunction("");
  bool ret = completed_handshake_ &&
             !core_->HasPendingAsyncOperation() &&
             !(core_->IsConnected() && core_->HasUnhandledReceivedData()) &&
             transport_->socket()->IsConnectedAndIdle();
  LeaveFunction("");
  return ret;
}

int SSLClientSocketNSS::GetPeerAddress(IPEndPoint* address) const {
  return transport_->socket()->GetPeerAddress(address);
}

int SSLClientSocketNSS::GetLocalAddress(IPEndPoint* address) const {
  return transport_->socket()->GetLocalAddress(address);
}

const BoundNetLog& SSLClientSocketNSS::NetLog() const {
  return net_log_;
}

void SSLClientSocketNSS::SetSubresourceSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetSubresourceSpeculation();
  } else {
    NOTREACHED();
  }
}

void SSLClientSocketNSS::SetOmniboxSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetOmniboxSpeculation();
  } else {
    NOTREACHED();
  }
}

bool SSLClientSocketNSS::WasEverUsed() const {
  DCHECK(core_.get());

  return core_->WasEverUsed();
}

bool SSLClientSocketNSS::UsingTCPFastOpen() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->UsingTCPFastOpen();
  }
  NOTREACHED();
  return false;
}

int SSLClientSocketNSS::Read(IOBuffer* buf, int buf_len,
                             const CompletionCallback& callback) {
  DCHECK(core_.get());
  DCHECK(!callback.is_null());

  EnterFunction(buf_len);
  int rv = core_->Read(buf, buf_len, callback);
  LeaveFunction(rv);

  return rv;
}

int SSLClientSocketNSS::Write(IOBuffer* buf, int buf_len,
                              const CompletionCallback& callback) {
  DCHECK(core_.get());
  DCHECK(!callback.is_null());

  EnterFunction(buf_len);
  int rv = core_->Write(buf, buf_len, callback);
  LeaveFunction(rv);

  return rv;
}

int SSLClientSocketNSS::SetReceiveBufferSize(int32 size) {
  return transport_->socket()->SetReceiveBufferSize(size);
}

int SSLClientSocketNSS::SetSendBufferSize(int32 size) {
  return transport_->socket()->SetSendBufferSize(size);
}

int SSLClientSocketNSS::Init() {
  EnterFunction("");
  // Initialize the NSS SSL library in a threadsafe way.  This also
  // initializes the NSS base library.
  EnsureNSSSSLInit();
  if (!NSS_IsInitialized())
    return ERR_UNEXPECTED;
#if defined(USE_NSS) || defined(OS_IOS)
  if (ssl_config_.cert_io_enabled) {
    // We must call EnsureNSSHttpIOInit() here, on the IO thread, to get the IO
    // loop by MessageLoopForIO::current().
    // X509Certificate::Verify() runs on a worker thread of CertVerifier.
    EnsureNSSHttpIOInit();
  }
#endif

  LeaveFunction("");
  return OK;
}

void SSLClientSocketNSS::InitCore() {
  core_ = new Core(base::ThreadTaskRunnerHandle::Get().get(),
                   nss_task_runner_.get(),
                   transport_.get(),
                   host_and_port_,
                   ssl_config_,
                   &net_log_,
                   server_bound_cert_service_);
}

int SSLClientSocketNSS::InitializeSSLOptions() {
  // Transport connected, now hook it up to nss
  nss_fd_ = memio_CreateIOLayer(kRecvBufferSize, kSendBufferSize);
  if (nss_fd_ == NULL) {
    return ERR_OUT_OF_MEMORY;  // TODO(port): map NSPR error code.
  }

  // Grab pointer to buffers
  memio_Private* nss_bufs = memio_GetSecret(nss_fd_);

  /* Create SSL state machine */
  /* Push SSL onto our fake I/O socket */
  if (SSL_ImportFD(GetNSSModelSocket(), nss_fd_) == NULL) {
    LogFailedNSSFunction(net_log_, "SSL_ImportFD", "");
    PR_Close(nss_fd_);
    nss_fd_ = NULL;
    return ERR_OUT_OF_MEMORY;  // TODO(port): map NSPR/NSS error code.
  }
  // TODO(port): set more ssl options!  Check errors!

  int rv;

  rv = SSL_OptionSet(nss_fd_, SSL_SECURITY, PR_TRUE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_SECURITY");
    return ERR_UNEXPECTED;
  }

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SSL2, PR_FALSE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_SSL2");
    return ERR_UNEXPECTED;
  }

  // Don't do V2 compatible hellos because they don't support TLS extensions.
  rv = SSL_OptionSet(nss_fd_, SSL_V2_COMPATIBLE_HELLO, PR_FALSE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_V2_COMPATIBLE_HELLO");
    return ERR_UNEXPECTED;
  }

  SSLVersionRange version_range;
  version_range.min = ssl_config_.version_min;
  version_range.max = ssl_config_.version_max;
  rv = SSL_VersionRangeSet(nss_fd_, &version_range);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_VersionRangeSet", "");
    return ERR_NO_SSL_VERSIONS_ENABLED;
  }

  if (ssl_config_.version_fallback) {
    rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_FALLBACK_SCSV, PR_TRUE);
    if (rv != SECSuccess) {
      LogFailedNSSFunction(
          net_log_, "SSL_OptionSet", "SSL_ENABLE_FALLBACK_SCSV");
    }
  }

  for (std::vector<uint16>::const_iterator it =
           ssl_config_.disabled_cipher_suites.begin();
       it != ssl_config_.disabled_cipher_suites.end(); ++it) {
    // This will fail if the specified cipher is not implemented by NSS, but
    // the failure is harmless.
    SSL_CipherPrefSet(nss_fd_, *it, PR_FALSE);
  }

  // Support RFC 5077
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SESSION_TICKETS, PR_TRUE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(
        net_log_, "SSL_OptionSet", "SSL_ENABLE_SESSION_TICKETS");
  }

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_FALSE_START,
                     ssl_config_.false_start_enabled);
  if (rv != SECSuccess)
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_FALSE_START");

  // We allow servers to request renegotiation. Since we're a client,
  // prohibiting this is rather a waste of time. Only servers are in a
  // position to prevent renegotiation attacks.
  // http://extendedsubset.com/?p=8

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_RENEGOTIATION,
                     SSL_RENEGOTIATE_TRANSITIONAL);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(
        net_log_, "SSL_OptionSet", "SSL_ENABLE_RENEGOTIATION");
  }

  rv = SSL_OptionSet(nss_fd_, SSL_CBC_RANDOM_IV, PR_TRUE);
  if (rv != SECSuccess)
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_CBC_RANDOM_IV");

// Added in NSS 3.15
#ifdef SSL_ENABLE_OCSP_STAPLING
  // Request OCSP stapling even on platforms that don't support it, in
  // order to extract Certificate Transparency information.
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_OCSP_STAPLING,
                     (IsOCSPStaplingSupported() ||
                      ssl_config_.signed_cert_timestamps_enabled));
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet",
                         "SSL_ENABLE_OCSP_STAPLING");
  }
#endif

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SIGNED_CERT_TIMESTAMPS,
                     ssl_config_.signed_cert_timestamps_enabled);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet",
                         "SSL_ENABLE_SIGNED_CERT_TIMESTAMPS");
  }

  rv = SSL_OptionSet(nss_fd_, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_HANDSHAKE_AS_CLIENT");
    return ERR_UNEXPECTED;
  }

  if (!core_->Init(nss_fd_, nss_bufs))
    return ERR_UNEXPECTED;

  // Tell SSL the hostname we're trying to connect to.
  SSL_SetURL(nss_fd_, host_and_port_.host().c_str());

  // Tell SSL we're a client; needed if not letting NSPR do socket I/O
  SSL_ResetHandshake(nss_fd_, PR_FALSE);

  return OK;
}

int SSLClientSocketNSS::InitializeSSLPeerName() {
  // Tell NSS who we're connected to
  IPEndPoint peer_address;
  int err = transport_->socket()->GetPeerAddress(&peer_address);
  if (err != OK)
    return err;

  SockaddrStorage storage;
  if (!peer_address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  PRNetAddr peername;
  memset(&peername, 0, sizeof(peername));
  DCHECK_LE(static_cast<size_t>(storage.addr_len), sizeof(peername));
  size_t len = std::min(static_cast<size_t>(storage.addr_len),
                        sizeof(peername));
  memcpy(&peername, storage.addr, len);

  // Adjust the address family field for BSD, whose sockaddr
  // structure has a one-byte length and one-byte address family
  // field at the beginning.  PRNetAddr has a two-byte address
  // family field at the beginning.
  peername.raw.family = storage.addr->sa_family;

  memio_SetPeerName(nss_fd_, &peername);

  // Set the peer ID for session reuse.  This is necessary when we create an
  // SSL tunnel through a proxy -- GetPeerName returns the proxy's address
  // rather than the destination server's address in that case.
  std::string peer_id = host_and_port_.ToString();
  // If the ssl_session_cache_shard_ is non-empty, we append it to the peer id.
  // This will cause session cache misses between sockets with different values
  // of ssl_session_cache_shard_ and this is used to partition the session cache
  // for incognito mode.
  if (!ssl_session_cache_shard_.empty()) {
    peer_id += "/" + ssl_session_cache_shard_;
  }
  SECStatus rv = SSL_SetSockPeerID(nss_fd_, const_cast<char*>(peer_id.c_str()));
  if (rv != SECSuccess)
    LogFailedNSSFunction(net_log_, "SSL_SetSockPeerID", peer_id.c_str());

  return OK;
}

void SSLClientSocketNSS::DoConnectCallback(int rv) {
  EnterFunction(rv);
  DCHECK_NE(ERR_IO_PENDING, rv);
  DCHECK(!user_connect_callback_.is_null());

  base::ResetAndReturn(&user_connect_callback_).Run(rv > OK ? OK : rv);
  LeaveFunction("");
}

void SSLClientSocketNSS::OnHandshakeIOComplete(int result) {
  EnterFunction(result);
  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
    DoConnectCallback(rv);
  }
  LeaveFunction("");
}

int SSLClientSocketNSS::DoHandshakeLoop(int last_io_result) {
  EnterFunction(last_io_result);
  int rv = last_io_result;
  do {
    // Default to STATE_NONE for next state.
    // (This is a quirk carried over from the windows
    // implementation.  It makes reading the logs a bit harder.)
    // State handlers can and often do call GotoState just
    // to stay in the current state.
    State state = next_handshake_state_;
    GotoState(STATE_NONE);
    switch (state) {
      case STATE_HANDSHAKE:
        rv = DoHandshake();
        break;
      case STATE_HANDSHAKE_COMPLETE:
        rv = DoHandshakeComplete(rv);
        break;
      case STATE_VERIFY_CERT:
        DCHECK(rv == OK);
        rv = DoVerifyCert(rv);
        break;
      case STATE_VERIFY_CERT_COMPLETE:
        rv = DoVerifyCertComplete(rv);
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state " << state;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_handshake_state_ != STATE_NONE);
  LeaveFunction("");
  return rv;
}

int SSLClientSocketNSS::DoHandshake() {
  EnterFunction("");
  int rv = core_->Connect(
      base::Bind(&SSLClientSocketNSS::OnHandshakeIOComplete,
                 base::Unretained(this)));
  GotoState(STATE_HANDSHAKE_COMPLETE);

  LeaveFunction(rv);
  return rv;
}

int SSLClientSocketNSS::DoHandshakeComplete(int result) {
  EnterFunction(result);

  if (result == OK) {
    // SSL handshake is completed. Let's verify the certificate.
    GotoState(STATE_VERIFY_CERT);
    // Done!
  }
  set_channel_id_sent(core_->state().channel_id_sent);
  set_signed_cert_timestamps_received(
      !core_->state().sct_list_from_tls_extension.empty());
  set_stapled_ocsp_response_received(
      !core_->state().stapled_ocsp_response.empty());

  LeaveFunction(result);
  return result;
}

int SSLClientSocketNSS::DoVerifyCert(int result) {
  DCHECK(!core_->state().server_cert_chain.empty());
  DCHECK(core_->state().server_cert_chain[0]);

  GotoState(STATE_VERIFY_CERT_COMPLETE);

  // If the certificate is expected to be bad we can use the expectation as
  // the cert status.
  base::StringPiece der_cert(
      reinterpret_cast<char*>(
          core_->state().server_cert_chain[0]->derCert.data),
      core_->state().server_cert_chain[0]->derCert.len);
  CertStatus cert_status;
  if (ssl_config_.IsAllowedBadCert(der_cert, &cert_status)) {
    DCHECK(start_cert_verification_time_.is_null());
    VLOG(1) << "Received an expected bad cert with status: " << cert_status;
    server_cert_verify_result_.Reset();
    server_cert_verify_result_.cert_status = cert_status;
    server_cert_verify_result_.verified_cert = core_->state().server_cert;
    return OK;
  }

  // We may have failed to create X509Certificate object if we are
  // running inside sandbox.
  if (!core_->state().server_cert.get()) {
    server_cert_verify_result_.Reset();
    server_cert_verify_result_.cert_status = CERT_STATUS_INVALID;
    return ERR_CERT_INVALID;
  }

  start_cert_verification_time_ = base::TimeTicks::Now();

  int flags = 0;
  if (ssl_config_.rev_checking_enabled)
    flags |= CertVerifier::VERIFY_REV_CHECKING_ENABLED;
  if (ssl_config_.verify_ev_cert)
    flags |= CertVerifier::VERIFY_EV_CERT;
  if (ssl_config_.cert_io_enabled)
    flags |= CertVerifier::VERIFY_CERT_IO_ENABLED;
  if (ssl_config_.rev_checking_required_local_anchors)
    flags |= CertVerifier::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  verifier_.reset(new SingleRequestCertVerifier(cert_verifier_));
  return verifier_->Verify(
      core_->state().server_cert.get(),
      host_and_port_.host(),
      flags,
      SSLConfigService::GetCRLSet().get(),
      &server_cert_verify_result_,
      base::Bind(&SSLClientSocketNSS::OnHandshakeIOComplete,
                 base::Unretained(this)),
      net_log_);
}

// Derived from AuthCertificateCallback() in
// mozilla/source/security/manager/ssl/src/nsNSSCallbacks.cpp.
int SSLClientSocketNSS::DoVerifyCertComplete(int result) {
  verifier_.reset();

  if (!start_cert_verification_time_.is_null()) {
    base::TimeDelta verify_time =
        base::TimeTicks::Now() - start_cert_verification_time_;
    if (result == OK)
        UMA_HISTOGRAM_TIMES("Net.SSLCertVerificationTime", verify_time);
    else
        UMA_HISTOGRAM_TIMES("Net.SSLCertVerificationTimeError", verify_time);
  }

  // We used to remember the intermediate CA certs in the NSS database
  // persistently.  However, NSS opens a connection to the SQLite database
  // during NSS initialization and doesn't close the connection until NSS
  // shuts down.  If the file system where the database resides is gone,
  // the database connection goes bad.  What's worse, the connection won't
  // recover when the file system comes back.  Until this NSS or SQLite bug
  // is fixed, we need to  avoid using the NSS database for non-essential
  // purposes.  See https://bugzilla.mozilla.org/show_bug.cgi?id=508081 and
  // http://crbug.com/15630 for more info.

  // TODO(hclam): Skip logging if server cert was expected to be bad because
  // |server_cert_verify_result_| doesn't contain all the information about
  // the cert.
  if (result == OK)
    LogConnectionTypeMetrics();

#if defined(OFFICIAL_BUILD) && !defined(OS_ANDROID) && !defined(OS_IOS)
  // Take care of any mandates for public key pinning.
  //
  // Pinning is only enabled for official builds to make sure that others don't
  // end up with pins that cannot be easily updated.
  //
  // TODO(agl): We might have an issue here where a request for foo.example.com
  // merges into a SPDY connection to www.example.com, and gets a different
  // certificate.

  // Perform pin validation if, and only if, all these conditions obtain:
  //
  // * a TransportSecurityState object is available;
  // * the server's certificate chain is valid (or suffers from only a minor
  //   error);
  // * the server's certificate chain chains up to a known root (i.e. not a
  //   user-installed trust anchor); and
  // * the build is recent (very old builds should fail open so that users
  //   have some chance to recover).
  //
  const CertStatus cert_status = server_cert_verify_result_.cert_status;
  if (transport_security_state_ &&
      (result == OK ||
       (IsCertificateError(result) && IsCertStatusMinorError(cert_status))) &&
      server_cert_verify_result_.is_issued_by_known_root &&
      TransportSecurityState::IsBuildTimely()) {
    bool sni_available =
        ssl_config_.version_max >= SSL_PROTOCOL_VERSION_TLS1 ||
        ssl_config_.version_fallback;
    const std::string& host = host_and_port_.host();

    if (transport_security_state_->HasPublicKeyPins(host, sni_available)) {
      if (!transport_security_state_->CheckPublicKeyPins(
              host,
              sni_available,
              server_cert_verify_result_.public_key_hashes,
              &pinning_failure_log_)) {
        LOG(ERROR) << pinning_failure_log_;
        result = ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
        UMA_HISTOGRAM_BOOLEAN("Net.PublicKeyPinSuccess", false);
        TransportSecurityState::ReportUMAOnPinFailure(host);
      } else {
        UMA_HISTOGRAM_BOOLEAN("Net.PublicKeyPinSuccess", true);
      }
    }
  }
#endif

  if (result == OK) {
    // Only check Certificate Transparency if there were no other errors with
    // the connection.
    VerifyCT();

    // Only cache the session if the certificate verified successfully.
    core_->CacheSessionIfNecessary();
  }

  completed_handshake_ = true;

  // Exit DoHandshakeLoop and return the result to the caller to Connect.
  DCHECK_EQ(STATE_NONE, next_handshake_state_);
  return result;
}

void SSLClientSocketNSS::VerifyCT() {
  if (!cert_transparency_verifier_)
    return;

  // Note that this is a completely synchronous operation: The CT Log Verifier
  // gets all the data it needs for SCT verification and does not do any
  // external communication.
  int result = cert_transparency_verifier_->Verify(
      server_cert_verify_result_.verified_cert,
      core_->state().stapled_ocsp_response,
      core_->state().sct_list_from_tls_extension,
      &ct_verify_result_,
      net_log_);
  // TODO(ekasper): wipe stapled_ocsp_response and sct_list_from_tls_extension
  // from the state after verification is complete, to conserve memory.

  VLOG(1) << "CT Verification complete: result " << result
          << " Invalid scts: " << ct_verify_result_.invalid_scts.size()
          << " Verified scts: " << ct_verify_result_.verified_scts.size()
          << " scts from unknown logs: "
          << ct_verify_result_.unknown_logs_scts.size();
}

void SSLClientSocketNSS::LogConnectionTypeMetrics() const {
  UpdateConnectionTypeHistograms(CONNECTION_SSL);
  int ssl_version = SSLConnectionStatusToVersion(
      core_->state().ssl_connection_status);
  switch (ssl_version) {
    case SSL_CONNECTION_VERSION_SSL2:
      UpdateConnectionTypeHistograms(CONNECTION_SSL_SSL2);
      break;
    case SSL_CONNECTION_VERSION_SSL3:
      UpdateConnectionTypeHistograms(CONNECTION_SSL_SSL3);
      break;
    case SSL_CONNECTION_VERSION_TLS1:
      UpdateConnectionTypeHistograms(CONNECTION_SSL_TLS1);
      break;
    case SSL_CONNECTION_VERSION_TLS1_1:
      UpdateConnectionTypeHistograms(CONNECTION_SSL_TLS1_1);
      break;
    case SSL_CONNECTION_VERSION_TLS1_2:
      UpdateConnectionTypeHistograms(CONNECTION_SSL_TLS1_2);
      break;
  };
}

void SSLClientSocketNSS::EnsureThreadIdAssigned() const {
  base::AutoLock auto_lock(lock_);
  if (valid_thread_id_ != base::kInvalidThreadId)
    return;
  valid_thread_id_ = base::PlatformThread::CurrentId();
}

bool SSLClientSocketNSS::CalledOnValidThread() const {
  EnsureThreadIdAssigned();
  base::AutoLock auto_lock(lock_);
  return valid_thread_id_ == base::PlatformThread::CurrentId();
}

void SSLClientSocketNSS::AddSCTInfoToSSLInfo(SSLInfo* ssl_info) const {
  for (ct::SCTList::const_iterator iter =
       ct_verify_result_.verified_scts.begin();
       iter != ct_verify_result_.verified_scts.end(); ++iter) {
    ssl_info->signed_certificate_timestamps.push_back(
        SignedCertificateTimestampAndStatus(*iter, ct::SCT_STATUS_OK));
  }
  for (ct::SCTList::const_iterator iter =
       ct_verify_result_.invalid_scts.begin();
       iter != ct_verify_result_.invalid_scts.end(); ++iter) {
    ssl_info->signed_certificate_timestamps.push_back(
        SignedCertificateTimestampAndStatus(*iter, ct::SCT_STATUS_INVALID));
  }
  for (ct::SCTList::const_iterator iter =
       ct_verify_result_.unknown_logs_scts.begin();
       iter != ct_verify_result_.unknown_logs_scts.end(); ++iter) {
    ssl_info->signed_certificate_timestamps.push_back(
        SignedCertificateTimestampAndStatus(*iter,
                                            ct::SCT_STATUS_LOG_UNKNOWN));
  }
}

scoped_refptr<X509Certificate>
SSLClientSocketNSS::GetUnverifiedServerCertificateChain() const {
  return core_->state().server_cert.get();
}

ServerBoundCertService* SSLClientSocketNSS::GetServerBoundCertService() const {
  return server_bound_cert_service_;
}

}  // namespace net
