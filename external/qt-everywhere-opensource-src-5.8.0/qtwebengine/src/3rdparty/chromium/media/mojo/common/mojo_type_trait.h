// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_COMMON_MOJO_TYPE_TRAIT_H_
#define MEDIA_MOJO_COMMON_MOJO_TYPE_TRAIT_H_

#include "media/base/media_keys.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/string.h"

namespace media {

// A trait struct to help get the corresponding mojo type for a native type.
// By default the mojo type is the same as the native type. This works well for
// primitive types like integers.
template <typename T>
struct MojoTypeTrait {
  typedef T MojoType;
  static MojoType DefaultValue() { return MojoType(); }
};

// Specialization for string.
template <>
struct MojoTypeTrait<std::string> {
  typedef mojo::String MojoType;
  static MojoType DefaultValue() { return MojoType(); }
};

}  // namespace media

#endif  // MEDIA_MOJO_COMMON_MOJO_TYPE_TRAIT_H_
