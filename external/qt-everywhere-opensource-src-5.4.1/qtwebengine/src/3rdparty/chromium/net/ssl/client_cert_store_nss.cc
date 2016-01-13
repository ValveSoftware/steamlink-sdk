// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_nss.h"

#include <nss.h>
#include <ssl.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/threading/worker_pool.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "net/cert/x509_util.h"

namespace net {

ClientCertStoreNSS::ClientCertStoreNSS(
    const PasswordDelegateFactory& password_delegate_factory)
    : password_delegate_factory_(password_delegate_factory) {}

ClientCertStoreNSS::~ClientCertStoreNSS() {}

void ClientCertStoreNSS::GetClientCerts(const SSLCertRequestInfo& request,
                                         CertificateList* selected_certs,
                                         const base::Closure& callback) {
  scoped_ptr<crypto::CryptoModuleBlockingPasswordDelegate> password_delegate;
  if (!password_delegate_factory_.is_null()) {
    password_delegate.reset(
        password_delegate_factory_.Run(request.host_and_port));
  }
  if (base::WorkerPool::PostTaskAndReply(
          FROM_HERE,
          base::Bind(&ClientCertStoreNSS::GetClientCertsOnWorkerThread,
                     // Caller is responsible for keeping the ClientCertStore
                     // alive until the callback is run.
                     base::Unretained(this),
                     base::Passed(&password_delegate),
                     &request,
                     selected_certs),
          callback,
          true))
    return;
  selected_certs->clear();
  callback.Run();
}

void ClientCertStoreNSS::GetClientCertsImpl(CERTCertList* cert_list,
                                            const SSLCertRequestInfo& request,
                                            bool query_nssdb,
                                            CertificateList* selected_certs) {
  DCHECK(cert_list);
  DCHECK(selected_certs);

  selected_certs->clear();

  // Create a "fake" CERTDistNames structure. No public API exists to create
  // one from a list of issuers.
  CERTDistNames ca_names;
  ca_names.arena = NULL;
  ca_names.nnames = 0;
  ca_names.names = NULL;
  ca_names.head = NULL;

  std::vector<SECItem> ca_names_items(request.cert_authorities.size());
  for (size_t i = 0; i < request.cert_authorities.size(); ++i) {
    const std::string& authority = request.cert_authorities[i];
    ca_names_items[i].type = siBuffer;
    ca_names_items[i].data =
        reinterpret_cast<unsigned char*>(const_cast<char*>(authority.data()));
    ca_names_items[i].len = static_cast<unsigned int>(authority.size());
  }
  ca_names.nnames = static_cast<int>(ca_names_items.size());
  if (!ca_names_items.empty())
    ca_names.names = &ca_names_items[0];

  size_t num_raw = 0;
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    ++num_raw;
    // Only offer unexpired certificates.
    if (CERT_CheckCertValidTimes(node->cert, PR_Now(), PR_TRUE) !=
        secCertTimeValid) {
      DVLOG(2) << "skipped expired cert: "
               << base::StringPiece(node->cert->nickname);
      continue;
    }

    scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromHandle(
        node->cert, X509Certificate::OSCertHandles());

    // Check if the certificate issuer is allowed by the server.
    if (request.cert_authorities.empty() ||
        (!query_nssdb &&
         cert->IsIssuedByEncoded(request.cert_authorities)) ||
        (query_nssdb &&
         NSS_CmpCertChainWCANames(node->cert, &ca_names) == SECSuccess)) {
      DVLOG(2) << "matched cert: " << base::StringPiece(node->cert->nickname);
      selected_certs->push_back(cert);
    }
    else
      DVLOG(2) << "skipped non-matching cert: "
               << base::StringPiece(node->cert->nickname);
  }
  DVLOG(2) << "num_raw:" << num_raw
           << " num_selected:" << selected_certs->size();

  std::sort(selected_certs->begin(), selected_certs->end(),
            x509_util::ClientCertSorter());
}

void ClientCertStoreNSS::GetClientCertsOnWorkerThread(
    scoped_ptr<crypto::CryptoModuleBlockingPasswordDelegate> password_delegate,
    const SSLCertRequestInfo* request,
    CertificateList* selected_certs) {
  CERTCertList* client_certs = CERT_FindUserCertsByUsage(
      CERT_GetDefaultCertDB(),
      certUsageSSLClient,
      PR_FALSE,
      PR_FALSE,
      password_delegate.get());
  // It is ok for a user not to have any client certs.
  if (!client_certs) {
    DVLOG(2) << "No client certs found.";
    selected_certs->clear();
    return;
  }

  GetClientCertsImpl(client_certs, *request, true, selected_certs);
  CERT_DestroyCertList(client_certs);
}

bool ClientCertStoreNSS::SelectClientCertsForTesting(
    const CertificateList& input_certs,
    const SSLCertRequestInfo& request,
    CertificateList* selected_certs) {
  CERTCertList* cert_list = CERT_NewCertList();
  if (!cert_list)
    return false;
  for (size_t i = 0; i < input_certs.size(); ++i) {
    CERT_AddCertToListTail(
        cert_list, CERT_DupCertificate(input_certs[i]->os_cert_handle()));
  }

  GetClientCertsImpl(cert_list, request, false, selected_certs);
  CERT_DestroyCertList(cert_list);
  return true;
}

}  // namespace net
