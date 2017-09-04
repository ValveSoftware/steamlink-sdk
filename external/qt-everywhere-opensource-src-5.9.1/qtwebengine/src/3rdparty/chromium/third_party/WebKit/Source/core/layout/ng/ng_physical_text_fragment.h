// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalTextFragment_h
#define NGPhysicalTextFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_physical_fragment_base.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT NGPhysicalTextFragment final : public NGPhysicalFragmentBase {
 public:
  NGPhysicalTextFragment(NGPhysicalSize size, NGPhysicalSize overflow)
      : NGPhysicalFragmentBase(size, overflow, FragmentText) {}

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    NGPhysicalFragmentBase::traceAfterDispatch(visitor);
  }
};

DEFINE_TYPE_CASTS(NGPhysicalTextFragment,
                  NGPhysicalFragmentBase,
                  text,
                  text->Type() == NGPhysicalFragmentBase::FragmentText,
                  text.Type() == NGPhysicalFragmentBase::FragmentText);

}  // namespace blink

#endif  // NGPhysicalTextFragment_h
