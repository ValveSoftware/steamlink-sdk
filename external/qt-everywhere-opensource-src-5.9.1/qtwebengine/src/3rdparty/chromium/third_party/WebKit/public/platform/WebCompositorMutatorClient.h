// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositorMutatorClient_h
#define WebCompositorMutatorClient_h

#include "WebCommon.h"

#include "cc/trees/layer_tree_mutator.h"

namespace blink {

// This is used by the compositor to invoke compositor worker callbacks.
class BLINK_PLATFORM_EXPORT WebCompositorMutatorClient
    : public cc::LayerTreeMutator {
 public:
  virtual ~WebCompositorMutatorClient() {}
};

}  // namespace blink

#endif  // WebCompositorMutatorClient_h
