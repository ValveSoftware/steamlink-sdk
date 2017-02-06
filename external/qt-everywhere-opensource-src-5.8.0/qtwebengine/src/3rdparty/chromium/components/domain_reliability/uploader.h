// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_UPLOADER_H_
#define COMPONENTS_DOMAIN_RELIABILITY_UPLOADER_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequest;
class URLRequestContextGetter;
}  // namespace net

namespace domain_reliability {

class MockableTime;

// Uploads Domain Reliability reports to collectors.
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityUploader {
 public:
  struct UploadResult {
    enum UploadStatus {
      FAILURE,
      SUCCESS,
      RETRY_AFTER,
    };

    bool is_success() const { return status == SUCCESS; }
    bool is_failure() const { return status == FAILURE; }
    bool is_retry_after() const { return status == RETRY_AFTER; }

    UploadStatus status;
    base::TimeDelta retry_after;
  };

  typedef base::Callback<void(const UploadResult& result)> UploadCallback;

  DomainReliabilityUploader();

  virtual ~DomainReliabilityUploader();

  // Creates an uploader that uses the given |url_request_context_getter| to
  // get a URLRequestContext to use for uploads. (See test_util.h for a mock
  // version.)
  static std::unique_ptr<DomainReliabilityUploader> Create(
      MockableTime* time,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter);

  // Uploads |report_json| to |upload_url| and calls |callback| when the upload
  // has either completed or failed.
  virtual void UploadReport(const std::string& report_json,
                            int max_beacon_depth,
                            const GURL& upload_url,
                            const UploadCallback& callback) = 0;

  virtual void set_discard_uploads(bool discard_uploads) = 0;

  static int GetURLRequestUploadDepth(const net::URLRequest& request);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_UPLOADER_H_
