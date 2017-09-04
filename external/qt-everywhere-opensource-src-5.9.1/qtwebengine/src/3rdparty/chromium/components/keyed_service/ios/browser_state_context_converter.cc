// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/ios/browser_state_context_converter.h"

namespace {

// Global BrowserStateContextConverter* instance, may be null.
BrowserStateContextConverter* g_browser_state_context_converter = nullptr;

}  // namespace

// static
void BrowserStateContextConverter::SetInstance(
    BrowserStateContextConverter* instance) {
  g_browser_state_context_converter = instance;
}

BrowserStateContextConverter* BrowserStateContextConverter::GetInstance() {
  return g_browser_state_context_converter;
}

BrowserStateContextConverter::BrowserStateContextConverter() {
}

BrowserStateContextConverter::~BrowserStateContextConverter() {
}
