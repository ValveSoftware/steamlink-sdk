// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_HTML_VIEWER_HTML_DOCUMENT_VIEW_H_
#define MOJO_EXAMPLES_HTML_VIEWER_HTML_DOCUMENT_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebViewClient.h"

namespace mojo {

namespace view_manager {
class Node;
class ViewManager;
class View;
}

namespace examples {

// A view for a single HTML document.
class HTMLDocumentView : public blink::WebViewClient,
                         public blink::WebFrameClient {
 public:
  explicit HTMLDocumentView(view_manager::ViewManager* view_manager);
  virtual ~HTMLDocumentView();

  void AttachToNode(view_manager::Node* node);

  void Load(URLResponsePtr response,
            ScopedDataPipeConsumerHandle response_body_stream);

 private:
  // WebWidgetClient methods:
  virtual void didInvalidateRect(const blink::WebRect& rect);
  virtual bool allowsBrokenNullLayerTreeView() const;

  // WebFrameClient methods:
  virtual void didAddMessageToConsole(
      const blink::WebConsoleMessage& message,
      const blink::WebString& source_name,
      unsigned source_line,
      const blink::WebString& stack_trace);

  void Repaint();

  view_manager::ViewManager* view_manager_;
  view_manager::View* view_;
  blink::WebView* web_view_;
  bool repaint_pending_;

  base::WeakPtrFactory<HTMLDocumentView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLDocumentView);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_HTML_VIEWER_HTML_DOCUMENT_VIEW_H_
