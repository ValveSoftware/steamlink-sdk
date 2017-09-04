// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_inline_layout_algorithm.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_coordinator.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/text/TextDirection.h"

namespace blink {

NGBox::NGBox(LayoutObject* layout_object)
    : NGLayoutInputNode(NGLayoutInputNodeType::LegacyBlock),
      layout_box_(toLayoutBox(layout_object)) {
  DCHECK(layout_box_);
}

NGBox::NGBox(ComputedStyle* style)
    : NGLayoutInputNode(NGLayoutInputNodeType::LegacyBlock),
      layout_box_(nullptr),
      style_(style) {
  DCHECK(style_);
}

bool NGBox::Layout(const NGConstraintSpace* constraint_space,
                   NGFragmentBase** out) {
  DCHECK(!minmax_algorithm_)
      << "Can't interleave Layout and ComputeMinAndMaxContentSizes";
  if (layout_box_ && layout_box_->isOutOfFlowPositioned())
    layout_box_->containingBlock()->insertPositionedObject(layout_box_);
  // We can either use the new layout code to do the layout and then copy the
  // resulting size to the LayoutObject, or use the old layout code and
  // synthesize a fragment.
  if (CanUseNewLayout()) {
    NGPhysicalFragmentBase* fragment;

    // Store a coordinator so Layout can preserve its existing semantic
    // of returning false until completed.
    if (!layout_coordinator_)
      layout_coordinator_ = new NGLayoutCoordinator(this, constraint_space);

    if (!layout_coordinator_->Tick(&fragment))
      return false;

    fragment_ = toNGPhysicalFragment(fragment);

    if (layout_box_) {
      CopyFragmentDataToLayoutBox(*constraint_space);
    }
  } else {
    DCHECK(layout_box_);
    fragment_ = RunOldLayout(*constraint_space);
  }
  *out = new NGFragment(constraint_space->WritingMode(), Style()->direction(),
                        fragment_.get());
  // Reset coordinator for future use
  layout_coordinator_ = nullptr;
  return true;
}

bool NGBox::ComputeMinAndMaxContentSizes(MinAndMaxContentSizes* sizes) {
  if (!CanUseNewLayout()) {
    DCHECK(layout_box_);
    // TODO(layout-ng): This could be somewhat optimized by directly calling
    // computeIntrinsicLogicalWidths, but that function is currently private.
    // Consider doing that if this becomes a performance issue.
    LayoutUnit borderAndPadding = layout_box_->borderAndPaddingLogicalWidth();
    sizes->min_content = layout_box_->computeLogicalWidthUsing(
                             MainOrPreferredSize, Length(MinContent),
                             LayoutUnit(), layout_box_->containingBlock()) -
                         borderAndPadding;
    sizes->max_content = layout_box_->computeLogicalWidthUsing(
                             MainOrPreferredSize, Length(MaxContent),
                             LayoutUnit(), layout_box_->containingBlock()) -
                         borderAndPadding;
    return true;
  }
  DCHECK(!layout_coordinator_)
      << "Can't interleave Layout and ComputeMinAndMaxContentSizes";
  if (!minmax_algorithm_) {
    NGConstraintSpaceBuilder builder(
        FromPlatformWritingMode(Style()->getWritingMode()));

    builder.SetAvailableSize(NGLogicalSize(LayoutUnit(), LayoutUnit()));
    builder.SetPercentageResolutionSize(
        NGLogicalSize(LayoutUnit(), LayoutUnit()));
    // TODO(layoutng): Use builder.ToConstraintSpace.ToLogicalConstraintSpace
    // once
    // that's available.
    NGConstraintSpace* constraint_space = new NGConstraintSpace(
        FromPlatformWritingMode(Style()->getWritingMode()),
        Style()->direction(), builder.ToConstraintSpace());

    minmax_algorithm_ = new NGBlockLayoutAlgorithm(
        Style(), toNGBox(FirstChild()), constraint_space);
  }
  // TODO(cbiesinger): For orthogonal children, we need to always synthesize.
  NGLayoutAlgorithm::MinAndMaxState state =
      minmax_algorithm_->ComputeMinAndMaxContentSizes(sizes);
  if (state == NGLayoutAlgorithm::Success)
    return true;
  if (state == NGLayoutAlgorithm::Pending)
    return false;
  DCHECK_EQ(state, NGLayoutAlgorithm::NotImplemented);

  // TODO(cbiesinger): Replace the loops below with a state machine like in
  // Layout.

  // Have to synthesize this value.
  NGPhysicalFragmentBase* physical_fragment;
  while (!minmax_algorithm_->Layout(nullptr, &physical_fragment, nullptr))
    continue;
  NGFragment* fragment = new NGFragment(
      FromPlatformWritingMode(Style()->getWritingMode()), Style()->direction(),
      toNGPhysicalFragment(physical_fragment));

  sizes->min_content = fragment->InlineOverflow();

  // Now, redo with infinite space for max_content
  NGConstraintSpaceBuilder builder(
      FromPlatformWritingMode(Style()->getWritingMode()));
  builder.SetAvailableSize(NGLogicalSize(LayoutUnit::max(), LayoutUnit()));
  builder.SetPercentageResolutionSize(
      NGLogicalSize(LayoutUnit(), LayoutUnit()));
  NGConstraintSpace* constraint_space =
      new NGConstraintSpace(FromPlatformWritingMode(Style()->getWritingMode()),
                            Style()->direction(), builder.ToConstraintSpace());

  minmax_algorithm_ = new NGBlockLayoutAlgorithm(Style(), toNGBox(FirstChild()),
                                                 constraint_space);
  while (!minmax_algorithm_->Layout(nullptr, &physical_fragment, nullptr))
    continue;

  fragment = new NGFragment(FromPlatformWritingMode(Style()->getWritingMode()),
                            Style()->direction(),
                            toNGPhysicalFragment(physical_fragment));
  sizes->max_content = fragment->InlineOverflow();
  minmax_algorithm_ = nullptr;
  return true;
}

ComputedStyle* NGBox::MutableStyle() {
  if (style_)
    return style_.get();
  DCHECK(layout_box_);
  return layout_box_->mutableStyle();
}

const ComputedStyle* NGBox::Style() const {
  if (style_)
    return style_.get();
  DCHECK(layout_box_);
  return layout_box_->style();
}

NGBox* NGBox::NextSibling() {
  if (!next_sibling_) {
    LayoutObject* next_sibling =
        layout_box_ ? layout_box_->nextSibling() : nullptr;
    NGBox* box = next_sibling ? new NGBox(next_sibling) : nullptr;
    SetNextSibling(box);
  }
  return next_sibling_;
}

NGLayoutInputNode* NGBox::FirstChild() {
  if (!first_child_) {
    LayoutObject* child = layout_box_ ? layout_box_->slowFirstChild() : nullptr;
    if (child) {
      if (child->isInline()) {
        SetFirstChild(new NGInlineBox(child, MutableStyle()));
      } else {
        SetFirstChild(new NGBox(child));
      }
    }
  }
  return first_child_;
}

void NGBox::SetNextSibling(NGBox* sibling) {
  next_sibling_ = sibling;
}

void NGBox::SetFirstChild(NGLayoutInputNode* child) {
  first_child_ = child;
}

DEFINE_TRACE(NGBox) {
  visitor->trace(layout_coordinator_);
  visitor->trace(minmax_algorithm_);
  visitor->trace(fragment_);
  visitor->trace(next_sibling_);
  visitor->trace(first_child_);
  NGLayoutInputNode::trace(visitor);
}

void NGBox::PositionUpdated() {
  if (!layout_box_)
    return;
  DCHECK(layout_box_->parent()) << "Should be called on children only.";

  layout_box_->setX(fragment_->LeftOffset());
  layout_box_->setY(fragment_->TopOffset());

  if (layout_box_->isFloating() && layout_box_->parent()->isLayoutBlockFlow()) {
    FloatingObject* floating_object = toLayoutBlockFlow(layout_box_->parent())
                                          ->insertFloatingObject(*layout_box_);
    floating_object->setX(fragment_->LeftOffset());
    floating_object->setY(fragment_->TopOffset());
    floating_object->setIsPlaced(true);
  }
}

bool NGBox::CanUseNewLayout() {
  if (!layout_box_)
    return true;
  if (!layout_box_->isLayoutBlockFlow())
    return false;
  if (RuntimeEnabledFeatures::layoutNGInlineEnabled())
    return true;
  if (HasInlineChildren())
    return false;
  return true;
}

bool NGBox::HasInlineChildren() {
  if (!layout_box_)
    return false;

  const LayoutBlockFlow* block_flow = toLayoutBlockFlow(layout_box_);
  LayoutObject* child = block_flow->firstChild();
  while (child) {
    if (child->isInline())
      return true;
    child = child->nextSibling();
  }

  return false;
}

void NGBox::CopyFragmentDataToLayoutBox(
    const NGConstraintSpace& constraint_space) {
  DCHECK(layout_box_);
  layout_box_->setWidth(fragment_->Width());
  layout_box_->setHeight(fragment_->Height());
  NGBoxStrut border_and_padding =
      ComputeBorders(*Style()) + ComputePadding(constraint_space, *Style());
  LayoutUnit intrinsic_logical_height =
      layout_box_->style()->isHorizontalWritingMode()
          ? fragment_->HeightOverflow()
          : fragment_->WidthOverflow();
  intrinsic_logical_height -= border_and_padding.BlockSum();
  layout_box_->setIntrinsicContentLogicalHeight(intrinsic_logical_height);

  // TODO(layout-dev): Currently we are not actually performing layout on
  // inline children. For now just clear the needsLayout bit so that we can
  // run unittests.
  if (HasInlineChildren()) {
    for (InlineWalker walker(
             LineLayoutBlockFlow(toLayoutBlockFlow(layout_box_)));
         !walker.atEnd(); walker.advance()) {
      LayoutObject* o = LineLayoutAPIShim::layoutObjectFrom(walker.current());
      o->clearNeedsLayout();
    }

    // Ensure the position of the children are copied across to the
    // LayoutObject tree.
  } else {
    for (NGBox* box = toNGBox(FirstChild()); box; box = box->NextSibling()) {
      if (box->fragment_)
        box->PositionUpdated();
    }
  }

  if (layout_box_->isLayoutBlock())
    toLayoutBlock(layout_box_)->layoutPositionedObjects(true);
  layout_box_->clearNeedsLayout();
  if (layout_box_->isLayoutBlockFlow()) {
    toLayoutBlockFlow(layout_box_)->updateIsSelfCollapsing();
  }
}

NGPhysicalFragment* NGBox::RunOldLayout(
    const NGConstraintSpace& constraint_space) {
  // TODO(layout-ng): If fixedSize is true, set the override width/height too
  NGLogicalSize available_size = constraint_space.AvailableSize();
  layout_box_->setOverrideContainingBlockContentLogicalWidth(
      available_size.inline_size);
  layout_box_->setOverrideContainingBlockContentLogicalHeight(
      available_size.block_size);
  if (layout_box_->isLayoutNGBlockFlow() && layout_box_->needsLayout()) {
    toLayoutNGBlockFlow(layout_box_)->LayoutBlockFlow::layoutBlock(true);
  } else {
    layout_box_->forceLayout();
  }
  LayoutRect overflow = layout_box_->layoutOverflowRect();
  // TODO(layout-ng): This does not handle writing modes correctly (for
  // overflow)
  NGFragmentBuilder builder(NGPhysicalFragmentBase::FragmentBox);
  builder.SetInlineSize(layout_box_->logicalWidth())
      .SetBlockSize(layout_box_->logicalHeight())
      .SetDirection(layout_box_->styleRef().direction())
      .SetWritingMode(
          FromPlatformWritingMode(layout_box_->styleRef().getWritingMode()))
      .SetInlineOverflow(overflow.width())
      .SetBlockOverflow(overflow.height());
  return builder.ToFragment();
}

}  // namespace blink
