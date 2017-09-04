// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/SharedGpuContext.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using testing::Test;

namespace blink {

namespace {

class SharedGpuContextTest : public Test {
 public:
  void SetUp() override {
    SharedGpuContext::setContextProviderFactoryForTesting([this] {
      m_gl.setIsContextLost(false);
      return std::unique_ptr<WebGraphicsContext3DProvider>(
          new FakeWebGraphicsContext3DProvider(&m_gl));
    });
  }

  void TearDown() override {
    SharedGpuContext::setContextProviderFactoryForTesting(nullptr);
  }

  FakeGLES2Interface m_gl;
};

TEST_F(SharedGpuContextTest, contextLossAutoRecovery) {
  EXPECT_TRUE(SharedGpuContext::isValid());
  unsigned contextId = SharedGpuContext::contextId();
  m_gl.setIsContextLost(true);
  EXPECT_FALSE(SharedGpuContext::isValidWithoutRestoring());
  EXPECT_TRUE(SharedGpuContext::isValid());
  EXPECT_NE(contextId, SharedGpuContext::contextId());
}

}  // unnamed namespace

}  // blink
