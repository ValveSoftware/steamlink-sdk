// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_content_browser_client.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"

namespace headless {

HeadlessContentBrowserClient::HeadlessContentBrowserClient(
    HeadlessBrowserImpl* browser)
    : browser_(browser) {}

HeadlessContentBrowserClient::~HeadlessContentBrowserClient() {}

content::BrowserMainParts* HeadlessContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams&) {
  std::unique_ptr<HeadlessBrowserMainParts> browser_main_parts =
      base::WrapUnique(new HeadlessBrowserMainParts(browser_));
  browser_->set_browser_main_parts(browser_main_parts.get());
  return browser_main_parts.release();
}

}  // namespace headless
