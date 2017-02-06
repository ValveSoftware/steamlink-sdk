// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_native_widget.h"

#import <objc/runtime.h>
#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#import "ui/base/cocoa/constrained_window/constrained_window_animation.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#import "ui/gfx/mac/nswindow_frame_controls.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/drag_drop_client_mac.h"
#import "ui/views/cocoa/cocoa_mouse_capture.h"
#import "ui/views/cocoa/cocoa_window_move_loop.h"
#include "ui/views/cocoa/tooltip_manager_mac.h"
#import "ui/views/cocoa/views_nswindow_delegate.h"
#import "ui/views/cocoa/widget_owner_nswindow_adapter.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/widget/widget_delegate.h"

extern "C" {

typedef int32_t CGSConnection;
CGSConnection _CGSDefaultConnection();
CGError CGSSetWindowBackgroundBlurRadius(CGSConnection connection,
                                         NSInteger windowNumber,
                                         int radius);

}

// The NSView that hosts the composited CALayer drawing the UI. It fills the
// window but is not hittable so that accessibility hit tests always go to the
// BridgedContentView.
@interface ViewsCompositorSuperview : NSView
@end

@implementation ViewsCompositorSuperview
- (NSView*)hitTest:(NSPoint)aPoint {
  return nil;
}
@end

// This class overrides NSAnimation methods to invalidate the shadow for each
// frame. It is required because the show animation uses CGSSetWindowWarp()
// which is touchy about the consistency of the points it is given. The show
// animation includes a translate, which fails to apply properly to the window
// shadow, when that shadow is derived from a layer-hosting view. So invalidate
// it. This invalidation is only needed to cater for the translate. It is not
// required if CGSSetWindowWarp() is used in a way that keeps the center point
// of the window stationary (e.g. a scale). It's also not required for the hide
// animation: in that case, the shadow is never invalidated so retains the
// shadow calculated before a translate is applied.
@interface ModalShowAnimationWithLayer : ConstrainedWindowAnimationShow
@end

@implementation ModalShowAnimationWithLayer
- (void)stopAnimation {
  [super stopAnimation];
  [window_ invalidateShadow];
}
- (void)setCurrentProgress:(NSAnimationProgress)progress {
  [super setCurrentProgress:progress];
  [window_ invalidateShadow];
}
@end

namespace {

using RankMap = std::map<NSView*, int>;

// SDK 10.11 contains incompatible changes of sortSubviewsUsingFunction.
// It takes (__kindof NSView*) as comparator argument.
// https://llvm.org/bugs/show_bug.cgi?id=25149
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
using NSViewComparatorValue = id;
#else
using NSViewComparatorValue = __kindof NSView*;
#endif

const CGFloat kMavericksMenuOpacity = 251.0 / 255.0;
const CGFloat kYosemiteMenuOpacity = 177.0 / 255.0;
const int kYosemiteMenuBlur = 80;

// Margin at edge and corners of the window that trigger resizing. These match
// actual Cocoa resize margins.
const int kResizeAreaEdgeSize = 3;
const int kResizeAreaCornerSize = 12;

int kWindowPropertiesKey;

float GetDeviceScaleFactorFromView(NSView* view) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(view);
  DCHECK(display.is_valid());
  return display.device_scale_factor();
}

// Returns true if bounds passed to window in SetBounds should be treated as
// though they are in screen coordinates.
bool PositionWindowInScreenCoordinates(views::Widget* widget,
                                       views::Widget::InitParams::Type type) {
  // Replicate the logic in desktop_aura/desktop_screen_position_client.cc.
  if (views::GetAuraWindowTypeForWidgetType(type) == ui::wm::WINDOW_TYPE_POPUP)
    return true;

  return widget && widget->is_top_level();
}

// Return the content size for a minimum or maximum widget size.
gfx::Size GetClientSizeForWindowSize(NSWindow* window,
                                     const gfx::Size& window_size) {
  NSRect frame_rect =
      NSMakeRect(0, 0, window_size.width(), window_size.height());
  // Note gfx::Size will prevent dimensions going negative. They are allowed to
  // be zero at this point, because Widget::GetMinimumSize() may later increase
  // the size.
  return gfx::Size([window contentRectForFrameRect:frame_rect].size);
}

// Determine whether a point is within the resize area at the edges and corners
// of a window. This is used to ensure that mouse downs which would resize the
// window are not reposted. As there's no way to determine this from Cocoa APIs,
// this should aim to match Cocoa behavior as closely as possible.
bool IsPointInResizeArea(NSPoint point, NSWindow* window) {
  if (!([window styleMask] & NSResizableWindowMask))
    return false;

  bool can_resize_x = [window maxSize].width > [window minSize].width;
  bool can_resize_y = [window maxSize].height > [window minSize].height;
  NSSize window_size = [window frame].size;

  if (can_resize_x && (point.x < kResizeAreaEdgeSize ||
                       point.x >= window_size.width - kResizeAreaEdgeSize))
    return true;

  if (can_resize_y && (point.y < kResizeAreaEdgeSize ||
                       point.y >= window_size.height - kResizeAreaEdgeSize))
    return true;

  if (can_resize_x && can_resize_y &&
      (point.x < kResizeAreaCornerSize ||
       point.x >= window_size.width - kResizeAreaCornerSize) &&
      (point.y < kResizeAreaCornerSize ||
       point.y >= window_size.height - kResizeAreaCornerSize))
    return true;

  return false;
}

BOOL WindowWantsMouseDownReposted(NSEvent* ns_event) {
  id delegate = [[ns_event window] delegate];
  return
      [delegate
          respondsToSelector:@selector(shouldRepostPendingLeftMouseDown:)] &&
      [delegate shouldRepostPendingLeftMouseDown:[ns_event locationInWindow]];
}

// Check if a mouse-down event should drag the window. If so, repost the event.
NSEvent* RepostEventIfHandledByWindow(NSEvent* ns_event) {
  enum RepostState {
    // Nothing reposted: hit-test new mouse-downs to see if they need to be
    // ignored and reposted after changing draggability.
    NONE,
    // Expecting the next event to be the reposted event: let it go through.
    EXPECTING_REPOST,
    // If, while reposting, another mousedown was received: when the reposted
    // event is seen, ignore it.
    REPOST_CANCELLED,
  };

  // Which repost we're expecting to receive.
  static RepostState repost_state = NONE;
  // The event number of the reposted event. This let's us track whether an
  // event is actually the repost since user-generated events have increasing
  // event numbers. This is only valid while |repost_state != NONE|.
  static NSInteger reposted_event_number;

  NSInteger event_number = [ns_event eventNumber];

  // The logic here is a bit convoluted because we want to mitigate race
  // conditions if somehow a different mouse-down occurs between reposts.
  // Specifically, we want to avoid:
  // - BridgedNativeWidget's draggability getting out of sync (e.g. if it is
  //   draggable outside of a repost cycle),
  // - any repost loop.

  if (repost_state == NONE) {
    if (WindowWantsMouseDownReposted(ns_event)) {
      repost_state = EXPECTING_REPOST;
      reposted_event_number = event_number;
      CGEventPost(kCGSessionEventTap, [ns_event CGEvent]);
      return nil;
    }

    return ns_event;
  }

  if (repost_state == EXPECTING_REPOST) {
    // Call through so that the window is made non-draggable again.
    WindowWantsMouseDownReposted(ns_event);

    if (reposted_event_number == event_number) {
      // Reposted event received.
      repost_state = NONE;
      return nil;
    }

    // We were expecting a repost, but since this is a new mouse-down, cancel
    // reposting and allow event to continue as usual.
    repost_state = REPOST_CANCELLED;
    return ns_event;
  }

  DCHECK_EQ(REPOST_CANCELLED, repost_state);
  if (reposted_event_number == event_number) {
    // Reposting was cancelled, now that we've received the event, we don't
    // expect to see it again.
    repost_state = NONE;
    return nil;
  }

  return ns_event;
}

// Support window caption/draggable regions.
// In AppKit, non-client regions are set by overriding
// -[NSView mouseDownCanMoveWindow]. NSApplication caches this area as views are
// installed and performs window moving when mouse-downs land in the area.
// In Views, non-client regions are determined via hit-tests when the event
// occurs.
// To bridge the two models, we monitor mouse-downs with
// +[NSEvent addLocalMonitorForEventsMatchingMask:handler:]. This receives
// events after window dragging is handled, so for mouse-downs that land on a
// draggable point, we cancel the event and repost it at the CGSessionEventTap
// level so that window dragging will be handled again.
void SetupDragEventMonitor() {
  static id monitor = nil;
  if (monitor)
    return;

  monitor = [NSEvent
      addLocalMonitorForEventsMatchingMask:NSLeftMouseDownMask
      handler:^NSEvent*(NSEvent* ns_event) {
        return RepostEventIfHandledByWindow(ns_event);
      }];
}

// Returns a task runner for creating a ui::Compositor. This allows compositor
// tasks to be funneled through ui::WindowResizeHelper's task runner to allow
// resize operations to coordinate with frames provided by the GPU process.
scoped_refptr<base::SingleThreadTaskRunner> GetCompositorTaskRunner() {
  // If the WindowResizeHelper's pumpable task runner is set, it means the GPU
  // process is directing messages there, and the compositor can synchronize
  // with it. Otherwise, just use the UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ui::WindowResizeHelperMac::Get()->task_runner();
  return task_runner ? task_runner : base::ThreadTaskRunnerHandle::Get();
}

void RankNSViews(views::View* view,
                 const views::BridgedNativeWidget::AssociatedViews& hosts,
                 RankMap* rank) {
  auto it = hosts.find(view);
  if (it != hosts.end())
    rank->emplace(it->second, rank->size());
  for (int i = 0; i < view->child_count(); ++i)
    RankNSViews(view->child_at(i), hosts, rank);
}

NSComparisonResult SubviewSorter(NSViewComparatorValue lhs,
                                 NSViewComparatorValue rhs,
                                 void* rank_as_void) {
  DCHECK_NE(lhs, rhs);

  const RankMap* rank = static_cast<const RankMap*>(rank_as_void);
  auto left_rank = rank->find(lhs);
  auto right_rank = rank->find(rhs);
  bool left_found = left_rank != rank->end();
  bool right_found = right_rank != rank->end();

  // Sort unassociated views above associated views.
  if (left_found != right_found)
    return left_found ? NSOrderedAscending : NSOrderedDescending;

  if (left_found) {
    return left_rank->second < right_rank->second ? NSOrderedAscending
                                                  : NSOrderedDescending;
  }

  // If both are unassociated, consider that order is not important
  return NSOrderedSame;
}

}  // namespace

namespace views {

// static
gfx::Size BridgedNativeWidget::GetWindowSizeForClientSize(
    NSWindow* window,
    const gfx::Size& content_size) {
  NSRect content_rect =
      NSMakeRect(0, 0, content_size.width(), content_size.height());
  NSRect frame_rect = [window frameRectForContentRect:content_rect];
  return gfx::Size(NSWidth(frame_rect), NSHeight(frame_rect));
}

BridgedNativeWidget::BridgedNativeWidget(NativeWidgetMac* parent)
    : native_widget_mac_(parent),
      focus_manager_(nullptr),
      widget_type_(Widget::InitParams::TYPE_WINDOW),  // Updated in Init().
      parent_(nullptr),
      target_fullscreen_state_(false),
      in_fullscreen_transition_(false),
      window_visible_(false),
      wants_to_be_visible_(false) {
  SetupDragEventMonitor();
  DCHECK(parent);
  window_delegate_.reset(
      [[ViewsNSWindowDelegate alloc] initWithBridgedNativeWidget:this]);
}

BridgedNativeWidget::~BridgedNativeWidget() {
  bool close_window = false;
  if ([window_ delegate]) {
    // If the delegate is still set on a modal dialog, it means it was not
    // closed via [NSApplication endSheet:]. This is probably OK if the widget
    // was never shown. But Cocoa ignores close() calls on open sheets. Calling
    // endSheet: here would work, but it messes up assumptions elsewhere. E.g.
    // DialogClientView assumes its delegate is alive when closing, which isn't
    // true after endSheet: synchronously calls OnNativeWidgetDestroyed().
    // So ban it. Modal dialogs should be closed via Widget::Close().
    DCHECK(!native_widget_mac_->IsWindowModalSheet());

    // If the delegate is still set, it means OnWindowWillClose() has not been
    // called and the window is still open. Usually, -[NSWindow close] would
    // synchronously call OnWindowWillClose() which removes the delegate and
    // notifies NativeWidgetMac, which then calls this with a nil delegate.
    // For other teardown flows (e.g. Widget::WIDGET_OWNS_NATIVE_WIDGET or
    // Widget::CloseNow()) the delegate must first be cleared to avoid AppKit
    // calling back into the bridge. This means OnWindowWillClose() needs to be
    // invoked manually, which is done below.
    // Note that if the window has children it can't be closed until the
    // children are gone, but removing child windows calls into AppKit for the
    // parent window, so the delegate must be cleared first.
    [window_ setDelegate:nil];
    close_window = true;
  }

  RemoveOrDestroyChildren();
  DCHECK(child_windows_.empty());
  SetFocusManager(nullptr);
  SetRootView(nullptr);
  DestroyCompositor();

  if (close_window) {
    OnWindowWillClose();
    [window_ close];
  }
}

void BridgedNativeWidget::Init(base::scoped_nsobject<NSWindow> window,
                               const Widget::InitParams& params) {
  widget_type_ = params.type;

  DCHECK(!window_);
  window_.swap(window);
  [window_ setDelegate:window_delegate_];

  // Register for application hide notifications so that visibility can be
  // properly tracked. This is not done in the delegate so that the lifetime is
  // tied to the C++ object, rather than the delegate (which may be reference
  // counted). This is required since the application hides do not send an
  // orderOut: to individual windows. Unhide, however, does send an order
  // message.
  [[NSNotificationCenter defaultCenter]
      addObserver:window_delegate_
         selector:@selector(onWindowOrderChanged:)
             name:NSApplicationDidHideNotification
           object:nil];

  // Validate the window's initial state, otherwise the bridge's initial
  // tracking state will be incorrect.
  DCHECK(![window_ isVisible]);
  DCHECK_EQ(0u, [window_ styleMask] & NSFullScreenWindowMask);

  if (params.parent) {
    // Disallow creating child windows of views not currently in an NSWindow.
    CHECK([params.parent window]);
    BridgedNativeWidget* bridged_native_widget_parent =
        NativeWidgetMac::GetBridgeForNativeWindow([params.parent window]);
    // If the parent is another BridgedNativeWidget, just add to the collection
    // of child windows it owns and manages. Otherwise, create an adapter to
    // anchor the child widget and observe when the parent NSWindow is closed.
    if (bridged_native_widget_parent) {
      parent_ = bridged_native_widget_parent;
      bridged_native_widget_parent->child_windows_.push_back(this);
    } else {
      parent_ = new WidgetOwnerNSWindowAdapter(this, params.parent);
    }
  }

  // OSX likes to put shadows on most things. However, frameless windows (with
  // styleMask = NSBorderlessWindowMask) default to no shadow. So change that.
  // SHADOW_TYPE_DROP is used for Menus, which get the same shadow style on Mac.
  switch (params.shadow_type) {
    case Widget::InitParams::SHADOW_TYPE_NONE:
      [window_ setHasShadow:NO];
      break;
    case Widget::InitParams::SHADOW_TYPE_DEFAULT:
    case Widget::InitParams::SHADOW_TYPE_DROP:
      [window_ setHasShadow:YES];
      break;
  }  // No default case, to pick up new types.

  // Set a meaningful initial bounds. Note that except for frameless widgets
  // with no WidgetDelegate, the bounds will be set again by Widget after
  // initializing the non-client view. In the former case, if bounds were not
  // set at all, the creator of the Widget is expected to call SetBounds()
  // before calling Widget::Show() to avoid a kWindowSizeDeterminedLater-sized
  // (i.e. 1x1) window appearing.
  if (!params.bounds.IsEmpty()) {
    SetBounds(params.bounds);
  } else {
    // If a position is set, but no size, complain. Otherwise, a 1x1 window
    // would appear there, which might be unexpected.
    DCHECK(params.bounds.origin().IsOrigin())
        << "Zero-sized windows not supported on Mac.";

    // Otherwise, bounds is all zeroes. Cocoa will currently have the window at
    // the bottom left of the screen. To support a client calling SetSize() only
    // (and for consistency across platforms) put it at the top-left instead.
    // Read back the current frame: it will be a 1x1 context rect but the frame
    // size also depends on the window style.
    NSRect frame_rect = [window_ frame];
    SetBounds(gfx::Rect(gfx::Point(),
                        gfx::Size(NSWidth(frame_rect), NSHeight(frame_rect))));
  }

  // Widgets for UI controls (usually layered above web contents) start visible.
  if (params.type == Widget::InitParams::TYPE_CONTROL)
    SetVisibilityState(SHOW_INACTIVE);

  // Tooltip Widgets shouldn't have their own tooltip manager, but tooltips are
  // native on Mac, so nothing should ever want one in Widget form.
  DCHECK_NE(params.type, Widget::InitParams::TYPE_TOOLTIP);
  tooltip_manager_.reset(new TooltipManagerMac(this));
}

void BridgedNativeWidget::SetFocusManager(FocusManager* focus_manager) {
  if (focus_manager_ == focus_manager)
    return;

  if (focus_manager_)
    focus_manager_->RemoveFocusChangeListener(this);

  if (focus_manager)
    focus_manager->AddFocusChangeListener(this);

  focus_manager_ = focus_manager;
}

void BridgedNativeWidget::SetBounds(const gfx::Rect& new_bounds) {
  Widget* widget = native_widget_mac_->GetWidget();
  // -[NSWindow contentMinSize] is only checked by Cocoa for user-initiated
  // resizes. This is not what toolkit-views expects, so clamp. Note there is
  // no check for maximum size (consistent with aura::Window::SetBounds()).
  gfx::Size clamped_content_size =
      GetClientSizeForWindowSize(window_, new_bounds.size());
  clamped_content_size.SetToMax(widget->GetMinimumSize());

  // A contentRect with zero width or height is a banned practice in ChromeMac,
  // due to unpredictable OSX treatment.
  DCHECK(!clamped_content_size.IsEmpty())
      << "Zero-sized windows not supported on Mac";

  if (!window_visible_ && native_widget_mac_->IsWindowModalSheet()) {
    // Window-Modal dialogs (i.e. sheets) are positioned by Cocoa when shown for
    // the first time. They also have no frame, so just update the content size.
    [window_ setContentSize:NSMakeSize(clamped_content_size.width(),
                                       clamped_content_size.height())];
    return;
  }
  gfx::Rect actual_new_bounds(
      new_bounds.origin(),
      GetWindowSizeForClientSize(window_, clamped_content_size));

  if (parent_ && !PositionWindowInScreenCoordinates(widget, widget_type_))
    actual_new_bounds.Offset(parent_->GetChildWindowOffset());

  [window_ setFrame:gfx::ScreenRectToNSRect(actual_new_bounds)
            display:YES
            animate:NO];
}

void BridgedNativeWidget::SetRootView(views::View* view) {
  if (view == [bridged_view_ hostedView])
    return;

  // If this is ever false, the compositor will need to be properly torn down
  // and replaced, pointing at the new view.
  DCHECK(!view || !compositor_widget_);

  drag_drop_client_.reset();
  [bridged_view_ clearView];
  bridged_view_.reset();
  // Note that there can still be references to the old |bridged_view_|
  // floating around in Cocoa libraries at this point. However, references to
  // the old views::View will be gone, so any method calls will become no-ops.

  if (view) {
    bridged_view_.reset([[BridgedContentView alloc] initWithView:view]);
    drag_drop_client_.reset(new DragDropClientMac(this, view));

    // Objective C initializers can return nil. However, if |view| is non-NULL
    // this should be treated as an error and caught early.
    CHECK(bridged_view_);
  }
  [window_ setContentView:bridged_view_];
}

void BridgedNativeWidget::SetVisibilityState(WindowVisibilityState new_state) {
  // Ensure that:
  //  - A window with an invisible parent is not made visible.
  //  - A parent changing visibility updates child window visibility.
  //    * But only when changed via this function - ignore changes via the
  //      NSWindow API, or changes propagating out from here.
  wants_to_be_visible_ = new_state != HIDE_WINDOW;

  if (new_state == HIDE_WINDOW) {
    [window_ orderOut:nil];
    DCHECK(!window_visible_);
    return;
  }

  DCHECK(wants_to_be_visible_);
  // If the parent (or an ancestor) is hidden, return and wait for it to become
  // visible.
  if (parent() && !parent()->IsVisibleParent())
    return;

  if (native_widget_mac_->IsWindowModalSheet()) {
    ShowAsModalSheet();
    return;
  }

  if (new_state == SHOW_AND_ACTIVATE_WINDOW) {
    [window_ makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
  } else {
    // ui::SHOW_STATE_INACTIVE is typically used to avoid stealing focus from a
    // parent window. So, if there's a parent, order above that. Otherwise, this
    // will order above all windows at the same level.
    NSInteger parent_window_number = 0;
    if (parent_)
      parent_window_number = [parent_->GetNSWindow() windowNumber];

    [window_ orderWindow:NSWindowAbove
              relativeTo:parent_window_number];
  }
  DCHECK(window_visible_);

  // For non-sheet modal types, use the constrained window animations to make
  // the window appear.
  if (native_widget_mac_->GetWidget()->IsModal()) {
    base::scoped_nsobject<NSAnimation> show_animation(
        [[ModalShowAnimationWithLayer alloc] initWithWindow:window_]);
    // The default mode is blocking, which would block the UI thread for the
    // duration of the animation, but would keep it smooth. The window also
    // hasn't yet received a frame from the compositor at this stage, so it is
    // fully transparent until the GPU sends a frame swap IPC. For the blocking
    // option, the animation needs to wait until AcceleratedWidgetSwapCompleted
    // has been called at least once, otherwise it will animate nothing.
    [show_animation setAnimationBlockingMode:NSAnimationNonblocking];
    [show_animation startAnimation];
  }
}

void BridgedNativeWidget::AcquireCapture() {
  DCHECK(!HasCapture());
  if (!window_visible_)
    return;  // Capture on hidden windows is disallowed.

  mouse_capture_.reset(new CocoaMouseCapture(this));

  // Initiating global event capture with addGlobalMonitorForEventsMatchingMask:
  // will reset the mouse cursor to an arrow. Asking the window for an update
  // here will restore what we want. However, it can sometimes cause the cursor
  // to flicker, once, on the initial mouseDown.
  // TOOD(tapted): Make this unnecessary by only asking for global mouse capture
  // for the cases that need it (e.g. menus, but not drag and drop).
  [window_ cursorUpdate:[NSApp currentEvent]];
}

void BridgedNativeWidget::ReleaseCapture() {
  mouse_capture_.reset();
}

bool BridgedNativeWidget::HasCapture() {
  return mouse_capture_ && mouse_capture_->IsActive();
}

Widget::MoveLoopResult BridgedNativeWidget::RunMoveLoop(
      const gfx::Vector2d& drag_offset) {
  DCHECK(!HasCapture());
  DCHECK(!window_move_loop_);

  // RunMoveLoop caller is responsible for updating the window to be under the
  // mouse, but it does this using possibly outdated coordinate from the mouse
  // event, and mouse is very likely moved beyound that point.

  // Compensate for mouse drift by shifting the initial mouse position we pass
  // to CocoaWindowMoveLoop, so as it handles incoming move events the window's
  // top left corner will be |drag_offset| from the current mouse position.

  const gfx::Rect frame = gfx::ScreenRectFromNSRect([window_ frame]);
  const gfx::Point mouse_in_screen(frame.x() + drag_offset.x(),
                                   frame.y() + drag_offset.y());
  window_move_loop_.reset(new CocoaWindowMoveLoop(
      this, gfx::ScreenPointToNSPoint(mouse_in_screen)));

  return window_move_loop_->Run();

  // |this| may be destroyed during the RunLoop, causing it to exit early.
  // Even if that doesn't happen, CocoaWindowMoveLoop will clean itself up by
  // calling EndMoveLoop(). So window_move_loop_ will always be null before the
  // function returns. But don't DCHECK since |this| might not be valid.
}

void BridgedNativeWidget::EndMoveLoop() {
  DCHECK(window_move_loop_);
  window_move_loop_->End();
  window_move_loop_.reset();
}

void BridgedNativeWidget::SetNativeWindowProperty(const char* name,
                                                  void* value) {
  NSString* key = [NSString stringWithUTF8String:name];
  if (value) {
    [GetWindowProperties() setObject:[NSValue valueWithPointer:value]
                              forKey:key];
  } else {
    [GetWindowProperties() removeObjectForKey:key];
  }
}

void* BridgedNativeWidget::GetNativeWindowProperty(const char* name) const {
  NSString* key = [NSString stringWithUTF8String:name];
  return [[GetWindowProperties() objectForKey:key] pointerValue];
}

void BridgedNativeWidget::SetCursor(NSCursor* cursor) {
  [window_delegate_ setCursor:cursor];
}

void BridgedNativeWidget::OnWindowWillClose() {
  // Ensure BridgedNativeWidget does not have capture, otherwise
  // OnMouseCaptureLost() may reference a deleted |native_widget_mac_| when
  // called via ~CocoaMouseCapture() upon the destruction of |mouse_capture_|.
  // See crbug.com/622201. Also we do this before setting the delegate to nil,
  // because this may lead to callbacks to bridge which rely on a valid
  // delegate.
  ReleaseCapture();

  if (parent_) {
    parent_->RemoveChildWindow(this);
    parent_ = nullptr;
  }
  [window_ setDelegate:nil];
  [[NSNotificationCenter defaultCenter] removeObserver:window_delegate_];
  native_widget_mac_->OnWindowWillClose();
}

void BridgedNativeWidget::OnFullscreenTransitionStart(
    bool target_fullscreen_state) {
  // Note: This can fail for fullscreen changes started externally, but a user
  // shouldn't be able to do that if the window is invisible to begin with.
  DCHECK(window_visible_);

  DCHECK_NE(target_fullscreen_state, target_fullscreen_state_);
  target_fullscreen_state_ = target_fullscreen_state;
  in_fullscreen_transition_ = true;

  // If going into fullscreen, store an answer for GetRestoredBounds().
  if (target_fullscreen_state)
    bounds_before_fullscreen_ = gfx::ScreenRectFromNSRect([window_ frame]);
}

void BridgedNativeWidget::OnFullscreenTransitionComplete(
    bool actual_fullscreen_state) {
  in_fullscreen_transition_ = false;

  if (target_fullscreen_state_ == actual_fullscreen_state) {
    // Ensure constraints are re-applied when completing a transition.
    OnSizeConstraintsChanged();
    return;
  }

  // First update to reflect reality so that OnTargetFullscreenStateChanged()
  // expects the change.
  target_fullscreen_state_ = actual_fullscreen_state;
  ToggleDesiredFullscreenState();

  // Usually ToggleDesiredFullscreenState() sets |in_fullscreen_transition_| via
  // OnFullscreenTransitionStart(). When it does not, it means Cocoa ignored the
  // toggleFullScreen: request. This can occur when the fullscreen transition
  // fails and Cocoa is *about* to send windowDidFailToEnterFullScreen:.
  // Annoyingly, for this case, Cocoa first sends windowDidExitFullScreen:.
  if (in_fullscreen_transition_)
    DCHECK_NE(target_fullscreen_state_, actual_fullscreen_state);
}

void BridgedNativeWidget::ToggleDesiredFullscreenState() {
  // If there is currently an animation into or out of fullscreen, then AppKit
  // emits the string "not in fullscreen state" to stdio and does nothing. For
  // this case, schedule a transition back into the desired state when the
  // animation completes.
  if (in_fullscreen_transition_) {
    target_fullscreen_state_ = !target_fullscreen_state_;
    return;
  }

  // Going fullscreen implicitly makes the window visible. AppKit does this.
  // That is, -[NSWindow isVisible] is always true after a call to -[NSWindow
  // toggleFullScreen:]. Unfortunately, this change happens after AppKit calls
  // -[NSWindowDelegate windowWillEnterFullScreen:], and AppKit doesn't send an
  // orderWindow message. So intercepting the implicit change is hard.
  // Luckily, to trigger externally, the window typically needs to be visible in
  // the first place. So we can just ensure the window is visible here instead
  // of relying on AppKit to do it, and not worry that OnVisibilityChanged()
  // won't be called for externally triggered fullscreen requests.
  if (!window_visible_)
    SetVisibilityState(SHOW_INACTIVE);

  // Enable fullscreen collection behavior because:
  // 1: -[NSWindow toggleFullscreen:] would otherwise be ignored,
  // 2: the fullscreen button must be enabled so the user can leave fullscreen.
  // This will be reset when a transition out of fullscreen completes.
  gfx::SetNSWindowCanFullscreen(window_, true);

  [window_ toggleFullScreen:nil];
}

void BridgedNativeWidget::OnSizeChanged() {
  gfx::Size new_size = GetClientAreaSize();
  native_widget_mac_->GetWidget()->OnNativeWidgetSizeChanged(new_size);
  if (layer()) {
    UpdateLayerProperties();
    if ([window_ inLiveResize])
      MaybeWaitForFrame(new_size);
  }

  // 10.9 is unable to generate a window shadow from the composited CALayer, so
  // use Quartz.
  // We don't update the window mask during a live resize, instead it is done
  // after the resize is completed in viewDidEndLiveResize: in
  // BridgedContentView.
  if (base::mac::IsOSMavericks() && ![window_ inLiveResize])
    [bridged_view_ updateWindowMask];
}

void BridgedNativeWidget::OnPositionChanged() {
  native_widget_mac_->GetWidget()->OnNativeWidgetMove();
}

void BridgedNativeWidget::OnVisibilityChanged() {
  const bool window_visible = [window_ isVisible];
  if (window_visible_ == window_visible)
    return;

  window_visible_ = window_visible;

  // If arriving via SetVisible(), |wants_to_be_visible_| should already be set.
  // If made visible externally (e.g. Cmd+H), just roll with it. Don't try (yet)
  // to distinguish being *hidden* externally from being hidden by a parent
  // window - we might not need that.
  if (window_visible_) {
    wants_to_be_visible_ = true;

    if (parent_)
      [parent_->GetNSWindow() addChildWindow:window_ ordered:NSWindowAbove];
  } else {
    ReleaseCapture();  // Capture on hidden windows is not permitted.

    // When becoming invisible, remove the entry in any parent's childWindow
    // list. Cocoa's childWindow management breaks down when child windows are
    // hidden.
    if (parent_)
      [parent_->GetNSWindow() removeChildWindow:window_];
  }

  // TODO(tapted): Investigate whether we want this for Mac. This is what Aura
  // does, and it is what tests expect. However, because layer drawing is
  // asynchronous (and things like deminiaturize in AppKit are not), it can
  // result in a CALayer appearing on screen before it has been redrawn in the
  // GPU process. This is a general problem. In content, a helper class,
  // RenderWidgetResizeHelper, blocks the UI thread in -[NSView setFrameSize:]
  // and RenderWidgetHostView::Show() until a frame is ready.
  if (layer()) {
    layer()->SetVisible(window_visible_);
    layer()->SchedulePaint(gfx::Rect(GetClientAreaSize()));
  }

  NotifyVisibilityChangeDown();

  native_widget_mac_->GetWidget()->OnNativeWidgetVisibilityChanged(
      window_visible_);

  // Toolkit-views suppresses redraws while not visible. To prevent Cocoa asking
  // for an "empty" draw, disable auto-display while hidden. For example, this
  // prevents Cocoa drawing just *after* a minimize, resulting in a blank window
  // represented in the deminiaturize animation.
  [window_ setAutodisplay:window_visible_];
}

void BridgedNativeWidget::OnBackingPropertiesChanged() {
  if (layer())
    UpdateLayerProperties();
}

void BridgedNativeWidget::OnWindowKeyStatusChangedTo(bool is_key) {
  Widget* widget = native_widget_mac()->GetWidget();
  widget->OnNativeWidgetActivationChanged(is_key);
  // The contentView is the BridgedContentView hosting the views::RootView. The
  // focus manager will already know if a native subview has focus.
  if ([window_ contentView] == [window_ firstResponder]) {
    if (is_key) {
      widget->OnNativeFocus();
      // Explicitly set the keyboard accessibility state on regaining key
      // window status.
      [bridged_view_ updateFullKeyboardAccess];
      widget->GetFocusManager()->RestoreFocusedView();
    } else {
      widget->OnNativeBlur();
      widget->GetFocusManager()->StoreFocusedView(true);
    }
  }
}

bool BridgedNativeWidget::ShouldRepostPendingLeftMouseDown(
    NSPoint location_in_window) {
  if (!bridged_view_)
    return false;

  if ([bridged_view_ mouseDownCanMoveWindow]) {
    // This is a re-post, the movement has already started, so we can make the
    // window non-draggable again.
    SetDraggable(false);
    return false;
  }

  if (IsPointInResizeArea(location_in_window, window_))
    return false;

  gfx::Point point(location_in_window.x,
                   NSHeight([window_ frame]) - location_in_window.y);
  bool should_move_window =
      native_widget_mac()->GetWidget()->GetNonClientComponent(point) ==
      HTCAPTION;

  // Check that the point is not obscured by non-content NSViews.
  for (NSView* subview : [[bridged_view_ superview] subviews]) {
    if (subview == bridged_view_.get())
      continue;

    if (![subview mouseDownCanMoveWindow] &&
        NSPointInRect(location_in_window, [subview frame])) {
      should_move_window = false;
      break;
    }
  }

  if (!should_move_window)
    return false;

  // Make the window draggable, then return true to repost the event.
  SetDraggable(true);
  return true;
}

void BridgedNativeWidget::OnSizeConstraintsChanged() {
  // Don't modify the size constraints or fullscreen collection behavior while
  // in fullscreen or during a transition. OnFullscreenTransitionComplete will
  // reset these after leaving fullscreen.
  if (target_fullscreen_state_ || in_fullscreen_transition_)
    return;

  Widget* widget = native_widget_mac()->GetWidget();
  gfx::Size min_size = widget->GetMinimumSize();
  gfx::Size max_size = widget->GetMaximumSize();
  bool is_resizable = widget->widget_delegate()->CanResize();
  bool shows_resize_controls =
      is_resizable && (min_size.IsEmpty() || min_size != max_size);
  bool shows_fullscreen_controls =
      is_resizable && widget->widget_delegate()->CanMaximize();

  gfx::ApplyNSWindowSizeConstraints(window_, min_size, max_size,
                                    shows_resize_controls,
                                    shows_fullscreen_controls);
}

ui::InputMethod* BridgedNativeWidget::GetInputMethod() {
  if (!input_method_) {
    input_method_ = ui::CreateInputMethod(this, nil);
    // For now, use always-focused mode on Mac for the input method.
    // TODO(tapted): Move this to OnWindowKeyStatusChangedTo() and balance.
    input_method_->OnFocus();
  }
  return input_method_.get();
}

gfx::Rect BridgedNativeWidget::GetRestoredBounds() const {
  if (target_fullscreen_state_ || in_fullscreen_transition_)
    return bounds_before_fullscreen_;

  return gfx::ScreenRectFromNSRect([window_ frame]);
}

void BridgedNativeWidget::CreateLayer(ui::LayerType layer_type,
                                      bool translucent) {
  DCHECK(bridged_view_);
  DCHECK(!layer());

  CreateCompositor();
  DCHECK(compositor_);

  SetLayer(new ui::Layer(layer_type));
  // Note, except for controls, this will set the layer to be hidden, since it
  // is only called during Init().
  layer()->SetVisible(window_visible_);
  layer()->set_delegate(this);

  InitCompositor();

  // Transparent window support.
  layer()->GetCompositor()->SetHostHasTransparentBackground(translucent);
  layer()->SetFillsBoundsOpaquely(!translucent);

  // Use the regular window background for window modal sheets. The layer() will
  // still paint over most of it, but the native -[NSApp beginSheet:] animation
  // blocks the UI thread, so there's no way to invalidate the shadow to match
  // the composited layer. This assumes the native window shape is a good match
  // for the composited NonClientFrameView, which should be the case since the
  // native shape is what's most appropriate for displaying sheets on Mac.
  if (translucent && !native_widget_mac_->IsWindowModalSheet()) {
    [window_ setOpaque:NO];
    // For Mac OS versions earlier than Yosemite, the Window server isn't able
    // to generate a window shadow from the composited CALayer. To get around
    // this, let the window background remain opaque and clip the window
    // boundary in drawRect method of BridgedContentView. See crbug.com/543671.
    if (base::mac::IsOSYosemiteOrLater())
      [window_ setBackgroundColor:[NSColor clearColor]];
  }

  UpdateLayerProperties();
}

void BridgedNativeWidget::SetAssociationForView(const views::View* view,
                                                NSView* native_view) {
  DCHECK_EQ(0u, associated_views_.count(view));
  associated_views_[view] = native_view;
  native_widget_mac_->GetWidget()->ReorderNativeViews();
}

void BridgedNativeWidget::ClearAssociationForView(const views::View* view) {
  auto it = associated_views_.find(view);
  DCHECK(it != associated_views_.end());
  associated_views_.erase(it);
}

void BridgedNativeWidget::ReorderChildViews() {
  RankMap rank;
  Widget* widget = native_widget_mac_->GetWidget();
  RankNSViews(widget->GetRootView(), associated_views_, &rank);
  // Unassociated NSViews should be ordered above associated ones. The exception
  // is the UI compositor's superview, which should always be on the very
  // bottom, so give it an explicit negative rank.
  if (compositor_superview_)
    rank[compositor_superview_] = -1;
  [bridged_view_ sortSubviewsUsingFunction:&SubviewSorter context:&rank];
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, internal::InputMethodDelegate:

ui::EventDispatchDetails BridgedNativeWidget::DispatchKeyEventPostIME(
    ui::KeyEvent* key) {
  DCHECK(focus_manager_);
  native_widget_mac_->GetWidget()->OnKeyEvent(key);
  if (!key->handled()) {
    if (!focus_manager_->OnKeyEvent(*key))
      key->StopPropagation();
  }
  return ui::EventDispatchDetails();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, CocoaMouseCaptureDelegate:

void BridgedNativeWidget::PostCapturedEvent(NSEvent* event) {
  [bridged_view_ processCapturedMouseEvent:event];
}

void BridgedNativeWidget::OnMouseCaptureLost() {
  native_widget_mac_->GetWidget()->OnMouseCaptureLost();
}

NSWindow* BridgedNativeWidget::GetWindow() const {
  return window_;
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, FocusChangeListener:

void BridgedNativeWidget::OnWillChangeFocus(View* focused_before,
                                            View* focused_now) {
}

void BridgedNativeWidget::OnDidChangeFocus(View* focused_before,
                                           View* focused_now) {
  ui::InputMethod* input_method =
      native_widget_mac_->GetWidget()->GetInputMethod();
  if (input_method) {
    ui::TextInputClient* input_client = input_method->GetTextInputClient();
    [bridged_view_ setTextInputClient:input_client];
  }
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, LayerDelegate:

void BridgedNativeWidget::OnPaintLayer(const ui::PaintContext& context) {
  DCHECK(window_visible_);
  native_widget_mac_->GetWidget()->OnNativeWidgetPaint(context);
}

void BridgedNativeWidget::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {
  NOTIMPLEMENTED();
}

void BridgedNativeWidget::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  native_widget_mac_->GetWidget()->DeviceScaleFactorChanged(
      device_scale_factor);
}

base::Closure BridgedNativeWidget::PrepareForLayerBoundsChange() {
  return base::Closure();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, AcceleratedWidgetMac:

NSView* BridgedNativeWidget::AcceleratedWidgetGetNSView() const {
  return compositor_superview_;
}

void BridgedNativeWidget::AcceleratedWidgetGetVSyncParameters(
  base::TimeTicks* timebase, base::TimeDelta* interval) const {
  // TODO(tapted): Add vsync support.
  *timebase = base::TimeTicks();
  *interval = base::TimeDelta();
}

void BridgedNativeWidget::AcceleratedWidgetSwapCompleted() {
  // Ignore frames arriving "late" for an old size. A frame at the new size
  // should arrive soon.
  if (!compositor_widget_->HasFrameOfSize(GetClientAreaSize()))
    return;

  if (invalidate_shadow_on_frame_swap_) {
    invalidate_shadow_on_frame_swap_ = false;
    [window_ invalidateShadow];
  }
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, BridgedNativeWidgetOwner:

NSWindow* BridgedNativeWidget::GetNSWindow() {
  return window_;
}

gfx::Vector2d BridgedNativeWidget::GetChildWindowOffset() const {
  return gfx::ScreenRectFromNSRect([window_ frame]).OffsetFromOrigin();
}

bool BridgedNativeWidget::IsVisibleParent() const {
  return parent_ ? window_visible_ && parent_->IsVisibleParent()
                 : window_visible_;
}

void BridgedNativeWidget::RemoveChildWindow(BridgedNativeWidget* child) {
  auto location = std::find(
      child_windows_.begin(), child_windows_.end(), child);
  DCHECK(location != child_windows_.end());
  child_windows_.erase(location);

  // Note the child is sometimes removed already by AppKit. This depends on OS
  // version, and possibly some unpredictable reference counting. Removing it
  // here should be safe regardless.
  [window_ removeChildWindow:child->window_];
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, private:

void BridgedNativeWidget::RemoveOrDestroyChildren() {
  // TODO(tapted): Implement unowned child windows if required.
  while (!child_windows_.empty()) {
    // The NSWindow can only be destroyed after -[NSWindow close] is complete.
    // Retain the window, otherwise the reference count can reach zero when the
    // child calls back into RemoveChildWindow() via its OnWindowWillClose().
    base::scoped_nsobject<NSWindow> child(
        [child_windows_.back()->ns_window() retain]);
    [child close];
  }
}

void BridgedNativeWidget::NotifyVisibilityChangeDown() {
  // Child windows sometimes like to close themselves in response to visibility
  // changes. That's supported, but only with the asynchronous Widget::Close().
  // Perform a heuristic to detect child removal that would break these loops.
  const size_t child_count = child_windows_.size();
  if (!window_visible_) {
    for (BridgedNativeWidget* child : child_windows_) {
      if (child->window_visible_)
        [child->ns_window() orderOut:nil];

      DCHECK(!child->window_visible_);
      CHECK_EQ(child_count, child_windows_.size());
    }
    // The orderOut calls above should result in a call to OnVisibilityChanged()
    // in each child. There, children will remove themselves from the NSWindow
    // childWindow list as well as propagate NotifyVisibilityChangeDown() calls
    // to any children of their own.
    DCHECK_EQ(0u, [[window_ childWindows] count]);
    return;
  }

  NSUInteger visible_children = 0;  // For a DCHECK below.
  NSInteger parent_window_number = [window_ windowNumber];
  for (BridgedNativeWidget* child: child_windows_) {
    // Note: order the child windows on top, regardless of whether or not they
    // are currently visible. They probably aren't, since the parent was hidden
    // prior to this, but they could have been made visible in other ways.
    if (child->wants_to_be_visible_) {
      ++visible_children;
      // Here -[NSWindow orderWindow:relativeTo:] is used to put the window on
      // screen. However, that by itself is insufficient to guarantee a correct
      // z-order relationship. If this function is being called from a z-order
      // change in the parent, orderWindow turns out to be unreliable (i.e. the
      // ordering doesn't always take effect). What this actually relies on is
      // the resulting call to OnVisibilityChanged() in the child, which will
      // then insert itself into -[NSWindow childWindows] to let Cocoa do its
      // internal layering magic.
      [child->ns_window() orderWindow:NSWindowAbove
                           relativeTo:parent_window_number];
      DCHECK(child->window_visible_);
    }
    CHECK_EQ(child_count, child_windows_.size());
  }
  DCHECK_EQ(visible_children, [[window_ childWindows] count]);
}

gfx::Size BridgedNativeWidget::GetClientAreaSize() const {
  NSRect content_rect = [window_ contentRectForFrameRect:[window_ frame]];
  return gfx::Size(NSWidth(content_rect), NSHeight(content_rect));
}

void BridgedNativeWidget::CreateCompositor() {
  DCHECK(!compositor_);
  DCHECK(!compositor_widget_);
  DCHECK(ViewsDelegate::GetInstance());

  ui::ContextFactory* context_factory =
      ViewsDelegate::GetInstance()->GetContextFactory();
  DCHECK(context_factory);

  AddCompositorSuperview();

  compositor_widget_.reset(new ui::AcceleratedWidgetMac());
  compositor_.reset(
      new ui::Compositor(context_factory, GetCompositorTaskRunner()));
  compositor_->SetAcceleratedWidget(compositor_widget_->accelerated_widget());
  compositor_widget_->SetNSView(this);
}

void BridgedNativeWidget::InitCompositor() {
  DCHECK(layer());
  float scale_factor = GetDeviceScaleFactorFromView(compositor_superview_);
  gfx::Size size_in_dip = GetClientAreaSize();
  compositor_->SetScaleAndSize(scale_factor,
                               ConvertSizeToPixel(scale_factor, size_in_dip));
  compositor_->SetRootLayer(layer());
}

void BridgedNativeWidget::DestroyCompositor() {
  if (layer()) {
    // LayerOwner supports a change in ownership, e.g., to animate a closing
    // window, but that won't work as expected for the root layer in
    // BridgedNativeWidget.
    DCHECK_EQ(this, layer()->owner());
    layer()->CompleteAllAnimations();
    layer()->SuppressPaint();
    layer()->set_delegate(nullptr);
  }
  DestroyLayer();

  if (!compositor_widget_) {
    DCHECK(!compositor_);
    return;
  }
  compositor_widget_->ResetNSView();
  compositor_.reset();
  compositor_widget_.reset();
}

void BridgedNativeWidget::AddCompositorSuperview() {
  DCHECK(!compositor_superview_);
  compositor_superview_.reset(
      [[ViewsCompositorSuperview alloc] initWithFrame:[bridged_view_ bounds]]);

  // Size and resize automatically with |bridged_view_|.
  [compositor_superview_
      setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  // Enable HiDPI backing when supported (only on 10.7+).
  if ([compositor_superview_ respondsToSelector:
      @selector(setWantsBestResolutionOpenGLSurface:)]) {
    [compositor_superview_ setWantsBestResolutionOpenGLSurface:YES];
  }

  base::scoped_nsobject<CALayer> background_layer([[CALayer alloc] init]);
  [background_layer
      setAutoresizingMask:kCALayerWidthSizable | kCALayerHeightSizable];

  if (widget_type_ == Widget::InitParams::TYPE_MENU) {
    // Giving the canvas opacity messes up subpixel font rendering, so use a
    // solid background, but make the CALayer transparent.
    if (base::mac::IsOSYosemiteOrLater()) {
      [background_layer setOpacity:kYosemiteMenuOpacity];
      CGSSetWindowBackgroundBlurRadius(
          _CGSDefaultConnection(), [window_ windowNumber], kYosemiteMenuBlur);
      // The blur effect does not occur with a fully transparent (or fully
      // layer-backed) window. Setting a window background will use square
      // corners, so ask the contentView to draw one instead.
      [bridged_view_ setDrawMenuBackgroundForBlur:YES];
    } else {
      [background_layer setOpacity:kMavericksMenuOpacity];
    }
  }

  // Set the layer first to create a layer-hosting view (not layer-backed).
  [compositor_superview_ setLayer:background_layer];
  [compositor_superview_ setWantsLayer:YES];

  // The UI compositor should always be the first subview, to ensure webviews
  // are drawn on top of it.
  DCHECK_EQ(0u, [[bridged_view_ subviews] count]);
  [bridged_view_ addSubview:compositor_superview_];
}

void BridgedNativeWidget::UpdateLayerProperties() {
  DCHECK(layer());
  DCHECK(compositor_superview_);
  gfx::Size size_in_dip = GetClientAreaSize();
  layer()->SetBounds(gfx::Rect(size_in_dip));

  float scale_factor = GetDeviceScaleFactorFromView(compositor_superview_);
  compositor_->SetScaleAndSize(scale_factor,
                               ConvertSizeToPixel(scale_factor, size_in_dip));

  // For a translucent window, the shadow calculation needs to be carried out
  // after the frame from the compositor arrives.
  if (![window_ isOpaque])
    invalidate_shadow_on_frame_swap_ = true;
}

void BridgedNativeWidget::MaybeWaitForFrame(const gfx::Size& size_in_dip) {
  if (!layer()->IsDrawn() || compositor_widget_->HasFrameOfSize(size_in_dip))
    return;

  const int kPaintMsgTimeoutMS = 50;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const base::TimeTicks timeout_time =
      start_time + base::TimeDelta::FromMilliseconds(kPaintMsgTimeoutMS);

  ui::WindowResizeHelperMac* resize_helper = ui::WindowResizeHelperMac::Get();
  for (base::TimeTicks now = start_time; now < timeout_time;
       now = base::TimeTicks::Now()) {
    if (!resize_helper->WaitForSingleTaskToRun(timeout_time - now))
      return;  // Timeout.

    // Since the UI thread is blocked, the size shouldn't change.
    DCHECK(size_in_dip == GetClientAreaSize());
    if (compositor_widget_->HasFrameOfSize(size_in_dip))
      return;  // Frame arrived.
  }
}

void BridgedNativeWidget::ShowAsModalSheet() {
  // -[NSApp beginSheet:] will block the UI thread while the animation runs.
  // So that it doesn't animate a fully transparent window, first wait for a
  // frame. The first step is to pretend that the window is already visible.
  window_visible_ = true;
  layer()->SetVisible(true);
  native_widget_mac_->GetWidget()->OnNativeWidgetVisibilityChanged(true);
  MaybeWaitForFrame(GetClientAreaSize());

  NSWindow* parent_window = parent_->GetNSWindow();
  DCHECK(parent_window);

  [NSApp beginSheet:window_
      modalForWindow:parent_window
       modalDelegate:[window_ delegate]
      didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:)
         contextInfo:nullptr];
}

NSMutableDictionary* BridgedNativeWidget::GetWindowProperties() const {
  NSMutableDictionary* properties = objc_getAssociatedObject(
      window_, &kWindowPropertiesKey);
  if (!properties) {
    properties = [NSMutableDictionary dictionary];
    objc_setAssociatedObject(window_, &kWindowPropertiesKey,
                             properties, OBJC_ASSOCIATION_RETAIN);
  }
  return properties;
}

void BridgedNativeWidget::SetDraggable(bool draggable) {
  [bridged_view_ setMouseDownCanMoveWindow:draggable];
  // AppKit will not update its cache of mouseDownCanMoveWindow unless something
  // changes. Previously we tried adding an NSView and removing it, but for some
  // reason it required reposting the mouse-down event, and didn't always work.
  // Calling the below seems to be an effective solution.
  [window_ setMovableByWindowBackground:NO];
  [window_ setMovableByWindowBackground:YES];
}

}  // namespace views
