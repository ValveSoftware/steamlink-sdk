// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to handle differences in Mach message IDs and structures
// that occur between different OS versions. The Mach messages that the sandbox
// is interested in are decoded using information derived from the open-source
// libraries, i.e. <http://www.opensource.apple.com/source/launchd/>. While
// these messages definitions are open source, they are not considered part of
// the stable OS API, and so differences do exist between OS versions.

#ifndef SANDBOX_MAC_OS_COMPATIBILITY_H_
#define SANDBOX_MAC_OS_COMPATIBILITY_H_

#include <mach/mach.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "sandbox/mac/message_server.h"

namespace sandbox {

class OSCompatibility {
 public:
  // Creates an OSCompatibility instance for the current OS X version.
  static std::unique_ptr<OSCompatibility> CreateForPlatform();

  virtual ~OSCompatibility();

  // Gets the message and subsystem ID of an IPC message.
  virtual uint64_t GetMessageSubsystem(const IPCMessage message) = 0;
  virtual uint64_t GetMessageID(const IPCMessage message) = 0;

  // Returns true if the message is a Launchd look up request.
  virtual bool IsServiceLookUpRequest(const IPCMessage message) = 0;

  // Returns true if the message is a Launchd vproc swap_integer request.
  virtual bool IsVprocSwapInteger(const IPCMessage message) = 0;

  // Returns true if the message is an XPC domain management message.
  virtual bool IsXPCDomainManagement(const IPCMessage message) = 0;

  // A function to take a look_up2 message and return the string service name
  // that was requested via the message.
  virtual std::string GetServiceLookupName(const IPCMessage message) = 0;

  // A function to formulate a reply to a look_up2 message, given the reply
  // message and the port to return as the service.
  virtual void WriteServiceLookUpReply(IPCMessage reply,
                                       mach_port_t service_port) = 0;

  // A function to take a swap_integer message and return true if the message
  // is only getting the value of a key, neither setting it directly, nor
  // swapping two keys.
  virtual bool IsSwapIntegerReadOnly(const IPCMessage message) = 0;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_OS_COMPATIBILITY_H_
