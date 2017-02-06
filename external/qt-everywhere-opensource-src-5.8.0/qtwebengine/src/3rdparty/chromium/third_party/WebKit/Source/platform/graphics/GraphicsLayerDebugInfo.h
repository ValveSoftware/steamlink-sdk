/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GraphicsLayerDebugInfo_h
#define GraphicsLayerDebugInfo_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/CompositingReasons.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/graphics/SquashingDisallowedReasons.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

#include <memory>

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace blink {

class GraphicsLayerDebugInfo final {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(GraphicsLayerDebugInfo);
public:
    GraphicsLayerDebugInfo();
    ~GraphicsLayerDebugInfo();

    std::unique_ptr<base::trace_event::TracedValue> asTracedValue() const;

    CompositingReasons getCompositingReasons() const { return m_compositingReasons; }
    void setCompositingReasons(CompositingReasons reasons) { m_compositingReasons = reasons; }

    SquashingDisallowedReasons getSquashingDisallowedReasons() const { return m_squashingDisallowedReasons; }
    void setSquashingDisallowedReasons(SquashingDisallowedReasons reasons) { m_squashingDisallowedReasons = reasons; }
    void setOwnerNodeId(int id) { m_ownerNodeId = id; }

    void appendAnnotatedInvalidateRect(const FloatRect&, PaintInvalidationReason);
    void clearAnnotatedInvalidateRects();

private:
    void appendAnnotatedInvalidateRects(base::trace_event::TracedValue*) const;
    void appendCompositingReasons(base::trace_event::TracedValue*) const;
    void appendSquashingDisallowedReasons(base::trace_event::TracedValue*) const;
    void appendOwnerNodeId(base::trace_event::TracedValue*) const;

    struct AnnotatedInvalidationRect {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
        FloatRect rect;
        PaintInvalidationReason reason;
    };

    CompositingReasons m_compositingReasons;
    SquashingDisallowedReasons m_squashingDisallowedReasons;
    int m_ownerNodeId;
    Vector<AnnotatedInvalidationRect> m_invalidations;
    Vector<AnnotatedInvalidationRect> m_previousInvalidations;
};

} // namespace blink

#endif
