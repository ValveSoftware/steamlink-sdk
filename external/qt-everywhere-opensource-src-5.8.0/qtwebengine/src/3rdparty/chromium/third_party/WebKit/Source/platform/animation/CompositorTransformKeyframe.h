// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorTransformKeyframe_h
#define CompositorTransformKeyframe_h

#include "platform/PlatformExport.h"
#include "platform/animation/CompositorTransformOperations.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class PLATFORM_EXPORT CompositorTransformKeyframe {
    WTF_MAKE_NONCOPYABLE(CompositorTransformKeyframe);
public:
    CompositorTransformKeyframe(double time, std::unique_ptr<CompositorTransformOperations> value);
    ~CompositorTransformKeyframe();

    double time() const;
    const CompositorTransformOperations& value() const;

private:
    double m_time;
    std::unique_ptr<CompositorTransformOperations> m_value;
};

} // namespace blink

#endif // CompositorTransformKeyframe_h
