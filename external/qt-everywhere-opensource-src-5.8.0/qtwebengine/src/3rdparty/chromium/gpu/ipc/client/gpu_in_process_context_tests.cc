// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_surface.h"

namespace {

class ContextTestBase : public testing::Test {
 public:
  void SetUp() override {
    gpu::gles2::ContextCreationAttribHelper attributes;
    attributes.alpha_size = 8;
    attributes.depth_size = 24;
    attributes.red_size = 8;
    attributes.green_size = 8;
    attributes.blue_size = 8;
    attributes.stencil_size = 8;
    attributes.samples = 4;
    attributes.sample_buffers = 1;
    attributes.bind_generates_resource = false;

    context_.reset(gpu::GLInProcessContext::Create(
        nullptr,                     /* service */
        nullptr,                     /* surface */
        true,                        /* offscreen */
        gfx::kNullAcceleratedWidget, /* window */
        nullptr,                     /* share_context */
        attributes, gpu::SharedMemoryLimits(),
        nullptr, /* gpu_memory_buffer_manager */
        nullptr /* image_factory */));
    gl_ = context_->GetImplementation();
    context_support_ = context_->GetImplementation();
  }

  void TearDown() override { context_.reset(NULL); }

 protected:
  gpu::gles2::GLES2Interface* gl_;
  gpu::ContextSupport* context_support_;

 private:
  std::unique_ptr<gpu::GLInProcessContext> context_;
};

}  // namespace

// Include the actual tests.
#define CONTEXT_TEST_F TEST_F
#include "gpu/ipc/client/gpu_context_tests.h"
