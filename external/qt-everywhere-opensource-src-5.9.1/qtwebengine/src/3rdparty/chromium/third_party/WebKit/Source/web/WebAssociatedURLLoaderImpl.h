// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAssociatedURLLoaderImpl_h
#define WebAssociatedURLLoaderImpl_h

#include "platform/heap/Handle.h"
#include "public/web/WebAssociatedURLLoader.h"
#include "public/web/WebAssociatedURLLoaderOptions.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class DocumentThreadableLoader;
class WebAssociatedURLLoaderClient;
class WebLocalFrameImpl;

// This class is used to implement WebFrame::createAssociatedURLLoader.
class WebAssociatedURLLoaderImpl final : public WebAssociatedURLLoader {
  WTF_MAKE_NONCOPYABLE(WebAssociatedURLLoaderImpl);

 public:
  WebAssociatedURLLoaderImpl(WebLocalFrameImpl*,
                             const WebAssociatedURLLoaderOptions&);
  ~WebAssociatedURLLoaderImpl();

  void loadAsynchronously(const WebURLRequest&,
                          WebAssociatedURLLoaderClient*) override;
  void cancel() override;
  void setDefersLoading(bool) override;
  void setLoadingTaskRunner(blink::WebTaskRunner*) override;

  // Called by |m_observer| to handle destruction of the Document associated
  // with the frame given to the constructor.
  void documentDestroyed();

  // Called by ClientAdapter to handle completion of loading.
  void clientAdapterDone();

 private:
  class ClientAdapter;
  class Observer;

  void cancelLoader();
  void disposeObserver();

  WebAssociatedURLLoaderClient* releaseClient() {
    WebAssociatedURLLoaderClient* client = m_client;
    m_client = nullptr;
    return client;
  }

  WebAssociatedURLLoaderClient* m_client;
  WebAssociatedURLLoaderOptions m_options;

  // An adapter which converts the DocumentThreadableLoaderClient method
  // calls into the WebURLLoaderClient method calls.
  std::unique_ptr<ClientAdapter> m_clientAdapter;
  Persistent<DocumentThreadableLoader> m_loader;

  // A ContextLifecycleObserver for cancelling |m_loader| when the Document
  // is detached.
  Persistent<Observer> m_observer;
};

}  // namespace blink

#endif
