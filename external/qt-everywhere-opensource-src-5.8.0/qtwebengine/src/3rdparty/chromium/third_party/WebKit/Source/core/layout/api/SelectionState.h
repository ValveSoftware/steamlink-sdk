// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SelectionState_h
#define SelectionState_h

namespace blink {

enum SelectionState {
    SelectionNone, // The object is not selected.
    SelectionStart, // The object either contains the start of a selection run or is the start of a run.
    SelectionInside, // The object is fully encompassed by a selection run.
    SelectionEnd, // The object either contains the end of a selection run or is the end of a run.
    SelectionBoth // The object contains an entire run or is the sole selected object in that run.
};

} // namespace blink

#endif // SelectionState_h
