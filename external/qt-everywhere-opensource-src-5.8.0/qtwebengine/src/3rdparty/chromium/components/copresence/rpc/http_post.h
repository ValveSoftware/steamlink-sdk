// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_RPC_HTTP_POST_H_
#define COMPONENTS_COPRESENCE_RPC_HTTP_POST_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace google {
namespace protobuf {
class MessageLite;
}
}

namespace net {
class URLRequestContextGetter;
}

namespace copresence {

// This class handles all Apiary calls to the Copresence server.
// It configures the HTTP request appropriately and reports any errors.
// If deleted, the HTTP request is cancelled.
//
// TODO(ckehoe): Add retry logic.
class HttpPost : public net::URLFetcherDelegate {
 public:
  // Callback to receive the HTTP status code and body of the response
  // (if any). A pointer to this HttpPost object is also passed along.
  using ResponseCallback = base::Callback<void(int, const std::string&)>;

  // Create a request to the Copresence server.
  // |url_context_getter| is owned by the caller,
  // and the context it provides must be available until the request completes.
  HttpPost(net::URLRequestContextGetter* url_context_getter,
           const std::string& server_host,
           // TODO(ckehoe): Condense some of these into a struct.
           const std::string& rpc_name,
           std::string api_key,  // If blank, we overwrite with a default.
           const std::string& auth_token,
           const std::string& tracing_token,
           const google::protobuf::MessageLite& request_proto);

  // HTTP requests are cancelled on delete.
  ~HttpPost() override;

  // Send an HttpPost request.
  void Start(const ResponseCallback& response_callback);

 private:
  static const int kUrlFetcherId = 1;
  static const char kApiKeyField[];
  static const char kTracingField[];

  friend class HttpPostTest;

  // Overridden from net::URLFetcherDelegate.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  ResponseCallback response_callback_;

  std::unique_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(HttpPost);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_RPC_HTTP_POST_H_
