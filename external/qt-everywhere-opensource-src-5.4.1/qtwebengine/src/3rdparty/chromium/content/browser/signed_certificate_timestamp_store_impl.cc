// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/signed_certificate_timestamp_store_impl.h"

#include "base/memory/singleton.h"

namespace content {

// static
SignedCertificateTimestampStore*
SignedCertificateTimestampStore::GetInstance() {
  return SignedCertificateTimestampStoreImpl::GetInstance();
}

// static
SignedCertificateTimestampStoreImpl*
SignedCertificateTimestampStoreImpl::GetInstance() {
  return Singleton<SignedCertificateTimestampStoreImpl>::get();
}

SignedCertificateTimestampStoreImpl::SignedCertificateTimestampStoreImpl() {}

SignedCertificateTimestampStoreImpl::~SignedCertificateTimestampStoreImpl() {}

int SignedCertificateTimestampStoreImpl::Store(
    net::ct::SignedCertificateTimestamp* sct,
    int process_id) {
  return store_.Store(sct, process_id);
}

bool SignedCertificateTimestampStoreImpl::Retrieve(
    int sct_id,
    scoped_refptr<net::ct::SignedCertificateTimestamp>* sct) {
  return store_.Retrieve(sct_id, sct);
}

}  // namespace content
