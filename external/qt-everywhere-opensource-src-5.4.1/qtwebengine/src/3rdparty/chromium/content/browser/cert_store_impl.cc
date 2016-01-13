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
  return Singleton<CertStoreImpl>::get();
}

CertStoreImpl::CertStoreImpl() {}

CertStoreImpl::~CertStoreImpl() {}

int CertStoreImpl::StoreCert(net::X509Certificate* cert, int process_id) {
  return store_.Store(cert, process_id);
}

bool CertStoreImpl::RetrieveCert(int cert_id,
                                 scoped_refptr<net::X509Certificate>* cert) {
  return store_.Retrieve(cert_id, cert);
}

}  // namespace content
