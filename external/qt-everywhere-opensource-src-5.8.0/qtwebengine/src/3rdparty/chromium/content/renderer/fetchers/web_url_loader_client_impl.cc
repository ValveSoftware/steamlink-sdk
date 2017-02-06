// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/web_url_loader_client_impl.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

namespace content {

WebURLLoaderClientImpl::WebURLLoaderClientImpl()
    : completed_(false)
    , status_(LOADING) {
}

WebURLLoaderClientImpl::~WebURLLoaderClientImpl() {
}

void WebURLLoaderClientImpl::didReceiveResponse(
    blink::WebURLLoader* loader, const blink::WebURLResponse& response) {
  DCHECK(!completed_);
  response_ = response;
}

void WebURLLoaderClientImpl::didReceiveData(
    blink::WebURLLoader* loader,
    const char* data,
    int data_length,
    int encoded_data_length) {
  // The AssociatedURLLoader will continue after a load failure.
  // For example, for an Access Control error.
  if (completed_)
    return;
  DCHECK(data_length > 0);

  data_.append(data, data_length);
}

void WebURLLoaderClientImpl::didReceiveCachedMetadata(
    blink::WebURLLoader* loader,
    const char* data,
    int data_length) {
  DCHECK(!completed_);
  DCHECK(data_length > 0);

  metadata_.assign(data, data_length);
}

void WebURLLoaderClientImpl::didFinishLoading(
    blink::WebURLLoader* loader,
    double finishTime,
    int64_t total_encoded_data_length) {
  // The AssociatedURLLoader will continue after a load failure.
  // For example, for an Access Control error.
  if (completed_)
    return;
  OnLoadCompleteInternal(LOAD_SUCCEEDED);
}

void WebURLLoaderClientImpl::didFail(blink::WebURLLoader* loader,
                                     const blink::WebURLError& error) {
  OnLoadCompleteInternal(LOAD_FAILED);
}

void WebURLLoaderClientImpl::Cancel() {
  OnLoadCompleteInternal(LOAD_FAILED);
}

void WebURLLoaderClientImpl::OnLoadCompleteInternal(LoadStatus status) {
  DCHECK(!completed_);
  DCHECK(status_ == LOADING);

  completed_ = true;
  status_ = status;

  OnLoadComplete();
}

}  // namespace content
