// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/menu/menu_runner_impl_cocoa.h"

#include "base/mac/sdk_forward_declarations.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/models/menu_model.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/controls/menu/menu_runner_impl_adapter.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace internal {
namespace {

// The menu run types that should show a native NSMenu rather than a toolkit-
// views menu. Only supported when the menu is backed by a ui::MenuModel.
const int kNativeRunTypes = MenuRunner::CONTEXT_MENU | MenuRunner::COMBOBOX;

const CGFloat kNativeCheckmarkWidth = 18;
const CGFloat kNativeMenuItemHeight = 18;

// Returns the first item in |menu_controller|'s menu that will be checked.
NSMenuItem* FirstCheckedItem(MenuController* menu_controller) {
  for (NSMenuItem* item in [[menu_controller menu] itemArray]) {
    if ([menu_controller model]->IsItemCheckedAt([item tag]))
      return item;
  }
  return nil;
}

// Places a temporary, hidden NSView at |screen_bounds| within |window|. Used
// with -[NSMenu popUpMenuPositioningItem:atLocation:inView:] to position the
// menu for a combobox. The caller must remove the returned NSView from its
// superview when the menu is closed.
base::scoped_nsobject<NSView> CreateMenuAnchorView(
    NSWindow* window,
    const gfx::Rect& screen_bounds,
    NSMenuItem* checked_item) {
  NSRect rect = gfx::ScreenRectToNSRect(screen_bounds);
  rect = [window convertRectFromScreen:rect];
  rect = [[window contentView] convertRect:rect fromView:nil];

  // If there's no checked item (e.g. Combobox::STYLE_ACTION), NSMenu will
  // anchor at the top left of the frame. Action buttons should anchor below.
  if (!checked_item) {
    rect.size.height = 0;
    if (base::i18n::IsRTL())
      rect.origin.x += rect.size.width;
  } else {
    // To ensure a consistent anchoring that's vertically centered in the
    // bounds, fix the height to be the same as a menu item.
    rect.origin.y = NSMidY(rect) - kNativeMenuItemHeight / 2;
    rect.size.height = kNativeMenuItemHeight;
    if (base::i18n::IsRTL()) {
      // The Views menu controller flips the MenuAnchorPosition value from left
      // to right in RTL. NSMenu does this automatically: the menu opens to the
      // left of the anchor, but AppKit doesn't account for the anchor width.
      // So the width needs to be added to anchor at the right of the view.
      // Note the checkmark width is not also added - it doesn't quite line up
      // the text. A Yosemite NSComboBox doesn't line up in RTL either: just
      // adding the width is a good match for the native behavior.
      rect.origin.x += rect.size.width;
    } else {
      rect.origin.x -= kNativeCheckmarkWidth;
    }
  }

  // A plain NSView will anchor below rather than "over", so use an NSButton.
  base::scoped_nsobject<NSView> anchor_view(
      [[NSButton alloc] initWithFrame:rect]);
  [anchor_view setHidden:YES];
  [[window contentView] addSubview:anchor_view];
  return anchor_view;
}

}  // namespace

// static
MenuRunnerImplInterface* MenuRunnerImplInterface::Create(
    ui::MenuModel* menu_model,
    int32_t run_types) {
  if ((run_types & kNativeRunTypes) != 0 &&
      (run_types & MenuRunner::IS_NESTED) == 0) {
    return new MenuRunnerImplCocoa(menu_model);
  }

  return new MenuRunnerImplAdapter(menu_model);
}

MenuRunnerImplCocoa::MenuRunnerImplCocoa(ui::MenuModel* menu)
    : running_(false),
      delete_after_run_(false),
      closing_event_time_(base::TimeTicks()) {
  menu_controller_.reset(
      [[MenuController alloc] initWithModel:menu useWithPopUpButtonCell:NO]);
}

bool MenuRunnerImplCocoa::IsRunning() const {
  return running_;
}

void MenuRunnerImplCocoa::Release() {
  if (IsRunning()) {
    if (delete_after_run_)
      return;  // We already canceled.

    delete_after_run_ = true;
    [menu_controller_ cancel];
  } else {
    delete this;
  }
}

MenuRunner::RunResult MenuRunnerImplCocoa::RunMenuAt(Widget* parent,
                                                     MenuButton* button,
                                                     const gfx::Rect& bounds,
                                                     MenuAnchorPosition anchor,
                                                     int32_t run_types) {
  DCHECK(run_types & kNativeRunTypes);
  DCHECK(!IsRunning());
  DCHECK(parent);
  closing_event_time_ = base::TimeTicks();
  running_ = true;

  if (run_types & MenuRunner::CONTEXT_MENU) {
    [NSMenu popUpContextMenu:[menu_controller_ menu]
                   withEvent:[NSApp currentEvent]
                     forView:parent->GetNativeView()];
  } else if (run_types & MenuRunner::COMBOBOX) {
    NSMenuItem* checked_item = FirstCheckedItem(menu_controller_);
    base::scoped_nsobject<NSView> anchor_view(
        CreateMenuAnchorView(parent->GetNativeWindow(), bounds, checked_item));
    NSMenu* menu = [menu_controller_ menu];
    [menu setMinimumWidth:bounds.width() + kNativeCheckmarkWidth];
    [menu popUpMenuPositioningItem:checked_item
                        atLocation:NSZeroPoint
                            inView:anchor_view];
    [anchor_view removeFromSuperview];
  } else {
    NOTREACHED();
  }

  closing_event_time_ = ui::EventTimeForNow();
  running_ = false;

  if (delete_after_run_) {
    delete this;
    return MenuRunner::MENU_DELETED;
  }

  return MenuRunner::NORMAL_EXIT;
}

void MenuRunnerImplCocoa::Cancel() {
  [menu_controller_ cancel];
}

base::TimeTicks MenuRunnerImplCocoa::GetClosingEventTime() const {
  return closing_event_time_;
}

MenuRunnerImplCocoa::~MenuRunnerImplCocoa() {
}

}  // namespace internal
}  // namespace views
