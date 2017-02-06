// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/os_compatibility.h"

#include <servers/bootstrap.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/ptr_util.h"
#include "sandbox/mac/xpc.h"

namespace sandbox {

namespace {

#pragma pack(push, 4)
// Verified from launchd-329.3.3 (10.6.8).
// look_up2_reply_10_7 is the same as the 10_6 version.
struct look_up2_reply_10_7 {
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

// Verified from:
//   launchd-329.3.3 (10.6.8)
//   launchd-392.39 (10.7.5)
//   launchd-442.26.2 (10.8.5)
//   launchd-842.1.4 (10.9.0)
typedef int vproc_gsk_t;  // Defined as an enum in liblaunch/vproc_priv.h.
struct swap_integer_request_10_7 {
  mach_msg_header_t Head;
  NDR_record_t NDR;
  vproc_gsk_t inkey;
  vproc_gsk_t outkey;
  int64_t inval;
};
#pragma pack(pop)

// TODO(rsesek): Libc provides strnlen() starting in 10.7.
size_t strnlen(const char* str, size_t maxlen) {
  size_t len = 0;
  for (; len < maxlen; ++len, ++str) {
    if (*str == '\0')
      break;
  }
  return len;
}

class OSCompatibility_10_7 : public OSCompatibility {
 public:
  OSCompatibility_10_7() {}
  ~OSCompatibility_10_7() override {}

  uint64_t GetMessageSubsystem(const IPCMessage message) override {
    return (message.mach->msgh_id / 100) * 100;
  }

  uint64_t GetMessageID(const IPCMessage message) override {
    return message.mach->msgh_id;
  }

  bool IsServiceLookUpRequest(const IPCMessage message) override {
    return GetMessageID(message) == 404;
  }

  bool IsVprocSwapInteger(const IPCMessage message) override {
    return GetMessageID(message) == 416;
  }

  bool IsXPCDomainManagement(const IPCMessage message) override {
    return false;
  }

  std::string GetServiceLookupName(const IPCMessage message) override {
    return GetRequestName<look_up2_request_10_7>(message);
  }

  void WriteServiceLookUpReply(IPCMessage message,
                               mach_port_t service_port) override {
    auto reply = reinterpret_cast<look_up2_reply_10_7*>(message.mach);
    reply->Head.msgh_size = sizeof(*reply);
    reply->Head.msgh_bits =
        MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
        MACH_MSGH_BITS_COMPLEX;
    reply->msgh_body.msgh_descriptor_count = 1;
    reply->service_port.name = service_port;
    reply->service_port.disposition = MACH_MSG_TYPE_COPY_SEND;
    reply->service_port.type = MACH_MSG_PORT_DESCRIPTOR;
  }

  bool IsSwapIntegerReadOnly(const IPCMessage message) override {
    auto request =
        reinterpret_cast<const swap_integer_request_10_7*>(message.mach);
    return request->inkey == 0 && request->inval == 0 && request->outkey != 0;
  }

 protected:
  // The 10.6 and 10.7 implementations are the same except for struct offsets,
  // so provide this templatized helper.
  template <typename R>
  static std::string GetRequestName(const IPCMessage message) {
    mach_msg_header_t* header = message.mach;
    DCHECK_EQ(sizeof(R), header->msgh_size);
    const R* request = reinterpret_cast<const R*>(header);
    // Make sure the name is properly NUL-terminated.
    const size_t name_length =
        strnlen(request->servicename, BOOTSTRAP_MAX_NAME_LEN);
    std::string name = std::string(request->servicename, name_length);
    return name;
  }
};

class OSCompatibility_10_10 : public OSCompatibility {
 public:
  OSCompatibility_10_10() {}
  ~OSCompatibility_10_10() override {}

  uint64_t GetMessageSubsystem(const IPCMessage message) override {
    return xpc_dictionary_get_uint64(message.xpc, "subsystem");
  }

  uint64_t GetMessageID(const IPCMessage message) override {
    return xpc_dictionary_get_uint64(message.xpc, "routine");
  }

  bool IsServiceLookUpRequest(const IPCMessage message) override {
    uint64_t subsystem = GetMessageSubsystem(message);
    uint64_t id = GetMessageID(message);
    // Lookup requests in XPC can either go through the Mach bootstrap subsytem
    // (5) from bootstrap_look_up(), or the XPC domain subsystem (3) for
    // xpc_connection_create(). Both use the same message format.
    return (subsystem == 5 && id == 207) || (subsystem == 3 && id == 804);
  }

  bool IsVprocSwapInteger(const IPCMessage message) override {
    return GetMessageSubsystem(message) == 6 && GetMessageID(message) == 301;
  }

  bool IsXPCDomainManagement(const IPCMessage message) override {
    return GetMessageSubsystem(message) == 3;
  }

  std::string GetServiceLookupName(const IPCMessage message) override {
    const char* name = xpc_dictionary_get_string(message.xpc, "name");
    const size_t name_length = strnlen(name, BOOTSTRAP_MAX_NAME_LEN);
    return std::string(name, name_length);
  }

  void WriteServiceLookUpReply(IPCMessage message,
                               mach_port_t service_port) override {
    xpc_dictionary_set_mach_send(message.xpc, "port", service_port);
  }

  bool IsSwapIntegerReadOnly(const IPCMessage message) override {
    return xpc_dictionary_get_bool(message.xpc, "set") == false &&
           xpc_dictionary_get_uint64(message.xpc, "ingsk") == 0 &&
           xpc_dictionary_get_int64(message.xpc, "in") == 0;
  }
};

}  // namespace

// static
std::unique_ptr<OSCompatibility> OSCompatibility::CreateForPlatform() {
  if (base::mac::IsOSMavericks())
    return base::WrapUnique(new OSCompatibility_10_7());
  else
    return base::WrapUnique(new OSCompatibility_10_10());
}

OSCompatibility::~OSCompatibility() {}

}  // namespace sandbox
