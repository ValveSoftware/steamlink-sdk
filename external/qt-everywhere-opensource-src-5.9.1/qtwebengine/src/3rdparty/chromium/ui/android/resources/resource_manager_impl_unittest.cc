// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "cc/animation/animation_host.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/test/stub_layer_tree_host_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/resources/system_ui_resource_type.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"


using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MatcherCast;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace ui {

class TestResourceManagerImpl : public ResourceManagerImpl {
 public:
  TestResourceManagerImpl(WindowAndroid* window_android)
      : ResourceManagerImpl(window_android) {}

  ~TestResourceManagerImpl() override {}

  void SetResourceAsLoaded(AndroidResourceType res_type, int res_id) {
    SkBitmap small_bitmap;
    SkCanvas canvas(small_bitmap);
    small_bitmap.allocPixels(
        SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
    canvas.drawColor(SK_ColorWHITE);
    small_bitmap.setImmutable();

    OnResourceReady(nullptr, nullptr, res_type, res_id,
                    gfx::ConvertToJavaBitmap(&small_bitmap), 0, 0, 0, 0, 0, 0,
                    0, 0);
  }

 protected:
  void PreloadResourceFromJava(AndroidResourceType res_type,
                               int res_id) override {}

  void RequestResourceFromJava(AndroidResourceType res_type,
                               int res_id) override {
    SetResourceAsLoaded(res_type, res_id);
  }
};

namespace {

const ui::SystemUIResourceType kTestResourceType = ui::OVERSCROLL_GLOW;

class MockUIResourceManager : public cc::UIResourceManager {
 public:
  MockUIResourceManager() {}

  MOCK_METHOD1(CreateUIResource, cc::UIResourceId(cc::UIResourceClient*));
  MOCK_METHOD1(DeleteUIResource, void(cc::UIResourceId));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUIResourceManager);
};

}  // namespace

class ResourceManagerTest : public testing::Test {
 public:
  ResourceManagerTest()
      : window_android_(WindowAndroid::CreateForTesting()),
        resource_manager_(window_android_) {
    resource_manager_.Init(&ui_resource_manager_);
  }

  ~ResourceManagerTest() override {
    window_android_->Destroy(nullptr, nullptr);
  }

  void PreloadResource(ui::SystemUIResourceType type) {
    resource_manager_.PreloadResource(ui::ANDROID_RESOURCE_TYPE_SYSTEM, type);
  }

  cc::UIResourceId GetUIResourceId(ui::SystemUIResourceType type) {
    return resource_manager_.GetUIResourceId(ui::ANDROID_RESOURCE_TYPE_SYSTEM,
                                             type);
  }

  void SetResourceAsLoaded(ui::SystemUIResourceType type) {
    resource_manager_.SetResourceAsLoaded(ui::ANDROID_RESOURCE_TYPE_SYSTEM,
                                          type);
  }

 private:
  base::MessageLoop message_loop_;
  WindowAndroid* window_android_;

 protected:
  MockUIResourceManager ui_resource_manager_;
  TestResourceManagerImpl resource_manager_;
  cc::StubLayerTreeHostClient stub_client_;
};

TEST_F(ResourceManagerTest, GetResource) {
  const cc::UIResourceId kResourceId = 99;
  EXPECT_CALL(ui_resource_manager_, CreateUIResource(_))
      .WillOnce(Return(kResourceId))
      .RetiresOnSaturation();
  EXPECT_EQ(kResourceId, GetUIResourceId(kTestResourceType));
}

TEST_F(ResourceManagerTest, PreloadEnsureResource) {
  const cc::UIResourceId kResourceId = 99;
  PreloadResource(kTestResourceType);
  EXPECT_CALL(ui_resource_manager_, CreateUIResource(_))
      .WillOnce(Return(kResourceId))
      .RetiresOnSaturation();
  SetResourceAsLoaded(kTestResourceType);
  EXPECT_EQ(kResourceId, GetUIResourceId(kTestResourceType));
}

TEST_F(ResourceManagerTest, ProcessCrushedSpriteFrameRects) {
  const size_t kNumFrames = 3;

  // Create input
  std::vector<int> frame0 = {35, 30, 38, 165, 18, 12, 0, 70, 0, 146, 72, 2};
  std::vector<int> frame1 = {};
  std::vector<int> frame2 = {0, 0, 73, 0, 72, 72};
  std::vector<std::vector<int>> frame_rects_vector;
  frame_rects_vector.push_back(frame0);
  frame_rects_vector.push_back(frame1);
  frame_rects_vector.push_back(frame2);

  // Create expected output
  CrushedSpriteResource::SrcDstRects expected_rects(kNumFrames);
  gfx::Rect frame0_rect0_src(38, 165, 18, 12);
  gfx::Rect frame0_rect0_dst(35, 30, 18, 12);
  gfx::Rect frame0_rect1_src(0, 146, 72, 2);
  gfx::Rect frame0_rect1_dst(0, 70, 72, 2);
  gfx::Rect frame2_rect0_src(73, 0, 72, 72);
  gfx::Rect frame2_rect0_dst(0, 0, 72, 72);
  expected_rects[0].push_back(
      std::pair<gfx::Rect, gfx::Rect>(frame0_rect0_src, frame0_rect0_dst));
  expected_rects[0].push_back(
      std::pair<gfx::Rect, gfx::Rect>(frame0_rect1_src, frame0_rect1_dst));
  expected_rects[2].push_back(
      std::pair<gfx::Rect, gfx::Rect>(frame2_rect0_src, frame2_rect0_dst));

  // Check actual against expected
  CrushedSpriteResource::SrcDstRects actual_rects =
      resource_manager_.ProcessCrushedSpriteFrameRects(frame_rects_vector);
  EXPECT_EQ(kNumFrames, actual_rects.size());
  for (size_t i = 0; i < kNumFrames; i++) {
    EXPECT_EQ(expected_rects[i].size(), actual_rects[i].size());
    for (size_t j = 0; j < actual_rects[i].size(); j++) {
      EXPECT_EQ(expected_rects[i][j], actual_rects[i][j]);
    }
  }
}

TEST_F(ResourceManagerTest, TestOnMemoryDumpEmitsData) {
  SetResourceAsLoaded(kTestResourceType);

  base::trace_event::MemoryDumpArgs dump_args = {
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump =
      base::MakeUnique<base::trace_event::ProcessMemoryDump>(nullptr,
                                                             dump_args);
  resource_manager_.OnMemoryDump(dump_args, process_memory_dump.get());
  const auto& allocator_dumps = process_memory_dump->allocator_dumps();
  const char* system_allocator_pool_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  size_t expected_dump_count = system_allocator_pool_name ? 2 : 1;
  EXPECT_EQ(expected_dump_count, allocator_dumps.size());
  for (const auto& dump : allocator_dumps) {
    ASSERT_TRUE(dump.first.find("ui/resource_manager") == 0 ||
                dump.first.find(system_allocator_pool_name) == 0);
  }
}

}  // namespace ui
