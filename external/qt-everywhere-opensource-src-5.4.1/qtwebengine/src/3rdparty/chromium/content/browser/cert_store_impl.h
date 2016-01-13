// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CERT_STORE_IMPL_H_
#define CONTENT_BROWSER_CERT_STORE_IMPL_H_

#include "base/memory/singleton.h"
#include "content/browser/renderer_data_memoizing_store.h"
#include "content/public/browser/cert_store.h"
#include "net/cert/x509_certificate.h"

namespace content {

class CertStoreImpl : public CertStore {
 public:
  // Returns the singleton instance of the CertStore.
  static CertStoreImpl* GetInstance();

  // CertStore implementation:
  virtual int StoreCert(net::X509Certificate* cert,
                        int render_process_host_id) OVERRIDE;
  virtual bool RetrieveCert(int cert_id,
                            scoped_refptr<net::X509Certificate>* cert) OVERRIDE;

 protected:
  CertStoreImpl();
  virtual ~CertStoreImpl();

 private:
  friend struct DefaultSingletonTraits<CertStoreImpl>;

  RendererDataMemoizingStore<net::X509Certificate> store_;

  DISALLOW_COPY_AND_ASSIGN(CertStoreImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CERT_STORE_IMPL_H_
