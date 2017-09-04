// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HISTOGRAM_INTERNALS_REQUEST_JOB_H_
#define CONTENT_BROWSER_HISTOGRAM_INTERNALS_REQUEST_JOB_H_

#include <string>

#include "base/macros.h"
#include "net/url_request/url_request_simple_job.h"

namespace content {

class HistogramInternalsRequestJob : public net::URLRequestSimpleJob {
 public:
  HistogramInternalsRequestJob(net::URLRequest* request,
                               net::NetworkDelegate* network_delegate);

  int GetData(std::string* mime_type,
              std::string* charset,
              std::string* data,
              const net::CompletionCallback& callback) const override;

 private:
  ~HistogramInternalsRequestJob() override {}

  // The string to select histograms which have |path_| as a substring.
  std::string path_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(HistogramInternalsRequestJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_HISTOGRAM_INTERNALS_REQUEST_JOB_H_
