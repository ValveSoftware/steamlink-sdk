// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLE_HTML_VIEWER_WEBURLLOADER_IMPL_H_
#define MOJO_EXAMPLE_HTML_VIEWER_WEBURLLOADER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "mojo/common/handle_watcher.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"

namespace mojo {
class NetworkService;

namespace examples {

class WebURLLoaderImpl : public blink::WebURLLoader,
                         public URLLoaderClient {
 public:
  explicit WebURLLoaderImpl(NetworkService* network_service);

 private:
  virtual ~WebURLLoaderImpl();

  // blink::WebURLLoader methods:
  virtual void loadSynchronously(
      const blink::WebURLRequest& request, blink::WebURLResponse& response,
      blink::WebURLError& error, blink::WebData& data) OVERRIDE;
  virtual void loadAsynchronously(
      const blink::WebURLRequest&, blink::WebURLLoaderClient* client) OVERRIDE;
  virtual void cancel() OVERRIDE;
  virtual void setDefersLoading(bool defers_loading) OVERRIDE;

  // URLLoaderClient methods:
  virtual void OnReceivedRedirect(URLResponsePtr response,
                                  const String& new_url,
                                  const String& new_method) OVERRIDE;
  virtual void OnReceivedResponse(URLResponsePtr response) OVERRIDE;
  virtual void OnReceivedError(NetworkErrorPtr error) OVERRIDE;
  virtual void OnReceivedEndOfResponseBody() OVERRIDE;

  void ReadMore();
  void WaitToReadMore();
  void OnResponseBodyStreamReady(MojoResult result);

  URLLoaderPtr url_loader_;
  blink::WebURLLoaderClient* client_;
  ScopedDataPipeConsumerHandle response_body_stream_;
  common::HandleWatcher handle_watcher_;

  base::WeakPtrFactory<WebURLLoaderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderImpl);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLE_HTML_VIEWER_WEBURLLOADER_IMPL_H_
