// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_CALLBACK_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_CALLBACK_INTERNAL_H_

namespace mojo {
class String;

namespace internal {

template <typename T>
struct Callback_ParamTraits {
  typedef T ForwardType;
};

template <>
struct Callback_ParamTraits<String> {
  typedef const String& ForwardType;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_CALLBACK_INTERNAL_H_
