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

#include <string>

namespace sandbox {

typedef std::string (*LookUp2GetRequestName)(const mach_msg_header_t*);
typedef void (*LookUp2FillReply)(mach_msg_header_t*, mach_port_t service_port);

typedef bool (*SwapIntegerIsGetOnly)(const mach_msg_header_t*);

struct LaunchdCompatibilityShim {
  // The msgh_id for look_up2.
  mach_msg_id_t msg_id_look_up2;

  // The msgh_id for swap_integer.
  mach_msg_id_t msg_id_swap_integer;

  // A function to take a look_up2 message and return the string service name
  // that was requested via the message.
  LookUp2GetRequestName look_up2_get_request_name;

  // A function to formulate a reply to a look_up2 message, given the reply
  // message and the port to return as the service.
  LookUp2FillReply look_up2_fill_reply;

  // A function to take a swap_integer message and return true if the message
  // is only getting the value of a key, neither setting it directly, nor
  // swapping two keys.
  SwapIntegerIsGetOnly swap_integer_is_get_only;
};

// Gets the compatibility shim for the launchd job subsystem.
const LaunchdCompatibilityShim GetLaunchdCompatibilityShim();

}  // namespace sandbox

#endif  // SANDBOX_MAC_OS_COMPATIBILITY_H_
