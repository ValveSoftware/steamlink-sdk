// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_SIZE_BASE_H_
#define UI_GFX_GEOMETRY_SIZE_BASE_H_

#include "ui/gfx/gfx_export.h"

namespace gfx {

// A size has width and height values.
template<typename Class, typename Type>
class GFX_EXPORT SizeBase {
 public:
  Type width() const { return width_; }
  Type height() const { return height_; }

  Type GetArea() const { return width_ * height_; }

  void SetSize(Type width, Type height) {
    set_width(width);
    set_height(height);
  }

  void Enlarge(Type width, Type height) {
    set_width(width_ + width);
    set_height(height_ + height);
  }

  void set_width(Type width) {
    width_ = width < 0 ? 0 : width;
  }
  void set_height(Type height) {
    height_ = height < 0 ? 0 : height;
  }

  void SetToMin(const Class& other) {
    width_ = width_ <= other.width_ ? width_ : other.width_;
    height_ = height_ <= other.height_ ? height_ : other.height_;
  }

  void SetToMax(const Class& other) {
    width_ = width_ >= other.width_ ? width_ : other.width_;
    height_ = height_ >= other.height_ ? height_ : other.height_;
  }

  bool IsEmpty() const {
    return (width_ == 0) || (height_ == 0);
  }

 protected:
  SizeBase(Type width, Type height)
      : width_(width < 0 ? 0 : width),
      height_(height < 0 ? 0 : height) {
  }

  // Destructor is intentionally made non virtual and protected.
  // Do not make this public.
  ~SizeBase() {}

 private:
  Type width_;
  Type height_;
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_SIZE_BASE_H_
