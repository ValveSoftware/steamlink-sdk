// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_ID_H_
#define CC_SURFACES_SURFACE_ID_H_

namespace cc {

struct SurfaceId {
  SurfaceId() : id(0) {}
  explicit SurfaceId(int id) : id(id) {}

  bool is_null() const { return id == 0; }

  int id;
};

inline bool operator==(const SurfaceId& a, const SurfaceId& b) {
  return a.id == b.id;
}

inline bool operator!=(const SurfaceId& a, const SurfaceId& b) {
  return !(a == b);
}

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_ID_H_
