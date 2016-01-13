// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/size_f.h"

#include "base/strings/stringprintf.h"

namespace gfx {

template class SizeBase<SizeF, float>;

std::string SizeF::ToString() const {
  return base::StringPrintf("%fx%f", width(), height());
}

SizeF ScaleSize(const SizeF& s, float x_scale, float y_scale) {
  SizeF scaled_s(s);
  scaled_s.Scale(x_scale, y_scale);
  return scaled_s;
}

}  // namespace gfx
