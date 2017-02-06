// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CachedDisplayItem_h
#define CachedDisplayItem_h

#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Assertions.h"

namespace blink {

// A placeholder of a DrawingDisplayItem or a subtree in the new paint DisplayItemList,
// to indicate that the DrawingDisplayItem/subtree has not been changed and should be replaced with
// the cached DrawingDisplayItem/subtree when merging new paint list to cached paint list.
class CachedDisplayItem final : public DisplayItem {
public:
    CachedDisplayItem(const DisplayItemClient& client, Type type)
        : DisplayItem(client, type, sizeof(*this))
    {
        ASSERT(isCachedType(type));
    }

private:
    // CachedDisplayItem is never replayed or appended to WebDisplayItemList.
    void replay(GraphicsContext&) const final { ASSERT_NOT_REACHED(); }
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const final { ASSERT_NOT_REACHED(); }
};

} // namespace blink

#endif // CachedDisplayItem_h
