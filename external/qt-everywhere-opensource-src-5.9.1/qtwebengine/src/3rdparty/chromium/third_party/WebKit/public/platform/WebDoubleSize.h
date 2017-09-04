// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDoubleSize_h
#define WebDoubleSize_h

#include "WebCommon.h"

#if INSIDE_BLINK
#include "platform/geometry/DoubleSize.h"
#else
#include <ui/gfx/geometry/size_f.h>
#include <ui/gfx/geometry/vector2d_f.h>
#endif

namespace blink {

class WebDoubleSize {
 public:
  bool isEmpty() const { return m_width <= 0 || m_height <= 0; }

  WebDoubleSize() : m_width(0), m_height(0) {}

  WebDoubleSize(double m_width, double m_height)
      : m_width(m_width), m_height(m_height) {}

#if INSIDE_BLINK
  WebDoubleSize(const DoubleSize& size)
      : m_width(size.width()), m_height(size.height()) {}

  WebDoubleSize& operator=(const DoubleSize& size) {
    m_width = size.width();
    m_height = size.height();
    return *this;
  }

  operator DoubleSize() const { return DoubleSize(m_width, m_height); }
#else
  WebDoubleSize(const gfx::SizeF& size)
      : m_width(size.width()), m_height(size.height()) {}

  WebDoubleSize(const gfx::Vector2dF& vector)
      : m_width(vector.x()), m_height(vector.y()) {}

  WebDoubleSize& operator=(const gfx::SizeF& size) {
    m_width = size.width();
    m_height = size.height();
    return *this;
  }

  WebDoubleSize& operator=(const gfx::Vector2dF& vector) {
    m_width = vector.x();
    m_height = vector.y();
    return *this;
  }
#endif

  double width() const { return m_width; }
  double height() const { return m_height; }

 private:
  double m_width;
  double m_height;
};

inline bool operator==(const WebDoubleSize& a, const WebDoubleSize& b) {
  return a.width() == b.width() && a.height() == b.height();
}

inline bool operator!=(const WebDoubleSize& a, const WebDoubleSize& b) {
  return !(a == b);
}

}  // namespace blink

#endif
