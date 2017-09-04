// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_SizeAssertions_h
#define WTF_SizeAssertions_h

namespace WTF {

// The ASSERT_SIZE macro can be used to check that a given struct is the same
// size as a class. This is useful to visualize where the space is being used in
// a class, as well as give a useful compile error message when the size doesn't
// match the expected value.
template <class T, class U>
struct assert_size {
  template <int ActualSize, int ExpectedSize>
  struct assertSizeEqual {
    static_assert(ActualSize == ExpectedSize, "Class should stay small");
    static const bool innerValue = true;
  };
  static const bool value = assertSizeEqual<sizeof(T), sizeof(U)>::innerValue;
};

}  // namespace WTF

#define ASSERT_SIZE(className, sameSizeAsClassName) \
  static_assert(WTF::assert_size<className, sameSizeAsClassName>::value, "");

#endif  // WTF_SizeAssertions_h
