// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/examples/example_base.h"

namespace content {
class BrowserContext;
}

namespace views {
class WebView;

namespace examples {

class WebViewExample : public ExampleBase {
 public:
  explicit WebViewExample(content::BrowserContext* browser_context);
  virtual ~WebViewExample();

  // ExampleBase:
  virtual void CreateExampleView(View* container) OVERRIDE;

 private:
  WebView* webview_;
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(WebViewExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_
