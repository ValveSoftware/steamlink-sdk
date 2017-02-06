// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cert_store_impl.h"

namespace content {

// static
CertStore* CertStore::GetInstance() {
  return CertStoreImpl::GetInstance();
}

//  static
CertStoreImpl* CertStoreImpl::GetInstance() {
  return base::Singleton<CertStoreImpl>::get();
}

int CertStoreImpl::StoreCert(net::X509Certificate* cert, int process_id) {
  scoped_refptr<HashAndCert> hash_and_cert(new HashAndCert);
  hash_and_cert->chain_hash =
      net::X509Certificate::CalculateChainFingerprint256(
          cert->os_cert_handle(), cert->GetIntermediateCertificates());
  hash_and_cert->cert = cert;
  return store_.Store(hash_and_cert.get(), process_id);
}

bool CertStoreImpl::RetrieveCert(int cert_id,
                                 scoped_refptr<net::X509Certificate>* cert) {
  scoped_refptr<HashAndCert> hash_and_cert;
  if (store_.Retrieve(cert_id, &hash_and_cert)) {
    *cert = hash_and_cert->cert;
    return true;
  }
  return false;
}

CertStoreImpl::CertStoreImpl() {}

CertStoreImpl::~CertStoreImpl() {}

CertStoreImpl::HashAndCert::HashAndCert() = default;

bool CertStoreImpl::HashAndCert::LessThan::operator()(
    const scoped_refptr<HashAndCert>& lhs,
    const scoped_refptr<HashAndCert>& rhs) const {
  return net::SHA256HashValueLessThan()(lhs->chain_hash, rhs->chain_hash);
}

CertStoreImpl::HashAndCert::~HashAndCert() = default;

}  // namespace content
