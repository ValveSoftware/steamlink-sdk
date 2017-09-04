// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RootScrollerUtil_h
#define RootScrollerUtil_h

namespace blink {

class Element;
class ScrollableArea;

namespace RootScrollerUtil {

ScrollableArea* scrollableAreaFor(const Element&);

}  // namespace RootScrollerUtil

}  // namespace blink

#endif  // RootScrollerUtil_h
