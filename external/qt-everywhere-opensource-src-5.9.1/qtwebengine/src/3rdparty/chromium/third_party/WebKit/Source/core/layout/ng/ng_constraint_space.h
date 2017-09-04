// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpace_h
#define NGConstraintSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_macros.h"
#include "core/layout/ng/ng_physical_constraint_space.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"
#include "wtf/Vector.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class NGFragment;
class NGLayoutOpportunityIterator;

// The NGConstraintSpace represents a set of constraints and available space
// which a layout algorithm may produce a NGFragment within. It is a view on
// top of a NGPhysicalConstraintSpace and provides accessor methods in the
// logical coordinate system defined by the writing mode given.
class CORE_EXPORT NGConstraintSpace final
    : public GarbageCollected<NGConstraintSpace> {
 public:
  // Constructs a constraint space based on an existing backing
  // NGPhysicalConstraintSpace. Sets this constraint space's size to the
  // physical constraint space's available size, converted to logical
  // coordinates.
  NGConstraintSpace(NGWritingMode, TextDirection, NGPhysicalConstraintSpace*);

  // This should live on NGBox or another layout bridge and probably take a root
  // NGConstraintSpace or a NGPhysicalConstraintSpace.
  static NGConstraintSpace* CreateFromLayoutObject(const LayoutBox&);

  // Mutable Getters.
  // TODO(layout-dev): remove const constraint from MutablePhysicalSpace method
  NGPhysicalConstraintSpace* MutablePhysicalSpace() const {
    return physical_space_;
  }

  // Read-only Getters.
  const NGPhysicalConstraintSpace* PhysicalSpace() const {
    return physical_space_;
  }

  const Vector<std::unique_ptr<const NGExclusion>>& Exclusions() const {
    WRITING_MODE_IGNORED(
        "Exclusions are stored directly in physical constraint space.");
    return PhysicalSpace()->Exclusions();
  }

  TextDirection Direction() const {
    return static_cast<TextDirection>(direction_);
  }

  NGWritingMode WritingMode() const {
    return static_cast<NGWritingMode>(writing_mode_);
  }

  // Adds the exclusion in the physical constraint space.
  void AddExclusion(const NGExclusion& exclusion) const;
  const NGExclusion* LastLeftFloatExclusion() const;
  const NGExclusion* LastRightFloatExclusion() const;

  // The size to use for percentage resolution.
  // See: https://drafts.csswg.org/css-sizing/#percentage-sizing
  NGLogicalSize PercentageResolutionSize() const;

  // The available space size.
  // See: https://drafts.csswg.org/css-sizing/#available
  NGLogicalSize AvailableSize() const;

  // Offset relative to the root constraint space.
  NGLogicalOffset Offset() const { return offset_; }
  void SetOffset(const NGLogicalOffset& offset) { offset_ = offset; }

  // Returns the effective size of the constraint space. Equal to the
  // AvailableSize() for the root constraint space but derived constraint spaces
  // return the size of the layout opportunity.
  virtual NGLogicalSize Size() const { return size_; }
  void SetSize(const NGLogicalSize& size) { size_ = size; }

  // Whether the current constraint space is for the newly established
  // Formatting Context.
  bool IsNewFormattingContext() const;

  // Whether exceeding the AvailableSize() triggers the presence of a scrollbar
  // for the indicated direction.
  // If exceeded the current layout should be aborted and invoked again with a
  // constraint space modified to reserve space for a scrollbar.
  bool InlineTriggersScrollbar() const;
  bool BlockTriggersScrollbar() const;

  // Some layout modes “stretch” their children to a fixed size (e.g. flex,
  // grid). These flags represented whether a layout needs to produce a
  // fragment that satisfies a fixed constraint in the inline and block
  // direction respectively.
  bool FixedInlineSize() const;
  bool FixedBlockSize() const;

  // If specified a layout should produce a Fragment which fragments at the
  // blockSize if possible.
  NGFragmentationType BlockFragmentationType() const;

  // Modifies constraint space to account for a placed fragment. Depending on
  // the shape of the fragment this will either modify the inline or block
  // size, or add an exclusion.
  void Subtract(const NGFragment*);

  NGLayoutOpportunityIterator* LayoutOpportunities(
      unsigned clear = NGClearNone,
      bool for_inline_or_bfc = false);

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(physical_space_); }

  // The setters for the NGConstraintSpace should only be used when constructing
  // a derived NGConstraintSpace.
  void SetOverflowTriggersScrollbar(bool inlineTriggers, bool blockTriggers);
  void SetFixedSize(bool inlineFixed, bool blockFixed);
  void SetFragmentationType(NGFragmentationType);
  void SetIsNewFormattingContext(bool is_new_fc);

  NGConstraintSpace* ChildSpace(const ComputedStyle* style) const;

  String ToString() const;

 private:
  Member<NGPhysicalConstraintSpace> physical_space_;
  NGLogicalOffset offset_;
  NGLogicalSize size_;
  unsigned writing_mode_ : 3;
  unsigned direction_ : 1;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGConstraintSpace& value) {
  return stream << value.ToString();
}

}  // namespace blink

#endif  // NGConstraintSpace_h
