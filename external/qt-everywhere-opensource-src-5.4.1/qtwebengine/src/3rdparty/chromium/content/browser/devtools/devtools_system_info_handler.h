// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SYSTEM_INFO_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SYSTEM_INFO_HANDLER_H_

#include "content/browser/devtools/devtools_protocol.h"

namespace content {

// This class provides information to DevTools about the system it's running on.
class DevToolsSystemInfoHandler
    : public DevToolsProtocol::Handler {
 public:
  DevToolsSystemInfoHandler();
  virtual ~DevToolsSystemInfoHandler();

 private:
  scoped_refptr<DevToolsProtocol::Response> OnGetInfo(
      scoped_refptr<DevToolsProtocol::Command> command);

  DISALLOW_COPY_AND_ASSIGN(DevToolsSystemInfoHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SYSTEM_INFO_HANDLER_H_
