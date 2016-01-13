// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_SIZE_F_H_
#define UI_GFX_GEOMETRY_SIZE_F_H_

#include <string>

#include "base/compiler_specific.h"
#include "ui/gfx/geometry/size_base.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// A floating version of gfx::Size.
class GFX_EXPORT SizeF : public SizeBase<SizeF, float> {
 public:
  SizeF() : SizeBase<SizeF, float>(0, 0) {}
  SizeF(float width, float height) : SizeBase<SizeF, float>(width, height) {}
  ~SizeF() {}

  void Scale(float scale) {
    Scale(scale, scale);
  }

  void Scale(float x_scale, float y_scale) {
    SetSize(width() * x_scale, height() * y_scale);
  }

  std::string ToString() const;
};

inline bool operator==(const SizeF& lhs, const SizeF& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

inline bool operator!=(const SizeF& lhs, const SizeF& rhs) {
  return !(lhs == rhs);
}

GFX_EXPORT SizeF ScaleSize(const SizeF& p, float x_scale, float y_scale);

inline SizeF ScaleSize(const SizeF& p, float scale) {
  return ScaleSize(p, scale, scale);
}

#if !defined(COMPILER_MSVC)
extern template class SizeBase<SizeF, float>;
#endif

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_SIZE_F_H_
