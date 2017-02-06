// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIEWPORT_H_
#define CC_LAYERS_VIEWPORT_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "cc/layers/layer_impl.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class LayerTreeHostImpl;

// Encapsulates gesture handling logic on the viewport layers. The "viewport"
// is made up of two scrolling layers, the inner viewport (visual) and the
// outer viewport (layout) scroll layers. These layers have different scroll
// bubbling behavior from the rest of the layer tree which is encoded in this
// class.
class CC_EXPORT Viewport {
 public:
  // If the pinch zoom anchor on the first PinchUpdate is within this length
  // of the screen edge, "snap" the zoom to that edge. Experimentally
  // determined.
  static const int kPinchZoomSnapMarginDips = 100;

  // TODO(tdresser): eventually |consumed_delta| should equal
  // |content_scrolled_delta|. See crbug.com/510045 for details.
  struct ScrollResult {
    gfx::Vector2dF consumed_delta;
    gfx::Vector2dF content_scrolled_delta;
  };

  static std::unique_ptr<Viewport> Create(LayerTreeHostImpl* host_impl);

  // Differs from scrolling in that only the visual viewport is moved, without
  // affecting the top controls or outer viewport.
  void Pan(const gfx::Vector2dF& delta);

  // Scrolls the viewport, applying the unique bubbling between the inner and
  // outer viewport. Scrolls can be consumed by top controls.
  ScrollResult ScrollBy(const gfx::Vector2dF& delta,
                        const gfx::Point& viewport_point,
                        bool is_wheel_scroll,
                        bool affect_top_controls);

  // Scrolls the viewport, bubbling the delta between the inner and outer
  // viewport. Only animates either of the two viewports.
  gfx::Vector2dF ScrollAnimated(const gfx::Vector2dF& delta);

  void PinchUpdate(float magnify_delta, const gfx::Point& anchor);
  void PinchEnd();

 private:
  explicit Viewport(LayerTreeHostImpl* host_impl);

  // Returns true if viewport_delta is stricly less than pending_delta.
  static bool ShouldAnimateViewport(const gfx::Vector2dF& viewport_delta,
                                    const gfx::Vector2dF& pending_delta);
  bool ShouldTopControlsConsumeScroll(const gfx::Vector2dF& scroll_delta) const;
  gfx::Vector2dF AdjustOverscroll(const gfx::Vector2dF& delta) const;

  // Sends the delta to the top controls, returns the amount applied.
  gfx::Vector2dF ScrollTopControls(const gfx::Vector2dF& delta);

  gfx::ScrollOffset MaxTotalScrollOffset() const;
  gfx::ScrollOffset TotalScrollOffset() const;

  LayerImpl* InnerScrollLayer() const;
  LayerImpl* OuterScrollLayer() const;

  void SnapPinchAnchorIfWithinMargin(const gfx::Point& anchor);

  LayerTreeHostImpl* host_impl_;

  bool pinch_zoom_active_;

  // The pinch zoom anchor point is adjusted by this amount during a pinch. This
  // is used to "snap" a pinch-zoom to the edge of the screen.
  gfx::Vector2d pinch_anchor_adjustment_;

  FRIEND_TEST_ALL_PREFIXES(ViewportTest, ShouldAnimateViewport);

  DISALLOW_COPY_AND_ASSIGN(Viewport);
};

}  // namespace cc

#endif  // CC_LAYERS_VIEWPORT_H_
