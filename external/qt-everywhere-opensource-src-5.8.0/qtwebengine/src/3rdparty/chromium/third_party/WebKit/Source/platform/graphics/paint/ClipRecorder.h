// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipRecorder_h
#define ClipRecorder_h

#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class GraphicsContext;

class PLATFORM_EXPORT ClipRecorder {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    WTF_MAKE_NONCOPYABLE(ClipRecorder);
public:
    ClipRecorder(GraphicsContext&, const DisplayItemClient&, DisplayItem::Type, const IntRect& clipRect);
    ~ClipRecorder();
private:
    const DisplayItemClient& m_client;
    GraphicsContext& m_context;
    DisplayItem::Type m_type;
};

} // namespace blink

#endif // ClipRecorder_h
