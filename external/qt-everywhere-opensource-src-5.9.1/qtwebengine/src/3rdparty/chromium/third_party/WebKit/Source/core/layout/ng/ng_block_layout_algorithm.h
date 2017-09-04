// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockLayoutAlgorithm_h
#define NGBlockLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_box.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "wtf/RefPtr.h"

namespace blink {

class ComputedStyle;
class NGConstraintSpace;
class NGPhysicalFragmentBase;
class NGBreakToken;

// A class for general block layout (e.g. a <div> with no special style).
// Lays out the children in sequence.
class CORE_EXPORT NGBlockLayoutAlgorithm : public NGLayoutAlgorithm {
 public:
  // Default constructor.
  // @param style Style reference of the block that is being laid out.
  // @param first_child Our first child; the algorithm will use its NextSibling
  //                    method to access all the children.
  // @param space The constraint space which the algorithm should generate a
  //              fragment within.
  NGBlockLayoutAlgorithm(PassRefPtr<const ComputedStyle>,
                         NGBox* first_child,
                         NGConstraintSpace* space,
                         NGBreakToken* break_token = nullptr);

  NGLayoutStatus Layout(NGFragmentBase*,
                        NGPhysicalFragmentBase**,
                        NGLayoutAlgorithm**) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  // Creates a new constraint space for the current child.
  NGConstraintSpace* CreateConstraintSpaceForCurrentChild() const;
  bool LayoutCurrentChild();

  // Computes collapsed margins for 2 adjoining blocks and updates the resultant
  // fragment's MarginStrut if needed.
  // See https://www.w3.org/TR/CSS2/box.html#collapsing-margins
  //
  // @param child_margins Margins information for the current child.
  // @param fragment Current child's fragment.
  // @return NGBoxStrut with margins block start/end.
  NGBoxStrut CollapseMargins(const NGBoxStrut& child_margins,
                             const NGFragment& fragment);

  // Calculates position of the in-flow block-level fragment that needs to be
  // positioned relative to the current fragment that is being built.
  //
  // @param fragment Fragment that needs to be placed.
  // @param child_margins Margins information for the current child fragment.
  // @return Position of the fragment in the parent's constraint space.
  NGLogicalOffset PositionFragment(const NGFragmentBase& fragment,
                                   const NGBoxStrut& child_margins);

  // Calculates position of the float fragment that needs to be
  // positioned relative to the current fragment that is being built.
  //
  // @param fragment Fragment that needs to be placed.
  // @param margins Margins information for the fragment.
  // @return Position of the fragment in the parent's constraint space.
  NGLogicalOffset PositionFloatFragment(const NGFragmentBase& fragment,
                                        const NGBoxStrut& margins);

  // Updates block-{start|end} of the currently constructed fragment.
  //
  // This method is supposed to be called on every child but it only updates
  // the block-start once (on the first non-zero height child fragment) and
  // keeps updating block-end (on every non-zero height child).
  void UpdateMarginStrut(const NGMarginStrut& from);

  // Read-only Getters.
  const ComputedStyle& CurrentChildStyle() const {
    DCHECK(current_child_);
    return *current_child_->Style();
  }

  const NGConstraintSpace& ConstraintSpace() const {
    return *constraint_space_;
  }

  const ComputedStyle& Style() const { return *style_; }

  enum State { kStateInit, kStateChildLayout, kStateFinalize };
  State state_;

  RefPtr<const ComputedStyle> style_;

  Member<NGBox> first_child_;
  Member<NGConstraintSpace> constraint_space_;
  Member<NGBreakToken> break_token_;
  Member<NGFragmentBuilder> builder_;
  Member<NGConstraintSpaceBuilder> space_builder_;
  Member<NGConstraintSpace> space_for_current_child_;
  Member<NGBox> current_child_;

  NGBoxStrut border_and_padding_;
  LayoutUnit content_size_;
  LayoutUnit max_inline_size_;
  // MarginStrut for the previous child.
  NGMarginStrut prev_child_margin_strut_;
  // Whether the block-start was set for the currently built
  // fragment's margin strut.
  bool is_fragment_margin_strut_block_start_updated_ : 1;
};

}  // namespace blink

#endif  // NGBlockLayoutAlgorithm_h
