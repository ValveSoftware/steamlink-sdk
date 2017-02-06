// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFilterKeyframe.h"

#include <memory>

namespace blink {

CompositorFilterKeyframe::CompositorFilterKeyframe(double time, std::unique_ptr<CompositorFilterOperations> value)
    : m_time(time)
    , m_value(std::move(value))
{
}

CompositorFilterKeyframe::~CompositorFilterKeyframe()
{
    m_value.reset();
}

} // namespace blink
