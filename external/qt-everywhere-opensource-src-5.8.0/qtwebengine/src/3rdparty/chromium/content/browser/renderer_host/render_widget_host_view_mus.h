// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MUS_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MUS_H_

#include <stddef.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/mus/public/cpp/input_event_handler.h"
#include "components/mus/public/cpp/scoped_window_ptr.h"
#include "components/mus/public/cpp/window.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/render_process_host_observer.h"

namespace content {

class RenderWidgetHost;
class RenderWidgetHostImpl;
struct NativeWebKeyboardEvent;
struct TextInputState;

// See comments in render_widget_host_view.h about this class and its members.
// This version of RenderWidgetHostView is for builds of Chrome that run through
// the mojo shell and use the Mandoline UI Service (Mus). Mus is responsible for
// windowing, compositing, and input event dispatch. The purpose of
// RenderWidgetHostViewMus is to manage the mus::Window owned by the content
// embedder. The browser is the owner of the mus::Window, controlling properties
// such as visibility, and bounds. Some aspects such as input, focus, and cursor
// are managed by Mus directly. Input event routing will be plumbed directly to
// the renderer from Mus.
class CONTENT_EXPORT RenderWidgetHostViewMus
    : public RenderWidgetHostViewBase,
      NON_EXPORTED_BASE(public mus::InputEventHandler) {
 public:
  RenderWidgetHostViewMus(mus::Window* parent_window,
                          RenderWidgetHostImpl* widget);
  ~RenderWidgetHostViewMus() override;

 private:
  // Set the bounds of the window and handle size changes.  Assumes the caller
  // has already adjusted the origin of |rect| to conform to whatever coordinate
  // space is required by the aura::Window.
  void InternalSetBounds(const gfx::Rect& rect);

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override;
  RenderWidgetHost* GetRenderWidgetHost() const override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  void Focus() override;
  bool HasFocus() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  gfx::Rect GetViewBounds() const override;
  gfx::Vector2dF GetLastScrollOffset() const override;
  void SetBackgroundColor(SkColor color) override;
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
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const ReadbackRequestCallback& callback,
      const SkColorType preferred_color_type) override;
  void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(const gfx::Rect&, bool)>& callback) override;
  bool CanCopyToVideoFrame() const override;
  bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  void ClearCompositorFrame() override {}
  bool LockMouse() override;
  void UnlockMouse() override;
  void GetScreenInfo(blink::WebScreenInfo* results) override;
  gfx::Rect GetBoundsInRootWindow() override;

#if defined(OS_MACOSX)
  // RenderWidgetHostView implementation.
  ui::AcceleratedWidgetMac* GetAcceleratedWidgetMac() const override;
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override;
  bool SupportsSpeech() const override;
  void SpeakSelection() override;
  bool IsSpeaking() const override;
  void StopSpeaking() override;
#endif  // defined(OS_MACOSX)

  void LockCompositingSurface() override;
  void UnlockCompositingSurface() override;

  // mus::InputEventHandler:
  void OnWindowInputEvent(
      mus::Window* target,
      const ui::Event& event,
      std::unique_ptr<base::Callback<void(mus::mojom::EventResult)>>*
          ack_callback) override;

  RenderWidgetHostImpl* host_;

  aura::Window* aura_window_;

  std::unique_ptr<mus::ScopedWindowPtr> mus_window_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewMus);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MUS_H_
