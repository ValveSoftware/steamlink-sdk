// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DetailsMarkerPainter_h
#define DetailsMarkerPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class Path;
class LayoutPoint;
class LayoutDetailsMarker;

class DetailsMarkerPainter {
    STACK_ALLOCATED();
public:
    DetailsMarkerPainter(const LayoutDetailsMarker& layoutDetailsMarker) : m_layoutDetailsMarker(layoutDetailsMarker) { }

    void paint(const PaintInfo&, const LayoutPoint& paintOffset);

private:
    Path getCanonicalPath() const;
    Path getPath(const LayoutPoint& origin) const;

    const LayoutDetailsMarker& m_layoutDetailsMarker;
};

} // namespace blink

#endif // DetailsMarkerPainter_h
