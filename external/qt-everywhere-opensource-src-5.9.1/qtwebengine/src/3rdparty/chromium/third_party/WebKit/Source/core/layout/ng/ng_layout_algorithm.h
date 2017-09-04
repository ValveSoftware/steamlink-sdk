// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutAlgorithm_h
#define NGLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

struct MinAndMaxContentSizes;
class NGBox;
class NGConstraintSpace;
class NGFragmentBase;
class NGPhysicalFragmentBase;
class NGPhysicalFragment;

enum NGLayoutStatus { NotFinished, ChildAlgorithmRequired, NewFragment };

// Base class for all LayoutNG algorithms.
class CORE_EXPORT NGLayoutAlgorithm
    : public GarbageCollectedFinalized<NGLayoutAlgorithm> {
  WTF_MAKE_NONCOPYABLE(NGLayoutAlgorithm);

 public:
  NGLayoutAlgorithm() {}
  virtual ~NGLayoutAlgorithm() {}

  // Actual layout function. Lays out the children and descendents within the
  // constraints given by the NGConstraintSpace. Returns a fragment with the
  // resulting layout information.
  // This function can not be const because for interruptible layout, we have
  // to be able to store state information.
  // If this function returns NotFinished, it has to be called again.
  // If it returns ChildAlgorithmRequired, the NGBox out parameter will
  // be set with the NGBox that needs to be layed out next.
  // If it returns NewFragment, the NGPhysicalFragmentBase out parameter
  // will contain the new fragment.
  virtual NGLayoutStatus Layout(NGFragmentBase*,
                                NGPhysicalFragmentBase**,
                                NGLayoutAlgorithm**) = 0;

  enum MinAndMaxState { Success, Pending, NotImplemented };

  // Computes the min-content and max-content intrinsic sizes for the given box.
  // The result will not take any min-width. max-width or width properties into
  // account. Implementations can return NotImpplemented in which case the
  // caller is expected ot synthesize this value from the overflow rect returned
  // from Layout called with a available width of 0 and LayoutUnit::max(),
  // respectively.
  // A Pending return value has the same meaning as a false return from layout,
  // i.e. it is a request to call this function again.
  virtual MinAndMaxState ComputeMinAndMaxContentSizes(MinAndMaxContentSizes*) {
    return NotImplemented;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif  // NGLayoutAlgorithm_h
