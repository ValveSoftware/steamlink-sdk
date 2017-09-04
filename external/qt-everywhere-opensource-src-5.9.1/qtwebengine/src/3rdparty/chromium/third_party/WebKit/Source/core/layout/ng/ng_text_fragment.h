// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGTextFragment_h
#define NGTextFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_fragment_base.h"
#include "core/layout/ng/ng_physical_text_fragment.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT NGTextFragment final : public NGFragmentBase {
 public:
  NGTextFragment(NGWritingMode writing_mode,
                 TextDirection direction,
                 NGPhysicalTextFragment* physical_text_fragment)
      : NGFragmentBase(writing_mode, direction, physical_text_fragment) {}
};

}  // namespace blink

#endif  // NGTextFragment_h
