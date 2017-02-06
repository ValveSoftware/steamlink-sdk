// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEBRTC_WEBRTC_IDENTITY_STORE_H_
#define CONTENT_BROWSER_MEDIA_WEBRTC_WEBRTC_IDENTITY_STORE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class FilePath;
class TaskRunner;
}  // namespace base

namespace storage {
class SpecialStoragePolicy;
}  // namespace storage

namespace content {
class WebRTCIdentityRequest;
struct WebRTCIdentityRequestResult;
class WebRTCIdentityStoreBackend;
class WebRTCIdentityStoreTest;

// A class for creating and fetching DTLS identities, i.e. the private key and
// the self-signed certificate.
// It can be created/destroyed on any thread, but the public methods must be
// called on the IO thread.
class CONTENT_EXPORT WebRTCIdentityStore
    : public base::RefCountedThreadSafe<WebRTCIdentityStore> {
 public:
  typedef base::Callback<void(int error,
                              const std::string& certificate,
                              const std::string& private_key)>
      CompletionCallback;

  // If |path| is empty, nothing will be saved to disk.
  WebRTCIdentityStore(const base::FilePath& path,
                      storage::SpecialStoragePolicy* policy);

  // Retrieve the cached DTLS private key and certificate, i.e. identity, for
  // the |origin| and |identity_name| pair if such an identity exists and
  // |enable_cache| is true. Otherwise, generate a new identity using
  // |common_name|.
  // If the given |common_name| is different from the common name in the cached
  // identity that has the same origin and identity_name, a new private key and
  // a new certificate will be generated, overwriting the old one.
  //
  // |origin| is the origin of the DTLS connection;
  // |identity_name| is used to identify an identity within an origin; it is
  // opaque to WebRTCIdentityStore and remains private to the caller, i.e. not
  // present in the certificate;
  // |common_name| is the common name used to generate the certificate and will
  // be shared with the peer of the DTLS connection. Identities created for
  // different origins or different identity names may have the same common
  // name.
  // |callback| is the callback to return the result as DER strings.
  // |enable_cache| is true if the persistent cache should be used to return the
  // certificate. If a new identity is generated, it will be not saved in the
  // cache if |enable_cache| is false.
  // Returns the Closure used to cancel the request if the request is accepted.
  // The Closure can only be called before the request completes.
  virtual base::Closure RequestIdentity(const GURL& origin,
                                        const std::string& identity_name,
                                        const std::string& common_name,
                                        const CompletionCallback& callback,
                                        bool enable_cache);

  // Delete the identities created between |delete_begin| and |delete_end|.
  // |callback| will be called when the operation is done.
  void DeleteBetween(base::Time delete_begin,
                     base::Time delete_end,
                     const base::Closure& callback);

 protected:
  // Only virtual to allow subclassing for test mock.
  virtual ~WebRTCIdentityStore();

 private:
  friend class base::RefCountedThreadSafe<WebRTCIdentityStore>;
  friend class WebRtcIdentityStoreTest;

  void SetValidityPeriodForTesting(base::TimeDelta validity_period);
  void SetTaskRunnerForTesting(
      const scoped_refptr<base::TaskRunner>& task_runner);

  void BackendFindCallback(WebRTCIdentityRequest* request,
                           int error,
                           const std::string& certificate,
                           const std::string& private_key);
  void GenerateIdentityCallback(WebRTCIdentityRequest* request,
                                WebRTCIdentityRequestResult* result);
  WebRTCIdentityRequest* FindRequest(const GURL& origin,
                                     const std::string& identity_name,
                                     const std::string& common_name);
  void PostRequestResult(WebRTCIdentityRequest* request,
                         const WebRTCIdentityRequestResult& result);

  void GenerateNewIdentity(WebRTCIdentityRequest* request);

  // The validity period of the certificates.
  base::TimeDelta validity_period_;

  // The TaskRunner for doing work on a worker thread.
  scoped_refptr<base::TaskRunner> task_runner_;

  // Weak references of the in flight requests. Used to join identical external
  // requests.
  std::vector<WebRTCIdentityRequest*> in_flight_requests_;

  scoped_refptr<WebRTCIdentityStoreBackend> backend_;

  DISALLOW_COPY_AND_ASSIGN(WebRTCIdentityStore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_WEBRTC_WEBRTC_IDENTITY_STORE_H_
