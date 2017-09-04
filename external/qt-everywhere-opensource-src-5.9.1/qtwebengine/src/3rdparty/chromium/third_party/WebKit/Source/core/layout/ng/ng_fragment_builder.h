// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentBuilder_h
#define NGFragmentBuilder_h

#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_units.h"

namespace blink {

class CORE_EXPORT NGFragmentBuilder final
    : public GarbageCollectedFinalized<NGFragmentBuilder> {
 public:
  NGFragmentBuilder(NGPhysicalFragmentBase::NGFragmentType);

  NGFragmentBuilder& SetWritingMode(NGWritingMode);
  NGFragmentBuilder& SetDirection(TextDirection);

  NGFragmentBuilder& SetInlineSize(LayoutUnit);
  NGFragmentBuilder& SetBlockSize(LayoutUnit);

  NGFragmentBuilder& SetInlineOverflow(LayoutUnit);
  NGFragmentBuilder& SetBlockOverflow(LayoutUnit);

  NGFragmentBuilder& AddChild(NGFragmentBase*, NGLogicalOffset);

  // Sets MarginStrut for the resultant fragment.
  NGFragmentBuilder& SetMarginStrutBlockStart(const NGMarginStrut& from);
  NGFragmentBuilder& SetMarginStrutBlockEnd(const NGMarginStrut& from);

  // Offsets are not supposed to be set during fragment construction, so we
  // do not provide a setter here.

  // Creates the fragment. Can only be called once.
  NGPhysicalFragment* ToFragment();

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(children_); }

 private:
  NGPhysicalFragmentBase::NGFragmentType type_;
  NGWritingMode writing_mode_;
  TextDirection direction_;

  NGLogicalSize size_;
  NGLogicalSize overflow_;

  NGMarginStrut margin_strut_;

  HeapVector<Member<NGPhysicalFragmentBase>> children_;
  Vector<NGLogicalOffset> offsets_;
};

}  // namespace blink

#endif  // NGFragmentBuilder
