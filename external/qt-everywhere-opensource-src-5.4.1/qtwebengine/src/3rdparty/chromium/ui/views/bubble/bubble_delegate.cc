// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_delegate.h"

#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

// The defaut margin between the content and the inside border, in pixels.
static const int kDefaultMargin = 6;

namespace views {

namespace {

// Create a widget to host the bubble.
Widget* CreateBubbleWidget(BubbleDelegateView* bubble) {
  Widget* bubble_widget = new Widget();
  Widget::InitParams bubble_params(Widget::InitParams::TYPE_BUBBLE);
  bubble_params.delegate = bubble;
  bubble_params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
  bubble_params.accept_events = bubble->accept_events();
  if (bubble->parent_window())
    bubble_params.parent = bubble->parent_window();
  else if (bubble->anchor_widget())
    bubble_params.parent = bubble->anchor_widget()->GetNativeView();
  bubble_params.activatable = bubble->CanActivate() ?
      Widget::InitParams::ACTIVATABLE_YES : Widget::InitParams::ACTIVATABLE_NO;
  bubble->OnBeforeBubbleWidgetInit(&bubble_params, bubble_widget);
  bubble_widget->Init(bubble_params);
  return bubble_widget;
}

}  // namespace

BubbleDelegateView::BubbleDelegateView()
    : close_on_esc_(true),
      close_on_deactivate_(true),
      anchor_view_storage_id_(ViewStorage::GetInstance()->CreateStorageID()),
      anchor_widget_(NULL),
      arrow_(BubbleBorder::TOP_LEFT),
      shadow_(BubbleBorder::SMALL_SHADOW),
      color_explicitly_set_(false),
      margins_(kDefaultMargin, kDefaultMargin, kDefaultMargin, kDefaultMargin),
      use_focusless_(false),
      accept_events_(true),
      border_accepts_events_(true),
      adjust_if_offscreen_(true),
      parent_window_(NULL) {
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  UpdateColorsFromTheme(GetNativeTheme());
}

BubbleDelegateView::BubbleDelegateView(
    View* anchor_view,
    BubbleBorder::Arrow arrow)
    : close_on_esc_(true),
      close_on_deactivate_(true),
      anchor_view_storage_id_(ViewStorage::GetInstance()->CreateStorageID()),
      anchor_widget_(NULL),
      arrow_(arrow),
      shadow_(BubbleBorder::SMALL_SHADOW),
      color_explicitly_set_(false),
      margins_(kDefaultMargin, kDefaultMargin, kDefaultMargin, kDefaultMargin),
      use_focusless_(false),
      accept_events_(true),
      border_accepts_events_(true),
      adjust_if_offscreen_(true),
      parent_window_(NULL) {
  SetAnchorView(anchor_view);
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  UpdateColorsFromTheme(GetNativeTheme());
}

BubbleDelegateView::~BubbleDelegateView() {
  if (GetWidget())
    GetWidget()->RemoveObserver(this);
  SetLayoutManager(NULL);
  SetAnchorView(NULL);
}

// static
Widget* BubbleDelegateView::CreateBubble(BubbleDelegateView* bubble_delegate) {
  bubble_delegate->Init();
  // Get the latest anchor widget from the anchor view at bubble creation time.
  bubble_delegate->SetAnchorView(bubble_delegate->GetAnchorView());
  Widget* bubble_widget = CreateBubbleWidget(bubble_delegate);

#if defined(OS_WIN)
  // If glass is enabled, the bubble is allowed to extend outside the bounds of
  // the parent frame and let DWM handle compositing.  If not, then we don't
  // want to allow the bubble to extend the frame because it will be clipped.
  bubble_delegate->set_adjust_if_offscreen(ui::win::IsAeroGlassEnabled());
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Linux clips bubble windows that extend outside their parent window bounds.
  bubble_delegate->set_adjust_if_offscreen(false);
#endif

  bubble_delegate->SizeToContents();
  bubble_widget->AddObserver(bubble_delegate);
  return bubble_widget;
}

BubbleDelegateView* BubbleDelegateView::AsBubbleDelegate() {
  return this;
}

bool BubbleDelegateView::CanActivate() const {
  return !use_focusless();
}

bool BubbleDelegateView::ShouldShowCloseButton() const {
  return false;
}

View* BubbleDelegateView::GetContentsView() {
  return this;
}

NonClientFrameView* BubbleDelegateView::CreateNonClientFrameView(
    Widget* widget) {
  BubbleFrameView* frame = new BubbleFrameView(margins());
  // Note: In CreateBubble, the call to SizeToContents() will cause
  // the relayout that this call requires.
  frame->SetTitleFontList(GetTitleFontList());
  BubbleBorder::Arrow adjusted_arrow = arrow();
  if (base::i18n::IsRTL())
    adjusted_arrow = BubbleBorder::horizontal_mirror(adjusted_arrow);
  frame->SetBubbleBorder(scoped_ptr<BubbleBorder>(
      new BubbleBorder(adjusted_arrow, shadow(), color())));
  return frame;
}

void BubbleDelegateView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_DIALOG;
}

void BubbleDelegateView::OnWidgetDestroying(Widget* widget) {
  if (anchor_widget() == widget)
    SetAnchorView(NULL);
}

void BubbleDelegateView::OnWidgetVisibilityChanging(Widget* widget,
                                                    bool visible) {
#if defined(OS_WIN)
  // On Windows we need to handle this before the bubble is visible or hidden.
  // Please see the comment on the OnWidgetVisibilityChanging function. On
  // other platforms it is fine to handle it after the bubble is shown/hidden.
  HandleVisibilityChanged(widget, visible);
#endif
}

void BubbleDelegateView::OnWidgetVisibilityChanged(Widget* widget,
                                                   bool visible) {
#if !defined(OS_WIN)
  HandleVisibilityChanged(widget, visible);
#endif
}

void BubbleDelegateView::OnWidgetActivationChanged(Widget* widget,
                                                   bool active) {
  if (close_on_deactivate() && widget == GetWidget() && !active)
    GetWidget()->Close();
}

void BubbleDelegateView::OnWidgetBoundsChanged(Widget* widget,
                                               const gfx::Rect& new_bounds) {
  if (anchor_widget() == widget)
    SizeToContents();
}

View* BubbleDelegateView::GetAnchorView() const {
  return ViewStorage::GetInstance()->RetrieveView(anchor_view_storage_id_);
}

gfx::Rect BubbleDelegateView::GetAnchorRect() const {
  if (!GetAnchorView())
    return anchor_rect_;

  anchor_rect_ = GetAnchorView()->GetBoundsInScreen();
  anchor_rect_.Inset(anchor_view_insets_);
  return anchor_rect_;
}

void BubbleDelegateView::OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                                  Widget* widget) const {
}

void BubbleDelegateView::SetAlignment(BubbleBorder::BubbleAlignment alignment) {
  GetBubbleFrameView()->bubble_border()->set_alignment(alignment);
  SizeToContents();
}

void BubbleDelegateView::SetArrowPaintType(
    BubbleBorder::ArrowPaintType paint_type) {
  GetBubbleFrameView()->bubble_border()->set_paint_arrow(paint_type);
  SizeToContents();
}

void BubbleDelegateView::OnAnchorBoundsChanged() {
  SizeToContents();
}

bool BubbleDelegateView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (!close_on_esc() || accelerator.key_code() != ui::VKEY_ESCAPE)
    return false;
  GetWidget()->Close();
  return true;
}

void BubbleDelegateView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateColorsFromTheme(theme);
}

void BubbleDelegateView::Init() {}

void BubbleDelegateView::SetAnchorView(View* anchor_view) {
  // When the anchor view gets set the associated anchor widget might
  // change as well.
  if (!anchor_view || anchor_widget() != anchor_view->GetWidget()) {
    if (anchor_widget()) {
      anchor_widget_->RemoveObserver(this);
      anchor_widget_ = NULL;
    }
    if (anchor_view) {
      anchor_widget_ = anchor_view->GetWidget();
      if (anchor_widget_)
        anchor_widget_->AddObserver(this);
    }
  }

  // Remove the old storage item and set the new (if there is one).
  ViewStorage* view_storage = ViewStorage::GetInstance();
  if (view_storage->RetrieveView(anchor_view_storage_id_))
    view_storage->RemoveView(anchor_view_storage_id_);
  if (anchor_view)
    view_storage->StoreView(anchor_view_storage_id_, anchor_view);

  // Do not update anchoring for NULL views; this could indicate that our
  // NativeWindow is being destroyed, so it would be dangerous for us to update
  // our anchor bounds at that point. (It's safe to skip this, since if we were
  // to update the bounds when |anchor_view| is NULL, the bubble won't move.)
  if (anchor_view && GetWidget())
    OnAnchorBoundsChanged();
}

void BubbleDelegateView::SetAnchorRect(const gfx::Rect& rect) {
  anchor_rect_ = rect;
  if (GetWidget())
    OnAnchorBoundsChanged();
}

void BubbleDelegateView::SizeToContents() {
  GetWidget()->SetBounds(GetBubbleBounds());
}

BubbleFrameView* BubbleDelegateView::GetBubbleFrameView() const {
  const NonClientView* view =
      GetWidget() ? GetWidget()->non_client_view() : NULL;
  return view ? static_cast<BubbleFrameView*>(view->frame_view()) : NULL;
}

gfx::Rect BubbleDelegateView::GetBubbleBounds() {
  // The argument rect has its origin at the bubble's arrow anchor point;
  // its size is the preferred size of the bubble's client view (this view).
  bool anchor_minimized = anchor_widget() && anchor_widget()->IsMinimized();
  return GetBubbleFrameView()->GetUpdatedWindowBounds(GetAnchorRect(),
      GetPreferredSize(), adjust_if_offscreen_ && !anchor_minimized);
}

const gfx::FontList& BubbleDelegateView::GetTitleFontList() const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontList(ui::ResourceBundle::MediumFont);
}


void BubbleDelegateView::UpdateColorsFromTheme(const ui::NativeTheme* theme) {
  if (!color_explicitly_set_)
    color_ = theme->GetSystemColor(ui::NativeTheme::kColorId_DialogBackground);
  set_background(Background::CreateSolidBackground(color()));
  BubbleFrameView* frame_view = GetBubbleFrameView();
  if (frame_view)
    frame_view->bubble_border()->set_background_color(color());
}

void BubbleDelegateView::HandleVisibilityChanged(Widget* widget, bool visible) {
  if (widget == GetWidget() && anchor_widget() &&
      anchor_widget()->GetTopLevelWidget()) {
    if (visible)
      anchor_widget()->GetTopLevelWidget()->DisableInactiveRendering();
    else
      anchor_widget()->GetTopLevelWidget()->EnableInactiveRendering();
  }
}

}  // namespace views
