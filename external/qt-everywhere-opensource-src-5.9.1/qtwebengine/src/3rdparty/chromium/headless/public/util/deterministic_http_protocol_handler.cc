// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/deterministic_http_protocol_handler.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/util/deterministic_dispatcher.h"
#include "headless/public/util/generic_url_request_job.h"
#include "headless/public/util/http_url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace headless {

class DeterministicHttpProtocolHandler::NopGenericURLRequestJobDelegate
    : public GenericURLRequestJob::Delegate {
 public:
  NopGenericURLRequestJobDelegate() {}
  ~NopGenericURLRequestJobDelegate() override {}

  // GenericURLRequestJob::Delegate methods:
  bool BlockOrRewriteRequest(
      const GURL& url,
      const std::string& method,
      const std::string& referrer,
      GenericURLRequestJob::RewriteCallback callback) override {
    return false;
  }

  const GenericURLRequestJob::HttpResponse* MaybeMatchResource(
      const GURL& url,
      const std::string& method,
      const net::HttpRequestHeaders& request_headers) override {
    return nullptr;
  }

  void OnResourceLoadComplete(const GURL& final_url,
                              const std::string& mime_type,
                              int http_response_code) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NopGenericURLRequestJobDelegate);
};

DeterministicHttpProtocolHandler::DeterministicHttpProtocolHandler(
    DeterministicDispatcher* deterministic_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : deterministic_dispatcher_(deterministic_dispatcher),
      io_task_runner_(io_task_runner),
      nop_delegate_(new NopGenericURLRequestJobDelegate()) {}

DeterministicHttpProtocolHandler::~DeterministicHttpProtocolHandler() {
  if (url_request_context_)
    io_task_runner_->DeleteSoon(FROM_HERE, url_request_context_.release());
  if (url_request_job_factory_)
    io_task_runner_->DeleteSoon(FROM_HERE, url_request_job_factory_.release());
}

net::URLRequestJob* DeterministicHttpProtocolHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  if (!url_request_context_) {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    // Create our own URLRequestContext with an empty URLRequestJobFactoryImpl
    // which lets us use the default http(s) RequestJobs.
    url_request_context_.reset(new net::URLRequestContext());
    url_request_context_->CopyFrom(request->context());
    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl());
    url_request_context_->set_job_factory(url_request_job_factory_.get());
  }
  return new GenericURLRequestJob(
      request, network_delegate, deterministic_dispatcher_,
      base::MakeUnique<HttpURLFetcher>(url_request_context_.get()),
      nop_delegate_.get());
}

}  // namespace headless
