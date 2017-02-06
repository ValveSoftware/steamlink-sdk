// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorTransformKeyframe.h"

#include <memory>

namespace blink {

CompositorTransformKeyframe::CompositorTransformKeyframe(double time, std::unique_ptr<CompositorTransformOperations> value)
    : m_time(time)
    , m_value(std::move(value))
{
}

CompositorTransformKeyframe::~CompositorTransformKeyframe()
{
    m_value.reset();
}

double CompositorTransformKeyframe::time() const
{
    return m_time;
}

const CompositorTransformOperations& CompositorTransformKeyframe::value() const
{
    return *m_value.get();
}

} // namespace blink
