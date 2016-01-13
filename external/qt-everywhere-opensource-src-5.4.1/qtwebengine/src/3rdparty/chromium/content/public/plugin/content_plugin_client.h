// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_PLUGIN_CONTENT_PLUGIN_CLIENT_H_
#define CONTENT_PUBLIC_PLUGIN_CONTENT_PLUGIN_CLIENT_H_

#include "base/strings/string16.h"
#include "content/public/common/content_client.h"

namespace content {

// Embedder API for participating in plugin logic.
class CONTENT_EXPORT ContentPluginClient {
 public:
  virtual ~ContentPluginClient() {}

  // Called to perform any per-process initialization necessary before
  // the sandbox is initialized for the plugin process.
  virtual void PreSandboxInitialization() {}

  // Notifies that a plugin process has started.
  virtual void PluginProcessStarted(const base::string16& plugin_name) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_PLUGIN_CONTENT_PLUGIN_CLIENT_H_
