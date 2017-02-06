// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipPaintPropertyNode_h
#define ClipPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

#include <iosfwd>

namespace blink {

// A clip rect created by a css property such as "overflow" or "clip".
// Along with a refernce to the transform space the clip rect is based on,
// and an (optional) parent ClipPaintPropertyNode for inherited clips.
class PLATFORM_EXPORT ClipPaintPropertyNode : public RefCounted<ClipPaintPropertyNode> {
public:
    static PassRefPtr<ClipPaintPropertyNode> create(
        PassRefPtr<TransformPaintPropertyNode> localTransformSpace,
        const FloatRoundedRect& clipRect,
        PassRefPtr<ClipPaintPropertyNode> parent = nullptr)
    {
        return adoptRef(new ClipPaintPropertyNode(localTransformSpace, clipRect, parent));
    }

    const TransformPaintPropertyNode* localTransformSpace() const { return m_localTransformSpace.get(); }
    const FloatRoundedRect& clipRect() const { return m_clipRect; }

    // Reference to inherited clips, or nullptr if this is the only clip.
    const ClipPaintPropertyNode* parent() const { return m_parent.get(); }

private:
    ClipPaintPropertyNode(PassRefPtr<TransformPaintPropertyNode> localTransformSpace, const FloatRoundedRect& clipRect, PassRefPtr<ClipPaintPropertyNode> parent)
        : m_localTransformSpace(localTransformSpace), m_clipRect(clipRect), m_parent(parent) { }

    RefPtr<TransformPaintPropertyNode> m_localTransformSpace;
    const FloatRoundedRect m_clipRect;
    RefPtr<ClipPaintPropertyNode> m_parent;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const ClipPaintPropertyNode&, std::ostream*);

} // namespace blink

#endif // ClipPaintPropertyNode_h
