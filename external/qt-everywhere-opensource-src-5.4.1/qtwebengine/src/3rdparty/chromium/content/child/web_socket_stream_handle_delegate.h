// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_SOCKET_STREAM_HANDLE_DELEGATE_H_
#define CONTENT_CHILD_WEB_SOCKET_STREAM_HANDLE_DELEGATE_H_

#include "base/strings/string16.h"

class GURL;

namespace blink {
class WebSocketStreamHandle;
}

namespace content {

class WebSocketStreamHandleDelegate {
 public:
  WebSocketStreamHandleDelegate() {}

  virtual void WillOpenStream(blink::WebSocketStreamHandle* handle,
                              const GURL& url) {}
  virtual void WillSendData(blink::WebSocketStreamHandle* handle,
                            const char* data, int len) {}

  virtual void DidOpenStream(blink::WebSocketStreamHandle* handle,
                             int max_amount_send_allowed) {}
  virtual void DidSendData(blink::WebSocketStreamHandle* handle,
                           int amount_sent) {}
  virtual void DidReceiveData(blink::WebSocketStreamHandle* handle,
                              const char* data, int len) {}
  virtual void DidClose(blink::WebSocketStreamHandle*) {}
  virtual void DidFail(blink::WebSocketStreamHandle* handle,
                       int error_code,
                       const base::string16& error_msg) {}

 protected:
  virtual ~WebSocketStreamHandleDelegate() {}
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_SOCKET_STREAM_HANDLE_DELEGATE_H_
