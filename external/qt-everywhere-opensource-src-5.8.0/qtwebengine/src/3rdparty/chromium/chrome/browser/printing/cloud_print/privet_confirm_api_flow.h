// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_CONFIRM_API_FLOW_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_CONFIRM_API_FLOW_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/printing/cloud_print/gcd_api_flow.h"
#include "net/url_request/url_request_context_getter.h"

namespace cloud_print {

// API call flow for server-side communication with CloudPrint for registration.
class PrivetConfirmApiCallFlow : public CloudPrintApiFlowRequest {
 public:
  typedef base::Callback<void(GCDApiFlow::Status /*success*/)> ResponseCallback;

  // Create an OAuth2-based confirmation
  PrivetConfirmApiCallFlow(const std::string& token,
                           const ResponseCallback& callback);

  ~PrivetConfirmApiCallFlow() override;

  void OnGCDApiFlowError(GCDApiFlow::Status status) override;
  void OnGCDApiFlowComplete(const base::DictionaryValue& value) override;
  net::URLFetcher::RequestType GetRequestType() override;

  GURL GetURL() override;

 private:
  ResponseCallback callback_;
  std::string token_;
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_CONFIRM_API_FLOW_H_
