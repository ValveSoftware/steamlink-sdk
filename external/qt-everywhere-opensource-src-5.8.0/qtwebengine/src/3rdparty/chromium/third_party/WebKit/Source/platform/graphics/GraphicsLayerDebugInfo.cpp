/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "platform/graphics/GraphicsLayerDebugInfo.h"

#include "base/trace_event/trace_event_argument.h"

namespace blink {

GraphicsLayerDebugInfo::GraphicsLayerDebugInfo()
    : m_compositingReasons(CompositingReasonNone)
    , m_squashingDisallowedReasons(SquashingDisallowedReasonsNone)
    , m_ownerNodeId(0)
{
}

GraphicsLayerDebugInfo::~GraphicsLayerDebugInfo() { }

std::unique_ptr<base::trace_event::TracedValue> GraphicsLayerDebugInfo::asTracedValue() const
{
    std::unique_ptr<base::trace_event::TracedValue> tracedValue(
        new base::trace_event::TracedValue());
    appendAnnotatedInvalidateRects(tracedValue.get());
    appendCompositingReasons(tracedValue.get());
    appendSquashingDisallowedReasons(tracedValue.get());
    appendOwnerNodeId(tracedValue.get());
    return tracedValue;
}

void GraphicsLayerDebugInfo::appendAnnotatedInvalidateRects(base::trace_event::TracedValue* tracedValue) const
{
    tracedValue->BeginArray("annotated_invalidation_rects");
    for (const auto& annotatedRect : m_previousInvalidations) {
        const FloatRect& rect = annotatedRect.rect;
        tracedValue->BeginDictionary();
        tracedValue->BeginArray("geometry_rect");
        tracedValue->AppendDouble(rect.x());
        tracedValue->AppendDouble(rect.y());
        tracedValue->AppendDouble(rect.width());
        tracedValue->AppendDouble(rect.height());
        tracedValue->EndArray();
        tracedValue->SetString("reason", paintInvalidationReasonToString(annotatedRect.reason));
        tracedValue->EndDictionary();
    }
    tracedValue->EndArray();
}

void GraphicsLayerDebugInfo::appendCompositingReasons(base::trace_event::TracedValue* tracedValue) const
{
    tracedValue->BeginArray("compositing_reasons");
    for (size_t i = 0; i < kNumberOfCompositingReasons; ++i) {
        if (!(m_compositingReasons & kCompositingReasonStringMap[i].reason))
            continue;
        tracedValue->AppendString(kCompositingReasonStringMap[i].description);
    }
    tracedValue->EndArray();
}

void GraphicsLayerDebugInfo::appendSquashingDisallowedReasons(base::trace_event::TracedValue* tracedValue) const
{
    tracedValue->BeginArray("squashing_disallowed_reasons");
    for (size_t i = 0; i < kNumberOfSquashingDisallowedReasons; ++i) {
        if (!(m_squashingDisallowedReasons & kSquashingDisallowedReasonStringMap[i].reason))
            continue;
        tracedValue->AppendString(kSquashingDisallowedReasonStringMap[i].description);
    }
    tracedValue->EndArray();
}

void GraphicsLayerDebugInfo::appendOwnerNodeId(base::trace_event::TracedValue* tracedValue) const
{
    if (!m_ownerNodeId)
        return;

    tracedValue->SetInteger("owner_node", m_ownerNodeId);
}

void GraphicsLayerDebugInfo::appendAnnotatedInvalidateRect(const FloatRect& rect, PaintInvalidationReason invalidationReason)
{
    AnnotatedInvalidationRect annotatedRect = {
        rect,
        invalidationReason
    };
    m_invalidations.append(annotatedRect);
}

void GraphicsLayerDebugInfo::clearAnnotatedInvalidateRects()
{
    m_previousInvalidations.clear();
    m_previousInvalidations.swap(m_invalidations);
}

} // namespace blink
