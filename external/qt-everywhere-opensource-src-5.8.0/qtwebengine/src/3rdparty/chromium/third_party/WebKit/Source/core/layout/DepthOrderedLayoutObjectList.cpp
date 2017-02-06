// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/DepthOrderedLayoutObjectList.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutObject.h"
#include <algorithm>

namespace blink {

void DepthOrderedLayoutObjectList::add(LayoutObject& object)
{
    ASSERT(!object.frameView()->isInPerformLayout());
    m_objects.add(&object);
    m_orderedObjects.clear();
}

void DepthOrderedLayoutObjectList::remove(LayoutObject& object)
{
    auto it = m_objects.find(&object);
    if (it == m_objects.end())
        return;
    ASSERT(!object.frameView()->isInPerformLayout());
    m_objects.remove(it);
    m_orderedObjects.clear();
}

void DepthOrderedLayoutObjectList::clear()
{
    m_objects.clear();
    m_orderedObjects.clear();
}

unsigned DepthOrderedLayoutObjectList::LayoutObjectWithDepth::determineDepth(LayoutObject* object)
{
    unsigned depth = 1;
    for (LayoutObject* parent = object->parent(); parent; parent = parent->parent())
        ++depth;
    return depth;
}

const Vector<DepthOrderedLayoutObjectList::LayoutObjectWithDepth>& DepthOrderedLayoutObjectList::ordered()
{
    if (m_objects.isEmpty() || !m_orderedObjects.isEmpty())
        return m_orderedObjects;

    copyToVector(m_objects, m_orderedObjects);
    std::sort(m_orderedObjects.begin(), m_orderedObjects.end());
    return m_orderedObjects;
}

} // namespace blink
