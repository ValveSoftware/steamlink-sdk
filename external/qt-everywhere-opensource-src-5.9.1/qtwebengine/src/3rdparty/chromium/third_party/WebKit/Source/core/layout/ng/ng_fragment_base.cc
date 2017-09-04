// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment_base.h"

#include "core/layout/ng/ng_physical_fragment_base.h"

namespace blink {

LayoutUnit NGFragmentBase::InlineSize() const {
  return writing_mode_ == HorizontalTopBottom ? physical_fragment_->Width()
                                              : physical_fragment_->Height();
}

LayoutUnit NGFragmentBase::BlockSize() const {
  return writing_mode_ == HorizontalTopBottom ? physical_fragment_->Height()
                                              : physical_fragment_->Width();
}

LayoutUnit NGFragmentBase::InlineOverflow() const {
  return writing_mode_ == HorizontalTopBottom
             ? physical_fragment_->WidthOverflow()
             : physical_fragment_->HeightOverflow();
}

LayoutUnit NGFragmentBase::BlockOverflow() const {
  return writing_mode_ == HorizontalTopBottom
             ? physical_fragment_->HeightOverflow()
             : physical_fragment_->WidthOverflow();
}

LayoutUnit NGFragmentBase::InlineOffset() const {
  return writing_mode_ == HorizontalTopBottom ? physical_fragment_->LeftOffset()
                                              : physical_fragment_->TopOffset();
}

LayoutUnit NGFragmentBase::BlockOffset() const {
  return writing_mode_ == HorizontalTopBottom
             ? physical_fragment_->TopOffset()
             : physical_fragment_->LeftOffset();
}

NGPhysicalFragmentBase::NGFragmentType NGFragmentBase::Type() const {
  return physical_fragment_->Type();
}

DEFINE_TRACE(NGFragmentBase) {
  visitor->trace(physical_fragment_);
}

}  // namespace blink
