// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VideoPainter_h
#define VideoPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutVideo;

class VideoPainter {
  STACK_ALLOCATED();

 public:
  VideoPainter(const LayoutVideo& layoutVideo) : m_layoutVideo(layoutVideo) {}

  void paintReplaced(const PaintInfo&, const LayoutPoint&);

 private:
  const LayoutVideo& m_layoutVideo;
};

}  // namespace blink

#endif  // VideoPainter_h
