// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/fake_plane_info.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/mock_hardware_display_plane_manager.h"
#include "ui/ozone/platform/drm/gpu/mock_scanout_buffer.h"

namespace {

const ui::FakePlaneInfo kOnePlanePerCrtc[] = {{10, 1}, {20, 2}};
const ui::FakePlaneInfo kTwoPlanesPerCrtc[] = {{10, 1},
                                               {11, 1},
                                               {20, 2},
                                               {21, 2}};
const ui::FakePlaneInfo kOnePlanePerCrtcWithShared[] = {{10, 1},
                                                        {20, 2},
                                                        {50, 3}};
const uint32_t kDummyFormat = 0;
const gfx::Size kDefaultBufferSize(2, 2);

class HardwareDisplayPlaneManagerTest : public testing::Test {
 public:
  HardwareDisplayPlaneManagerTest() {}

  void SetUp() override;

 protected:
  std::unique_ptr<ui::MockHardwareDisplayPlaneManager> plane_manager_;
  ui::HardwareDisplayPlaneList state_;
  std::vector<uint32_t> default_crtcs_;
  scoped_refptr<ui::ScanoutBuffer> fake_buffer_;
  scoped_refptr<ui::MockDrmDevice> fake_drm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlaneManagerTest);
};

void HardwareDisplayPlaneManagerTest::SetUp() {
  fake_buffer_ = new ui::MockScanoutBuffer(kDefaultBufferSize);
  default_crtcs_.push_back(100);
  default_crtcs_.push_back(200);

  fake_drm_ = new ui::MockDrmDevice(false, default_crtcs_, 2);
  plane_manager_.reset(
      new ui::MockHardwareDisplayPlaneManager(fake_drm_.get()));
}

TEST_F(HardwareDisplayPlaneManagerTest, SinglePlaneAssignment) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kOnePlanePerCrtc, arraysize(kOnePlanePerCrtc),
                              default_crtcs_);
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(1, plane_manager_->plane_count());
}

TEST_F(HardwareDisplayPlaneManagerTest, BadCrtc) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kOnePlanePerCrtc, arraysize(kOnePlanePerCrtc),
                              default_crtcs_);
  EXPECT_FALSE(
      plane_manager_->AssignOverlayPlanes(&state_, assigns, 1, nullptr));
}

TEST_F(HardwareDisplayPlaneManagerTest, MultiplePlaneAssignment) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kTwoPlanesPerCrtc, arraysize(kTwoPlanesPerCrtc),
                              default_crtcs_);
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(2, plane_manager_->plane_count());
}

TEST_F(HardwareDisplayPlaneManagerTest, NotEnoughPlanes) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kOnePlanePerCrtc, arraysize(kOnePlanePerCrtc),
                              default_crtcs_);

  EXPECT_FALSE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                   default_crtcs_[0], nullptr));
}

TEST_F(HardwareDisplayPlaneManagerTest, MultipleCrtcs) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kOnePlanePerCrtc, arraysize(kOnePlanePerCrtc),
                              default_crtcs_);

  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[1], nullptr));
  EXPECT_EQ(2, plane_manager_->plane_count());
}

TEST_F(HardwareDisplayPlaneManagerTest, MultiplePlanesAndCrtcs) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kTwoPlanesPerCrtc, arraysize(kTwoPlanesPerCrtc),
                              default_crtcs_);
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[1], nullptr));
  EXPECT_EQ(4, plane_manager_->plane_count());
}

TEST_F(HardwareDisplayPlaneManagerTest, MultipleFrames) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kTwoPlanesPerCrtc, arraysize(kTwoPlanesPerCrtc),
                              default_crtcs_);

  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(1, plane_manager_->plane_count());
  // Pretend we committed the frame.
  state_.plane_list.swap(state_.old_plane_list);
  plane_manager_->BeginFrame(&state_);
  ui::HardwareDisplayPlane* old_plane = state_.old_plane_list[0];
  // The same plane should be used.
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(2, plane_manager_->plane_count());
  EXPECT_EQ(state_.plane_list[0], old_plane);
}

TEST_F(HardwareDisplayPlaneManagerTest, MultipleFramesDifferentPlanes) {
  ui::OverlayPlaneList assigns;
  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  plane_manager_->InitForTest(kTwoPlanesPerCrtc, arraysize(kTwoPlanesPerCrtc),
                              default_crtcs_);

  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(1, plane_manager_->plane_count());
  // The other plane should be used.
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  EXPECT_EQ(2, plane_manager_->plane_count());
  EXPECT_NE(state_.plane_list[0], state_.plane_list[1]);
}

TEST_F(HardwareDisplayPlaneManagerTest, SharedPlanes) {
  ui::OverlayPlaneList assigns;
  scoped_refptr<ui::MockScanoutBuffer> buffer =
      new ui::MockScanoutBuffer(gfx::Size(1, 1));

  assigns.push_back(ui::OverlayPlane(fake_buffer_));
  assigns.push_back(ui::OverlayPlane(buffer));
  plane_manager_->InitForTest(kOnePlanePerCrtcWithShared,
                              arraysize(kOnePlanePerCrtcWithShared),
                              default_crtcs_);

  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[1], nullptr));
  EXPECT_EQ(2, plane_manager_->plane_count());
  // The shared plane is now unavailable for use by the other CRTC.
  EXPECT_FALSE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                   default_crtcs_[0], nullptr));
}

TEST_F(HardwareDisplayPlaneManagerTest, CheckFramebufferFormatMatch) {
  ui::OverlayPlaneList assigns;
  scoped_refptr<ui::MockScanoutBuffer> buffer =
      new ui::MockScanoutBuffer(kDefaultBufferSize, kDummyFormat);
  assigns.push_back(ui::OverlayPlane(buffer));
  plane_manager_->InitForTest(kOnePlanePerCrtc, arraysize(kOnePlanePerCrtc),
                              default_crtcs_);
  plane_manager_->BeginFrame(&state_);
  // This should return false as plane manager creates planes which support
  // DRM_FORMAT_XRGB8888 while buffer returns kDummyFormat as its pixelFormat.
  EXPECT_FALSE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                   default_crtcs_[0], nullptr));
  assigns.clear();
  scoped_refptr<ui::MockScanoutBuffer> xrgb_buffer =
      new ui::MockScanoutBuffer(kDefaultBufferSize);
  assigns.push_back(ui::OverlayPlane(xrgb_buffer));
  plane_manager_->BeginFrame(&state_);
  EXPECT_TRUE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                  default_crtcs_[0], nullptr));
  plane_manager_->BeginFrame(&state_);
  EXPECT_FALSE(plane_manager_->AssignOverlayPlanes(&state_, assigns,
                                                   default_crtcs_[0], nullptr));
}

TEST(HardwareDisplayPlaneManagerLegacyTest, UnusedPlanesAreReleased) {
  std::vector<uint32_t> crtcs;
  crtcs.push_back(100);

  scoped_refptr<ui::MockDrmDevice> drm = new ui::MockDrmDevice(false, crtcs, 2);
  ui::OverlayPlaneList assigns;
  scoped_refptr<ui::MockScanoutBuffer> primary_buffer =
      new ui::MockScanoutBuffer(kDefaultBufferSize);
  scoped_refptr<ui::MockScanoutBuffer> overlay_buffer =
      new ui::MockScanoutBuffer(gfx::Size(1, 1));
  assigns.push_back(ui::OverlayPlane(primary_buffer));
  assigns.push_back(ui::OverlayPlane(overlay_buffer));
  ui::HardwareDisplayPlaneList hdpl;
  ui::CrtcController crtc(drm, crtcs[0], 0);
  drm->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(drm->plane_manager()->AssignOverlayPlanes(&hdpl, assigns,
                                                        crtcs[0], &crtc));
  EXPECT_TRUE(drm->plane_manager()->Commit(&hdpl, false));
  assigns.clear();
  assigns.push_back(ui::OverlayPlane(primary_buffer));
  drm->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(drm->plane_manager()->AssignOverlayPlanes(&hdpl, assigns,
                                                        crtcs[0], &crtc));
  EXPECT_EQ(0, drm->get_overlay_clear_call_count());
  EXPECT_TRUE(drm->plane_manager()->Commit(&hdpl, false));
  EXPECT_EQ(1, drm->get_overlay_clear_call_count());
}

}  // namespace
