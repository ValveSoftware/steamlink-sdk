// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/rpc/http_post.h"

// TODO(ckehoe): Support third-party protobufs too.
#include <google/protobuf/message_lite.h>

#include "base/bind.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace copresence {

const char HttpPost::kApiKeyField[] = "key";
const char HttpPost::kTracingField[] = "trace";

HttpPost::HttpPost(net::URLRequestContextGetter* url_context_getter,
                   const std::string& server_host,
                   const std::string& rpc_name,
                   std::string api_key,
                   const std::string& auth_token,
                   const std::string& tracing_token,
                   const google::protobuf::MessageLite& request_proto) {
  // Create the base URL to call.
  GURL url(server_host + "/" + rpc_name);

  // Add the tracing token, if specified.
  if (!tracing_token.empty()) {
    url = net::AppendQueryParameter(
        url, kTracingField, "token:" + tracing_token);
  }

  // If we have no auth token, authenticate using the API key.
  // If no API key is specified, use the Chrome API key.
  if (auth_token.empty()) {
    if (api_key.empty()) {
#ifdef GOOGLE_CHROME_BUILD
      DCHECK(google_apis::HasKeysConfigured());
      api_key = google_apis::GetAPIKey();
#else
      LOG(ERROR) << "No Copresence API key provided";
#endif
    }
    url = net::AppendQueryParameter(url, kApiKeyField, api_key);
  }

  // Serialize the proto for transmission.
  std::string request_data;
  bool serialize_success = request_proto.SerializeToString(&request_data);
  DCHECK(serialize_success);

  // Configure and send the request.
  url_fetcher_ =
      net::URLFetcher::Create(kUrlFetcherId, url, net::URLFetcher::POST, this);
  url_fetcher_->SetRequestContext(url_context_getter);
  url_fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE |
                             net::LOAD_DISABLE_CACHE |
                             net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->SetUploadData("application/x-protobuf", request_data);
  if (!auth_token.empty()) {
    url_fetcher_->AddExtraRequestHeader("Authorization: Bearer " + auth_token);
  }
}

HttpPost::~HttpPost() {}

void HttpPost::Start(const ResponseCallback& response_callback) {
  response_callback_ = response_callback;
  DVLOG(3) << "Sending Copresence request to "
           << url_fetcher_->GetOriginalURL().spec();
  url_fetcher_->Start();
}

void HttpPost::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(url_fetcher_.get(), source);

  // Gather response info.
  std::string response;
  source->GetResponseAsString(&response);
  int response_code = source->GetResponseCode();

  // Log any errors.
  if (response_code < 0) {
    net::URLRequestStatus status = source->GetStatus();
    LOG(WARNING) << "Couldn't contact the Copresence server at "
                 << source->GetURL() << ". Status code " << status.status();
    LOG_IF(WARNING, status.error())
        << "Network error: " << net::ErrorToString(status.error());
    LOG_IF(WARNING, !response.empty()) << "HTTP response: " << response;
  } else if (response_code != net::HTTP_OK) {
    LOG(WARNING) << "Copresence request got HTTP response code "
                 << response_code << ". Response:\n" << response;
  }

  // Return the response.
  response_callback_.Run(response_code, response);
  delete this;
}

}  // namespace copresence
