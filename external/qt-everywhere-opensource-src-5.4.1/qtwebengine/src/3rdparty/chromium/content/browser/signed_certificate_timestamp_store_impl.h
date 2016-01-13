// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SIGNED_CERTIFICATE_STORE_IMPL_H_
#define CONTENT_BROWSER_SIGNED_CERTIFICATE_STORE_IMPL_H_

#include "content/browser/renderer_data_memoizing_store.h"
#include "content/public/browser/signed_certificate_timestamp_store.h"
#include "net/cert/signed_certificate_timestamp.h"

template <typename T> struct DefaultSingletonTraits;

namespace content {

class SignedCertificateTimestampStoreImpl
    : public SignedCertificateTimestampStore {
 public:
  // Returns the singleton instance of the SignedCertificateTimestampStore.
  static SignedCertificateTimestampStoreImpl* GetInstance();

  // SignedCertificateTimestampStore implementation:
  virtual int Store(
      net::ct::SignedCertificateTimestamp* sct,
      int render_process_host_id) OVERRIDE;
  virtual bool Retrieve(
      int sct_id,
      scoped_refptr<net::ct::SignedCertificateTimestamp>* sct) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<SignedCertificateTimestampStoreImpl>;

  SignedCertificateTimestampStoreImpl();
  virtual ~SignedCertificateTimestampStoreImpl();

  RendererDataMemoizingStore<net::ct::SignedCertificateTimestamp> store_;

  DISALLOW_COPY_AND_ASSIGN(SignedCertificateTimestampStoreImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SIGNED_CERTIFICATE_STORE_IMPL_H_
