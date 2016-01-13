// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/os_compatibility.h"

#include <servers/bootstrap.h>
#include <unistd.h>

#include "base/mac/mac_util.h"

namespace sandbox {

namespace {

// Verified from launchd-329.3.3 (10.6.8).
struct look_up2_request_10_6 {
  mach_msg_header_t Head;
  NDR_record_t NDR;
  name_t servicename;
  pid_t targetpid;
  uint64_t flags;
};

struct look_up2_reply_10_6 {
  mach_msg_header_t Head;
  mach_msg_body_t msgh_body;
  mach_msg_port_descriptor_t service_port;
};

// Verified from:
//   launchd-392.39 (10.7.5)
//   launchd-442.26.2 (10.8.5)
//   launchd-842.1.4 (10.9.0)
struct look_up2_request_10_7 {
  mach_msg_header_t Head;
  NDR_record_t NDR;
  name_t servicename;
  pid_t targetpid;
  uuid_t instanceid;
  uint64_t flags;
};

// look_up2_reply_10_7 is the same as the 10_6 version.

// Verified from:
//   launchd-329.3.3 (10.6.8)
//   launchd-392.39 (10.7.5)
//   launchd-442.26.2 (10.8.5)
//   launchd-842.1.4 (10.9.0)
typedef int vproc_gsk_t;  // Defined as an enum in liblaunch/vproc_priv.h.
struct swap_integer_request_10_6 {
  mach_msg_header_t Head;
  NDR_record_t NDR;
  vproc_gsk_t inkey;
  vproc_gsk_t outkey;
  int64_t inval;
};

// TODO(rsesek): Libc provides strnlen() starting in 10.7.
size_t strnlen(const char* str, size_t maxlen) {
  size_t len = 0;
  for (; len < maxlen; ++len, ++str) {
    if (*str == '\0')
      break;
  }
  return len;
}

template <typename R>
std::string LaunchdLookUp2GetRequestName(const mach_msg_header_t* header) {
  DCHECK_EQ(sizeof(R), header->msgh_size);
  const R* request = reinterpret_cast<const R*>(header);
  // Make sure the name is properly NUL-terminated.
  const size_t name_length =
      strnlen(request->servicename, BOOTSTRAP_MAX_NAME_LEN);
  std::string name = std::string(request->servicename, name_length);
  return name;
}

template <typename R>
void LaunchdLookUp2FillReply(mach_msg_header_t* header, mach_port_t port) {
  R* reply = reinterpret_cast<R*>(header);
  reply->Head.msgh_size = sizeof(R);
  reply->Head.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
      MACH_MSGH_BITS_COMPLEX;
  reply->msgh_body.msgh_descriptor_count = 1;
  reply->service_port.name = port;
  reply->service_port.disposition = MACH_MSG_TYPE_COPY_SEND;
  reply->service_port.type = MACH_MSG_PORT_DESCRIPTOR;
}

template <typename R>
bool LaunchdSwapIntegerIsGetOnly(const mach_msg_header_t* header) {
  const R* request = reinterpret_cast<const R*>(header);
  return request->inkey == 0 && request->inval == 0 && request->outkey != 0;
}

}  // namespace

const LaunchdCompatibilityShim GetLaunchdCompatibilityShim() {
  LaunchdCompatibilityShim shim = {
    .msg_id_look_up2 = 404,
    .msg_id_swap_integer = 416,
    .look_up2_fill_reply = &LaunchdLookUp2FillReply<look_up2_reply_10_6>,
    .swap_integer_is_get_only =
        &LaunchdSwapIntegerIsGetOnly<swap_integer_request_10_6>,
  };

  if (base::mac::IsOSSnowLeopard()) {
    shim.look_up2_get_request_name =
        &LaunchdLookUp2GetRequestName<look_up2_request_10_6>;
  } else if (base::mac::IsOSLionOrLater() &&
             !base::mac::IsOSYosemiteOrLater()) {
    shim.look_up2_get_request_name =
        &LaunchdLookUp2GetRequestName<look_up2_request_10_7>;
  } else {
    DLOG(ERROR) << "Unknown OS, using launchd compatibility shim from 10.7.";
    shim.look_up2_get_request_name =
        &LaunchdLookUp2GetRequestName<look_up2_request_10_7>;
  }

  return shim;
}

}  // namespace sandbox
