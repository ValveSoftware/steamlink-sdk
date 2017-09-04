// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentBase_h
#define NGFragmentBase_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_physical_constraint_space.h"
#include "core/layout/ng/ng_physical_fragment_base.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "core/layout/ng/ng_units.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT NGFragmentBase : public GarbageCollected<NGFragmentBase> {
 public:
  NGWritingMode WritingMode() const {
    return static_cast<NGWritingMode>(writing_mode_);
  }
  TextDirection Direction() const {
    return static_cast<TextDirection>(direction_);
  }

  // Returns the border-box size.
  LayoutUnit InlineSize() const;
  LayoutUnit BlockSize() const;

  // Returns the total size, including the contents outside of the border-box.
  LayoutUnit InlineOverflow() const;
  LayoutUnit BlockOverflow() const;

  // Returns the offset relative to the parent fragement's content-box.
  LayoutUnit InlineOffset() const;
  LayoutUnit BlockOffset() const;

  NGPhysicalFragmentBase::NGFragmentType Type() const;

  NGPhysicalFragmentBase* PhysicalFragment() const {
    return physical_fragment_;
  };

  DECLARE_TRACE();

 protected:
  NGFragmentBase(NGWritingMode writing_mode,
                 TextDirection direction,
                 NGPhysicalFragmentBase* physical_fragment)
      : physical_fragment_(physical_fragment),
        writing_mode_(writing_mode),
        direction_(direction) {}

  Member<NGPhysicalFragmentBase> physical_fragment_;

  unsigned writing_mode_ : 3;
  unsigned direction_ : 1;
};

}  // namespace blink

#endif  // NGFragmentBase_h
