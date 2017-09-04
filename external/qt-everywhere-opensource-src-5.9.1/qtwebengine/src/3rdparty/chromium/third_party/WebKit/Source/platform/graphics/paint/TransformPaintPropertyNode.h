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
// along with a reference to the parent TransformPaintPropertyNode.
//
// The transform tree is rooted at a node with no parent. This root node should
// not be modified.
class PLATFORM_EXPORT TransformPaintPropertyNode
    : public RefCounted<TransformPaintPropertyNode> {
 public:
  static TransformPaintPropertyNode* root();

  static PassRefPtr<TransformPaintPropertyNode> create(
      PassRefPtr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      bool flattensInheritedTransform = false,
      unsigned renderingContextID = 0) {
    return adoptRef(new TransformPaintPropertyNode(
        matrix, origin, std::move(parent), flattensInheritedTransform,
        renderingContextID));
  }

  void update(PassRefPtr<const TransformPaintPropertyNode> parent,
              const TransformationMatrix& matrix,
              const FloatPoint3D& origin,
              bool flattensInheritedTransform = false,
              unsigned renderingContextID = 0) {
    DCHECK(!isRoot());
    DCHECK(parent != this);
    m_parent = parent;
    m_matrix = matrix;
    m_origin = origin;
    m_flattensInheritedTransform = flattensInheritedTransform;
    m_renderingContextID = renderingContextID;
  }

  const TransformationMatrix& matrix() const { return m_matrix; }
  const FloatPoint3D& origin() const { return m_origin; }

  // Parent transform that this transform is relative to, or nullptr if this
  // is the root transform.
  const TransformPaintPropertyNode* parent() const { return m_parent.get(); }
  bool isRoot() const { return !m_parent; }

  // If true, content with this transform node (or its descendant) appears in
  // the plane of its parent. This is implemented by flattening the total
  // accumulated transform from its ancestors.
  bool flattensInheritedTransform() const {
    return m_flattensInheritedTransform;
  }

  // Content whose transform nodes have a common rendering context ID are 3D
  // sorted. If this is 0, content will not be 3D sorted.
  unsigned renderingContextID() const { return m_renderingContextID; }
  bool hasRenderingContext() const { return m_renderingContextID; }

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // a transform node before it has been updated, to later detect changes.
  PassRefPtr<TransformPaintPropertyNode> clone() const {
    return adoptRef(new TransformPaintPropertyNode(m_matrix, m_origin, m_parent,
                                                   m_flattensInheritedTransform,
                                                   m_renderingContextID));
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a transform node has changed.
  bool operator==(const TransformPaintPropertyNode& o) const {
    return m_matrix == o.m_matrix && m_origin == o.m_origin &&
           m_parent == o.m_parent &&
           m_flattensInheritedTransform == o.m_flattensInheritedTransform &&
           m_renderingContextID == o.m_renderingContextID;
  }
#endif

 private:
  TransformPaintPropertyNode(
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      PassRefPtr<const TransformPaintPropertyNode> parent,
      bool flattensInheritedTransform,
      unsigned renderingContextID)
      : m_matrix(matrix),
        m_origin(origin),
        m_parent(parent),
        m_flattensInheritedTransform(flattensInheritedTransform),
        m_renderingContextID(renderingContextID) {}

  TransformationMatrix m_matrix;
  FloatPoint3D m_origin;
  RefPtr<const TransformPaintPropertyNode> m_parent;
  bool m_flattensInheritedTransform;
  unsigned m_renderingContextID;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const TransformPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // TransformPaintPropertyNode_h
