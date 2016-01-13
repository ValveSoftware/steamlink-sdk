// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebSocketStreamHandle.

#include "content/child/web_socket_stream_handle_impl.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "content/child/child_thread.h"
#include "content/child/socket_stream_dispatcher.h"
#include "content/child/web_socket_stream_handle_bridge.h"
#include "content/child/web_socket_stream_handle_delegate.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebSocketStreamError.h"
#include "third_party/WebKit/public/platform/WebSocketStreamHandleClient.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebData;
using blink::WebSocketStreamError;
using blink::WebSocketStreamHandle;
using blink::WebSocketStreamHandleClient;
using blink::WebURL;

namespace content {

// WebSocketStreamHandleImpl::Context -----------------------------------------

class WebSocketStreamHandleImpl::Context
    : public base::RefCounted<Context>,
      public WebSocketStreamHandleDelegate {
 public:
  explicit Context(WebSocketStreamHandleImpl* handle);

  WebSocketStreamHandleClient* client() const { return client_; }
  void set_client(WebSocketStreamHandleClient* client) {
    client_ = client;
  }

  void Connect(const WebURL& url);
  bool Send(const WebData& data);
  void Close();

  // Must be called before |handle_| or |client_| is deleted.
  // Once detached, it never calls |client_| back.
  void Detach();

  // WebSocketStreamHandleDelegate methods:
  virtual void DidOpenStream(WebSocketStreamHandle*, int) OVERRIDE;
  virtual void DidSendData(WebSocketStreamHandle*, int) OVERRIDE;
  virtual void DidReceiveData(WebSocketStreamHandle*,
                              const char*,
                              int) OVERRIDE;
  virtual void DidClose(WebSocketStreamHandle*) OVERRIDE;
  virtual void DidFail(WebSocketStreamHandle*,
                       int,
                       const base::string16&) OVERRIDE;

 private:
  friend class base::RefCounted<Context>;
  virtual ~Context() {
    DCHECK(!handle_);
    DCHECK(!client_);
    DCHECK(!bridge_.get());
  }

  WebSocketStreamHandleImpl* handle_;
  WebSocketStreamHandleClient* client_;
  // |bridge_| is alive from Connect to DidClose, so Context must be alive
  // in the time period.
  scoped_refptr<WebSocketStreamHandleBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

WebSocketStreamHandleImpl::Context::Context(WebSocketStreamHandleImpl* handle)
    : handle_(handle),
      client_(NULL) {
}

void WebSocketStreamHandleImpl::Context::Connect(const WebURL& url) {
  VLOG(1) << "Connect url=" << url;
  DCHECK(!bridge_.get());

  SocketStreamDispatcher* dispatcher =
      ChildThread::current()->socket_stream_dispatcher();
  bridge_ = dispatcher->CreateBridge(handle_, this);

  AddRef();  // Will be released by DidClose().
  bridge_->Connect(url);
}

bool WebSocketStreamHandleImpl::Context::Send(const WebData& data) {
  VLOG(1) << "Send data.size=" << data.size();
  DCHECK(bridge_.get());
  return bridge_->Send(
      std::vector<char>(data.data(), data.data() + data.size()));
}

void WebSocketStreamHandleImpl::Context::Close() {
  VLOG(1) << "Close";
  if (bridge_.get())
    bridge_->Close();
}

void WebSocketStreamHandleImpl::Context::Detach() {
  handle_ = NULL;
  client_ = NULL;
  // If Connect was called, |bridge_| is not NULL, so that this Context closes
  // the |bridge_| here.  Then |bridge_| will call back DidClose, and will
  // be released by itself.
  // Otherwise, |bridge_| is NULL.
  if (bridge_.get())
    bridge_->Close();
}

void WebSocketStreamHandleImpl::Context::DidOpenStream(
    WebSocketStreamHandle* web_handle, int max_amount_send_allowed) {
  VLOG(1) << "DidOpen";
  if (client_)
    client_->didOpenStream(handle_, max_amount_send_allowed);
}

void WebSocketStreamHandleImpl::Context::DidSendData(
    WebSocketStreamHandle* web_handle, int amount_sent) {
  if (client_)
    client_->didSendData(handle_, amount_sent);
}

void WebSocketStreamHandleImpl::Context::DidReceiveData(
    WebSocketStreamHandle* web_handle, const char* data, int size) {
  if (client_)
    client_->didReceiveData(handle_, WebData(data, size));
}

void WebSocketStreamHandleImpl::Context::DidClose(
    WebSocketStreamHandle* web_handle) {
  VLOG(1) << "DidClose";
  bridge_ = NULL;
  WebSocketStreamHandleImpl* handle = handle_;
  handle_ = NULL;
  if (client_) {
    WebSocketStreamHandleClient* client = client_;
    client_ = NULL;
    client->didClose(handle);
  }
  Release();
}

void WebSocketStreamHandleImpl::Context::DidFail(
    WebSocketStreamHandle* web_handle,
    int error_code,
    const base::string16& error_msg) {
  VLOG(1) << "DidFail";
  if (client_) {
    client_->didFail(
        handle_,
        WebSocketStreamError(error_code, error_msg));
  }
}

// WebSocketStreamHandleImpl ------------------------------------------------

WebSocketStreamHandleImpl::WebSocketStreamHandleImpl()
    : context_(new Context(this)) {
}

WebSocketStreamHandleImpl::~WebSocketStreamHandleImpl() {
  // We won't receive any events from |context_|.
  // |context_| is ref counted, and will be released when it received
  // DidClose.
  context_->Detach();
}

void WebSocketStreamHandleImpl::connect(
    const WebURL& url, WebSocketStreamHandleClient* client) {
  VLOG(1) << "connect url=" << url;
  DCHECK(!context_->client());
  context_->set_client(client);

  context_->Connect(url);
}

bool WebSocketStreamHandleImpl::send(const WebData& data) {
  return context_->Send(data);
}

void WebSocketStreamHandleImpl::close() {
  context_->Close();
}

}  // namespace content
