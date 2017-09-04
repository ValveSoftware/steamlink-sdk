// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_devtools.h"
#include "headless/lib/browser/headless_screen.h"
#include "ui/aura/env.h"
#include "ui/display/screen.h"

namespace headless {

namespace {

void PlatformInitialize(const gfx::Size& screen_size) {
  HeadlessScreen* screen = HeadlessScreen::Create(screen_size);
  display::Screen::SetScreenInstance(screen);
}

void PlatformExit() {
}

}  // namespace

HeadlessBrowserMainParts::HeadlessBrowserMainParts(HeadlessBrowserImpl* browser)
    : browser_(browser)
    , devtools_http_handler_started_(false) {}

HeadlessBrowserMainParts::~HeadlessBrowserMainParts() {}

void HeadlessBrowserMainParts::PreMainMessageLoopRun() {
  if (browser_->options()->devtools_endpoint.address().IsValid()) {
    StartLocalDevToolsHttpHandler(browser_->options());
    devtools_http_handler_started_ = true;
  }
  PlatformInitialize(browser_->options()->window_size);
}

void HeadlessBrowserMainParts::PostMainMessageLoopRun() {
  if (devtools_http_handler_started_) {
    StopLocalDevToolsHttpHandler();
    devtools_http_handler_started_ = false;
  }
  PlatformExit();
}

}  // namespace headless
