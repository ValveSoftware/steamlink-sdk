// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES // For VC++ to get M_PI. This has to be first.

#include "ui/views/view.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ime/input_method.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/transform_recorder.h"
#include "ui/display/screen.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/drag_controller.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/scoped_gdi_object.h"
#include "ui/native_theme/native_theme_win.h"
#endif

namespace views {

namespace {

#if defined(OS_WIN)
const bool kContextMenuOnMousePress = false;
#else
const bool kContextMenuOnMousePress = true;
#endif

// Default horizontal drag threshold in pixels.
// Same as what gtk uses.
const int kDefaultHorizontalDragThreshold = 8;

// Default vertical drag threshold in pixels.
// Same as what gtk uses.
const int kDefaultVerticalDragThreshold = 8;

// Returns the top view in |view|'s hierarchy.
const View* GetHierarchyRoot(const View* view) {
  const View* root = view;
  while (root && root->parent())
    root = root->parent();
  return root;
}

}  // namespace

// static
const char View::kViewClassName[] = "View";

////////////////////////////////////////////////////////////////////////////////
// View, public:

// Creation and lifetime -------------------------------------------------------

View::View()
    : owned_by_client_(false),
      id_(0),
      group_(-1),
      parent_(NULL),
      visible_(true),
      enabled_(true),
      notify_enter_exit_on_child_(false),
      registered_for_visible_bounds_notification_(false),
      needs_layout_(true),
      snap_layer_to_pixel_boundary_(false),
      flip_canvas_on_paint_for_rtl_ui_(false),
      paint_to_layer_(false),
      accelerator_focus_manager_(NULL),
      registered_accelerator_count_(0),
      next_focusable_view_(NULL),
      previous_focusable_view_(NULL),
      focus_behavior_(FocusBehavior::NEVER),
      context_menu_controller_(NULL),
      drag_controller_(NULL),
      native_view_accessibility_(NULL) {
  SetTargetHandler(this);
}

View::~View() {
  if (parent_)
    parent_->RemoveChildView(this);

  ViewStorage::GetInstance()->ViewRemoved(this);

  for (Views::const_iterator i(children_.begin()); i != children_.end(); ++i) {
    (*i)->parent_ = NULL;
    if (!(*i)->owned_by_client_)
      delete *i;
  }

  // Release ownership of the native accessibility object, but it's
  // reference-counted on some platforms, so it may not be deleted right away.
  if (native_view_accessibility_)
    native_view_accessibility_->Destroy();
}

// Tree operations -------------------------------------------------------------

const Widget* View::GetWidget() const {
  // The root view holds a reference to this view hierarchy's Widget.
  return parent_ ? parent_->GetWidget() : NULL;
}

Widget* View::GetWidget() {
  return const_cast<Widget*>(const_cast<const View*>(this)->GetWidget());
}

void View::AddChildView(View* view) {
  if (view->parent_ == this)
    return;
  AddChildViewAt(view, child_count());
}

void View::AddChildViewAt(View* view, int index) {
  CHECK_NE(view, this) << "You cannot add a view as its own child";
  DCHECK_GE(index, 0);
  DCHECK_LE(index, child_count());

  // If |view| has a parent, remove it from its parent.
  View* parent = view->parent_;
  ui::NativeTheme* old_theme = NULL;
  if (parent) {
    old_theme = view->GetNativeTheme();
    if (parent == this) {
      ReorderChildView(view, index);
      return;
    }
    parent->DoRemoveChildView(view, true, true, false, this);
  }

  // Sets the prev/next focus views.
  InitFocusSiblings(view, index);

  view->parent_ = this;
  children_.insert(children_.begin() + index, view);

  // Ensure the layer tree matches the view tree before calling to any client
  // code. This way if client code further modifies the view tree we are in a
  // sane state.
  const bool did_reparent_any_layers = view->UpdateParentLayers();
  Widget* widget = GetWidget();
  if (did_reparent_any_layers && widget)
    widget->UpdateRootLayers();

  ReorderLayers();

  // Make sure the visibility of the child layers are correct.
  // If any of the parent View is hidden, then the layers of the subtree
  // rooted at |this| should be hidden. Otherwise, all the child layers should
  // inherit the visibility of the owner View.
  view->UpdateLayerVisibility();

  if (widget) {
    const ui::NativeTheme* new_theme = view->GetNativeTheme();
    if (new_theme != old_theme)
      view->PropagateNativeThemeChanged(new_theme);
  }

  ViewHierarchyChangedDetails details(true, this, view, parent);

  for (View* v = this; v; v = v->parent_)
    v->ViewHierarchyChangedImpl(false, details);

  view->PropagateAddNotifications(details);

  UpdateTooltip();

  if (widget) {
    RegisterChildrenForVisibleBoundsNotification(view);

    if (view->visible())
      view->SchedulePaint();
  }

  if (layout_manager_.get())
    layout_manager_->ViewAdded(this, view);
}

void View::ReorderChildView(View* view, int index) {
  DCHECK_EQ(view->parent_, this);
  if (index < 0)
    index = child_count() - 1;
  else if (index >= child_count())
    return;
  if (children_[index] == view)
    return;

  const Views::iterator i(std::find(children_.begin(), children_.end(), view));
  DCHECK(i != children_.end());
  children_.erase(i);

  // Unlink the view first
  View* next_focusable = view->next_focusable_view_;
  View* prev_focusable = view->previous_focusable_view_;
  if (prev_focusable)
    prev_focusable->next_focusable_view_ = next_focusable;
  if (next_focusable)
    next_focusable->previous_focusable_view_ = prev_focusable;

  // Add it in the specified index now.
  InitFocusSiblings(view, index);
  children_.insert(children_.begin() + index, view);

  ReorderLayers();
}

void View::RemoveChildView(View* view) {
  DoRemoveChildView(view, true, true, false, NULL);
}

void View::RemoveAllChildViews(bool delete_children) {
  while (!children_.empty())
    DoRemoveChildView(children_.front(), false, false, delete_children, NULL);
  UpdateTooltip();
}

bool View::Contains(const View* view) const {
  for (const View* v = view; v; v = v->parent_) {
    if (v == this)
      return true;
  }
  return false;
}

int View::GetIndexOf(const View* view) const {
  Views::const_iterator i(std::find(children_.begin(), children_.end(), view));
  return i != children_.end() ? static_cast<int>(i - children_.begin()) : -1;
}

// Size and disposition --------------------------------------------------------

void View::SetBounds(int x, int y, int width, int height) {
  SetBoundsRect(gfx::Rect(x, y, std::max(0, width), std::max(0, height)));
}

void View::SetBoundsRect(const gfx::Rect& bounds) {
  if (bounds == bounds_) {
    if (needs_layout_) {
      needs_layout_ = false;
      Layout();
    }
    return;
  }

  if (visible_) {
    // Paint where the view is currently.
    SchedulePaintBoundsChanged(
        bounds_.size() == bounds.size() ? SCHEDULE_PAINT_SIZE_SAME :
        SCHEDULE_PAINT_SIZE_CHANGED);
  }

  gfx::Rect prev = bounds_;
  bounds_ = bounds;
  BoundsChanged(prev);
}

void View::SetSize(const gfx::Size& size) {
  SetBounds(x(), y(), size.width(), size.height());
}

void View::SetPosition(const gfx::Point& position) {
  SetBounds(position.x(), position.y(), width(), height());
}

void View::SetX(int x) {
  SetBounds(x, y(), width(), height());
}

void View::SetY(int y) {
  SetBounds(x(), y, width(), height());
}

gfx::Rect View::GetContentsBounds() const {
  gfx::Rect contents_bounds(GetLocalBounds());
  if (border_.get())
    contents_bounds.Inset(border_->GetInsets());
  return contents_bounds;
}

gfx::Rect View::GetLocalBounds() const {
  return gfx::Rect(size());
}

gfx::Insets View::GetInsets() const {
  return border_.get() ? border_->GetInsets() : gfx::Insets();
}

gfx::Rect View::GetVisibleBounds() const {
  if (!IsDrawn())
    return gfx::Rect();
  gfx::Rect vis_bounds(GetLocalBounds());
  gfx::Rect ancestor_bounds;
  const View* view = this;
  gfx::Transform transform;

  while (view != NULL && !vis_bounds.IsEmpty()) {
    transform.ConcatTransform(view->GetTransform());
    gfx::Transform translation;
    translation.Translate(static_cast<float>(view->GetMirroredX()),
                          static_cast<float>(view->y()));
    transform.ConcatTransform(translation);

    vis_bounds = view->ConvertRectToParent(vis_bounds);
    const View* ancestor = view->parent_;
    if (ancestor != NULL) {
      ancestor_bounds.SetRect(0, 0, ancestor->width(), ancestor->height());
      vis_bounds.Intersect(ancestor_bounds);
    } else if (!view->GetWidget()) {
      // If the view has no Widget, we're not visible. Return an empty rect.
      return gfx::Rect();
    }
    view = ancestor;
  }
  if (vis_bounds.IsEmpty())
    return vis_bounds;
  // Convert back to this views coordinate system.
  gfx::RectF views_vis_bounds(vis_bounds);
  transform.TransformRectReverse(&views_vis_bounds);
  // Partially visible pixels should be considered visible.
  return gfx::ToEnclosingRect(views_vis_bounds);
}

gfx::Rect View::GetBoundsInScreen() const {
  gfx::Point origin;
  View::ConvertPointToScreen(this, &origin);
  return gfx::Rect(origin, size());
}

gfx::Size View::GetPreferredSize() const {
  if (layout_manager_.get())
    return layout_manager_->GetPreferredSize(this);
  return gfx::Size();
}

int View::GetBaseline() const {
  return -1;
}

void View::SizeToPreferredSize() {
  gfx::Size prefsize = GetPreferredSize();
  if ((prefsize.width() != width()) || (prefsize.height() != height()))
    SetBounds(x(), y(), prefsize.width(), prefsize.height());
}

gfx::Size View::GetMinimumSize() const {
  return GetPreferredSize();
}

gfx::Size View::GetMaximumSize() const {
  return gfx::Size();
}

int View::GetHeightForWidth(int w) const {
  if (layout_manager_.get())
    return layout_manager_->GetPreferredHeightForWidth(this, w);
  return GetPreferredSize().height();
}

void View::SetVisible(bool visible) {
  if (visible != visible_) {
    // If the View is currently visible, schedule paint to refresh parent.
    // TODO(beng): not sure we should be doing this if we have a layer.
    if (visible_)
      SchedulePaint();

    visible_ = visible;
    AdvanceFocusIfNecessary();

    // Notify the parent.
    if (parent_) {
      parent_->ChildVisibilityChanged(this);
      parent_->NotifyAccessibilityEvent(ui::AX_EVENT_CHILDREN_CHANGED, false);
    }

    // This notifies all sub-views recursively.
    PropagateVisibilityNotifications(this, visible_);
    UpdateLayerVisibility();

    // If we are newly visible, schedule paint.
    if (visible_)
      SchedulePaint();
  }
}

bool View::IsDrawn() const {
  return visible_ && parent_ ? parent_->IsDrawn() : false;
}

void View::SetEnabled(bool enabled) {
  if (enabled != enabled_) {
    enabled_ = enabled;
    AdvanceFocusIfNecessary();
    OnEnabledChanged();
  }
}

void View::OnEnabledChanged() {
  SchedulePaint();
}

// Transformations -------------------------------------------------------------

gfx::Transform View::GetTransform() const {
  return layer() ? layer()->transform() : gfx::Transform();
}

void View::SetTransform(const gfx::Transform& transform) {
  if (transform.IsIdentity()) {
    if (layer()) {
      layer()->SetTransform(transform);
      if (!paint_to_layer_)
        DestroyLayer();
    } else {
      // Nothing.
    }
  } else {
    if (!layer())
      CreateLayer();
    layer()->SetTransform(transform);
    layer()->ScheduleDraw();
  }
}

void View::SetPaintToLayer(bool paint_to_layer) {
  if (paint_to_layer_ == paint_to_layer)
    return;

  paint_to_layer_ = paint_to_layer;
  if (paint_to_layer_ && !layer()) {
    CreateLayer();
  } else if (!paint_to_layer_ && layer()) {
    DestroyLayer();
  }
}

std::unique_ptr<ui::Layer> View::RecreateLayer() {
  std::unique_ptr<ui::Layer> old_layer = LayerOwner::RecreateLayer();
  Widget* widget = GetWidget();
  if (widget)
    widget->UpdateRootLayers();
  return old_layer;
}

// RTL positioning -------------------------------------------------------------

gfx::Rect View::GetMirroredBounds() const {
  gfx::Rect bounds(bounds_);
  bounds.set_x(GetMirroredX());
  return bounds;
}

gfx::Point View::GetMirroredPosition() const {
  return gfx::Point(GetMirroredX(), y());
}

int View::GetMirroredX() const {
  return parent_ ? parent_->GetMirroredXForRect(bounds_) : x();
}

int View::GetMirroredXForRect(const gfx::Rect& bounds) const {
  return base::i18n::IsRTL() ?
      (width() - bounds.x() - bounds.width()) : bounds.x();
}

int View::GetMirroredXInView(int x) const {
  return base::i18n::IsRTL() ? width() - x : x;
}

int View::GetMirroredXWithWidthInView(int x, int w) const {
  return base::i18n::IsRTL() ? width() - x - w : x;
}

// Layout ----------------------------------------------------------------------

void View::Layout() {
  needs_layout_ = false;

  // If we have a layout manager, let it handle the layout for us.
  if (layout_manager_.get())
    layout_manager_->Layout(this);

  // Make sure to propagate the Layout() call to any children that haven't
  // received it yet through the layout manager and need to be laid out. This
  // is needed for the case when the child requires a layout but its bounds
  // weren't changed by the layout manager. If there is no layout manager, we
  // just propagate the Layout() call down the hierarchy, so whoever receives
  // the call can take appropriate action.
  for (int i = 0, count = child_count(); i < count; ++i) {
    View* child = child_at(i);
    if (child->needs_layout_ || !layout_manager_.get()) {
      TRACE_EVENT1("views", "View::Layout", "class", child->GetClassName());
      child->needs_layout_ = false;
      child->Layout();
    }
  }
}

void View::InvalidateLayout() {
  // Always invalidate up. This is needed to handle the case of us already being
  // valid, but not our parent.
  needs_layout_ = true;
  if (parent_)
    parent_->InvalidateLayout();
}

LayoutManager* View::GetLayoutManager() const {
  return layout_manager_.get();
}

void View::SetLayoutManager(LayoutManager* layout_manager) {
  if (layout_manager_.get())
    layout_manager_->Uninstalled(this);

  layout_manager_.reset(layout_manager);
  if (layout_manager_.get())
    layout_manager_->Installed(this);
}

void View::SnapLayerToPixelBoundary() {
  if (!layer())
    return;

  if (snap_layer_to_pixel_boundary_ && layer()->parent() &&
      layer()->GetCompositor()) {
    ui::SnapLayerToPhysicalPixelBoundary(layer()->parent(), layer());
  } else {
    // Reset the offset.
    layer()->SetSubpixelPositionOffset(gfx::Vector2dF());
  }
}

// Attributes ------------------------------------------------------------------

const char* View::GetClassName() const {
  return kViewClassName;
}

const View* View::GetAncestorWithClassName(const std::string& name) const {
  for (const View* view = this; view; view = view->parent_) {
    if (!strcmp(view->GetClassName(), name.c_str()))
      return view;
  }
  return NULL;
}

View* View::GetAncestorWithClassName(const std::string& name) {
  return const_cast<View*>(const_cast<const View*>(this)->
      GetAncestorWithClassName(name));
}

const View* View::GetViewByID(int id) const {
  if (id == id_)
    return const_cast<View*>(this);

  for (int i = 0, count = child_count(); i < count; ++i) {
    const View* view = child_at(i)->GetViewByID(id);
    if (view)
      return view;
  }
  return NULL;
}

View* View::GetViewByID(int id) {
  return const_cast<View*>(const_cast<const View*>(this)->GetViewByID(id));
}

void View::SetGroup(int gid) {
  // Don't change the group id once it's set.
  DCHECK(group_ == -1 || group_ == gid);
  group_ = gid;
}

int View::GetGroup() const {
  return group_;
}

bool View::IsGroupFocusTraversable() const {
  return true;
}

void View::GetViewsInGroup(int group, Views* views) {
  if (group_ == group)
    views->push_back(this);

  for (int i = 0, count = child_count(); i < count; ++i)
    child_at(i)->GetViewsInGroup(group, views);
}

View* View::GetSelectedViewForGroup(int group) {
  Views views;
  GetWidget()->GetRootView()->GetViewsInGroup(group, &views);
  return views.empty() ? NULL : views[0];
}

// Coordinate conversion -------------------------------------------------------

// static
void View::ConvertPointToTarget(const View* source,
                                const View* target,
                                gfx::Point* point) {
  DCHECK(source);
  DCHECK(target);
  if (source == target)
    return;

  const View* root = GetHierarchyRoot(target);
  CHECK_EQ(GetHierarchyRoot(source), root);

  if (source != root)
    source->ConvertPointForAncestor(root, point);

  if (target != root)
    target->ConvertPointFromAncestor(root, point);
}

// static
void View::ConvertRectToTarget(const View* source,
                               const View* target,
                               gfx::RectF* rect) {
  DCHECK(source);
  DCHECK(target);
  if (source == target)
    return;

  const View* root = GetHierarchyRoot(target);
  CHECK_EQ(GetHierarchyRoot(source), root);

  if (source != root)
    source->ConvertRectForAncestor(root, rect);

  if (target != root)
    target->ConvertRectFromAncestor(root, rect);
}

// static
void View::ConvertPointToWidget(const View* src, gfx::Point* p) {
  DCHECK(src);
  DCHECK(p);

  src->ConvertPointForAncestor(NULL, p);
}

// static
void View::ConvertPointFromWidget(const View* dest, gfx::Point* p) {
  DCHECK(dest);
  DCHECK(p);

  dest->ConvertPointFromAncestor(NULL, p);
}

// static
void View::ConvertPointToScreen(const View* src, gfx::Point* p) {
  DCHECK(src);
  DCHECK(p);

  // If the view is not connected to a tree, there's nothing we can do.
  const Widget* widget = src->GetWidget();
  if (widget) {
    ConvertPointToWidget(src, p);
    *p += widget->GetClientAreaBoundsInScreen().OffsetFromOrigin();
  }
}

// static
void View::ConvertPointFromScreen(const View* dst, gfx::Point* p) {
  DCHECK(dst);
  DCHECK(p);

  const views::Widget* widget = dst->GetWidget();
  if (!widget)
    return;
  *p -= widget->GetClientAreaBoundsInScreen().OffsetFromOrigin();
  ConvertPointFromWidget(dst, p);
}

gfx::Rect View::ConvertRectToParent(const gfx::Rect& rect) const {
  gfx::RectF x_rect = gfx::RectF(rect);
  GetTransform().TransformRect(&x_rect);
  x_rect.Offset(GetMirroredPosition().OffsetFromOrigin());
  // Pixels we partially occupy in the parent should be included.
  return gfx::ToEnclosingRect(x_rect);
}

gfx::Rect View::ConvertRectToWidget(const gfx::Rect& rect) const {
  gfx::Rect x_rect = rect;
  for (const View* v = this; v; v = v->parent_)
    x_rect = v->ConvertRectToParent(x_rect);
  return x_rect;
}

// Painting --------------------------------------------------------------------

void View::SchedulePaint() {
  SchedulePaintInRect(GetLocalBounds());
}

void View::SchedulePaintInRect(const gfx::Rect& rect) {
  if (!visible_)
    return;

  if (layer()) {
    layer()->SchedulePaint(rect);
  } else if (parent_) {
    // Translate the requested paint rect to the parent's coordinate system
    // then pass this notification up to the parent.
    parent_->SchedulePaintInRect(ConvertRectToParent(rect));
  }
}

void View::Paint(const ui::PaintContext& parent_context) {
  if (!visible_)
    return;
  if (size().IsEmpty())
    return;

  gfx::Vector2d offset_to_parent;
  if (!layer()) {
    // If the View has a layer() then it is a paint root. Otherwise, we need to
    // add the offset from the parent into the total offset from the paint root.
    DCHECK(parent() || bounds().origin() == gfx::Point());
    offset_to_parent = GetMirroredPosition().OffsetFromOrigin();
  }
  ui::PaintContext context(parent_context, offset_to_parent);

  bool is_invalidated = true;
  if (context.CanCheckInvalid()) {
#if DCHECK_IS_ON()
    gfx::Vector2d offset;
    context.Visited(this);
    View* view = this;
    while (view->parent() && !view->layer()) {
      DCHECK(view->GetTransform().IsIdentity());
      offset += view->GetMirroredPosition().OffsetFromOrigin();
      view = view->parent();
    }
    // The offset in the PaintContext should be the offset up to the paint root,
    // which we compute and verify here.
    DCHECK_EQ(context.PaintOffset().x(), offset.x());
    DCHECK_EQ(context.PaintOffset().y(), offset.y());
    // The above loop will stop when |view| is the paint root, which should be
    // the root of the current paint walk, as verified by storing the root in
    // the PaintContext.
    DCHECK_EQ(context.RootVisited(), view);
#endif

    // If the View wasn't invalidated, don't waste time painting it, the output
    // would be culled.
    is_invalidated = context.IsRectInvalid(GetLocalBounds());
  }

  TRACE_EVENT1("views", "View::Paint", "class", GetClassName());

  // If the view is backed by a layer, it should paint with itself as the origin
  // rather than relative to its parent.
  bool paint_relative_to_parent = !layer();

  // TODO(danakj): Rework clip and transform recorder usage here to use
  // std::optional once we can do so.
  ui::ClipRecorder clip_recorder(parent_context);
  if (paint_relative_to_parent) {
    // Set the clip rect to the bounds of this View, or |clip_path_| if it's
    // been set. Note that the X (or left) position we pass to ClipRect takes
    // into consideration whether or not the View uses a right-to-left layout so
    // that we paint the View in its mirrored position if need be.
    if (clip_path_.isEmpty()) {
      clip_recorder.ClipRect(GetMirroredBounds());
    } else {
      gfx::Path clip_path_in_parent = clip_path_;
      clip_path_in_parent.offset(GetMirroredX(), y());
      clip_recorder.ClipPathWithAntiAliasing(clip_path_in_parent);
    }
  }

  ui::TransformRecorder transform_recorder(context);
  if (paint_relative_to_parent) {
    // Translate the graphics such that 0,0 corresponds to where
    // this View is located relative to its parent.
    gfx::Transform transform_from_parent;
    gfx::Vector2d offset_from_parent = GetMirroredPosition().OffsetFromOrigin();
    transform_from_parent.Translate(offset_from_parent.x(),
                                    offset_from_parent.y());
    transform_from_parent.PreconcatTransform(GetTransform());
    transform_recorder.Transform(transform_from_parent, size());
  }

  // Note that the cache is not aware of the offset of the view
  // relative to the parent since painting is always done relative to
  // the top left of the individual view.
  if (is_invalidated || !paint_cache_.UseCache(context, size())) {
    ui::PaintRecorder recorder(context, size(), &paint_cache_);
    gfx::Canvas* canvas = recorder.canvas();

    // If the View we are about to paint requested the canvas to be flipped, we
    // should change the transform appropriately.
    // The canvas mirroring is undone once the View is done painting so that we
    // don't pass the canvas with the mirrored transform to Views that didn't
    // request the canvas to be flipped.
    if (FlipCanvasOnPaintForRTLUI()) {
      canvas->Translate(gfx::Vector2d(width(), 0));
      canvas->Scale(-1, 1);
    }

    // Delegate painting the contents of the View to the virtual OnPaint method.
    OnPaint(canvas);
  }

  // View::Paint() recursion over the subtree.
  PaintChildren(context);
}

void View::set_background(Background* b) {
  background_.reset(b);
}

void View::SetBorder(std::unique_ptr<Border> b) {
  border_ = std::move(b);
}

const ui::ThemeProvider* View::GetThemeProvider() const {
  const Widget* widget = GetWidget();
  return widget ? widget->GetThemeProvider() : nullptr;
}

const ui::NativeTheme* View::GetNativeTheme() const {
  const Widget* widget = GetWidget();
  if (widget)
    return widget->GetNativeTheme();

#if defined(OS_WIN)
  // On Windows, ui::NativeTheme::GetInstanceForWeb() returns NativeThemeWinAura
  // because that's what the renderer wants, but Views should default to
  // NativeThemeWin. TODO(estade): clean this up, see http://crbug.com/558029
  return ui::NativeThemeWin::instance();
#else
  return ui::NativeTheme::GetInstanceForWeb();
#endif
}

// RTL painting ----------------------------------------------------------------

void View::EnableCanvasFlippingForRTLUI(bool enable) {
  flip_canvas_on_paint_for_rtl_ui_ = enable;
}

// Input -----------------------------------------------------------------------

View* View::GetEventHandlerForPoint(const gfx::Point& point) {
  return GetEventHandlerForRect(gfx::Rect(point, gfx::Size(1, 1)));
}

View* View::GetEventHandlerForRect(const gfx::Rect& rect) {
  return GetEffectiveViewTargeter()->TargetForRect(this, rect);
}

bool View::CanProcessEventsWithinSubtree() const {
  return true;
}

View* View::GetTooltipHandlerForPoint(const gfx::Point& point) {
  // TODO(tdanderson): Move this implementation into ViewTargetDelegate.
  if (!HitTestPoint(point) || !CanProcessEventsWithinSubtree())
    return NULL;

  // Walk the child Views recursively looking for the View that most
  // tightly encloses the specified point.
  for (int i = child_count() - 1; i >= 0; --i) {
    View* child = child_at(i);
    if (!child->visible())
      continue;

    gfx::Point point_in_child_coords(point);
    ConvertPointToTarget(this, child, &point_in_child_coords);
    View* handler = child->GetTooltipHandlerForPoint(point_in_child_coords);
    if (handler)
      return handler;
  }
  return this;
}

gfx::NativeCursor View::GetCursor(const ui::MouseEvent& event) {
#if defined(OS_WIN)
  static ui::Cursor arrow;
  if (!arrow.platform())
    arrow.SetPlatformCursor(LoadCursor(NULL, IDC_ARROW));
  return arrow;
#else
  return gfx::kNullCursor;
#endif
}

bool View::HitTestPoint(const gfx::Point& point) const {
  return HitTestRect(gfx::Rect(point, gfx::Size(1, 1)));
}

bool View::HitTestRect(const gfx::Rect& rect) const {
  return GetEffectiveViewTargeter()->DoesIntersectRect(this, rect);
}

bool View::IsMouseHovered() const {
  // If we haven't yet been placed in an onscreen view hierarchy, we can't be
  // hovered.
  if (!GetWidget())
    return false;

  // If mouse events are disabled, then the mouse cursor is invisible and
  // is therefore not hovering over this button.
  if (!GetWidget()->IsMouseEventsEnabled())
    return false;

  gfx::Point cursor_pos(display::Screen::GetScreen()->GetCursorScreenPoint());
  ConvertPointFromScreen(this, &cursor_pos);
  return HitTestPoint(cursor_pos);
}

bool View::OnMousePressed(const ui::MouseEvent& event) {
  return false;
}

bool View::OnMouseDragged(const ui::MouseEvent& event) {
  return false;
}

void View::OnMouseReleased(const ui::MouseEvent& event) {
}

void View::OnMouseCaptureLost() {
}

void View::OnMouseMoved(const ui::MouseEvent& event) {
}

void View::OnMouseEntered(const ui::MouseEvent& event) {
}

void View::OnMouseExited(const ui::MouseEvent& event) {
}

void View::SetMouseHandler(View* new_mouse_handler) {
  // |new_mouse_handler| may be NULL.
  if (parent_)
    parent_->SetMouseHandler(new_mouse_handler);
}

bool View::OnKeyPressed(const ui::KeyEvent& event) {
  return false;
}

bool View::OnKeyReleased(const ui::KeyEvent& event) {
  return false;
}

bool View::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return false;
}

void View::OnKeyEvent(ui::KeyEvent* event) {
  bool consumed = (event->type() == ui::ET_KEY_PRESSED) ? OnKeyPressed(*event) :
                                                          OnKeyReleased(*event);
  if (consumed)
    event->StopPropagation();
}

void View::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      if (ProcessMousePressed(*event))
        event->SetHandled();
      return;

    case ui::ET_MOUSE_MOVED:
      if ((event->flags() & (ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_RIGHT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON)) == 0) {
        OnMouseMoved(*event);
        return;
      }
      // FALL-THROUGH
    case ui::ET_MOUSE_DRAGGED:
      if (ProcessMouseDragged(*event))
        event->SetHandled();
      return;

    case ui::ET_MOUSE_RELEASED:
      ProcessMouseReleased(*event);
      return;

    case ui::ET_MOUSEWHEEL:
      if (OnMouseWheel(*event->AsMouseWheelEvent()))
        event->SetHandled();
      break;

    case ui::ET_MOUSE_ENTERED:
      if (event->flags() & ui::EF_TOUCH_ACCESSIBILITY)
        NotifyAccessibilityEvent(ui::AX_EVENT_HOVER, true);
      OnMouseEntered(*event);
      break;

    case ui::ET_MOUSE_EXITED:
      OnMouseExited(*event);
      break;

    default:
      return;
  }
}

void View::OnScrollEvent(ui::ScrollEvent* event) {
}

void View::OnTouchEvent(ui::TouchEvent* event) {
  NOTREACHED() << "Views should not receive touch events.";
}

void View::OnGestureEvent(ui::GestureEvent* event) {
}

const ui::InputMethod* View::GetInputMethod() const {
  Widget* widget = const_cast<Widget*>(GetWidget());
  return widget ? const_cast<const ui::InputMethod*>(widget->GetInputMethod())
                : nullptr;
}

std::unique_ptr<ViewTargeter> View::SetEventTargeter(
    std::unique_ptr<ViewTargeter> targeter) {
  std::unique_ptr<ViewTargeter> old_targeter = std::move(targeter_);
  targeter_ = std::move(targeter);
  return old_targeter;
}

ViewTargeter* View::GetEffectiveViewTargeter() const {
  DCHECK(GetWidget());
  ViewTargeter* view_targeter = targeter();
  if (!view_targeter)
    view_targeter = GetWidget()->GetRootView()->targeter();
  CHECK(view_targeter);
  return view_targeter;
}

bool View::CanAcceptEvent(const ui::Event& event) {
  return IsDrawn();
}

ui::EventTarget* View::GetParentTarget() {
  return parent_;
}

std::unique_ptr<ui::EventTargetIterator> View::GetChildIterator() const {
  return base::WrapUnique(new ui::EventTargetIteratorImpl<View>(children_));
}

ui::EventTargeter* View::GetEventTargeter() {
  return targeter_.get();
}

void View::ConvertEventToTarget(ui::EventTarget* target,
                                ui::LocatedEvent* event) {
  event->ConvertLocationToTarget(this, static_cast<View*>(target));
}

// Accelerators ----------------------------------------------------------------

void View::AddAccelerator(const ui::Accelerator& accelerator) {
  if (!accelerators_.get())
    accelerators_.reset(new std::vector<ui::Accelerator>());

  if (!ContainsValue(*accelerators_.get(), accelerator))
    accelerators_->push_back(accelerator);

  RegisterPendingAccelerators();
}

void View::RemoveAccelerator(const ui::Accelerator& accelerator) {
  if (!accelerators_.get()) {
    NOTREACHED() << "Removing non-existing accelerator";
    return;
  }

  std::vector<ui::Accelerator>::iterator i(
      std::find(accelerators_->begin(), accelerators_->end(), accelerator));
  if (i == accelerators_->end()) {
    NOTREACHED() << "Removing non-existing accelerator";
    return;
  }

  size_t index = i - accelerators_->begin();
  accelerators_->erase(i);
  if (index >= registered_accelerator_count_) {
    // The accelerator is not registered to FocusManager.
    return;
  }
  --registered_accelerator_count_;

  // Providing we are attached to a Widget and registered with a focus manager,
  // we should de-register from that focus manager now.
  if (GetWidget() && accelerator_focus_manager_)
    accelerator_focus_manager_->UnregisterAccelerator(accelerator, this);
}

void View::ResetAccelerators() {
  if (accelerators_.get())
    UnregisterAccelerators(false);
}

bool View::AcceleratorPressed(const ui::Accelerator& accelerator) {
  return false;
}

bool View::CanHandleAccelerators() const {
  const Widget* widget = GetWidget();
  if (!enabled() || !IsDrawn() || !widget || !widget->IsVisible())
    return false;
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  // Non-ChromeOS aura windows have an associated FocusManagerEventHandler which
  // adds currently focused view as an event PreTarget (see
  // DesktopNativeWidgetAura::InitNativeWidget). However, the focused view isn't
  // always the right view to handle accelerators: It should only handle them
  // when active. Only top level widgets can be active, so for child widgets
  // check if they are focused instead. ChromeOS also behaves different than
  // Linux when an extension popup is about to handle the accelerator.
  bool child = widget && widget->GetTopLevelWidget() != widget;
  bool focus_in_child =
      widget &&
      widget->GetRootView()->Contains(GetFocusManager()->GetFocusedView());
  if ((child && !focus_in_child) || (!child && !widget->IsActive()))
    return false;
#endif
  return true;
}

// Focus -----------------------------------------------------------------------

bool View::HasFocus() const {
  const FocusManager* focus_manager = GetFocusManager();
  return focus_manager && (focus_manager->GetFocusedView() == this);
}

View* View::GetNextFocusableView() {
  return next_focusable_view_;
}

const View* View::GetNextFocusableView() const {
  return next_focusable_view_;
}

View* View::GetPreviousFocusableView() {
  return previous_focusable_view_;
}

void View::SetNextFocusableView(View* view) {
  if (view)
    view->previous_focusable_view_ = this;
  next_focusable_view_ = view;
}

void View::SetFocusBehavior(FocusBehavior focus_behavior) {
  if (focus_behavior_ == focus_behavior)
    return;

  focus_behavior_ = focus_behavior;
  AdvanceFocusIfNecessary();
}

bool View::IsFocusable() const {
  return focus_behavior_ == FocusBehavior::ALWAYS && enabled_ && IsDrawn();
}

bool View::IsAccessibilityFocusable() const {
  return focus_behavior_ != FocusBehavior::NEVER && enabled_ && IsDrawn();
}

FocusManager* View::GetFocusManager() {
  Widget* widget = GetWidget();
  return widget ? widget->GetFocusManager() : NULL;
}

const FocusManager* View::GetFocusManager() const {
  const Widget* widget = GetWidget();
  return widget ? widget->GetFocusManager() : NULL;
}

void View::RequestFocus() {
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager) {
    bool focusable = focus_manager->keyboard_accessible()
                         ? IsAccessibilityFocusable()
                         : IsFocusable();
    if (focusable)
      focus_manager->SetFocusedView(this);
  }
}

bool View::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  return false;
}

FocusTraversable* View::GetFocusTraversable() {
  return NULL;
}

FocusTraversable* View::GetPaneFocusTraversable() {
  return NULL;
}

// Tooltips --------------------------------------------------------------------

bool View::GetTooltipText(const gfx::Point& p, base::string16* tooltip) const {
  return false;
}

bool View::GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* loc) const {
  return false;
}

// Context menus ---------------------------------------------------------------

void View::ShowContextMenu(const gfx::Point& p,
                           ui::MenuSourceType source_type) {
  if (!context_menu_controller_)
    return;

  context_menu_controller_->ShowContextMenuForView(this, p, source_type);
}

// static
bool View::ShouldShowContextMenuOnMousePress() {
  return kContextMenuOnMousePress;
}

gfx::Point View::GetKeyboardContextMenuLocation() {
  gfx::Rect vis_bounds = GetVisibleBounds();
  gfx::Point screen_point(vis_bounds.x() + vis_bounds.width() / 2,
                          vis_bounds.y() + vis_bounds.height() / 2);
  ConvertPointToScreen(this, &screen_point);
  return screen_point;
}

// Drag and drop ---------------------------------------------------------------

bool View::GetDropFormats(int* formats,
                          std::set<ui::Clipboard::FormatType>* format_types) {
  return false;
}

bool View::AreDropTypesRequired() {
  return false;
}

bool View::CanDrop(const OSExchangeData& data) {
  // TODO(sky): when I finish up migration, this should default to true.
  return false;
}

void View::OnDragEntered(const ui::DropTargetEvent& event) {
}

int View::OnDragUpdated(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_NONE;
}

void View::OnDragExited() {
}

int View::OnPerformDrop(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_NONE;
}

void View::OnDragDone() {
}

// static
bool View::ExceededDragThreshold(const gfx::Vector2d& delta) {
  return (abs(delta.x()) > GetHorizontalDragThreshold() ||
          abs(delta.y()) > GetVerticalDragThreshold());
}

// Accessibility----------------------------------------------------------------

gfx::NativeViewAccessible View::GetNativeViewAccessible() {
  if (!native_view_accessibility_)
    native_view_accessibility_ = NativeViewAccessibility::Create(this);
  if (native_view_accessibility_)
    return native_view_accessibility_->GetNativeObject();
  return NULL;
}

void View::NotifyAccessibilityEvent(
    ui::AXEvent event_type,
    bool send_native_event) {
  if (ViewsDelegate::GetInstance())
    ViewsDelegate::GetInstance()->NotifyAccessibilityEvent(this, event_type);

  if (send_native_event && GetWidget()) {
    if (!native_view_accessibility_)
      native_view_accessibility_ = NativeViewAccessibility::Create(this);
    if (native_view_accessibility_)
      native_view_accessibility_->NotifyAccessibilityEvent(event_type);
  }
}

// Scrolling -------------------------------------------------------------------

void View::ScrollRectToVisible(const gfx::Rect& rect) {
  // We must take RTL UI mirroring into account when adjusting the position of
  // the region.
  if (parent_) {
    gfx::Rect scroll_rect(rect);
    scroll_rect.Offset(GetMirroredX(), y());
    parent_->ScrollRectToVisible(scroll_rect);
  }
}

int View::GetPageScrollIncrement(ScrollView* scroll_view,
                                 bool is_horizontal, bool is_positive) {
  return 0;
}

int View::GetLineScrollIncrement(ScrollView* scroll_view,
                                 bool is_horizontal, bool is_positive) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// View, protected:

// Size and disposition --------------------------------------------------------

void View::OnBoundsChanged(const gfx::Rect& previous_bounds) {
}

void View::PreferredSizeChanged() {
  InvalidateLayout();
  if (parent_)
    parent_->ChildPreferredSizeChanged(this);
}

bool View::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return false;
}

void View::OnVisibleBoundsChanged() {
}

// Tree operations -------------------------------------------------------------

void View::ViewHierarchyChanged(const ViewHierarchyChangedDetails& details) {
}

void View::VisibilityChanged(View* starting_from, bool is_visible) {
}

void View::NativeViewHierarchyChanged() {
  FocusManager* focus_manager = GetFocusManager();
  if (accelerator_focus_manager_ != focus_manager) {
    UnregisterAccelerators(true);

    if (focus_manager)
      RegisterPendingAccelerators();
  }
}

// Painting --------------------------------------------------------------------

void View::PaintChildren(const ui::PaintContext& context) {
  TRACE_EVENT1("views", "View::PaintChildren", "class", GetClassName());
  for (int i = 0, count = child_count(); i < count; ++i)
    if (!child_at(i)->layer())
      child_at(i)->Paint(context);
}

void View::OnPaint(gfx::Canvas* canvas) {
  TRACE_EVENT1("views", "View::OnPaint", "class", GetClassName());
  OnPaintBackground(canvas);
  OnPaintBorder(canvas);
}

void View::OnPaintBackground(gfx::Canvas* canvas) {
  if (background_.get()) {
    TRACE_EVENT2("views", "View::OnPaintBackground",
                 "width", canvas->sk_canvas()->getBaseLayerSize().width(),
                 "height", canvas->sk_canvas()->getBaseLayerSize().height());
    background_->Paint(canvas, this);
  }
}

void View::OnPaintBorder(gfx::Canvas* canvas) {
  if (border_.get()) {
    TRACE_EVENT2("views", "View::OnPaintBorder",
                 "width", canvas->sk_canvas()->getBaseLayerSize().width(),
                 "height", canvas->sk_canvas()->getBaseLayerSize().height());
    border_->Paint(*this, canvas);
  }
}

// Accelerated Painting --------------------------------------------------------

gfx::Vector2d View::CalculateOffsetToAncestorWithLayer(
    ui::Layer** layer_parent) {
  if (layer()) {
    if (layer_parent)
      *layer_parent = layer();
    return gfx::Vector2d();
  }
  if (!parent_)
    return gfx::Vector2d();

  return gfx::Vector2d(GetMirroredX(), y()) +
      parent_->CalculateOffsetToAncestorWithLayer(layer_parent);
}

void View::UpdateParentLayer() {
  if (!layer())
    return;

  ui::Layer* parent_layer = NULL;
  gfx::Vector2d offset(GetMirroredX(), y());

  if (parent_)
    offset += parent_->CalculateOffsetToAncestorWithLayer(&parent_layer);

  ReparentLayer(offset, parent_layer);
}

void View::MoveLayerToParent(ui::Layer* parent_layer,
                             const gfx::Point& point) {
  gfx::Point local_point(point);
  if (parent_layer != layer())
    local_point.Offset(GetMirroredX(), y());
  if (layer() && parent_layer != layer()) {
    parent_layer->Add(layer());
    SetLayerBounds(gfx::Rect(local_point.x(), local_point.y(),
                             width(), height()));
  } else {
    for (int i = 0, count = child_count(); i < count; ++i)
      child_at(i)->MoveLayerToParent(parent_layer, local_point);
  }
}

void View::UpdateLayerVisibility() {
  bool visible = visible_;
  for (const View* v = parent_; visible && v && !v->layer(); v = v->parent_)
    visible = v->visible();

  UpdateChildLayerVisibility(visible);
}

void View::UpdateChildLayerVisibility(bool ancestor_visible) {
  if (layer()) {
    layer()->SetVisible(ancestor_visible && visible_);
  } else {
    for (int i = 0, count = child_count(); i < count; ++i)
      child_at(i)->UpdateChildLayerVisibility(ancestor_visible && visible_);
  }
}

void View::UpdateChildLayerBounds(const gfx::Vector2d& offset) {
  if (layer()) {
    SetLayerBounds(GetLocalBounds() + offset);
  } else {
    for (int i = 0, count = child_count(); i < count; ++i) {
      View* child = child_at(i);
      child->UpdateChildLayerBounds(
          offset + gfx::Vector2d(child->GetMirroredX(), child->y()));
    }
  }
}

void View::OnPaintLayer(const ui::PaintContext& context) {
  Paint(context);
}

void View::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {
}

void View::OnDeviceScaleFactorChanged(float device_scale_factor) {
  snap_layer_to_pixel_boundary_ =
      (device_scale_factor - std::floor(device_scale_factor)) != 0.0f;
  SnapLayerToPixelBoundary();
  // Repainting with new scale factor will paint the content at the right scale.
}

base::Closure View::PrepareForLayerBoundsChange() {
  return base::Closure();
}

void View::ReorderLayers() {
  View* v = this;
  while (v && !v->layer())
    v = v->parent();

  Widget* widget = GetWidget();
  if (!v) {
    if (widget) {
      ui::Layer* layer = widget->GetLayer();
      if (layer)
        widget->GetRootView()->ReorderChildLayers(layer);
    }
  } else {
    v->ReorderChildLayers(v->layer());
  }

  if (widget) {
    // Reorder the widget's child NativeViews in case a child NativeView is
    // associated with a view (eg via a NativeViewHost). Always do the
    // reordering because the associated NativeView's layer (if it has one)
    // is parented to the widget's layer regardless of whether the host view has
    // an ancestor with a layer.
    widget->ReorderNativeViews();
  }
}

void View::ReorderChildLayers(ui::Layer* parent_layer) {
  if (layer() && layer() != parent_layer) {
    DCHECK_EQ(parent_layer, layer()->parent());
    parent_layer->StackAtBottom(layer());
  } else {
    // Iterate backwards through the children so that a child with a layer
    // which is further to the back is stacked above one which is further to
    // the front.
    for (Views::reverse_iterator it(children_.rbegin());
         it != children_.rend(); ++it) {
      (*it)->ReorderChildLayers(parent_layer);
    }
  }
}

// Input -----------------------------------------------------------------------

View::DragInfo* View::GetDragInfo() {
  return parent_ ? parent_->GetDragInfo() : NULL;
}

// Focus -----------------------------------------------------------------------

void View::OnFocus() {
  // TODO(beng): Investigate whether it's possible for us to move this to
  //             Focus().
  // By default, we clear the native focus. This ensures that no visible native
  // view as the focus and that we still receive keyboard inputs.
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->ClearNativeFocus();

  // TODO(beng): Investigate whether it's possible for us to move this to
  //             Focus().
  // Notify assistive technologies of the focus change.
  NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, true);
}

void View::OnBlur() {
}

void View::Focus() {
  OnFocus();
}

void View::Blur() {
  OnBlur();
}

// Tooltips --------------------------------------------------------------------

void View::TooltipTextChanged() {
  Widget* widget = GetWidget();
  // TooltipManager may be null if there is a problem creating it.
  if (widget && widget->GetTooltipManager())
    widget->GetTooltipManager()->TooltipTextChanged(this);
}

// Drag and drop ---------------------------------------------------------------

int View::GetDragOperations(const gfx::Point& press_pt) {
  return drag_controller_ ?
      drag_controller_->GetDragOperationsForView(this, press_pt) :
      ui::DragDropTypes::DRAG_NONE;
}

void View::WriteDragData(const gfx::Point& press_pt, OSExchangeData* data) {
  DCHECK(drag_controller_);
  drag_controller_->WriteDragDataForView(this, press_pt, data);
}

bool View::InDrag() const {
  const Widget* widget = GetWidget();
  return widget ? widget->dragged_view() == this : false;
}

int View::GetHorizontalDragThreshold() {
  // TODO(jennyz): This value may need to be adjusted for different platforms
  // and for different display density.
  return kDefaultHorizontalDragThreshold;
}

int View::GetVerticalDragThreshold() {
  // TODO(jennyz): This value may need to be adjusted for different platforms
  // and for different display density.
  return kDefaultVerticalDragThreshold;
}

// Debugging -------------------------------------------------------------------

#if !defined(NDEBUG)

std::string View::PrintViewGraph(bool first) {
  return DoPrintViewGraph(first, this);
}

std::string View::DoPrintViewGraph(bool first, View* view_with_children) {
  // 64-bit pointer = 16 bytes of hex + "0x" + '\0' = 19.
  const size_t kMaxPointerStringLength = 19;

  std::string result;

  if (first)
    result.append("digraph {\n");

  // Node characteristics.
  char p[kMaxPointerStringLength];

  const std::string class_name(GetClassName());
  size_t base_name_index = class_name.find_last_of('/');
  if (base_name_index == std::string::npos)
    base_name_index = 0;
  else
    base_name_index++;

  char bounds_buffer[512];

  // Information about current node.
  base::snprintf(p, arraysize(bounds_buffer), "%p", view_with_children);
  result.append("  N");
  result.append(p + 2);
  result.append(" [label=\"");

  result.append(class_name.substr(base_name_index).c_str());

  base::snprintf(bounds_buffer,
                 arraysize(bounds_buffer),
                 "\\n bounds: (%d, %d), (%dx%d)",
                 bounds().x(),
                 bounds().y(),
                 bounds().width(),
                 bounds().height());
  result.append(bounds_buffer);

  gfx::DecomposedTransform decomp;
  if (!GetTransform().IsIdentity() &&
      gfx::DecomposeTransform(&decomp, GetTransform())) {
    base::snprintf(bounds_buffer,
                   arraysize(bounds_buffer),
                   "\\n translation: (%f, %f)",
                   decomp.translate[0],
                   decomp.translate[1]);
    result.append(bounds_buffer);

    base::snprintf(bounds_buffer,
                   arraysize(bounds_buffer),
                   "\\n rotation: %3.2f",
                   std::acos(decomp.quaternion[3]) * 360.0 / M_PI);
    result.append(bounds_buffer);

    base::snprintf(bounds_buffer,
                   arraysize(bounds_buffer),
                   "\\n scale: (%2.4f, %2.4f)",
                   decomp.scale[0],
                   decomp.scale[1]);
    result.append(bounds_buffer);
  }

  result.append("\"");
  if (!parent_)
    result.append(", shape=box");
  if (layer()) {
    if (layer()->has_external_content())
      result.append(", color=green");
    else
      result.append(", color=red");

    if (layer()->fills_bounds_opaquely())
      result.append(", style=filled");
  }
  result.append("]\n");

  // Link to parent.
  if (parent_) {
    char pp[kMaxPointerStringLength];

    base::snprintf(pp, kMaxPointerStringLength, "%p", parent_);
    result.append("  N");
    result.append(pp + 2);
    result.append(" -> N");
    result.append(p + 2);
    result.append("\n");
  }

  // Children.
  for (int i = 0, count = view_with_children->child_count(); i < count; ++i)
    result.append(view_with_children->child_at(i)->PrintViewGraph(false));

  if (first)
    result.append("}\n");

  return result;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// View, private:

// DropInfo --------------------------------------------------------------------

void View::DragInfo::Reset() {
  possible_drag = false;
  start_pt = gfx::Point();
}

void View::DragInfo::PossibleDrag(const gfx::Point& p) {
  possible_drag = true;
  start_pt = p;
}

// Painting --------------------------------------------------------------------

void View::SchedulePaintBoundsChanged(SchedulePaintType type) {
  // If we have a layer and the View's size did not change, we do not need to
  // schedule any paints since the layer will be redrawn at its new location
  // during the next Draw() cycle in the compositor.
  if (!layer() || type == SCHEDULE_PAINT_SIZE_CHANGED) {
    // Otherwise, if the size changes or we don't have a layer then we need to
    // use SchedulePaint to invalidate the area occupied by the View.
    SchedulePaint();
  } else if (parent_ && type == SCHEDULE_PAINT_SIZE_SAME) {
    // The compositor doesn't Draw() until something on screen changes, so
    // if our position changes but nothing is being animated on screen, then
    // tell the compositor to redraw the scene. We know layer() exists due to
    // the above if clause.
    layer()->ScheduleDraw();
  }
}

void View::SchedulePaintOnParent() {
  if (parent_) {
    // Translate the requested paint rect to the parent's coordinate system
    // then pass this notification up to the parent.
    parent_->SchedulePaintInRect(ConvertRectToParent(GetLocalBounds()));
  }
}

// Tree operations -------------------------------------------------------------

void View::DoRemoveChildView(View* view,
                             bool update_focus_cycle,
                             bool update_tool_tip,
                             bool delete_removed_view,
                             View* new_parent) {
  DCHECK(view);

  const Views::iterator i(std::find(children_.begin(), children_.end(), view));
  if (i == children_.end())
    return;

  std::unique_ptr<View> view_to_be_deleted;
  if (update_focus_cycle) {
    View* next_focusable = view->next_focusable_view_;
    View* prev_focusable = view->previous_focusable_view_;
    if (prev_focusable)
      prev_focusable->next_focusable_view_ = next_focusable;
    if (next_focusable)
      next_focusable->previous_focusable_view_ = prev_focusable;
  }

  Widget* widget = GetWidget();
  if (widget) {
    UnregisterChildrenForVisibleBoundsNotification(view);
    if (view->visible())
      view->SchedulePaint();

    if (!new_parent || new_parent->GetWidget() != widget)
      widget->NotifyWillRemoveView(view);
  }

  // Make sure the layers belonging to the subtree rooted at |view| get
  // removed.
  view->OrphanLayers();
  if (widget)
    widget->UpdateRootLayers();

  view->PropagateRemoveNotifications(this, new_parent);
  view->parent_ = nullptr;

  if (delete_removed_view && !view->owned_by_client_)
    view_to_be_deleted.reset(view);

  children_.erase(i);

  if (update_tool_tip)
    UpdateTooltip();

  if (layout_manager_)
    layout_manager_->ViewRemoved(this, view);
}

void View::PropagateRemoveNotifications(View* old_parent, View* new_parent) {
  for (int i = 0, count = child_count(); i < count; ++i)
    child_at(i)->PropagateRemoveNotifications(old_parent, new_parent);

  ViewHierarchyChangedDetails details(false, old_parent, this, new_parent);
  for (View* v = this; v; v = v->parent_)
    v->ViewHierarchyChangedImpl(true, details);
}

void View::PropagateAddNotifications(
    const ViewHierarchyChangedDetails& details) {
  for (int i = 0, count = child_count(); i < count; ++i)
    child_at(i)->PropagateAddNotifications(details);
  ViewHierarchyChangedImpl(true, details);
}

void View::PropagateNativeViewHierarchyChanged() {
  for (int i = 0, count = child_count(); i < count; ++i)
    child_at(i)->PropagateNativeViewHierarchyChanged();
  NativeViewHierarchyChanged();
}

void View::ViewHierarchyChangedImpl(
    bool register_accelerators,
    const ViewHierarchyChangedDetails& details) {
  if (register_accelerators) {
    if (details.is_add) {
      // If you get this registration, you are part of a subtree that has been
      // added to the view hierarchy.
      if (GetFocusManager())
        RegisterPendingAccelerators();
    } else {
      if (details.child == this)
        UnregisterAccelerators(true);
    }
  }

  ViewHierarchyChanged(details);
  details.parent->needs_layout_ = true;
}

void View::PropagateNativeThemeChanged(const ui::NativeTheme* theme) {
  for (int i = 0, count = child_count(); i < count; ++i)
    child_at(i)->PropagateNativeThemeChanged(theme);
  OnNativeThemeChanged(theme);
}

// Size and disposition --------------------------------------------------------

void View::PropagateVisibilityNotifications(View* start, bool is_visible) {
  for (int i = 0, count = child_count(); i < count; ++i)
    child_at(i)->PropagateVisibilityNotifications(start, is_visible);
  VisibilityChangedImpl(start, is_visible);
}

void View::VisibilityChangedImpl(View* starting_from, bool is_visible) {
  VisibilityChanged(starting_from, is_visible);
}

void View::BoundsChanged(const gfx::Rect& previous_bounds) {
  if (visible_) {
    // Paint the new bounds.
    SchedulePaintBoundsChanged(
        bounds_.size() == previous_bounds.size() ? SCHEDULE_PAINT_SIZE_SAME :
        SCHEDULE_PAINT_SIZE_CHANGED);
  }

  if (layer()) {
    if (parent_) {
      SetLayerBounds(GetLocalBounds() +
                     gfx::Vector2d(GetMirroredX(), y()) +
                     parent_->CalculateOffsetToAncestorWithLayer(NULL));
    } else {
      SetLayerBounds(bounds_);
    }

    // In RTL mode, if our width has changed, our children's mirrored bounds
    // will have changed. Update the child's layer bounds, or if it is not a
    // layer, the bounds of any layers inside the child.
    if (base::i18n::IsRTL() && bounds_.width() != previous_bounds.width()) {
      for (int i = 0; i < child_count(); ++i) {
        View* child = child_at(i);
        child->UpdateChildLayerBounds(
            gfx::Vector2d(child->GetMirroredX(), child->y()));
      }
    }
  } else {
    // If our bounds have changed, then any descendant layer bounds may have
    // changed. Update them accordingly.
    UpdateChildLayerBounds(CalculateOffsetToAncestorWithLayer(NULL));
  }

  OnBoundsChanged(previous_bounds);

  if (needs_layout_ || previous_bounds.size() != size()) {
    needs_layout_ = false;
    Layout();
  }

  if (GetNeedsNotificationWhenVisibleBoundsChange())
    OnVisibleBoundsChanged();

  // Notify interested Views that visible bounds within the root view may have
  // changed.
  if (descendants_to_notify_.get()) {
    for (Views::iterator i(descendants_to_notify_->begin());
         i != descendants_to_notify_->end(); ++i) {
      (*i)->OnVisibleBoundsChanged();
    }
  }
}

// static
void View::RegisterChildrenForVisibleBoundsNotification(View* view) {
  if (view->GetNeedsNotificationWhenVisibleBoundsChange())
    view->RegisterForVisibleBoundsNotification();
  for (int i = 0; i < view->child_count(); ++i)
    RegisterChildrenForVisibleBoundsNotification(view->child_at(i));
}

// static
void View::UnregisterChildrenForVisibleBoundsNotification(View* view) {
  if (view->GetNeedsNotificationWhenVisibleBoundsChange())
    view->UnregisterForVisibleBoundsNotification();
  for (int i = 0; i < view->child_count(); ++i)
    UnregisterChildrenForVisibleBoundsNotification(view->child_at(i));
}

void View::RegisterForVisibleBoundsNotification() {
  if (registered_for_visible_bounds_notification_)
    return;

  registered_for_visible_bounds_notification_ = true;
  for (View* ancestor = parent_; ancestor; ancestor = ancestor->parent_)
    ancestor->AddDescendantToNotify(this);
}

void View::UnregisterForVisibleBoundsNotification() {
  if (!registered_for_visible_bounds_notification_)
    return;

  registered_for_visible_bounds_notification_ = false;
  for (View* ancestor = parent_; ancestor; ancestor = ancestor->parent_)
    ancestor->RemoveDescendantToNotify(this);
}

void View::AddDescendantToNotify(View* view) {
  DCHECK(view);
  if (!descendants_to_notify_.get())
    descendants_to_notify_.reset(new Views);
  descendants_to_notify_->push_back(view);
}

void View::RemoveDescendantToNotify(View* view) {
  DCHECK(view && descendants_to_notify_.get());
  Views::iterator i(std::find(
      descendants_to_notify_->begin(), descendants_to_notify_->end(), view));
  DCHECK(i != descendants_to_notify_->end());
  descendants_to_notify_->erase(i);
  if (descendants_to_notify_->empty())
    descendants_to_notify_.reset();
}

void View::SetLayerBounds(const gfx::Rect& bounds) {
  layer()->SetBounds(bounds);
  SnapLayerToPixelBoundary();
}

// Transformations -------------------------------------------------------------

bool View::GetTransformRelativeTo(const View* ancestor,
                                  gfx::Transform* transform) const {
  const View* p = this;

  while (p && p != ancestor) {
    transform->ConcatTransform(p->GetTransform());
    gfx::Transform translation;
    translation.Translate(static_cast<float>(p->GetMirroredX()),
                          static_cast<float>(p->y()));
    transform->ConcatTransform(translation);

    p = p->parent_;
  }

  return p == ancestor;
}

// Coordinate conversion -------------------------------------------------------

bool View::ConvertPointForAncestor(const View* ancestor,
                                   gfx::Point* point) const {
  gfx::Transform trans;
  // TODO(sad): Have some way of caching the transformation results.
  bool result = GetTransformRelativeTo(ancestor, &trans);
  auto p = gfx::Point3F(gfx::PointF(*point));
  trans.TransformPoint(&p);
  *point = gfx::ToFlooredPoint(p.AsPointF());
  return result;
}

bool View::ConvertPointFromAncestor(const View* ancestor,
                                    gfx::Point* point) const {
  gfx::Transform trans;
  bool result = GetTransformRelativeTo(ancestor, &trans);
  auto p = gfx::Point3F(gfx::PointF(*point));
  trans.TransformPointReverse(&p);
  *point = gfx::ToFlooredPoint(p.AsPointF());
  return result;
}

bool View::ConvertRectForAncestor(const View* ancestor,
                                  gfx::RectF* rect) const {
  gfx::Transform trans;
  // TODO(sad): Have some way of caching the transformation results.
  bool result = GetTransformRelativeTo(ancestor, &trans);
  trans.TransformRect(rect);
  return result;
}

bool View::ConvertRectFromAncestor(const View* ancestor,
                                   gfx::RectF* rect) const {
  gfx::Transform trans;
  bool result = GetTransformRelativeTo(ancestor, &trans);
  trans.TransformRectReverse(rect);
  return result;
}

// Accelerated painting --------------------------------------------------------

void View::CreateLayer() {
  // A new layer is being created for the view. So all the layers of the
  // sub-tree can inherit the visibility of the corresponding view.
  for (int i = 0, count = child_count(); i < count; ++i)
    child_at(i)->UpdateChildLayerVisibility(true);

  SetLayer(new ui::Layer());
  layer()->set_delegate(this);
  layer()->set_name(GetClassName());

  UpdateParentLayers();
  UpdateLayerVisibility();

  // The new layer needs to be ordered in the layer tree according
  // to the view tree. Children of this layer were added in order
  // in UpdateParentLayers().
  if (parent())
    parent()->ReorderLayers();

  Widget* widget = GetWidget();
  if (widget)
    widget->UpdateRootLayers();

  // Before having its own Layer, this View may have painted in to a Layer owned
  // by an ancestor View. Scheduling a paint on the parent View will erase this
  // View's painting effects on the ancestor View's Layer.
  // (See crbug.com/551492)
  SchedulePaintOnParent();
}

bool View::UpdateParentLayers() {
  // Attach all top-level un-parented layers.
  if (layer()) {
    if (!layer()->parent()) {
      UpdateParentLayer();
      return true;
    }
    // The layers of any child views are already in place, so we can stop
    // iterating here.
    return false;
  }
  bool result = false;
  for (int i = 0, count = child_count(); i < count; ++i) {
    if (child_at(i)->UpdateParentLayers())
      result = true;
  }
  return result;
}

void View::OrphanLayers() {
  if (layer()) {
    if (layer()->parent())
      layer()->parent()->Remove(layer());

    // The layer belonging to this View has already been orphaned. It is not
    // necessary to orphan the child layers.
    return;
  }
  for (int i = 0, count = child_count(); i < count; ++i)
    child_at(i)->OrphanLayers();
}

void View::ReparentLayer(const gfx::Vector2d& offset, ui::Layer* parent_layer) {
  layer()->SetBounds(GetLocalBounds() + offset);
  DCHECK_NE(layer(), parent_layer);
  if (parent_layer)
    parent_layer->Add(layer());
  layer()->SchedulePaint(GetLocalBounds());
  MoveLayerToParent(layer(), gfx::Point());
}

void View::DestroyLayer() {
  ui::Layer* new_parent = layer()->parent();
  std::vector<ui::Layer*> children = layer()->children();
  for (size_t i = 0; i < children.size(); ++i) {
    layer()->Remove(children[i]);
    if (new_parent)
      new_parent->Add(children[i]);
  }

  LayerOwner::DestroyLayer();

  if (new_parent)
    ReorderLayers();

  UpdateChildLayerBounds(CalculateOffsetToAncestorWithLayer(NULL));

  SchedulePaint();

  Widget* widget = GetWidget();
  if (widget)
    widget->UpdateRootLayers();
}

// Input -----------------------------------------------------------------------

bool View::ProcessMousePressed(const ui::MouseEvent& event) {
  int drag_operations =
      (enabled_ && event.IsOnlyLeftMouseButton() &&
       HitTestPoint(event.location())) ?
       GetDragOperations(event.location()) : 0;
  ContextMenuController* context_menu_controller = event.IsRightMouseButton() ?
      context_menu_controller_ : 0;
  View::DragInfo* drag_info = GetDragInfo();

  // TODO(sky): for debugging 360238.
  int storage_id = 0;
  if (event.IsOnlyRightMouseButton() && context_menu_controller &&
      kContextMenuOnMousePress && HitTestPoint(event.location())) {
    ViewStorage* view_storage = ViewStorage::GetInstance();
    storage_id = view_storage->CreateStorageID();
    view_storage->StoreView(storage_id, this);
  }

  const bool enabled = enabled_;
  const bool result = OnMousePressed(event);

  if (!enabled)
    return result;

  if (event.IsOnlyRightMouseButton() && context_menu_controller &&
      kContextMenuOnMousePress) {
    // Assume that if there is a context menu controller we won't be deleted
    // from mouse pressed.
    gfx::Point location(event.location());
    if (HitTestPoint(location)) {
      if (storage_id != 0)
        CHECK_EQ(this, ViewStorage::GetInstance()->RetrieveView(storage_id));
      ConvertPointToScreen(this, &location);
      ShowContextMenu(location, ui::MENU_SOURCE_MOUSE);
      return true;
    }
  }

  // WARNING: we may have been deleted, don't use any View variables.
  if (drag_operations != ui::DragDropTypes::DRAG_NONE) {
    drag_info->PossibleDrag(event.location());
    return true;
  }
  return !!context_menu_controller || result;
}

bool View::ProcessMouseDragged(const ui::MouseEvent& event) {
  // Copy the field, that way if we're deleted after drag and drop no harm is
  // done.
  ContextMenuController* context_menu_controller = context_menu_controller_;
  const bool possible_drag = GetDragInfo()->possible_drag;
  if (possible_drag &&
      ExceededDragThreshold(GetDragInfo()->start_pt - event.location()) &&
      (!drag_controller_ ||
       drag_controller_->CanStartDragForView(
           this, GetDragInfo()->start_pt, event.location()))) {
    DoDrag(event, GetDragInfo()->start_pt,
           ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  } else {
    if (OnMouseDragged(event))
      return true;
    // Fall through to return value based on context menu controller.
  }
  // WARNING: we may have been deleted.
  return (context_menu_controller != NULL) || possible_drag;
}

void View::ProcessMouseReleased(const ui::MouseEvent& event) {
  if (!kContextMenuOnMousePress && context_menu_controller_ &&
      event.IsOnlyRightMouseButton()) {
    // Assume that if there is a context menu controller we won't be deleted
    // from mouse released.
    gfx::Point location(event.location());
    OnMouseReleased(event);
    if (HitTestPoint(location)) {
      ConvertPointToScreen(this, &location);
      ShowContextMenu(location, ui::MENU_SOURCE_MOUSE);
    }
  } else {
    OnMouseReleased(event);
  }
  // WARNING: we may have been deleted.
}

// Accelerators ----------------------------------------------------------------

void View::RegisterPendingAccelerators() {
  if (!accelerators_.get() ||
      registered_accelerator_count_ == accelerators_->size()) {
    // No accelerators are waiting for registration.
    return;
  }

  if (!GetWidget()) {
    // The view is not yet attached to a widget, defer registration until then.
    return;
  }

  accelerator_focus_manager_ = GetFocusManager();
  if (!accelerator_focus_manager_) {
    // Some crash reports seem to show that we may get cases where we have no
    // focus manager (see bug #1291225).  This should never be the case, just
    // making sure we don't crash.
    NOTREACHED();
    return;
  }
  for (std::vector<ui::Accelerator>::const_iterator i(
           accelerators_->begin() + registered_accelerator_count_);
       i != accelerators_->end(); ++i) {
    accelerator_focus_manager_->RegisterAccelerator(
        *i, ui::AcceleratorManager::kNormalPriority, this);
  }
  registered_accelerator_count_ = accelerators_->size();
}

void View::UnregisterAccelerators(bool leave_data_intact) {
  if (!accelerators_.get())
    return;

  if (GetWidget()) {
    if (accelerator_focus_manager_) {
      accelerator_focus_manager_->UnregisterAccelerators(this);
      accelerator_focus_manager_ = NULL;
    }
    if (!leave_data_intact) {
      accelerators_->clear();
      accelerators_.reset();
    }
    registered_accelerator_count_ = 0;
  }
}

// Focus -----------------------------------------------------------------------

void View::InitFocusSiblings(View* v, int index) {
  int count = child_count();

  if (count == 0) {
    v->next_focusable_view_ = NULL;
    v->previous_focusable_view_ = NULL;
  } else {
    if (index == count) {
      // We are inserting at the end, but the end of the child list may not be
      // the last focusable element. Let's try to find an element with no next
      // focusable element to link to.
      View* last_focusable_view = NULL;
      for (Views::iterator i(children_.begin()); i != children_.end(); ++i) {
          if (!(*i)->next_focusable_view_) {
            last_focusable_view = *i;
            break;
          }
      }
      if (last_focusable_view == NULL) {
        // Hum... there is a cycle in the focus list. Let's just insert ourself
        // after the last child.
        View* prev = children_[index - 1];
        v->previous_focusable_view_ = prev;
        v->next_focusable_view_ = prev->next_focusable_view_;
        prev->next_focusable_view_->previous_focusable_view_ = v;
        prev->next_focusable_view_ = v;
      } else {
        last_focusable_view->next_focusable_view_ = v;
        v->next_focusable_view_ = NULL;
        v->previous_focusable_view_ = last_focusable_view;
      }
    } else {
      View* prev = children_[index]->GetPreviousFocusableView();
      v->previous_focusable_view_ = prev;
      v->next_focusable_view_ = children_[index];
      if (prev)
        prev->next_focusable_view_ = v;
      children_[index]->previous_focusable_view_ = v;
    }
  }
}

void View::AdvanceFocusIfNecessary() {
  // Focus should only be advanced if this is the focused view and has become
  // unfocusable. If the view is still focusable or is not focused, we can
  // return early avoiding furthur unnecessary checks. Focusability check is
  // performed first as it tends to be faster.
  if (IsAccessibilityFocusable() || !HasFocus())
    return;

  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->AdvanceFocusIfNecessary();
}

// System events ---------------------------------------------------------------

void View::PropagateThemeChanged() {
  for (int i = child_count() - 1; i >= 0; --i)
    child_at(i)->PropagateThemeChanged();
  OnThemeChanged();
}

void View::PropagateLocaleChanged() {
  for (int i = child_count() - 1; i >= 0; --i)
    child_at(i)->PropagateLocaleChanged();
  OnLocaleChanged();
}

void View::PropagateDeviceScaleFactorChanged(float device_scale_factor) {
  for (int i = child_count() - 1; i >= 0; --i)
    child_at(i)->PropagateDeviceScaleFactorChanged(device_scale_factor);

  // If the view is drawing to the layer, OnDeviceScaleFactorChanged() is called
  // through LayerDelegate callback.
  if (!layer())
    OnDeviceScaleFactorChanged(device_scale_factor);
}

// Tooltips --------------------------------------------------------------------

void View::UpdateTooltip() {
  Widget* widget = GetWidget();
  // TODO(beng): The TooltipManager NULL check can be removed when we
  //             consolidate Init() methods and make views_unittests Init() all
  //             Widgets that it uses.
  if (widget && widget->GetTooltipManager())
    widget->GetTooltipManager()->UpdateTooltip();
}

// Drag and drop ---------------------------------------------------------------

bool View::DoDrag(const ui::LocatedEvent& event,
                  const gfx::Point& press_pt,
                  ui::DragDropTypes::DragEventSource source) {
  int drag_operations = GetDragOperations(press_pt);
  if (drag_operations == ui::DragDropTypes::DRAG_NONE)
    return false;

  Widget* widget = GetWidget();
  // We should only start a drag from an event, implying we have a widget.
  DCHECK(widget);

  // Don't attempt to start a drag while in the process of dragging. This is
  // especially important on X where we can get multiple mouse move events when
  // we start the drag.
  if (widget->dragged_view())
    return false;

  OSExchangeData data;
  WriteDragData(press_pt, &data);

  // Message the RootView to do the drag and drop. That way if we're removed
  // the RootView can detect it and avoid calling us back.
  gfx::Point widget_location(event.location());
  ConvertPointToWidget(this, &widget_location);
  widget->RunShellDrag(this, data, widget_location, drag_operations, source);
  // WARNING: we may have been deleted.
  return true;
}

}  // namespace views
