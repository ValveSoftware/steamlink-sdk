// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformPaintPropertyNode_h
#define TransformPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

#include <iosfwd>

namespace blink {

// A transform created by a css property such as "transform" or "perspective"
// along with a reference to the parent TransformPaintPropertyNode, or nullptr
// for the root.
class PLATFORM_EXPORT TransformPaintPropertyNode : public RefCounted<TransformPaintPropertyNode> {
public:
    static PassRefPtr<TransformPaintPropertyNode> create(const TransformationMatrix& matrix, const FloatPoint3D& origin, PassRefPtr<TransformPaintPropertyNode> parent = nullptr)
    {
        return adoptRef(new TransformPaintPropertyNode(matrix, origin, parent));
    }

    const TransformationMatrix& matrix() const { return m_matrix; }
    const FloatPoint3D& origin() const { return m_origin; }

    // Parent transform that this transform is relative to, or nullptr if this
    // is the root transform.
    const TransformPaintPropertyNode* parent() const { return m_parent.get(); }

private:
    TransformPaintPropertyNode(const TransformationMatrix& matrix, const FloatPoint3D& origin, PassRefPtr<TransformPaintPropertyNode> parent)
        : m_matrix(matrix), m_origin(origin), m_parent(parent) { }

    const TransformationMatrix m_matrix;
    const FloatPoint3D m_origin;
    RefPtr<TransformPaintPropertyNode> m_parent;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const TransformPaintPropertyNode&, std::ostream*);

} // namespace blink

#endif // TransformPaintPropertyNode_h
