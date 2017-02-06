// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/devtools_protocol_handler.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"

namespace content {

class RenderFrameHost;

namespace devtools {
namespace storage {

class StorageHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  StorageHandler();
  ~StorageHandler();

  void SetRenderFrameHost(RenderFrameHost* host);

  Response ClearDataForOrigin(
      const std::string& origin,
      const std::string& storage_types);

 private:
  RenderFrameHost* host_;

  DISALLOW_COPY_AND_ASSIGN(StorageHandler);
};

}  // namespace storage
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
