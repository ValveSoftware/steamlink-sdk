// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebURLLoaderMockFactoryImpl_h
#define WebURLLoaderMockFactoryImpl_h

#include <map>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/HashMap.h"
#include "wtf/WeakPtr.h"

namespace blink {

class WebData;
class WebURLLoader;
class WebURLLoaderMock;
class WebURLLoaderTestDelegate;

// A factory that creates WebURLLoaderMock to simulate resource loading in
// tests.
// You register files for specific URLs, the content of the file is then served
// when these URLs are loaded.
// In order to serve the asynchronous requests, you need to invoke
// ServeAsynchronousRequest.
class WebURLLoaderMockFactoryImpl : public WebURLLoaderMockFactory {
 public:
  WebURLLoaderMockFactoryImpl();
  virtual ~WebURLLoaderMockFactoryImpl();

  // WebURLLoaderMockFactory:
  virtual WebURLLoader* createURLLoader(WebURLLoader* default_loader) override;
  void registerURL(const WebURL& url,
                   const WebURLResponse& response,
                   const WebString& filePath = WebString()) override;
  void registerErrorURL(const WebURL& url,
                        const WebURLResponse& response,
                        const WebURLError& error) override;
  void unregisterURL(const WebURL& url) override;
  void unregisterAllURLs() override;
  void serveAsynchronousRequests() override;
  void setLoaderDelegate(WebURLLoaderTestDelegate* delegate) override {
    delegate_ = delegate;
  }

  // Returns true if |url| was registered for being mocked.
  bool IsMockedURL(const WebURL& url);

  // Called by the loader to load a resource.
  void LoadSynchronously(const WebURLRequest& request,
                         WebURLResponse* response,
                         WebURLError* error,
                         WebData* data);
  void LoadAsynchronouly(const WebURLRequest& request,
                         WebURLLoaderMock* loader);

  // Removes the loader from the list of pending loaders.
  void CancelLoad(WebURLLoaderMock* loader);

 private:
  struct ResponseInfo {
    WebURLResponse response;
    base::FilePath file_path;
  };

  // Loads the specified request and populates the response, error and data
  // accordingly.
  void LoadRequest(const WebURLRequest& request,
                   WebURLResponse* response,
                   WebURLError* error,
                   WebData* data);

  // Checks if the loader is pending. Otherwise, it may have been deleted.
  bool IsPending(WeakPtr<WebURLLoaderMock> loader);

  // Reads |m_filePath| and puts its content in |data|.
  // Returns true if it successfully read the file.
  static bool ReadFile(const base::FilePath& file_path, WebData* data);

  WebURLLoaderTestDelegate* delegate_ = nullptr;

  // The loaders that have not being served data yet.
  using LoaderToRequestMap = HashMap<WebURLLoaderMock*, WebURLRequest>;
  LoaderToRequestMap pending_loaders_;

  typedef HashMap<KURL, WebURLError> URLToErrorMap;
  URLToErrorMap url_to_error_info_;

  // Table of the registered URLs and the responses that they should receive.
  using URLToResponseMap = HashMap<KURL, ResponseInfo>;
  URLToResponseMap url_to_reponse_info_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderMockFactoryImpl);
};

} // namespace blink

#endif  // WebURLLoaderMockFactoryImpl_h
