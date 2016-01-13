// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_SIZE_H_
#define UI_GFX_GEOMETRY_SIZE_H_

#include <string>

#include "base/compiler_specific.h"
#include "ui/gfx/geometry/size_base.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/gfx_export.h"

#if defined(OS_WIN)
typedef struct tagSIZE SIZE;
#elif defined(OS_IOS)
#include <CoreGraphics/CoreGraphics.h>
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

// A size has width and height values.
class GFX_EXPORT Size : public SizeBase<Size, int> {
 public:
  Size() : SizeBase<Size, int>(0, 0) {}
  Size(int width, int height) : SizeBase<Size, int>(width, height) {}
#if defined(OS_MACOSX)
  explicit Size(const CGSize& s);
#endif

  ~Size() {}

#if defined(OS_MACOSX)
  Size& operator=(const CGSize& s);
#endif

#if defined(OS_WIN)
  SIZE ToSIZE() const;
#elif defined(OS_MACOSX)
  CGSize ToCGSize() const;
#endif

  operator SizeF() const {
    return SizeF(width(), height());
  }

  std::string ToString() const;
};

inline bool operator==(const Size& lhs, const Size& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

inline bool operator!=(const Size& lhs, const Size& rhs) {
  return !(lhs == rhs);
}

#if !defined(COMPILER_MSVC)
extern template class SizeBase<Size, int>;
#endif

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_SIZE_H_
