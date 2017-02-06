// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"

#include <stdint.h>

#include <memory>

#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;

namespace gpu {
namespace gles2 {

class ContextGroupTest : public GpuServiceTest {
 public:
  static const bool kBindGeneratesResource = false;

  ContextGroupTest() {}

 protected:
  void SetUp() override {
    GpuServiceTest::SetUp();
    decoder_.reset(new MockGLES2Decoder());
    scoped_refptr<FeatureInfo> feature_info = new FeatureInfo;
    group_ = scoped_refptr<ContextGroup>(
        new ContextGroup(gpu_preferences_, NULL, NULL, NULL, NULL, feature_info,
                         kBindGeneratesResource, nullptr));
  }

  GpuPreferences gpu_preferences_;
  std::unique_ptr<MockGLES2Decoder> decoder_;
  scoped_refptr<ContextGroup> group_;
};

TEST_F(ContextGroupTest, Basic) {
  // Test it starts off uninitialized.
  EXPECT_EQ(0u, group_->max_vertex_attribs());
  EXPECT_EQ(0u, group_->max_texture_units());
  EXPECT_EQ(0u, group_->max_texture_image_units());
  EXPECT_EQ(0u, group_->max_vertex_texture_image_units());
  EXPECT_EQ(0u, group_->max_fragment_uniform_vectors());
  EXPECT_EQ(0u, group_->max_varying_vectors());
  EXPECT_EQ(0u, group_->max_vertex_uniform_vectors());
  EXPECT_TRUE(group_->buffer_manager() == NULL);
  EXPECT_TRUE(group_->framebuffer_manager() == NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() == NULL);
  EXPECT_TRUE(group_->texture_manager() == NULL);
  EXPECT_TRUE(group_->program_manager() == NULL);
  EXPECT_TRUE(group_->shader_manager() == NULL);
}

TEST_F(ContextGroupTest, InitializeNoExtensions) {
  TestHelper::SetupContextGroupInitExpectations(
      gl_.get(), DisallowedFeatures(), "", "", kBindGeneratesResource);
  group_->Initialize(decoder_.get(), CONTEXT_TYPE_OPENGLES2,
                     DisallowedFeatures());
  EXPECT_EQ(static_cast<uint32_t>(TestHelper::kNumVertexAttribs),
            group_->max_vertex_attribs());
  EXPECT_EQ(static_cast<uint32_t>(TestHelper::kNumTextureUnits),
            group_->max_texture_units());
  EXPECT_EQ(static_cast<uint32_t>(TestHelper::kMaxTextureImageUnits),
            group_->max_texture_image_units());
  EXPECT_EQ(static_cast<uint32_t>(TestHelper::kMaxVertexTextureImageUnits),
            group_->max_vertex_texture_image_units());
  EXPECT_EQ(static_cast<uint32_t>(TestHelper::kMaxFragmentUniformVectors),
            group_->max_fragment_uniform_vectors());
  EXPECT_EQ(static_cast<uint32_t>(TestHelper::kMaxVaryingVectors),
            group_->max_varying_vectors());
  EXPECT_EQ(static_cast<uint32_t>(TestHelper::kMaxVertexUniformVectors),
            group_->max_vertex_uniform_vectors());
  EXPECT_TRUE(group_->buffer_manager() != NULL);
  EXPECT_TRUE(group_->framebuffer_manager() != NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() != NULL);
  EXPECT_TRUE(group_->texture_manager() != NULL);
  EXPECT_TRUE(group_->program_manager() != NULL);
  EXPECT_TRUE(group_->shader_manager() != NULL);

  group_->Destroy(decoder_.get(), false);
  EXPECT_TRUE(group_->buffer_manager() == NULL);
  EXPECT_TRUE(group_->framebuffer_manager() == NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() == NULL);
  EXPECT_TRUE(group_->texture_manager() == NULL);
  EXPECT_TRUE(group_->program_manager() == NULL);
  EXPECT_TRUE(group_->shader_manager() == NULL);
}

TEST_F(ContextGroupTest, MultipleContexts) {
  std::unique_ptr<MockGLES2Decoder> decoder2_(new MockGLES2Decoder());
  TestHelper::SetupContextGroupInitExpectations(
      gl_.get(), DisallowedFeatures(), "", "", kBindGeneratesResource);
  EXPECT_TRUE(group_->Initialize(decoder_.get(), CONTEXT_TYPE_OPENGLES2,
                                 DisallowedFeatures()));
  EXPECT_FALSE(group_->Initialize(decoder2_.get(), CONTEXT_TYPE_WEBGL1,
                                  DisallowedFeatures()));
  EXPECT_FALSE(group_->Initialize(decoder2_.get(), CONTEXT_TYPE_WEBGL2,
                                  DisallowedFeatures()));
  EXPECT_FALSE(group_->Initialize(decoder2_.get(), CONTEXT_TYPE_OPENGLES3,
                                  DisallowedFeatures()));
  EXPECT_TRUE(group_->Initialize(decoder2_.get(), CONTEXT_TYPE_OPENGLES2,
                                 DisallowedFeatures()));

  EXPECT_TRUE(group_->buffer_manager() != NULL);
  EXPECT_TRUE(group_->framebuffer_manager() != NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() != NULL);
  EXPECT_TRUE(group_->texture_manager() != NULL);
  EXPECT_TRUE(group_->program_manager() != NULL);
  EXPECT_TRUE(group_->shader_manager() != NULL);

  group_->Destroy(decoder_.get(), false);

  EXPECT_TRUE(group_->buffer_manager() != NULL);
  EXPECT_TRUE(group_->framebuffer_manager() != NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() != NULL);
  EXPECT_TRUE(group_->texture_manager() != NULL);
  EXPECT_TRUE(group_->program_manager() != NULL);
  EXPECT_TRUE(group_->shader_manager() != NULL);

  group_->Destroy(decoder2_.get(), false);

  EXPECT_TRUE(group_->buffer_manager() == NULL);
  EXPECT_TRUE(group_->framebuffer_manager() == NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() == NULL);
  EXPECT_TRUE(group_->texture_manager() == NULL);
  EXPECT_TRUE(group_->program_manager() == NULL);
  EXPECT_TRUE(group_->shader_manager() == NULL);
}

}  // namespace gles2
}  // namespace gpu


