// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/resize_params.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebInputMethodController.h"

namespace content {

class RenderWidgetTest : public RenderViewTest {
 protected:
  RenderWidget* widget() {
    return static_cast<RenderViewImpl*>(view_)->GetWidget();
  }

  void OnResize(const ResizeParams& params) {
    widget()->OnResize(params);
  }

  void GetCompositionRange(gfx::Range* range) {
    widget()->GetCompositionRange(range);
  }

  bool next_paint_is_resize_ack() {
    return widget()->next_paint_is_resize_ack();
  }
};

TEST_F(RenderWidgetTest, OnResize) {
  // The initial bounds is empty, so setting it to the same thing should do
  // nothing.
  ResizeParams resize_params;
  resize_params.screen_info = ScreenInfo();
  resize_params.new_size = gfx::Size();
  resize_params.physical_backing_size = gfx::Size();
  resize_params.top_controls_height = 0.f;
  resize_params.browser_controls_shrink_blink_size = false;
  resize_params.is_fullscreen_granted = false;
  resize_params.needs_resize_ack = false;
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  // Setting empty physical backing size should not send the ack.
  resize_params.new_size = gfx::Size(10, 10);
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  // Setting the bounds to a "real" rect should send the ack.
  render_thread_->sink().ClearMessages();
  gfx::Size size(100, 100);
  resize_params.new_size = size;
  resize_params.physical_backing_size = size;
  resize_params.needs_resize_ack = true;
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  // Clear the flag.
  widget()->DidReceiveCompositorFrameAck();

  // Setting the same size again should not send the ack.
  resize_params.needs_resize_ack = false;
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  // Resetting the rect to empty should not send the ack.
  resize_params.new_size = gfx::Size();
  resize_params.physical_backing_size = gfx::Size();
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  // Changing the screen info should not send the ack.
  resize_params.screen_info.orientation_angle = 90;
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  resize_params.screen_info.orientation_type =
      SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());
}

class RenderWidgetInitialSizeTest : public RenderWidgetTest {
 public:
  RenderWidgetInitialSizeTest()
      : RenderWidgetTest(), initial_size_(200, 100) {}

 protected:
  std::unique_ptr<ResizeParams> InitialSizeParams() override {
    std::unique_ptr<ResizeParams> initial_size_params(new ResizeParams());
    initial_size_params->new_size = initial_size_;
    initial_size_params->physical_backing_size = initial_size_;
    initial_size_params->needs_resize_ack = true;
    return initial_size_params;
  }

  gfx::Size initial_size_;
};

TEST_F(RenderWidgetInitialSizeTest, InitialSize) {
  EXPECT_EQ(initial_size_, widget()->size());
  EXPECT_EQ(initial_size_, gfx::Size(widget()->GetWebWidget()->size()));
  EXPECT_TRUE(next_paint_is_resize_ack());
}

TEST_F(RenderWidgetTest, GetCompositionRangeValidComposition) {
  LoadHTML(
      "<div contenteditable>EDITABLE</div>"
      "<script> document.querySelector('div').focus(); </script>");
  blink::WebVector<blink::WebCompositionUnderline> emptyUnderlines;
  DCHECK(widget()->GetInputMethodController());
  widget()->GetInputMethodController()->setComposition("hello", emptyUnderlines,
                                                       3, 3);
  gfx::Range range;
  GetCompositionRange(&range);
  EXPECT_TRUE(range.IsValid());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(5U, range.end());
}

TEST_F(RenderWidgetTest, GetCompositionRangeForSelection) {
  LoadHTML(
      "<div>NOT EDITABLE</div>"
      "<script> document.execCommand('selectAll'); </script>");
  gfx::Range range;
  GetCompositionRange(&range);
  // Selection range should not be treated as composition range.
  EXPECT_FALSE(range.IsValid());
}

TEST_F(RenderWidgetTest, GetCompositionRangeInvalid) {
  LoadHTML("<div>NOT EDITABLE</div>");
  gfx::Range range;
  GetCompositionRange(&range);
  // If this test ever starts failing, one likely outcome is that WebRange
  // and gfx::Range::InvalidRange are no longer expressed in the same
  // values of start/end.
  EXPECT_FALSE(range.IsValid());
}

}  // namespace content
