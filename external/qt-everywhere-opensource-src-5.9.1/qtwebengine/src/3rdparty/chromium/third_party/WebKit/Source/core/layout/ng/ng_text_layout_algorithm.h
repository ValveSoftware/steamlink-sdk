// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineLayoutAlgorithm_h
#define NGInlineLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_inline_box.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "wtf/RefPtr.h"

namespace blink {

class ComputedStyle;
class NGConstraintSpace;
class NGPhysicalFragment;
class NGBreakToken;

// A class for text layout. This takes a NGInlineBox which consists only
// non-atomic inlines and produces NGTextFragments.
//
// Unlike other layout algorithms this takes a NGInlineBox as its input instead
// of the ComputedStyle as it operates over multiple inlines with different
// style.
class CORE_EXPORT NGTextLayoutAlgorithm : public NGLayoutAlgorithm {
 public:
  // Default constructor.
  // @param inline_box The inline box to produce fragments from.
  // @param space The constraint space which the algorithm should generate a
  //              fragments within.
  NGTextLayoutAlgorithm(NGInlineBox* inline_box,
                        NGConstraintSpace* space,
                        NGBreakToken* break_token = nullptr);

  NGLayoutStatus Layout(NGFragmentBase*,
                        NGPhysicalFragmentBase**,
                        NGLayoutAlgorithm**) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  Member<NGInlineBox> inline_box_;
  Member<NGConstraintSpace> constraint_space_;
  Member<NGBreakToken> break_token_;
};

}  // namespace blink

#endif  // NGInlineLayoutAlgorithm_h
