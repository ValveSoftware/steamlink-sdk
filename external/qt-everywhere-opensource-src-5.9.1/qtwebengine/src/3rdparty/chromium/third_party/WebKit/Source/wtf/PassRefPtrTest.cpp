// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/PassRefPtr.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefCounted.h"

namespace WTF {

class RefCountedClass : public RefCounted<RefCountedClass> {};

TEST(PassRefPtrTest, MoveConstructor) {
  PassRefPtr<RefCountedClass> oldPassRefPtr = adoptRef(new RefCountedClass());
  RefCountedClass* rawPtr = oldPassRefPtr.get();
  EXPECT_EQ(rawPtr->refCount(), 1);
  PassRefPtr<RefCountedClass> newPassRefPtr(std::move(oldPassRefPtr));
  EXPECT_EQ(oldPassRefPtr.get(), nullptr);
  EXPECT_EQ(newPassRefPtr.get(), rawPtr);
  EXPECT_EQ(rawPtr->refCount(), 1);
}

}  // namespace WTF
