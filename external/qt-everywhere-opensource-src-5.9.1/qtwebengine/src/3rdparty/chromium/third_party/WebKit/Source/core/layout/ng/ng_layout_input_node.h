// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutInputNode_h
#define NGLayoutInputNode_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class NGConstraintSpace;
class NGFragmentBase;
class NGLayoutAlgorithm;

// Represents the input to a layout algorithm for a given node. The layout
// engine should use the style, node type to determine which type of layout
// algorithm to use to produce fragments for this node.
class CORE_EXPORT NGLayoutInputNode
    : public GarbageCollectedFinalized<NGLayoutInputNode> {
 public:
  enum NGLayoutInputNodeType { LegacyBlock = 0, LegacyInline = 1 };

  virtual ~NGLayoutInputNode(){};

  // Returns true when done; when this function returns false, it has to be
  // called again. The out parameter will only be set when this function
  // returns true. The same constraint space has to be passed each time.
  virtual bool Layout(const NGConstraintSpace*, NGFragmentBase**) = 0;

  // Returns the next sibling.
  virtual NGLayoutInputNode* NextSibling() = 0;

  NGLayoutInputNodeType Type() const {
    return static_cast<NGLayoutInputNodeType>(type_);
  }

  static NGLayoutAlgorithm* AlgorithmForInputNode(NGLayoutInputNode*,
                                                  const NGConstraintSpace*);

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  explicit NGLayoutInputNode(NGLayoutInputNodeType type) : type_(type) {}

 private:
  unsigned type_ : 1;
};

}  // namespace blink

#endif  // NGInlineBox_h
