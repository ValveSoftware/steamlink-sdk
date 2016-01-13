// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_TETHERING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_TETHERING_HANDLER_H_

#include <map>

#include "content/browser/devtools/devtools_protocol.h"

namespace content {

class DevToolsHttpHandlerDelegate;

// This class implements reversed tethering handler.
class TetheringHandler : public DevToolsProtocol::Handler {
 public:
  static const char kDomain[];

  TetheringHandler(DevToolsHttpHandlerDelegate* delegate);
  virtual ~TetheringHandler();

  void Accepted(int port, const std::string& name);

 private:
  class BoundSocket;
  scoped_refptr<DevToolsProtocol::Response> OnBind(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> OnUnbind(
      scoped_refptr<DevToolsProtocol::Command>  command);

  typedef std::map<int, BoundSocket*> BoundSockets;
  BoundSockets bound_sockets_;
  DevToolsHttpHandlerDelegate* delegate_;
  DISALLOW_COPY_AND_ASSIGN(TetheringHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_TETHERING_HANDLER_H_
