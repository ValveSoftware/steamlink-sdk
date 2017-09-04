// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HitTestAction_h
#define HitTestAction_h

namespace blink {

enum HitTestAction {
  HitTestBlockBackground,
  HitTestChildBlockBackground,
  HitTestChildBlockBackgrounds,
  HitTestFloat,
  HitTestForeground
};

}  // namespace blink

#endif  // HitTestAction_h
