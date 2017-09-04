// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_window_client.h"


namespace extensions {

namespace {

AppWindowClient* g_client = NULL;

}  // namespace

AppWindowClient* AppWindowClient::Get() {
  return g_client;
}

void AppWindowClient::Set(AppWindowClient* client) {
  // This can happen in unit tests, where the utility thread runs in-process.
  if (g_client)
    return;

  g_client = client;
}

}  // namespace extensions
