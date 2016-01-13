// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PLUGIN_STRING_STREAM_H_
#define CONTENT_CHILD_PLUGIN_STRING_STREAM_H_

#include "content/child/npapi/plugin_stream.h"

class GURL;

namespace content {

class PluginInstance;

// An NPAPI stream from a string.
class PluginStringStream : public PluginStream {
 public:
  // Create a new stream for sending to the plugin.
  // If notify_needed, will notify the plugin after the data has
  // all been sent.
  PluginStringStream(PluginInstance* instance,
                     const GURL& url,
                     bool notify_needed,
                     void* notify_data);

  // Initiates the sending of data to the plugin.
  void SendToPlugin(const std::string& data,
                    const std::string& mime_type);

 private:
  virtual ~PluginStringStream();

  DISALLOW_COPY_AND_ASSIGN(PluginStringStream);
};


}  // namespace content

#endif // CONTENT_CHILD_PLUGIN_STRING_STREAM_H_
