// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebURLLoaderMock_h
#define WebURLLoaderMock_h

#include "base/macros.h"
#include "public/platform/WebURLLoader.h"
#include "wtf/WeakPtr.h"
#include <memory>

namespace blink {

class WebData;
struct WebURLError;
class WebURLLoaderClient;
class WebURLLoaderMockFactoryImpl;
class WebURLLoaderTestDelegate;
class WebURLRequest;
class WebURLResponse;

// A simple class for mocking WebURLLoader.
// If the WebURLLoaderMockFactory it is associated with has been configured to
// mock the request it gets, it serves the mocked resource.  Otherwise it just
// forwards it to the default loader.
class WebURLLoaderMock : public WebURLLoader {
 public:
  // This object becomes the owner of |default_loader|.
  WebURLLoaderMock(WebURLLoaderMockFactoryImpl* factory,
                   WebURLLoader* default_loader);
  ~WebURLLoaderMock() override;

  // Simulates the asynchronous request being served.
  void ServeAsynchronousRequest(WebURLLoaderTestDelegate* delegate,
                                const WebURLResponse& response,
                                const WebData& data,
                                const WebURLError& error);

  // Simulates the redirect being served.
  WebURLRequest ServeRedirect(
      const WebURLRequest& request,
      const WebURLResponse& redirectResponse);

  // WebURLLoader methods:
  void loadSynchronously(const WebURLRequest& request,
                         WebURLResponse& response,
                         WebURLError& error,
                         WebData& data) override;
  void loadAsynchronously(const WebURLRequest& request,
                          WebURLLoaderClient* client) override;
  void cancel() override;
  void setDefersLoading(bool defer) override;
  void setLoadingTaskRunner(WebTaskRunner*) override;

  bool is_deferred() { return is_deferred_; }
  bool is_cancelled() { return !client_; }

  WeakPtr<WebURLLoaderMock> GetWeakPtr();

 private:
  WebURLLoaderMockFactoryImpl* factory_ = nullptr;
  WebURLLoaderClient* client_ = nullptr;
  std::unique_ptr<WebURLLoader> default_loader_;
  bool using_default_loader_ = false;
  bool is_deferred_ = false;

  WeakPtrFactory<WebURLLoaderMock> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderMock);
};

} // namespace blink

#endif  // WebURLLoaderMock_h
