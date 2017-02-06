// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "components/exo/buffer.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

using SurfaceTest = test::ExoTestBase;

void ReleaseBuffer(int* release_buffer_call_count) {
  (*release_buffer_call_count)++;
}

TEST_F(SurfaceTest, Attach) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  // Set the release callback that will be run when buffer is no longer in use.
  int release_buffer_call_count = 0;
  buffer->set_release_callback(
      base::Bind(&ReleaseBuffer, base::Unretained(&release_buffer_call_count)));

  std::unique_ptr<Surface> surface(new Surface);

  // Attach the buffer to surface1.
  surface->Attach(buffer.get());
  surface->Commit();

  // Commit without calling Attach() should have no effect.
  surface->Commit();
  EXPECT_EQ(0, release_buffer_call_count);

  // Attach a null buffer to surface, this should release the previously
  // attached buffer.
  surface->Attach(nullptr);
  surface->Commit();
  ASSERT_EQ(1, release_buffer_call_count);
}

TEST_F(SurfaceTest, Damage) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  // Attach the buffer to the surface. This will update the pending bounds of
  // the surface to the buffer size.
  surface->Attach(buffer.get());

  // Mark areas inside the bounds of the surface as damaged. This should result
  // in pending damage.
  surface->Damage(gfx::Rect(0, 0, 10, 10));
  surface->Damage(gfx::Rect(10, 10, 10, 10));
  EXPECT_TRUE(surface->HasPendingDamageForTesting(gfx::Rect(0, 0, 10, 10)));
  EXPECT_TRUE(surface->HasPendingDamageForTesting(gfx::Rect(10, 10, 10, 10)));
  EXPECT_FALSE(surface->HasPendingDamageForTesting(gfx::Rect(5, 5, 10, 10)));
}

void SetFrameTime(base::TimeTicks* result, base::TimeTicks frame_time) {
  *result = frame_time;
}

TEST_F(SurfaceTest, RequestFrameCallback) {
  std::unique_ptr<Surface> surface(new Surface);

  base::TimeTicks frame_time;
  surface->RequestFrameCallback(
      base::Bind(&SetFrameTime, base::Unretained(&frame_time)));
  surface->Commit();

  // Callback should not run synchronously.
  EXPECT_TRUE(frame_time.is_null());
}

TEST_F(SurfaceTest, SetOpaqueRegion) {
  std::unique_ptr<Surface> surface(new Surface);

  // Setting a non-empty opaque region should succeed.
  surface->SetOpaqueRegion(SkRegion(SkIRect::MakeWH(256, 256)));

  // Setting an empty opaque region should succeed.
  surface->SetOpaqueRegion(SkRegion(SkIRect::MakeEmpty()));
}

TEST_F(SurfaceTest, SetInputRegion) {
  std::unique_ptr<Surface> surface(new Surface);

  // Setting a non-empty input region should succeed.
  surface->SetInputRegion(SkRegion(SkIRect::MakeWH(256, 256)));

  // Setting an empty input region should succeed.
  surface->SetInputRegion(SkRegion(SkIRect::MakeEmpty()));
}

TEST_F(SurfaceTest, SetBufferScale) {
  gfx::Size buffer_size(512, 512);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  // This will update the bounds of the surface and take the buffer scale into
  // account.
  const float kBufferScale = 2.0f;
  surface->Attach(buffer.get());
  surface->SetBufferScale(kBufferScale);
  surface->Commit();
  EXPECT_EQ(
      gfx::ScaleToFlooredSize(buffer_size, 1.0f / kBufferScale).ToString(),
      surface->window()->bounds().size().ToString());
  EXPECT_EQ(
      gfx::ScaleToFlooredSize(buffer_size, 1.0f / kBufferScale).ToString(),
      surface->content_size().ToString());
}

TEST_F(SurfaceTest, RecreateLayer) {
  gfx::Size buffer_size(512, 512);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  surface->Attach(buffer.get());
  surface->Commit();

  EXPECT_EQ(buffer_size, surface->window()->bounds().size());
  EXPECT_EQ(buffer_size, surface->window()->layer()->bounds().size());
  std::unique_ptr<ui::LayerTreeOwner> old_layer_owner =
      ::wm::RecreateLayers(surface->window(), nullptr);
  EXPECT_EQ(buffer_size, surface->window()->bounds().size());
  EXPECT_EQ(buffer_size, surface->window()->layer()->bounds().size());
  EXPECT_EQ(buffer_size, old_layer_owner->root()->bounds().size());
  EXPECT_TRUE(surface->window()->layer()->has_external_content());
  EXPECT_TRUE(old_layer_owner->root()->has_external_content());
}

TEST_F(SurfaceTest, SetViewport) {
  gfx::Size buffer_size(1, 1);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  // This will update the bounds of the surface and take the viewport into
  // account.
  surface->Attach(buffer.get());
  gfx::Size viewport(256, 256);
  surface->SetViewport(viewport);
  surface->Commit();
  EXPECT_EQ(viewport.ToString(), surface->content_size().ToString());

  // This will update the bounds of the surface and take the viewport2 into
  // account.
  gfx::Size viewport2(512, 512);
  surface->SetViewport(viewport2);
  surface->Commit();
  EXPECT_EQ(viewport2.ToString(),
            surface->window()->bounds().size().ToString());
  EXPECT_EQ(viewport2.ToString(), surface->content_size().ToString());
}

TEST_F(SurfaceTest, SetCrop) {
  gfx::Size buffer_size(16, 16);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  surface->Attach(buffer.get());
  gfx::Size crop_size(12, 12);
  surface->SetCrop(gfx::RectF(gfx::PointF(2.0, 2.0), gfx::SizeF(crop_size)));
  surface->Commit();
  EXPECT_EQ(crop_size.ToString(),
            surface->window()->bounds().size().ToString());
  EXPECT_EQ(crop_size.ToString(), surface->content_size().ToString());
}

const cc::DelegatedFrameData* GetFrameFromSurface(Surface* surface) {
  cc::SurfaceId surface_id = surface->surface_id();
  cc::SurfaceManager* surface_manager =
      aura::Env::GetInstance()->context_factory()->GetSurfaceManager();
  const cc::CompositorFrame& frame =
      surface_manager->GetSurfaceForId(surface_id)->GetEligibleFrame();
  return frame.delegated_frame_data.get();
}

TEST_F(SurfaceTest, SetBlendMode) {
  gfx::Size buffer_size(1, 1);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  surface->Attach(buffer.get());
  surface->SetBlendMode(SkXfermode::kSrc_Mode);
  surface->Commit();

  const cc::DelegatedFrameData* frame_data = GetFrameFromSurface(surface.get());
  ASSERT_EQ(1u, frame_data->render_pass_list.size());
  ASSERT_EQ(1u, frame_data->render_pass_list.back()->quad_list.size());
  EXPECT_FALSE(frame_data->render_pass_list.back()
                   ->quad_list.back()
                   ->ShouldDrawWithBlending());
}

TEST_F(SurfaceTest, SetAlpha) {
  gfx::Size buffer_size(1, 1);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  surface->Attach(buffer.get());
  surface->SetAlpha(0.5f);
  surface->Commit();
}

TEST_F(SurfaceTest, Commit) {
  std::unique_ptr<Surface> surface(new Surface);

  // Calling commit without a buffer should succeed.
  surface->Commit();
}

}  // namespace
}  // namespace exo
