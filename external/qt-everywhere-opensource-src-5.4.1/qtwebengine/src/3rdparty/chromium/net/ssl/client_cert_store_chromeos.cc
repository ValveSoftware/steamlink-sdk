// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_chromeos.h"

#include <cert.h>

#include "base/bind.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "crypto/nss_util_internal.h"

namespace net {

ClientCertStoreChromeOS::ClientCertStoreChromeOS(
    const std::string& username_hash,
    const PasswordDelegateFactory& password_delegate_factory)
    : ClientCertStoreNSS(password_delegate_factory),
      username_hash_(username_hash) {}

ClientCertStoreChromeOS::~ClientCertStoreChromeOS() {}

void ClientCertStoreChromeOS::GetClientCerts(
    const SSLCertRequestInfo& cert_request_info,
    CertificateList* selected_certs,
    const base::Closure& callback) {
  crypto::ScopedPK11Slot private_slot(crypto::GetPrivateSlotForChromeOSUser(
      username_hash_,
      base::Bind(&ClientCertStoreChromeOS::DidGetPrivateSlot,
                 // Caller is responsible for keeping the ClientCertStore alive
                 // until the callback is run.
                 base::Unretained(this),
                 &cert_request_info,
                 selected_certs,
                 callback)));
  if (private_slot)
    DidGetPrivateSlot(
        &cert_request_info, selected_certs, callback, private_slot.Pass());
}

void ClientCertStoreChromeOS::GetClientCertsImpl(CERTCertList* cert_list,
                                            const SSLCertRequestInfo& request,
                                            bool query_nssdb,
                                            CertificateList* selected_certs) {
  ClientCertStoreNSS::GetClientCertsImpl(
      cert_list, request, query_nssdb, selected_certs);

  size_t pre_size = selected_certs->size();
  selected_certs->erase(
      std::remove_if(
          selected_certs->begin(),
          selected_certs->end(),
          NSSProfileFilterChromeOS::CertNotAllowedForProfilePredicate(
              profile_filter_)),
      selected_certs->end());
  DVLOG(1) << "filtered " << pre_size - selected_certs->size() << " of "
           << pre_size << " certs";
}

void ClientCertStoreChromeOS::DidGetPrivateSlot(
    const SSLCertRequestInfo* request,
    CertificateList* selected_certs,
    const base::Closure& callback,
    crypto::ScopedPK11Slot private_slot) {
  profile_filter_.Init(crypto::GetPublicSlotForChromeOSUser(username_hash_),
                       private_slot.Pass());
  ClientCertStoreNSS::GetClientCerts(*request, selected_certs, callback);
}

void ClientCertStoreChromeOS::InitForTesting(
    crypto::ScopedPK11Slot public_slot,
    crypto::ScopedPK11Slot private_slot) {
  profile_filter_.Init(public_slot.Pass(), private_slot.Pass());
}

bool ClientCertStoreChromeOS::SelectClientCertsForTesting(
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
