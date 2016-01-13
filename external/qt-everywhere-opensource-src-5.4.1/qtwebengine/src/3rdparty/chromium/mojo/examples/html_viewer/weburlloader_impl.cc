// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/html_viewer/weburlloader_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

namespace mojo {
namespace examples {
namespace {

blink::WebURLResponse ToWebURLResponse(const URLResponsePtr& url_response) {
  blink::WebURLResponse result;
  result.initialize();
  result.setURL(GURL(url_response->url));
  // TODO(darin): Copy other fields.
  return result;
}

}  // namespace

WebURLLoaderImpl::WebURLLoaderImpl(NetworkService* network_service)
    : client_(NULL),
      weak_factory_(this) {
  network_service->CreateURLLoader(Get(&url_loader_));
  url_loader_.set_client(this);
}

WebURLLoaderImpl::~WebURLLoaderImpl() {
}

void WebURLLoaderImpl::loadSynchronously(
    const blink::WebURLRequest& request,
    blink::WebURLResponse& response,
    blink::WebURLError& error,
    blink::WebData& data) {
  NOTIMPLEMENTED();
}

void WebURLLoaderImpl::loadAsynchronously(const blink::WebURLRequest& request,
                                          blink::WebURLLoaderClient* client) {
  client_ = client;

  URLRequestPtr url_request(URLRequest::New());
  url_request->url = request.url().spec();
  url_request->auto_follow_redirects = false;
  // TODO(darin): Copy other fields.

  DataPipe pipe;
  url_loader_->Start(url_request.Pass(), pipe.producer_handle.Pass());
  response_body_stream_ = pipe.consumer_handle.Pass();
}

void WebURLLoaderImpl::cancel() {
  url_loader_.reset();
  response_body_stream_.reset();
  // TODO(darin): Need to asynchronously call didFail.
}

void WebURLLoaderImpl::setDefersLoading(bool defers_loading) {
  NOTIMPLEMENTED();
}

void WebURLLoaderImpl::OnReceivedRedirect(URLResponsePtr url_response,
                                          const String& new_url,
                                          const String& new_method) {
  blink::WebURLRequest new_request;
  new_request.initialize();
  new_request.setURL(GURL(new_url));

  client_->willSendRequest(this, new_request, ToWebURLResponse(url_response));
  // TODO(darin): Check if new_request was rejected.

  url_loader_->FollowRedirect();
}

void WebURLLoaderImpl::OnReceivedResponse(URLResponsePtr url_response) {
  client_->didReceiveResponse(this, ToWebURLResponse(url_response));

  // Start streaming data
  ReadMore();
}

void WebURLLoaderImpl::OnReceivedError(NetworkErrorPtr error) {
  // TODO(darin): Construct a meaningful WebURLError.
  client_->didFail(this, blink::WebURLError());
}

void WebURLLoaderImpl::OnReceivedEndOfResponseBody() {
  // This is the signal that the response body was not truncated.
}

void WebURLLoaderImpl::ReadMore() {
  const void* buf;
  uint32_t buf_size;
  MojoResult rv = BeginReadDataRaw(response_body_stream_.get(),
                                   &buf,
                                   &buf_size,
                                   MOJO_READ_DATA_FLAG_NONE);
  if (rv == MOJO_RESULT_OK) {
    client_->didReceiveData(this, static_cast<const char*>(buf), buf_size, -1);
    EndReadDataRaw(response_body_stream_.get(), buf_size);
    WaitToReadMore();
  } else if (rv == MOJO_RESULT_SHOULD_WAIT) {
    WaitToReadMore();
  } else if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
    // We reached end-of-file.
    double finish_time = base::Time::Now().ToDoubleT();
    client_->didFinishLoading(
        this,
        finish_time,
        blink::WebURLLoaderClient::kUnknownEncodedDataLength);
  } else {
    // TODO(darin): Oops!
  }
}

void WebURLLoaderImpl::WaitToReadMore() {
  handle_watcher_.Start(
      response_body_stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_DEADLINE_INDEFINITE,
      base::Bind(&WebURLLoaderImpl::OnResponseBodyStreamReady,
                 weak_factory_.GetWeakPtr()));
}

void WebURLLoaderImpl::OnResponseBodyStreamReady(MojoResult result) {
  ReadMore();
}

}  // namespace examples
}  // namespace mojo
