// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace cloud_print {

// API flow for communicating with cloud print and cloud devices.
class GCDApiFlow {
 public:
  // TODO(noamsml): Better error model for this class.
  enum Status {
    SUCCESS,
    ERROR_TOKEN,
    ERROR_NETWORK,
    ERROR_HTTP_CODE,
    ERROR_FROM_SERVER,
    ERROR_MALFORMED_RESPONSE
  };

  // Provides GCDApiFlowImpl with parameters required to make request.
  // Parses results of requests.
  class Request {
   public:
    Request();
    virtual ~Request();

    virtual void OnGCDApiFlowError(Status status) = 0;

    virtual void OnGCDApiFlowComplete(const base::DictionaryValue& value) = 0;

    virtual GURL GetURL() = 0;

    virtual std::string GetOAuthScope() = 0;

    virtual net::URLFetcher::RequestType GetRequestType();

    virtual std::vector<std::string> GetExtraRequestHeaders() = 0;

    // If there is no data, set upload_type and upload_data to ""
    virtual void GetUploadData(std::string* upload_type,
                               std::string* upload_data);

   private:
    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  GCDApiFlow();
  virtual ~GCDApiFlow();

  static std::unique_ptr<GCDApiFlow> Create(
      net::URLRequestContextGetter* request_context,
      OAuth2TokenService* token_service,
      const std::string& account_id);

  virtual void Start(std::unique_ptr<Request> request) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCDApiFlow);
};

class CloudPrintApiFlowRequest : public GCDApiFlow::Request {
 public:
  CloudPrintApiFlowRequest();
  ~CloudPrintApiFlowRequest() override;

  // GCDApiFlowRequest implementation
  std::string GetOAuthScope() override;
  std::vector<std::string> GetExtraRequestHeaders() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPrintApiFlowRequest);
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_H_
