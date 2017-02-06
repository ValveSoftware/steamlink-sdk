// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EffectPaintPropertyNode_h
#define EffectPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

#include <iosfwd>

namespace blink {

// A paint effect created by the opacity css property along with a reference to
// the parent effect node, or nullptr for the root.
// TODO(pdr): Support more effects than just opacity.
class PLATFORM_EXPORT EffectPaintPropertyNode : public RefCounted<EffectPaintPropertyNode> {
public:
    static PassRefPtr<EffectPaintPropertyNode> create(float opacity, PassRefPtr<EffectPaintPropertyNode> parent = nullptr)
    {
        return adoptRef(new EffectPaintPropertyNode(opacity, parent));
    }

    float opacity() const { return m_opacity; }

    // Parent effect or nullptr if this is the root effect.
    const EffectPaintPropertyNode* parent() const { return m_parent.get(); }

private:
    EffectPaintPropertyNode(float opacity, PassRefPtr<EffectPaintPropertyNode> parent)
        : m_opacity(opacity), m_parent(parent) { }

    const float m_opacity;
    RefPtr<EffectPaintPropertyNode> m_parent;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const EffectPaintPropertyNode&, std::ostream*);

} // namespace blink

#endif // EffectPaintPropertyNode_h
