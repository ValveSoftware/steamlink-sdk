// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/plugin_string_stream.h"

#include "url/gurl.h"

namespace content {

PluginStringStream::PluginStringStream(
    PluginInstance* instance,
    const GURL& url,
    bool notify_needed,
    void* notify_data)
    : PluginStream(instance, url.spec().c_str(), notify_needed, notify_data) {
}

PluginStringStream::~PluginStringStream() {
}

void PluginStringStream::SendToPlugin(const std::string &data,
                                      const std::string &mime_type) {
  // Protect the stream against it being destroyed or the whole plugin instance
  // being destroyed within the plugin stream callbacks.
  scoped_refptr<PluginStringStream> protect(this);

  int length = static_cast<int>(data.length());
  if (Open(mime_type, std::string(), length, 0, false)) {
    // TODO - check if it was not fully sent, and figure out a backup plan.
    int written = Write(data.c_str(), length, 0);
    NPReason reason = written == length ? NPRES_DONE : NPRES_NETWORK_ERR;
    Close(reason);
  }
}

}  // namespace content
