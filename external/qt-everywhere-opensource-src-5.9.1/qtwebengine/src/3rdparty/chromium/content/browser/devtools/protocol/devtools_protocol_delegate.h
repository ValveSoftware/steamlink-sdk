// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_DELEGATE_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_DELEGATE_H_

#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT DevToolsProtocolDelegate {
public:
  virtual ~DevToolsProtocolDelegate() { }
  virtual void SendProtocolResponse(int session_id,
                                    const std::string& message) = 0;
  virtual void SendProtocolNotification(const std::string& message) = 0;
};

} // content

#endif // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_DELEGATE_H_
