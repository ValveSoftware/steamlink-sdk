// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include "components/devtools_http_handler/devtools_http_handler.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_devtools.h"
#include "headless/lib/browser/headless_screen.h"
#include "ui/aura/env.h"
#include "ui/display/screen.h"

namespace headless {

namespace {

void PlatformInitialize() {
  HeadlessScreen* screen = HeadlessScreen::Create(gfx::Size());
  display::Screen::SetScreenInstance(screen);
}

void PlatformExit() {
}

}  // namespace

HeadlessBrowserMainParts::HeadlessBrowserMainParts(HeadlessBrowserImpl* browser)
    : browser_(browser) {}

HeadlessBrowserMainParts::~HeadlessBrowserMainParts() {}

void HeadlessBrowserMainParts::PreMainMessageLoopRun() {
  browser_context_.reset(new HeadlessBrowserContextImpl(ProtocolHandlerMap(),
                                                        browser_->options()));
  if (browser_->options()->devtools_endpoint.address().IsValid()) {
    devtools_http_handler_ =
        CreateLocalDevToolsHttpHandler(browser_context_.get());
  }
  PlatformInitialize();
}

void HeadlessBrowserMainParts::PostMainMessageLoopRun() {
  browser_context_.reset();
  devtools_http_handler_.reset();
  PlatformExit();
}

HeadlessBrowserContextImpl* HeadlessBrowserMainParts::default_browser_context()
    const {
  return browser_context_.get();
}

}  // namespace headless
