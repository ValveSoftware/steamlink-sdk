// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragment_h
#define NGFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_fragment_base.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_units.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT NGFragment final : public NGFragmentBase {
 public:
  NGFragment(NGWritingMode writing_mode,
             TextDirection direction,
             NGPhysicalFragment* physical_fragment)
      : NGFragmentBase(writing_mode, direction, physical_fragment) {}

  NGMarginStrut MarginStrut() const;
};

DEFINE_TYPE_CASTS(NGFragment,
                  NGFragmentBase,
                  fragment,
                  fragment->Type() == NGPhysicalFragmentBase::FragmentBox,
                  fragment.Type() == NGPhysicalFragmentBase::FragmentBox);

}  // namespace blink

#endif  // NGFragment_h
