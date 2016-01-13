// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CERT_STORE_H_
#define CONTENT_PUBLIC_BROWSER_CERT_STORE_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace net {
class X509Certificate;
}

namespace content {

// The purpose of the cert store is to provide an easy way to store/retrieve
// X509Certificate objects.  When stored, an X509Certificate object is
// associated with a RenderProcessHost.  If all the RenderProcessHosts
// associated with the cert have exited, the cert is removed from the store.
// This class is used by the SSLManager to keep track of the certs associated
// to loaded resources.
// It can be accessed from the UI and IO threads (it is thread-safe).
// Note that the cert ids will overflow if we register more than 2^32 - 1 certs
// in 1 browsing session (which is highly unlikely to happen).
class CertStore  {
 public:
  // Returns the singleton instance of the CertStore.
  CONTENT_EXPORT static CertStore* GetInstance();

  // Stores the specified cert and returns the id associated with it.  The cert
  // is associated to the specified RenderProcessHost.
  // When all the RenderProcessHosts associated with a cert have exited, the
  // cert is removed from the store.
  // Note: ids starts at 1.
  virtual int StoreCert(net::X509Certificate* cert,
                        int render_process_host_id) = 0;

  // Tries to retrieve the previously stored cert associated with the specified
  // |cert_id|. Returns whether the cert could be found, and, if |cert| is
  // non-NULL, copies it in.
  virtual bool RetrieveCert(int cert_id,
                            scoped_refptr<net::X509Certificate>* cert) = 0;

 protected:
   virtual ~CertStore() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CERT_STORE_H_
