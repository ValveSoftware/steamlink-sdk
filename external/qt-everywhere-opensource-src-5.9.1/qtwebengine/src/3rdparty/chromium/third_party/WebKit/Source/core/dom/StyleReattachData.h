// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleReattachData_h
#define StyleReattachData_h

namespace blink {

struct StyleReattachData {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  DEFINE_INLINE_TRACE() { visitor->trace(nextTextSibling); }

  RefPtr<ComputedStyle> computedStyle;
  Member<Text> nextTextSibling;
};

}  // namespace blink

#endif  // StyleReattachData_h
