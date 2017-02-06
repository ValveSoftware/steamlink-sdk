// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_scrollbar_layer.h"

#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_scrollbar.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;
using ::testing::_;

namespace cc {

namespace {

class MockScrollbar : public FakeScrollbar {
 public:
  MockScrollbar() : FakeScrollbar(true, true, true) {}
  MOCK_METHOD3(PaintPart,
               void(SkCanvas* canvas,
                    ScrollbarPart part,
                    const gfx::Rect& content_rect));
};

TEST(PaintedScrollbarLayerTest, NeedsPaint) {
  FakeLayerTreeHostClient fake_client_(FakeLayerTreeHostClient::DIRECT_3D);
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;

  layer_tree_host_ =
      FakeLayerTreeHost::Create(&fake_client_, &task_graph_runner_);
  RendererCapabilities renderer_capabilities;
  renderer_capabilities.max_texture_size = 2048;
  layer_tree_host_->set_renderer_capabilities(renderer_capabilities);

  MockScrollbar* scrollbar = new MockScrollbar();
  scoped_refptr<PaintedScrollbarLayer> scrollbar_layer =
      PaintedScrollbarLayer::Create(std::unique_ptr<Scrollbar>(scrollbar), 1);

  scrollbar_layer->SetIsDrawable(true);
  scrollbar_layer->SetBounds(gfx::Size(100, 100));

  layer_tree_host_->SetRootLayer(scrollbar_layer);
  EXPECT_EQ(scrollbar_layer->layer_tree_host(), layer_tree_host_.get());
  scrollbar_layer->SavePaintProperties();

  // Request no paint, but expect them to be painted because they have not
  // yet been initialized.
  scrollbar->set_needs_paint_thumb(false);
  scrollbar->set_needs_paint_track(false);
  EXPECT_CALL(*scrollbar, PaintPart(_, THUMB, _)).Times(1);
  EXPECT_CALL(*scrollbar, PaintPart(_, TRACK, _)).Times(1);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar);

  // The next update will paint nothing because the first update caused a paint.
  EXPECT_CALL(*scrollbar, PaintPart(_, THUMB, _)).Times(0);
  EXPECT_CALL(*scrollbar, PaintPart(_, TRACK, _)).Times(0);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar);

  // Enable the thumb.
  EXPECT_CALL(*scrollbar, PaintPart(_, THUMB, _)).Times(1);
  EXPECT_CALL(*scrollbar, PaintPart(_, TRACK, _)).Times(0);
  scrollbar->set_needs_paint_thumb(true);
  scrollbar->set_needs_paint_track(false);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar);

  // Enable the track.
  EXPECT_CALL(*scrollbar, PaintPart(_, THUMB, _)).Times(0);
  EXPECT_CALL(*scrollbar, PaintPart(_, TRACK, _)).Times(1);
  scrollbar->set_needs_paint_thumb(false);
  scrollbar->set_needs_paint_track(true);
  scrollbar_layer->Update();
  Mock::VerifyAndClearExpectations(scrollbar);
}

}  // namespace
}  // namespace cc
