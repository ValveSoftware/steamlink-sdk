// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/resource_fetcher_impl.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"

namespace content {

// static
ResourceFetcher* ResourceFetcher::Create(const GURL& url) {
  return new ResourceFetcherImpl(url);
}

ResourceFetcherImpl::ResourceFetcherImpl(const GURL& url)
    : request_(url) {
}

ResourceFetcherImpl::~ResourceFetcherImpl() {
  if (!completed() && loader_)
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

void ResourceFetcherImpl::SetSkipServiceWorker(
    blink::WebURLRequest::SkipServiceWorker skip_service_worker) {
  DCHECK(!request_.isNull());
  DCHECK(!loader_);

  request_.setSkipServiceWorker(skip_service_worker);
}

void ResourceFetcherImpl::SetCachePolicy(blink::WebCachePolicy policy) {
  DCHECK(!request_.isNull());
  DCHECK(!loader_);

  request_.setCachePolicy(policy);
}

void ResourceFetcherImpl::SetLoaderOptions(
    const blink::WebURLLoaderOptions& options) {
  DCHECK(!request_.isNull());
  DCHECK(!loader_);

  options_ = options;
}

void ResourceFetcherImpl::Start(
    blink::WebFrame* frame,
    blink::WebURLRequest::RequestContext request_context,
    blink::WebURLRequest::FrameType frame_type,
    LoaderType loader_type,
    const Callback& callback) {
  DCHECK(!loader_);
  DCHECK(!request_.isNull());
  DCHECK(callback_.is_null());
  DCHECK(!completed());
  if (!request_.httpBody().isNull())
    DCHECK_NE("GET", request_.httpMethod().utf8()) << "GETs can't have bodies.";

  callback_ = callback;

  request_.setRequestContext(request_context);
  request_.setFrameType(frame_type);
  request_.setFirstPartyForCookies(frame->document().firstPartyForCookies());
  frame->dispatchWillSendRequest(request_);

  switch (loader_type) {
    case PLATFORM_LOADER:
      loader_.reset(blink::Platform::current()->createURLLoader());
      break;
    case FRAME_ASSOCIATED_LOADER:
      loader_.reset(frame->createAssociatedURLLoader(options_));
      break;
  }
  loader_->loadAsynchronously(request_, this);

  // No need to hold on to the request.
  request_.reset();
}

void ResourceFetcherImpl::SetTimeout(const base::TimeDelta& timeout) {
  DCHECK(loader_);
  DCHECK(!completed());

  timeout_timer_.Start(FROM_HERE, timeout, this, &ResourceFetcherImpl::Cancel);
}

void ResourceFetcherImpl::OnLoadComplete() {
  timeout_timer_.Stop();
  if (callback_.is_null())
    return;

  // Take a reference to the callback as running the callback may lead to our
  // destruction.
  Callback callback = callback_;
  callback.Run(status() == LOAD_FAILED ? blink::WebURLResponse() : response(),
               status() == LOAD_FAILED ? std::string() : data());
}

void ResourceFetcherImpl::Cancel() {
  loader_->cancel();
  WebURLLoaderClientImpl::Cancel();
}

}  // namespace content
