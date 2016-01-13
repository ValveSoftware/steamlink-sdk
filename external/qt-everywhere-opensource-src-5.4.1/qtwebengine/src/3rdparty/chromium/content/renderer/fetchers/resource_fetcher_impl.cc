// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/resource_fetcher_impl.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"

using base::TimeDelta;
using blink::WebFrame;
using blink::WebHTTPBody;
using blink::WebSecurityPolicy;
using blink::WebURLError;
using blink::WebURLLoader;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace content {

// static
ResourceFetcher* ResourceFetcher::Create(const GURL& url) {
  return new ResourceFetcherImpl(url);
}

ResourceFetcherImpl::ResourceFetcherImpl(const GURL& url)
    : request_(url),
      completed_(false) {
}

ResourceFetcherImpl::~ResourceFetcherImpl() {
  if (!completed_ && loader_)
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

  WebHTTPBody web_http_body;
  web_http_body.initialize();
  web_http_body.appendData(blink::WebData(body));
  request_.setHTTPBody(web_http_body);
}

void ResourceFetcherImpl::SetHeader(const std::string& header,
                                    const std::string& value) {
  DCHECK(!request_.isNull());
  DCHECK(!loader_);

  if (LowerCaseEqualsASCII(header, "referer")) {
    blink::WebString referrer = WebSecurityPolicy::generateReferrerHeader(
        blink::WebReferrerPolicyDefault,
        request_.url(),
        blink::WebString::fromUTF8(value));
    request_.setHTTPReferrer(referrer, blink::WebReferrerPolicyDefault);
  } else {
    request_.setHTTPHeaderField(blink::WebString::fromUTF8(header),
                                blink::WebString::fromUTF8(value));
  }
}

void ResourceFetcherImpl::Start(WebFrame* frame,
                                WebURLRequest::TargetType target_type,
                                const Callback& callback) {
  DCHECK(!loader_);
  DCHECK(!request_.isNull());
  DCHECK(callback_.is_null());
  DCHECK(!completed_);
  if (!request_.httpBody().isNull())
    DCHECK_NE("GET", request_.httpMethod().utf8()) << "GETs can't have bodies.";

  callback_ = callback;

  request_.setTargetType(target_type);
  request_.setFirstPartyForCookies(frame->document().firstPartyForCookies());
  frame->dispatchWillSendRequest(request_);
  loader_.reset(blink::Platform::current()->createURLLoader());
  loader_->loadAsynchronously(request_, this);

  // No need to hold on to the request.
  request_.reset();
}

void ResourceFetcherImpl::SetTimeout(const base::TimeDelta& timeout) {
  DCHECK(loader_);
  DCHECK(!completed_);

  timeout_timer_.Start(FROM_HERE, timeout, this,
                       &ResourceFetcherImpl::TimeoutFired);
}

void ResourceFetcherImpl::RunCallback(const WebURLResponse& response,
                                      const std::string& data) {
  completed_ = true;
  timeout_timer_.Stop();
  if (callback_.is_null())
    return;

  // Take a reference to the callback as running the callback may lead to our
  // destruction.
  Callback callback = callback_;
  callback.Run(response, data);
}

void ResourceFetcherImpl::TimeoutFired() {
  DCHECK(!completed_);
  loader_->cancel();
  RunCallback(WebURLResponse(), std::string());
}

/////////////////////////////////////////////////////////////////////////////
// WebURLLoaderClient methods

void ResourceFetcherImpl::willSendRequest(
    WebURLLoader* loader, WebURLRequest& new_request,
    const WebURLResponse& redirect_response) {
}

void ResourceFetcherImpl::didSendData(
    WebURLLoader* loader, unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
}

void ResourceFetcherImpl::didReceiveResponse(
    WebURLLoader* loader, const WebURLResponse& response) {
  DCHECK(!completed_);
  response_ = response;
}

void ResourceFetcherImpl::didReceiveData(
    WebURLLoader* loader, const char* data, int data_length,
    int encoded_data_length) {
  DCHECK(!completed_);
  DCHECK(data_length > 0);

  data_.append(data, data_length);
}

void ResourceFetcherImpl::didReceiveCachedMetadata(
    WebURLLoader* loader, const char* data, int data_length) {
  DCHECK(!completed_);
  DCHECK(data_length > 0);

  metadata_.assign(data, data_length);
}

void ResourceFetcherImpl::didFinishLoading(
    WebURLLoader* loader, double finishTime,
    int64_t total_encoded_data_length) {
  DCHECK(!completed_);

  RunCallback(response_, data_);
}

void ResourceFetcherImpl::didFail(WebURLLoader* loader,
                                  const WebURLError& error) {
  DCHECK(!completed_);

  // Go ahead and tell our delegate that we're done.
  RunCallback(WebURLResponse(), std::string());
}

}  // namespace content
