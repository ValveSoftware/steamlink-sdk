// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mus.h"

#include <utility>

#include "build/build_config.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/render_widget_window_tree_client_factory.mojom.h"
#include "content/common/text_input_state.h"
#include "content/public/common/mojo_shell_connection.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"

namespace blink {
struct WebScreenInfo;
}

namespace content {

RenderWidgetHostViewMus::RenderWidgetHostViewMus(mus::Window* parent_window,
                                                 RenderWidgetHostImpl* host)
    : host_(host), aura_window_(nullptr) {
  DCHECK(parent_window);
  mus::Window* window = parent_window->window_tree()->NewWindow();
  window->SetVisible(true);
  window->SetBounds(gfx::Rect(300, 300));
  window->set_input_event_handler(this);
  parent_window->AddChild(window);
  mus_window_.reset(new mus::ScopedWindowPtr(window));
  host_->SetView(this);

  // Connect to the renderer, pass it a WindowTreeClient interface request
  // and embed that client inside our mus window.
  mojom::RenderWidgetWindowTreeClientFactoryPtr factory;
  host_->GetProcess()->GetChildConnection()->GetInterface(&factory);

  mus::mojom::WindowTreeClientPtr window_tree_client;
  factory->CreateWindowTreeClientForRenderWidget(
      host_->GetRoutingID(), mojo::GetProxy(&window_tree_client));
  mus_window_->window()->Embed(std::move(window_tree_client),
                               mus::mojom::kEmbedFlagEmbedderInterceptsEvents);
}

RenderWidgetHostViewMus::~RenderWidgetHostViewMus() {}

void RenderWidgetHostViewMus::InternalSetBounds(const gfx::Rect& rect) {
  aura_window_->SetBounds(rect);
  gfx::Rect bounds = aura_window_->GetBoundsInRootWindow();
  mus_window_->window()->SetBounds(bounds);
  host_->WasResized();
}

void RenderWidgetHostViewMus::Show() {
  // TODO(fsamuel): Update visibility in Mus.
  // There is some interstitial complexity that we'll need to figure out here.
  host_->WasShown(ui::LatencyInfo());
}

void RenderWidgetHostViewMus::Hide() {
  host_->WasHidden();
}

bool RenderWidgetHostViewMus::IsShowing() {
  return !host_->is_hidden();
}

void RenderWidgetHostViewMus::SetSize(const gfx::Size& size) {
  // For a SetSize operation, we don't care what coordinate system the origin
  // of the window is in, it's only important to make sure that the origin
  // remains constant after the operation.
  InternalSetBounds(gfx::Rect(aura_window_->bounds().origin(), size));
}

void RenderWidgetHostViewMus::SetBounds(const gfx::Rect& rect) {
  gfx::Point relative_origin(rect.origin());

  // RenderWidgetHostViewMus::SetBounds() takes screen coordinates, but
  // Window::SetBounds() takes parent coordinates, so do the conversion here.
  aura::Window* root = aura_window_->GetRootWindow();
  if (root) {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root);
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(aura_window_->parent(),
                                                     &relative_origin);
    }
  }

  InternalSetBounds(gfx::Rect(relative_origin, rect.size()));
}

void RenderWidgetHostViewMus::Focus() {
  // TODO(fsamuel): Request focus for the associated Mus::Window
  // We need to be careful how we propagate focus as we navigate to and
  // from interstitials.
}

bool RenderWidgetHostViewMus::HasFocus() const {
  return true;
}

bool RenderWidgetHostViewMus::IsSurfaceAvailableForCopy() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect RenderWidgetHostViewMus::GetViewBounds() const {
  return aura_window_->GetBoundsInScreen();
}

gfx::Vector2dF RenderWidgetHostViewMus::GetLastScrollOffset() const {
  return gfx::Vector2dF();
}

void RenderWidgetHostViewMus::RenderProcessGone(base::TerminationStatus status,
                                                int error_code) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewMus::Destroy() {
  delete aura_window_;
}

gfx::Size RenderWidgetHostViewMus::GetPhysicalBackingSize() const {
  return RenderWidgetHostViewBase::GetPhysicalBackingSize();
}

base::string16 RenderWidgetHostViewMus::GetSelectedText() const {
  NOTIMPLEMENTED();
  return base::string16();
}

void RenderWidgetHostViewMus::SetTooltipText(
    const base::string16& tooltip_text) {
  // TOOD(fsamuel): Ask window manager for tooltip?
}

void RenderWidgetHostViewMus::InitAsChild(gfx::NativeView parent_view) {
  aura_window_ = new aura::Window(nullptr);
  aura_window_->SetType(ui::wm::WINDOW_TYPE_CONTROL);
  aura_window_->Init(ui::LAYER_SOLID_COLOR);
  aura_window_->SetName("RenderWidgetHostViewMus");
  aura_window_->layer()->SetColor(background_color_);

  if (parent_view)
    parent_view->AddChild(GetNativeView());
}

RenderWidgetHost* RenderWidgetHostViewMus::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewMus::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds) {
  // TODO(fsamuel): Implement popups in Mus.
}

void RenderWidgetHostViewMus::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  // TODO(fsamuel): Implement full screen windows in Mus.
}

gfx::NativeView RenderWidgetHostViewMus::GetNativeView() const {
  return aura_window_;
}

gfx::NativeViewAccessible RenderWidgetHostViewMus::GetNativeViewAccessible() {
  return gfx::NativeViewAccessible();
}

void RenderWidgetHostViewMus::UpdateCursor(const WebCursor& cursor) {
  // TODO(fsamuel): Implement cursors in Mus.
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewMus::SetIsLoading(bool is_loading) {
}

void RenderWidgetHostViewMus::TextInputStateChanged(
    const TextInputState& params) {
  // TODO(fsamuel): Implement an IME mojo app.
}

void RenderWidgetHostViewMus::ImeCancelComposition() {
  // TODO(fsamuel): Implement an IME mojo app.
}

void RenderWidgetHostViewMus::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  // TODO(fsamuel): Implement IME.
}

void RenderWidgetHostViewMus::SelectionChanged(const base::string16& text,
                                               size_t offset,
                                               const gfx::Range& range) {
}

void RenderWidgetHostViewMus::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  // TODO(fsamuel): Implement selection.
}

void RenderWidgetHostViewMus::SetBackgroundColor(SkColor color) {
  // TODO(fsamuel): Implement background color and opacity.
}

void RenderWidgetHostViewMus::CopyFromCompositingSurface(
    const gfx::Rect& /* src_subrect */,
    const gfx::Size& /* dst_size */,
    const ReadbackRequestCallback& callback,
    const SkColorType /* preferred_color_type */) {
  // TODO(fsamuel): Implement read back.
  NOTIMPLEMENTED();
  callback.Run(SkBitmap(), READBACK_FAILED);
}

void RenderWidgetHostViewMus::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(gfx::Rect(), false);
}

bool RenderWidgetHostViewMus::CanCopyToVideoFrame() const {
  return false;
}

bool RenderWidgetHostViewMus::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  return false;
}

bool RenderWidgetHostViewMus::LockMouse() {
  // TODO(fsamuel): Implement mouse lock in Mus.
  return false;
}

void RenderWidgetHostViewMus::UnlockMouse() {
  // TODO(fsamuel): Implement mouse lock in Mus.
}

void RenderWidgetHostViewMus::GetScreenInfo(blink::WebScreenInfo* results) {
  // TODO(fsamuel): Populate screen info from Mus.
}

gfx::Rect RenderWidgetHostViewMus::GetBoundsInRootWindow() {
  aura::Window* top_level = aura_window_->GetToplevelWindow();
  gfx::Rect bounds(top_level->GetBoundsInScreen());
  return bounds;
}

#if defined(OS_MACOSX)
ui::AcceleratedWidgetMac* RenderWidgetHostViewMus::GetAcceleratedWidgetMac()
    const {
  return nullptr;
}

void RenderWidgetHostViewMus::SetActive(bool active) {
}

void RenderWidgetHostViewMus::ShowDefinitionForSelection() {
  // TODO(fsamuel): Implement this on Mac.
}

bool RenderWidgetHostViewMus::SupportsSpeech() const {
  // TODO(fsamuel): Implement this on mac.
  return false;
}

void RenderWidgetHostViewMus::SpeakSelection() {
  // TODO(fsamuel): Implement this on Mac.
}

bool RenderWidgetHostViewMus::IsSpeaking() const {
  // TODO(fsamuel): Implement this on Mac.
  return false;
}

void RenderWidgetHostViewMus::StopSpeaking() {
  // TODO(fsamuel): Implement this on Mac.
}
#endif  // defined(OS_MACOSX)

void RenderWidgetHostViewMus::LockCompositingSurface() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewMus::UnlockCompositingSurface() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewMus::OnWindowInputEvent(
    mus::Window* window,
    const ui::Event& event,
    std::unique_ptr<base::Callback<void(mus::mojom::EventResult)>>*
        ack_callback) {
  // TODO(sad): Dispatch |event| to the RenderWidgetHost.
}

}  // namespace content
