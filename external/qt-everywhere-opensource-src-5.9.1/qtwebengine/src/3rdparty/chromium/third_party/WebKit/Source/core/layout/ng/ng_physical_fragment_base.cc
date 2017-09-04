// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_fragment_base.h"

#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_physical_text_fragment.h"

namespace blink {

DEFINE_TRACE(NGPhysicalFragmentBase) {
  if (Type() == FragmentText)
    static_cast<NGPhysicalTextFragment*>(this)->traceAfterDispatch(visitor);
  else
    static_cast<NGPhysicalFragment*>(this)->traceAfterDispatch(visitor);
}

}  // namespace blink
