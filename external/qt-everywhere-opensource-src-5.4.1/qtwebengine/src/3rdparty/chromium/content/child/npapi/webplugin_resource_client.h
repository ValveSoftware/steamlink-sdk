// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NPAPI_WEBPLUGIN_RESOURCE_CLIENT_H_
#define CONTENT_CHILD_NPAPI_WEBPLUGIN_RESOURCE_CLIENT_H_

#include <string>

#include "base/basictypes.h"

class GURL;

namespace content {

// Simpler version of ResourceHandleClient that lends itself to proxying.
class WebPluginResourceClient {
 public:
  virtual ~WebPluginResourceClient() {}

  virtual void WillSendRequest(const GURL& url, int http_status_code) = 0;
  // The request_is_seekable parameter indicates whether byte range requests
  // can be issued for the underlying stream.
  virtual void DidReceiveResponse(const std::string& mime_type,
                                  const std::string& headers,
                                  uint32 expected_length,
                                  uint32 last_modified,
                                  bool request_is_seekable) = 0;
  virtual void DidReceiveData(const char* buffer, int length,
                              int data_offset) = 0;
  // The resource ids passed here ensures that data for range requests
  // is cleared. This applies for seekable streams.
  virtual void DidFinishLoading(unsigned long resource_id) = 0;
  virtual void DidFail(unsigned long resource_id) = 0;
  virtual bool IsMultiByteResponseExpected() = 0;
  virtual int ResourceId() = 0;
  // Tells this object that it will get responses from multiple resources.
  // This is necessary since the plugin process uses a single instance of
  // PluginStreamUrl object for multiple range requests.
  virtual void AddRangeRequestResourceId(unsigned long resource_id) { }
};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_WEBPLUGIN_RESOURCE_CLIENT_H_
