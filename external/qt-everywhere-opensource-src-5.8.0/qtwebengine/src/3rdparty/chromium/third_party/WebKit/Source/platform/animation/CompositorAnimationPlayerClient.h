// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationPlayerClient_h
#define CompositorAnimationPlayerClient_h

#include "public/platform/WebCommon.h"

namespace blink {

class CompositorAnimationPlayer;

// A client for compositor representation of AnimationPlayer.
class BLINK_PLATFORM_EXPORT CompositorAnimationPlayerClient {
public:
    virtual ~CompositorAnimationPlayerClient();

    virtual CompositorAnimationPlayer* compositorPlayer() const = 0;
};

} // namespace blink

#endif // CompositorAnimationPlayerClient_h
