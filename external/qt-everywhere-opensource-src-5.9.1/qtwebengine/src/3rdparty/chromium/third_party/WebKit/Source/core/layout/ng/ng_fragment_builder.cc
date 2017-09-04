// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment_builder.h"

namespace blink {

NGFragmentBuilder::NGFragmentBuilder(
    NGPhysicalFragmentBase::NGFragmentType type)
    : type_(type), writing_mode_(HorizontalTopBottom), direction_(LTR) {}

NGFragmentBuilder& NGFragmentBuilder::SetWritingMode(
    NGWritingMode writing_mode) {
  writing_mode_ = writing_mode;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetDirection(TextDirection direction) {
  direction_ = direction;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetInlineSize(LayoutUnit size) {
  size_.inline_size = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetBlockSize(LayoutUnit size) {
  size_.block_size = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetInlineOverflow(LayoutUnit size) {
  overflow_.inline_size = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetBlockOverflow(LayoutUnit size) {
  overflow_.block_size = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddChild(NGFragmentBase* child,
                                               NGLogicalOffset offset) {
  DCHECK_EQ(type_, NGPhysicalFragmentBase::FragmentBox)
      << "Only box fragments can have children";
  children_.append(child->PhysicalFragment());
  offsets_.append(offset);
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetMarginStrutBlockStart(
    const NGMarginStrut& from) {
  margin_strut_.margin_block_start = from.margin_block_start;
  margin_strut_.negative_margin_block_start = from.negative_margin_block_start;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetMarginStrutBlockEnd(
    const NGMarginStrut& from) {
  margin_strut_.margin_block_end = from.margin_block_end;
  margin_strut_.negative_margin_block_end = from.negative_margin_block_end;
  return *this;
}

NGPhysicalFragment* NGFragmentBuilder::ToFragment() {
  // TODO(layout-ng): Support text fragments
  DCHECK_EQ(type_, NGPhysicalFragmentBase::FragmentBox);
  DCHECK_EQ(offsets_.size(), children_.size());

  NGPhysicalSize physical_size = size_.ConvertToPhysical(writing_mode_);
  HeapVector<Member<const NGPhysicalFragmentBase>> children;
  children.reserveCapacity(children_.size());

  for (size_t i = 0; i < children_.size(); ++i) {
    NGPhysicalFragmentBase* child = children_[i].get();
    child->SetOffset(offsets_[i].ConvertToPhysical(
        writing_mode_, direction_, physical_size, child->Size()));
    children.append(child);
  }
  return new NGPhysicalFragment(physical_size,
                                overflow_.ConvertToPhysical(writing_mode_),
                                children, margin_strut_);
}

}  // namespace blink
