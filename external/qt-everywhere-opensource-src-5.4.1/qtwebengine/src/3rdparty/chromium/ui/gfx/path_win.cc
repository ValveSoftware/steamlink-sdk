// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/path_win.h"

#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_gdi_object.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/path.h"

namespace gfx {

HRGN CreateHRGNFromSkRegion(const SkRegion& region) {
  base::win::ScopedRegion temp(::CreateRectRgn(0, 0, 0, 0));
  base::win::ScopedRegion result(::CreateRectRgn(0, 0, 0, 0));

  for (SkRegion::Iterator i(region); !i.done(); i.next()) {
    const SkIRect& rect = i.rect();
    ::SetRectRgn(temp, rect.left(), rect.top(), rect.right(), rect.bottom());
    ::CombineRgn(result, result, temp, RGN_OR);
  }

  return result.release();
}

HRGN CreateHRGNFromSkPath(const SkPath& path) {
  int point_count = path.getPoints(NULL, 0);
  scoped_ptr<SkPoint[]> points(new SkPoint[point_count]);
  path.getPoints(points.get(), point_count);
  scoped_ptr<POINT[]> windows_points(new POINT[point_count]);
  for (int i = 0; i < point_count; ++i) {
    windows_points[i].x = SkScalarRoundToInt(points[i].fX);
    windows_points[i].y = SkScalarRoundToInt(points[i].fY);
  }

  return ::CreatePolygonRgn(windows_points.get(), point_count, ALTERNATE);
}

// See path_aura.cc for Aura definition of these methods:
#if !defined(USE_AURA)

NativeRegion Path::CreateNativeRegion() const {
  return CreateHRGNFromSkPath(*this);
}

// static
NativeRegion Path::IntersectRegions(NativeRegion r1, NativeRegion r2) {
  HRGN dest = CreateRectRgn(0, 0, 1, 1);
  CombineRgn(dest, r1, r2, RGN_AND);
  return dest;
}

// static
NativeRegion Path::CombineRegions(NativeRegion r1, NativeRegion r2) {
  HRGN dest = CreateRectRgn(0, 0, 1, 1);
  CombineRgn(dest, r1, r2, RGN_OR);
  return dest;
}

// static
NativeRegion Path::SubtractRegion(NativeRegion r1, NativeRegion r2) {
  HRGN dest = CreateRectRgn(0, 0, 1, 1);
  CombineRgn(dest, r1, r2, RGN_DIFF);
  return dest;
}

#endif

}  // namespace gfx
