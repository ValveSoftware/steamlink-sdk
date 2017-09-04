// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EditingStrategy_h
#define EditingStrategy_h

#include "core/CoreExport.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "wtf/Allocator.h"

namespace blink {

// Editing algorithm defined on node traversal.
template <typename Traversal>
class CORE_TEMPLATE_CLASS_EXPORT EditingAlgorithm : public Traversal {
  STATIC_ONLY(EditingAlgorithm);

 public:
  static int caretMaxOffset(const Node&);
  // This method is used to create positions in the DOM. It returns the
  // maximum valid offset in a node. It returns 1 for some elements even
  // though they do not have children, which creates technically invalid DOM
  // Positions. Be sure to call |parentAnchoredEquivalent()| on a Position
  // before using it to create a DOM Range, or an exception will be thrown.
  static int lastOffsetForEditing(const Node*);
  static Node* rootUserSelectAllForNode(Node*);
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    EditingAlgorithm<NodeTraversal>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    EditingAlgorithm<FlatTreeTraversal>;

// DOM tree version of editing algorithm
using EditingStrategy = EditingAlgorithm<NodeTraversal>;
// Flat tree version of editing algorithm
using EditingInFlatTreeStrategy = EditingAlgorithm<FlatTreeTraversal>;

}  // namespace blink

#endif  // EditingStrategy_h
