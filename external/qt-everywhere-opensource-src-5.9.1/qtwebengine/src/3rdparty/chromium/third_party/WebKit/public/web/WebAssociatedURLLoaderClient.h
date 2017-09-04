// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAssociatedURLLoaderClient_h
#define WebAssociatedURLLoaderClient_h

namespace blink {

class WebURLRequest;
class WebURLResponse;
struct WebURLError;

class WebAssociatedURLLoaderClient {
 public:
  virtual bool willFollowRedirect(const WebURLRequest& newRequest,
                                  const WebURLResponse& redirectResponse) {
    return true;
  }
  virtual void didSendData(unsigned long long bytesSent,
                           unsigned long long totalBytesToBeSent) {}
  virtual void didReceiveResponse(const WebURLResponse&) {}
  virtual void didDownloadData(int dataLength) {}
  virtual void didReceiveData(const char* data, int dataLength) {}
  virtual void didReceiveCachedMetadata(const char* data, int dataLength) {}
  virtual void didFinishLoading(double finishTime) {}
  virtual void didFail(const WebURLError&) {}

 protected:
  virtual ~WebAssociatedURLLoaderClient() {}
};

}  // namespace blink

#endif
