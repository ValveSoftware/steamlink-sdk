// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_BINDING_TEST_H_
#define EXTENSIONS_RENDERER_API_BINDING_TEST_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace gin {
class ContextHolder;
class IsolateHolder;
}

namespace extensions {

// A common unit test class for testing API bindings. Creates an isolate and an
// initial v8 context, and checks for v8 leaks at the end of the test.
class APIBindingTest : public testing::Test {
 protected:
  APIBindingTest();
  ~APIBindingTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  v8::Local<v8::Context> ContextLocal();

  // Returns the associated isolate. Defined out-of-line to avoid the include
  // for IsolateHolder in the header.
  v8::Isolate* isolate();

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  std::unique_ptr<gin::ContextHolder> context_holder_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingTest);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_BINDING_TEST_H_
