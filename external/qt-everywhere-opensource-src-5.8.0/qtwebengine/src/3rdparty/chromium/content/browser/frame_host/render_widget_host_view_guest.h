// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserPluginGuest;
class RenderWidgetHost;
class RenderWidgetHostImpl;
struct NativeWebKeyboardEvent;
struct TextInputState;

// See comments in render_widget_host_view.h about this class and its members.
// This version is for the BrowserPlugin which handles a lot of the
// functionality in a diffent place and isn't platform specific.
// The BrowserPlugin is currently a special case for out-of-process rendered
// content and therefore inherits from RenderWidgetHostViewChildFrame.
// Eventually all RenderWidgetHostViewGuest code will be subsumed by
// RenderWidgetHostViewChildFrame and this class will be removed.
//
// Some elements that are platform specific will be deal with by delegating
// the relevant calls to the platform view.
class CONTENT_EXPORT RenderWidgetHostViewGuest
    : public RenderWidgetHostViewChildFrame,
      public ui::GestureConsumer {
 public:
  RenderWidgetHostViewGuest(
      RenderWidgetHost* widget,
      BrowserPluginGuest* guest,
      base::WeakPtr<RenderWidgetHostViewBase> platform_view);
  ~RenderWidgetHostViewGuest() override;

  bool OnMessageReceivedFromEmbedder(const IPC::Message& message,
                                     RenderWidgetHostImpl* embedder);

  // RenderWidgetHostView implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void InitAsChild(gfx::NativeView parent_view) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  void Focus() override;
  bool HasFocus() const override;
  void Show() override;
  void Hide() override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  gfx::Rect GetViewBounds() const override;
  gfx::Rect GetBoundsInRootWindow() override;
  gfx::Size GetPhysicalBackingSize() const override;
  base::string16 GetSelectedText() const override;

  // RenderWidgetHostViewBase implementation.
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& bounds) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  void UpdateCursor(const WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void TextInputStateChanged(const TextInputState& params) override;
  void ImeCancelComposition() override;
#if defined(OS_MACOSX) || defined(USE_AURA)
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
#endif
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  void SelectionChanged(const base::string16& text,
                        size_t offset,
                        const gfx::Range& range) override;
  void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
  void OnSwapCompositorFrame(uint32_t output_surface_id,
                             cc::CompositorFrame frame) override;
#if defined(USE_AURA)
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override;
#endif
  void ProcessTouchEvent(const blink::WebTouchEvent& event,
                         const ui::LatencyInfo& latency) override;

  bool LockMouse() override;
  void UnlockMouse() override;
  void GetScreenInfo(blink::WebScreenInfo* results) override;

#if defined(OS_MACOSX)
  // RenderWidgetHostView implementation.
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override;
  bool SupportsSpeech() const override;
  void SpeakSelection() override;
  bool IsSpeaking() const override;
  void StopSpeaking() override;
#endif  // defined(OS_MACOSX)

  void LockCompositingSurface() override;
  void UnlockCompositingSurface() override;

  void WheelEventAck(const blink::WebMouseWheelEvent& event,
                     InputEventAckState ack_result) override;

  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result) override;

 protected:
  friend class RenderWidgetHostView;

 private:
  RenderWidgetHostViewBase* GetOwnerRenderWidgetHostView() const;

  void OnHandleInputEvent(RenderWidgetHostImpl* embedder,
                          int browser_plugin_instance_id,
                          const blink::WebInputEvent* event);

  // BrowserPluginGuest and RenderWidgetHostViewGuest's lifetimes are not tied
  // to one another, therefore we access |guest_| through WeakPtr.
  base::WeakPtr<BrowserPluginGuest> guest_;
  gfx::Size size_;
  // The platform view for this RenderWidgetHostView.
  // RenderWidgetHostViewGuest mostly only cares about stuff related to
  // compositing, the rest are directly forwarded to this |platform_view_|.
  base::WeakPtr<RenderWidgetHostViewBase> platform_view_;
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_
