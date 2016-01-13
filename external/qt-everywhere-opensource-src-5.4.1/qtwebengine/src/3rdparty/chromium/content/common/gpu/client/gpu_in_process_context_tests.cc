// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <cmath>
#include <string>
#include <vector>

#include "content/public/test/unittest_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace {

using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;

class ContextTestBase : public testing::Test {
 public:
  virtual void SetUp() {
    blink::WebGraphicsContext3D::Attributes attributes;
    bool lose_context_when_out_of_memory = false;
    typedef WebGraphicsContext3DInProcessCommandBufferImpl WGC3DIPCBI;
    context_ = WGC3DIPCBI::CreateOffscreenContext(
        attributes, lose_context_when_out_of_memory);
    context_->makeContextCurrent();
    context_support_ = context_->GetContextSupport();
  }

  virtual void TearDown() {
    context_.reset(NULL);
  }

 protected:
  scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context_;
  gpu::ContextSupport* context_support_;
};

}  // namespace

// Include the actual tests.
#define CONTEXT_TEST_F TEST_F
#include "content/common/gpu/client/gpu_context_tests.h"
