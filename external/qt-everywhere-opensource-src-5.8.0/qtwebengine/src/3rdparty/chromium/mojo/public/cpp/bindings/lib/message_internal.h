// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_INTERNAL_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"

namespace mojo {
namespace internal {

#pragma pack(push, 1)

struct MessageHeader : internal::StructHeader {
  // Interface ID for identifying multiple interfaces running on the same
  // message pipe.
  uint32_t interface_id;
  // Message name, which is scoped to the interface that the message belongs to.
  uint32_t name;
  // 0 or either of the enum values defined above.
  uint32_t flags;
  // Unused padding to make the struct size a multiple of 8 bytes.
  uint32_t padding;
};
static_assert(sizeof(MessageHeader) == 24, "Bad sizeof(MessageHeader)");

struct MessageHeaderWithRequestID : MessageHeader {
  // Only used if either kFlagExpectsResponse or kFlagIsResponse is set in
  // order to match responses with corresponding requests.
  uint64_t request_id;
};
static_assert(sizeof(MessageHeaderWithRequestID) == 32,
              "Bad sizeof(MessageHeaderWithRequestID)");

#pragma pack(pop)

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_INTERNAL_H_
