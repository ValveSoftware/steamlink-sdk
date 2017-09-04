// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/resource_fetcher_impl.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"

namespace content {

// static
ResourceFetcher* ResourceFetcher::Create(const GURL& url) {
  return new ResourceFetcherImpl(url);
}

class ResourceFetcherImpl::ClientImpl : public blink::WebURLLoaderClient {
 public:
  ClientImpl(ResourceFetcherImpl* parent, const Callback& callback)
      : parent_(parent),
        completed_(false),
        status_(LOADING),
        callback_(callback) {}

  ~ClientImpl() override {}

  virtual void Cancel() { OnLoadCompleteInternal(LOAD_FAILED); }

  bool completed() const { return completed_; }

 private:
  enum LoadStatus {
    LOADING,
    LOAD_FAILED,
    LOAD_SUCCEEDED,
  };

  void OnLoadCompleteInternal(LoadStatus status) {
    DCHECK(!completed_);
    DCHECK_EQ(status_, LOADING);

    completed_ = true;
    status_ = status;

    parent_->OnLoadComplete();

    if (callback_.is_null())
      return;

    // Take a reference to the callback as running the callback may lead to our
    // destruction.
    Callback callback = callback_;
    callback.Run(status_ == LOAD_FAILED ? blink::WebURLResponse() : response_,
                 status_ == LOAD_FAILED ? std::string() : data_);
  }

  // WebURLLoaderClient methods:
  void didReceiveResponse(blink::WebURLLoader* loader,
                          const blink::WebURLResponse& response) override {
    DCHECK(!completed_);

    response_ = response;
  }
  void didReceiveCachedMetadata(blink::WebURLLoader* loader,
                                const char* data,
                                int data_length) override {
    DCHECK(!completed_);
    DCHECK_GT(data_length, 0);
  }
  void didReceiveData(blink::WebURLLoader* loader,
                      const char* data,
                      int data_length,
                      int encoded_data_length,
                      int encoded_body_length) override {
    DCHECK(!completed_);
    DCHECK_GT(data_length, 0);

    data_.append(data, data_length);
  }
  void didFinishLoading(blink::WebURLLoader* loader,
                        double finishTime,
                        int64_t total_encoded_data_length) override {
    DCHECK(!completed_);

    OnLoadCompleteInternal(LOAD_SUCCEEDED);
  }
  void didFail(blink::WebURLLoader* loader,
               const blink::WebURLError& error) override {
    OnLoadCompleteInternal(LOAD_FAILED);
  }

 private:
  ResourceFetcherImpl* parent_;

  // Set to true once the request is complete.
  bool completed_;

  // Buffer to hold the content from the server.
  std::string data_;

  // A copy of the original resource response.
  blink::WebURLResponse response_;

  LoadStatus status_;

  // Callback when we're done.
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(ClientImpl);
};

ResourceFetcherImpl::ResourceFetcherImpl(const GURL& url)
    : request_(url) {
}

ResourceFetcherImpl::~ResourceFetcherImpl() {
  if (!loader_)
    return;

  DCHECK(client_);

  if (!client_->completed())
    loader_->cancel();
}

void ResourceFetcherImpl::SetMethod(const std::string& method) {
  DCHECK(!request_.isNull());
  DCHECK(!loader_);

  request_.setHTTPMethod(blink::WebString::fromUTF8(method));
}

void ResourceFetcherImpl::SetBody(const std::string& body) {
  DCHECK(!request_.isNull());
  DCHECK(!loader_);

  blink::WebHTTPBody web_http_body;
  web_http_body.initialize();
  web_http_body.appendData(blink::WebData(body));
  request_.setHTTPBody(web_http_body);
}

void ResourceFetcherImpl::SetHeader(const std::string& header,
                                    const std::string& value) {
  DCHECK(!request_.isNull());
  DCHECK(!loader_);

  if (base::LowerCaseEqualsASCII(header, "referer")) {
    blink::WebString referrer =
        blink::WebSecurityPolicy::generateReferrerHeader(
            blink::WebReferrerPolicyDefault,
            request_.url(),
            blink::WebString::fromUTF8(value));
    request_.setHTTPReferrer(referrer, blink::WebReferrerPolicyDefault);
  } else {
    request_.setHTTPHeaderField(blink::WebString::fromUTF8(header),
                                blink::WebString::fromUTF8(value));
  }
}

void ResourceFetcherImpl::Start(
    blink::WebFrame* frame,
    blink::WebURLRequest::RequestContext request_context,
    blink::WebURLRequest::FrameType frame_type,
    const Callback& callback) {
  DCHECK(!loader_);
  DCHECK(!client_);
  DCHECK(!request_.isNull());
  if (!request_.httpBody().isNull())
    DCHECK_NE("GET", request_.httpMethod().utf8()) << "GETs can't have bodies.";

  request_.setRequestContext(request_context);
  request_.setFrameType(frame_type);
  request_.setFirstPartyForCookies(frame->document().firstPartyForCookies());
  frame->dispatchWillSendRequest(request_);

  client_.reset(new ClientImpl(this, callback));

  loader_.reset(blink::Platform::current()->createURLLoader());
  loader_->loadAsynchronously(request_, client_.get());

  // No need to hold on to the request; reset it now.
  request_ = blink::WebURLRequest();
}

void ResourceFetcherImpl::SetTimeout(const base::TimeDelta& timeout) {
  DCHECK(loader_);
  DCHECK(client_);
  DCHECK(!client_->completed());

  timeout_timer_.Start(FROM_HERE, timeout, this, &ResourceFetcherImpl::Cancel);
}

void ResourceFetcherImpl::OnLoadComplete() {
  timeout_timer_.Stop();
}

void ResourceFetcherImpl::Cancel() {
  loader_->cancel();
  client_->Cancel();
}

}  // namespace content
