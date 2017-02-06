// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CERT_STORE_IMPL_H_
#define CONTENT_BROWSER_CERT_STORE_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "content/browser/renderer_data_memoizing_store.h"
#include "content/public/browser/cert_store.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"

namespace content {

class CertStoreImpl : public CertStore {
 public:
  // Returns the singleton instance of the CertStore.
  static CertStoreImpl* GetInstance();

  // CertStore implementation:
  int StoreCert(net::X509Certificate* cert,
                int render_process_host_id) override;
  bool RetrieveCert(int cert_id,
                    scoped_refptr<net::X509Certificate>* cert) override;

 protected:
  CertStoreImpl();
  ~CertStoreImpl() override;

 private:
  friend struct base::DefaultSingletonTraits<CertStoreImpl>;

  // Utility structure that allows memoization to be based on the
  // hash of |cert|'s certificate chain, to avoid needing to compare
  // every certificate individually. This is purely an optimization.
  class HashAndCert : public base::RefCountedThreadSafe<HashAndCert> {
   public:
    HashAndCert();

    // Comparator for RendererDataMemoizingStore.
    struct LessThan {
      bool operator()(const scoped_refptr<HashAndCert>& lhs,
                      const scoped_refptr<HashAndCert>& rhs) const;
    };

    net::SHA256HashValue chain_hash;
    scoped_refptr<net::X509Certificate> cert;

   private:
    friend class base::RefCountedThreadSafe<HashAndCert>;

    ~HashAndCert();

    DISALLOW_COPY_AND_ASSIGN(HashAndCert);
  };
  RendererDataMemoizingStore<HashAndCert> store_;

  DISALLOW_COPY_AND_ASSIGN(CertStoreImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CERT_STORE_IMPL_H_
