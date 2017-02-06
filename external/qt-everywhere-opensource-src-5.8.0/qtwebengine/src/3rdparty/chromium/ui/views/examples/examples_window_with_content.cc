// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_window_with_content.h"

#include <utility>

#include "content/public/browser/browser_context.h"
#include "ui/views/examples/webview_example.h"

namespace views {
namespace examples {

void ShowExamplesWindowWithContent(Operation operation,
                                   content::BrowserContext* browser_context,
                                   gfx::NativeWindow window_context) {
  std::unique_ptr<ScopedVector<ExampleBase>> extra_examples(
      new ScopedVector<ExampleBase>);
  extra_examples->push_back(new WebViewExample(browser_context));
  ShowExamplesWindow(operation, window_context, std::move(extra_examples));
}

}  // namespace examples
}  // namespace views
