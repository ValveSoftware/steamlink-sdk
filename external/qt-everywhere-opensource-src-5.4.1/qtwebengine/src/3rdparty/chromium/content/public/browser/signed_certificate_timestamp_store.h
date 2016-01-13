// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SIGNED_CERTIFICATE_TIMESTAMP_STORE_H_
#define CONTENT_PUBLIC_BROWSER_SIGNED_CERTIFICATE_TIMESTAMP_STORE_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace net {
namespace ct {
struct SignedCertificateTimestamp;
}  // namespace ct
}  // namespace net

namespace content {

// The purpose of the SignedCertificateTimestampStore is to provide an easy way
// to store/retrieve SignedCertificateTimestamp objects.  When stored,
// SignedCertificateTimestamp objects are associated with a RenderProcessHost.
// If all the RenderProcessHosts associated with the SCT have exited, the SCT
// is removed from the store.  This class is used by the SSLManager to keep
// track of the SCTs associated with loaded resources.  It can be accessed from
// the UI and IO threads (it is thread-safe).  Note that the SCT ids will
// overflow if we register more than 2^32 - 1 SCTs in 1 browsing session (which
// is highly unlikely to happen).
class SignedCertificateTimestampStore {
 public:
  // Returns the singleton instance of the SignedCertificateTimestampStore.
  CONTENT_EXPORT static SignedCertificateTimestampStore* GetInstance();

  // Stores the specified SCT and returns the id associated with it.  The SCT
  // is associated with the specified RenderProcessHost.
  // When all the RenderProcessHosts associated with a SCT have exited, the
  // SCT is removed from the store.
  // Note: ids start at 1.
  virtual int Store(net::ct::SignedCertificateTimestamp* sct,
                    int render_process_host_id) = 0;

  // Tries to retrieve the previously stored SCT associated with the specified
  // |sct_id|. Returns whether the SCT could be found, and, if |sct| is
  // non-NULL, copies it in.
  virtual bool Retrieve(
      int sct_id, scoped_refptr<net::ct::SignedCertificateTimestamp>* sct) = 0;

 protected:
  virtual ~SignedCertificateTimestampStore() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SIGNED_CERTIFICATE_TIMESTAMP_STORE_H_
