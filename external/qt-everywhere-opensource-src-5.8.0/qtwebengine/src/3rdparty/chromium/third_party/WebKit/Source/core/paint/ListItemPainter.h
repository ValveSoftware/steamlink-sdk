// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ListItemPainter_h
#define ListItemPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutListItem;
class LayoutPoint;

class ListItemPainter {
    STACK_ALLOCATED();
public:
    ListItemPainter(const LayoutListItem& layoutListItem) : m_layoutListItem(layoutListItem) { }

    void paint(const PaintInfo&, const LayoutPoint& paintOffset);

private:
    const LayoutListItem& m_layoutListItem;
};

} // namespace blink

#endif // ListItemPainter_h
