// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "content/public/browser/browser_main_parts.h"
#include "headless/public/headless_browser.h"

namespace devtools_http_handler {
class DevToolsHttpHandler;
}

namespace headless {

class HeadlessBrowserContextImpl;
class HeadlessBrowserImpl;

class HeadlessBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit HeadlessBrowserMainParts(HeadlessBrowserImpl* browser);
  ~HeadlessBrowserMainParts() override;

  // content::BrowserMainParts implementation:
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;

  HeadlessBrowserContextImpl* default_browser_context() const;

 private:
  HeadlessBrowserImpl* browser_;  // Not owned.
  std::unique_ptr<HeadlessBrowserContextImpl> browser_context_;
  std::unique_ptr<devtools_http_handler::DevToolsHttpHandler>
      devtools_http_handler_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserMainParts);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_
