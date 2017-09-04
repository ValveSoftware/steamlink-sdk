// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/common/test_common_custom_types.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

class TestString16Impl : public mojo::common::test::blink::TestString16 {
 public:
  explicit TestString16Impl(
      mojo::common::test::blink::TestString16Request request)
      : m_binding(this, std::move(request)) {}

  // TestString16 implementation:
  void BounceString16(const String& in,
                      const BounceString16Callback& callback) override {
    callback.Run(in);
  }

 private:
  mojo::Binding<mojo::common::test::blink::TestString16> m_binding;
};

class CommonCustomTypesStructTraitsTest : public testing::Test {
 protected:
  CommonCustomTypesStructTraitsTest() {}
  ~CommonCustomTypesStructTraitsTest() override {}

 private:
  base::MessageLoop m_messageLoop;

  DISALLOW_COPY_AND_ASSIGN(CommonCustomTypesStructTraitsTest);
};

}  // namespace

TEST_F(CommonCustomTypesStructTraitsTest, String16) {
  mojo::common::test::blink::TestString16Ptr ptr;
  TestString16Impl impl(GetProxy(&ptr));

  // |str| is 8-bit.
  String str = String::fromUTF8("hello world");
  String output;

  ptr->BounceString16(str, &output);
  ASSERT_EQ(str, output);

  // Replace the "o"s in "hello world" with "o"s with acute, so that |str| is
  // 16-bit.
  str = String::fromUTF8("hell\xC3\xB3 w\xC3\xB3rld");

  ptr->BounceString16(str, &output);
  ASSERT_EQ(str, output);
}

TEST_F(CommonCustomTypesStructTraitsTest, EmptyString16) {
  mojo::common::test::blink::TestString16Ptr ptr;
  TestString16Impl impl(GetProxy(&ptr));

  String str = String::fromUTF8("");
  String output;

  ptr->BounceString16(str, &output);

  ASSERT_EQ(str, output);
}

}  // namespace blink
