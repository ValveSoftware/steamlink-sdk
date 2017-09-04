// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_COMPOSITOR_PROTO_STATE_H_
#define CC_BLIMP_COMPOSITOR_PROTO_STATE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "cc/base/cc_export.h"

namespace cc {
namespace proto {
class CompositorMessage;
}

class SwapPromise;

class CC_EXPORT CompositorProtoState {
 public:
  CompositorProtoState();
  ~CompositorProtoState();

  // The SwapPromises associated with this frame update.
  std::vector<std::unique_ptr<SwapPromise>> swap_promises;

  // Serialized CompositorState.
  std::unique_ptr<proto::CompositorMessage> compositor_message;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorProtoState);
};

}  // namespace cc

#endif  // CC_BLIMP_COMPOSITOR_PROTO_STATE_H_
