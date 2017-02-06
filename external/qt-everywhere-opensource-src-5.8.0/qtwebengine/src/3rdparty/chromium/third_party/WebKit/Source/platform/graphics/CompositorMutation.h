// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutation_h
#define CompositorMutation_h

#include "platform/graphics/CompositorMutableProperties.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "wtf/HashMap.h"
#include <memory>

namespace blink {

class CompositorMutation {
public:
    void setOpacity(float opacity)
    {
        m_mutatedFlags |= CompositorMutableProperty::kOpacity;
        m_opacity = opacity;
    }
    void setScrollLeft(float scrollLeft)
    {
        m_mutatedFlags |= CompositorMutableProperty::kScrollLeft;
        m_scrollLeft = scrollLeft;
    }
    void setScrollTop(float scrollTop)
    {
        m_mutatedFlags |= CompositorMutableProperty::kScrollTop;
        m_scrollTop = scrollTop;
    }
    void setTransform(const SkMatrix44& transform)
    {
        m_mutatedFlags |= CompositorMutableProperty::kTransform;
        m_transform = transform;
    }

    bool isOpacityMutated() const { return m_mutatedFlags & CompositorMutableProperty::kOpacity; }
    bool isScrollLeftMutated() const { return m_mutatedFlags & CompositorMutableProperty::kScrollLeft; }
    bool isScrollTopMutated() const { return m_mutatedFlags & CompositorMutableProperty::kScrollTop; }
    bool isTransformMutated() const { return m_mutatedFlags & CompositorMutableProperty::kTransform; }

    float opacity() const { return m_opacity; }
    float scrollLeft() const { return m_scrollLeft; }
    float scrollTop() const { return m_scrollTop; }
    SkMatrix44 transform() const { return m_transform; }

private:
    uint32_t m_mutatedFlags = 0;
    float m_opacity = 0;
    float m_scrollLeft = 0;
    float m_scrollTop = 0;
    SkMatrix44 m_transform;
};

struct CompositorMutations {
    HashMap<uint64_t, std::unique_ptr<CompositorMutation>> map;
};

} // namespace blink

#endif // CompositorMutation_h
