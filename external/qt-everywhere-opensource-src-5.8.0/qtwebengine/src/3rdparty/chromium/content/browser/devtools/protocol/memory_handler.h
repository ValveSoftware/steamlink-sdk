// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEMORY_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEMORY_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"

namespace content {
namespace devtools {
namespace memory {

class MemoryHandler {
 public:
  MemoryHandler();
  ~MemoryHandler();

  typedef DevToolsProtocolClient::Response Response;
  Response SetPressureNotificationsSuppressed(bool suppressed);
  Response SimulatePressureNotification(const std::string& level);

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryHandler);
};

}  // namespace memory
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEMORY_HANDLER_H_
