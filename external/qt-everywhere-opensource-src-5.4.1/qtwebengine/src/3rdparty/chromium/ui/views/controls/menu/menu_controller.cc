// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller.h"

#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/vector2d.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller_delegate.h"
#include "ui/views/controls/menu/menu_host_root_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_message_loop.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/drag_utils.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/view.h"
#include "ui/views/view_constants.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/internal_constants.h"
#include "ui/gfx/win/dpi.h"
#include "ui/views/win/hwnd_util.h"
#endif

using base::Time;
using base::TimeDelta;
using ui::OSExchangeData;

// Period of the scroll timer (in milliseconds).
static const int kScrollTimerMS = 30;

// Amount of time from when the drop exits the menu and the menu is hidden.
static const int kCloseOnExitTime = 1200;

// If a context menu is invoked by touch, we shift the menu by this offset so
// that the finger does not obscure the menu.
static const int kCenteredContextMenuYOffset = -15;

namespace views {

namespace {

// When showing context menu on mouse down, the user might accidentally select
// the menu item on the subsequent mouse up. To prevent this, we add the
// following delay before the user is able to select an item.
static int menu_selection_hold_time_ms = kMinimumMsPressedToActivate;

// The spacing offset for the bubble tip.
const int kBubbleTipSizeLeftRight = 12;
const int kBubbleTipSizeTopBottom = 11;

// The maximum distance (in DIPS) that the mouse can be moved before it should
// trigger a mouse menu item activation (regardless of how long the menu has
// been showing).
const float kMaximumLengthMovedToActivate = 4.0f;

// Returns true if the mnemonic of |menu| matches key.
bool MatchesMnemonic(MenuItemView* menu, base::char16 key) {
  return key != 0 && menu->GetMnemonic() == key;
}

// Returns true if |menu| doesn't have a mnemonic and first character of the its
// title is |key|.
bool TitleMatchesMnemonic(MenuItemView* menu, base::char16 key) {
  if (menu->GetMnemonic())
    return false;

  base::string16 lower_title = base::i18n::ToLower(menu->title());
  return !lower_title.empty() && lower_title[0] == key;
}

}  // namespace

// Returns the first descendant of |view| that is hot tracked.
static CustomButton* GetFirstHotTrackedView(View* view) {
  if (!view)
    return NULL;
  CustomButton* button = CustomButton::AsCustomButton(view);
  if (button) {
    if (button->IsHotTracked())
      return button;
  }

  for (int i = 0; i < view->child_count(); ++i) {
    CustomButton* hot_view = GetFirstHotTrackedView(view->child_at(i));
    if (hot_view)
      return hot_view;
  }
  return NULL;
}

// Recurses through the child views of |view| returning the first view starting
// at |start| that is focusable. A value of -1 for |start| indicates to start at
// the first view (if |forward| is false, iterating starts at the last view). If
// |forward| is true the children are considered first to last, otherwise last
// to first.
static View* GetFirstFocusableView(View* view, int start, bool forward) {
  if (forward) {
    for (int i = start == -1 ? 0 : start; i < view->child_count(); ++i) {
      View* deepest = GetFirstFocusableView(view->child_at(i), -1, forward);
      if (deepest)
        return deepest;
    }
  } else {
    for (int i = start == -1 ? view->child_count() - 1 : start; i >= 0; --i) {
      View* deepest = GetFirstFocusableView(view->child_at(i), -1, forward);
      if (deepest)
        return deepest;
    }
  }
  return view->IsFocusable() ? view : NULL;
}

// Returns the first child of |start| that is focusable.
static View* GetInitialFocusableView(View* start, bool forward) {
  return GetFirstFocusableView(start, -1, forward);
}

// Returns the next view after |start_at| that is focusable. Returns NULL if
// there are no focusable children of |ancestor| after |start_at|.
static View* GetNextFocusableView(View* ancestor,
                                  View* start_at,
                                  bool forward) {
  DCHECK(ancestor->Contains(start_at));
  View* parent = start_at;
  do {
    View* new_parent = parent->parent();
    int index = new_parent->GetIndexOf(parent);
    index += forward ? 1 : -1;
    if (forward || index != -1) {
      View* next = GetFirstFocusableView(new_parent, index, forward);
      if (next)
        return next;
    }
    parent = new_parent;
  } while (parent != ancestor);
  return NULL;
}

// MenuScrollTask --------------------------------------------------------------

// MenuScrollTask is used when the SubmenuView does not all fit on screen and
// the mouse is over the scroll up/down buttons. MenuScrollTask schedules
// itself with a RepeatingTimer. When Run is invoked MenuScrollTask scrolls
// appropriately.

class MenuController::MenuScrollTask {
 public:
  MenuScrollTask() : submenu_(NULL), is_scrolling_up_(false), start_y_(0) {
    pixels_per_second_ = MenuItemView::pref_menu_height() * 20;
  }

  void Update(const MenuController::MenuPart& part) {
    if (!part.is_scroll()) {
      StopScrolling();
      return;
    }
    DCHECK(part.submenu);
    SubmenuView* new_menu = part.submenu;
    bool new_is_up = (part.type == MenuController::MenuPart::SCROLL_UP);
    if (new_menu == submenu_ && is_scrolling_up_ == new_is_up)
      return;

    start_scroll_time_ = base::Time::Now();
    start_y_ = part.submenu->GetVisibleBounds().y();
    submenu_ = new_menu;
    is_scrolling_up_ = new_is_up;

    if (!scrolling_timer_.IsRunning()) {
      scrolling_timer_.Start(FROM_HERE,
                             TimeDelta::FromMilliseconds(kScrollTimerMS),
                             this, &MenuScrollTask::Run);
    }
  }

  void StopScrolling() {
    if (scrolling_timer_.IsRunning()) {
      scrolling_timer_.Stop();
      submenu_ = NULL;
    }
  }

  // The menu being scrolled. Returns null if not scrolling.
  SubmenuView* submenu() const { return submenu_; }

 private:
  void Run() {
    DCHECK(submenu_);
    gfx::Rect vis_rect = submenu_->GetVisibleBounds();
    const int delta_y = static_cast<int>(
        (base::Time::Now() - start_scroll_time_).InMilliseconds() *
        pixels_per_second_ / 1000);
    vis_rect.set_y(is_scrolling_up_ ?
        std::max(0, start_y_ - delta_y) :
        std::min(submenu_->height() - vis_rect.height(), start_y_ + delta_y));
    submenu_->ScrollRectToVisible(vis_rect);
  }

  // SubmenuView being scrolled.
  SubmenuView* submenu_;

  // Direction scrolling.
  bool is_scrolling_up_;

  // Timer to periodically scroll.
  base::RepeatingTimer<MenuScrollTask> scrolling_timer_;

  // Time we started scrolling at.
  base::Time start_scroll_time_;

  // How many pixels to scroll per second.
  int pixels_per_second_;

  // Y-coordinate of submenu_view_ when scrolling started.
  int start_y_;

  DISALLOW_COPY_AND_ASSIGN(MenuScrollTask);
};

// MenuController:SelectByCharDetails ----------------------------------------

struct MenuController::SelectByCharDetails {
  SelectByCharDetails()
      : first_match(-1),
        has_multiple(false),
        index_of_item(-1),
        next_match(-1) {
  }

  // Index of the first menu with the specified mnemonic.
  int first_match;

  // If true there are multiple menu items with the same mnemonic.
  bool has_multiple;

  // Index of the selected item; may remain -1.
  int index_of_item;

  // If there are multiple matches this is the index of the item after the
  // currently selected item whose mnemonic matches. This may remain -1 even
  // though there are matches.
  int next_match;
};

// MenuController:State ------------------------------------------------------

MenuController::State::State()
    : item(NULL),
      submenu_open(false),
      anchor(MENU_ANCHOR_TOPLEFT),
      context_menu(false) {
}

MenuController::State::~State() {}

// MenuController ------------------------------------------------------------

// static
MenuController* MenuController::active_instance_ = NULL;

// static
MenuController* MenuController::GetActiveInstance() {
  return active_instance_;
}

MenuItemView* MenuController::Run(Widget* parent,
                                  MenuButton* button,
                                  MenuItemView* root,
                                  const gfx::Rect& bounds,
                                  MenuAnchorPosition position,
                                  bool context_menu,
                                  int* result_event_flags) {
  exit_type_ = EXIT_NONE;
  possible_drag_ = false;
  drag_in_progress_ = false;
  closing_event_time_ = base::TimeDelta();
  menu_start_time_ = base::TimeTicks::Now();
  menu_start_mouse_press_loc_ = gfx::Point();

  // If we are shown on mouse press, we will eat the subsequent mouse down and
  // the parent widget will not be able to reset its state (it might have mouse
  // capture from the mouse down). So we clear its state here.
  if (parent) {
    View* root_view = parent->GetRootView();
    if (root_view) {
      root_view->SetMouseHandler(NULL);
      const ui::Event* event =
          static_cast<internal::RootView*>(root_view)->current_event();
      if (event && event->type() == ui::ET_MOUSE_PRESSED) {
        gfx::Point screen_loc(
            static_cast<const ui::MouseEvent*>(event)->location());
        View::ConvertPointToScreen(
            static_cast<View*>(event->target()), &screen_loc);
        menu_start_mouse_press_loc_ = screen_loc;
      }
    }
  }

  bool nested_menu = showing_;
  if (showing_) {
    // Only support nesting of blocking_run menus, nesting of
    // blocking/non-blocking shouldn't be needed.
    DCHECK(blocking_run_);

    // We're already showing, push the current state.
    menu_stack_.push_back(state_);

    // The context menu should be owned by the same parent.
    DCHECK_EQ(owner_, parent);
  } else {
    showing_ = true;
  }

  // Reset current state.
  pending_state_ = State();
  state_ = State();
  UpdateInitialLocation(bounds, position, context_menu);

  if (owner_)
    owner_->RemoveObserver(this);
  owner_ = parent;
  if (owner_)
    owner_->AddObserver(this);

  // Set the selection, which opens the initial menu.
  SetSelection(root, SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);

  if (!blocking_run_) {
    // Start the timer to hide the menu. This is needed as we get no
    // notification when the drag has finished.
    StartCancelAllTimer();
    return NULL;
  }

  if (button)
    menu_button_ = button;

  // Make sure Chrome doesn't attempt to shut down while the menu is showing.
  if (ViewsDelegate::views_delegate)
    ViewsDelegate::views_delegate->AddRef();

  // We need to turn on nestable tasks as in some situations (pressing alt-f for
  // one) the menus are run from a task. If we don't do this and are invoked
  // from a task none of the tasks we schedule are processed and the menu
  // appears totally broken.
  message_loop_depth_++;
  DCHECK_LE(message_loop_depth_, 2);
  RunMessageLoop(nested_menu);
  message_loop_depth_--;

  if (ViewsDelegate::views_delegate)
    ViewsDelegate::views_delegate->ReleaseRef();

  // Close any open menus.
  SetSelection(NULL, SELECTION_UPDATE_IMMEDIATELY | SELECTION_EXIT);

#if defined(OS_WIN)
  // On Windows, if we select the menu item by touch and if the window at the
  // location is another window on the same thread, that window gets a
  // WM_MOUSEACTIVATE message and ends up activating itself, which is not
  // correct. We workaround this by setting a property on the window at the
  // current cursor location. We check for this property in our
  // WM_MOUSEACTIVATE handler and don't activate the window if the property is
  // set.
  if (item_selected_by_touch_) {
    item_selected_by_touch_ = false;
    POINT cursor_pos;
    ::GetCursorPos(&cursor_pos);
     HWND window = ::WindowFromPoint(cursor_pos);
     if (::GetWindowThreadProcessId(window, NULL) ==
                                    ::GetCurrentThreadId()) {
       ::SetProp(window, ui::kIgnoreTouchMouseActivateForWindow,
                 reinterpret_cast<HANDLE>(true));
     }
  }
#endif

  if (nested_menu) {
    DCHECK(!menu_stack_.empty());
    // We're running from within a menu, restore the previous state.
    // The menus are already showing, so we don't have to show them.
    state_ = menu_stack_.back();
    pending_state_ = menu_stack_.back();
    menu_stack_.pop_back();
  } else {
    showing_ = false;
    did_capture_ = false;
  }

  MenuItemView* result = result_;
  // In case we're nested, reset result_.
  result_ = NULL;

  if (result_event_flags)
    *result_event_flags = accept_event_flags_;

  if (exit_type_ == EXIT_OUTERMOST) {
    SetExitType(EXIT_NONE);
  } else {
    if (nested_menu && result) {
      // We're nested and about to return a value. The caller might enter
      // another blocking loop. We need to make sure all menus are hidden
      // before that happens otherwise the menus will stay on screen.
      CloseAllNestedMenus();
      SetSelection(NULL, SELECTION_UPDATE_IMMEDIATELY | SELECTION_EXIT);

      // Set exit_all_, which makes sure all nested loops exit immediately.
      if (exit_type_ != EXIT_DESTROYED)
        SetExitType(EXIT_ALL);
    }
  }

  // If we stopped running because one of the menus was destroyed chances are
  // the button was also destroyed.
  if (exit_type_ != EXIT_DESTROYED && menu_button_) {
    menu_button_->SetState(CustomButton::STATE_NORMAL);
    menu_button_->SchedulePaint();
  }
  return result;
}

void MenuController::Cancel(ExitType type) {
  // If the menu has already been destroyed, no further cancellation is
  // needed.  We especially don't want to set the |exit_type_| to a lesser
  // value.
  if (exit_type_ == EXIT_DESTROYED || exit_type_ == type)
    return;

  if (!showing_) {
    // This occurs if we're in the process of notifying the delegate for a drop
    // and the delegate cancels us.
    return;
  }

  MenuItemView* selected = state_.item;
  SetExitType(type);

  SendMouseCaptureLostToActiveView();

  // Hide windows immediately.
  SetSelection(NULL, SELECTION_UPDATE_IMMEDIATELY | SELECTION_EXIT);

  if (!blocking_run_) {
    // If we didn't block the caller we need to notify the menu, which
    // triggers deleting us.
    DCHECK(selected);
    showing_ = false;
    delegate_->DropMenuClosed(
        internal::MenuControllerDelegate::NOTIFY_DELEGATE,
        selected->GetRootMenuItem());
    // WARNING: the call to MenuClosed deletes us.
    return;
  }
}

void MenuController::OnMousePressed(SubmenuView* source,
                                    const ui::MouseEvent& event) {
  SetSelectionOnPointerDown(source, event);
}

void MenuController::OnMouseDragged(SubmenuView* source,
                                    const ui::MouseEvent& event) {
  MenuPart part = GetMenuPart(source, event.location());
  UpdateScrolling(part);

  if (!blocking_run_)
    return;

  if (possible_drag_) {
    if (View::ExceededDragThreshold(event.location() - press_pt_))
      StartDrag(source, press_pt_);
    return;
  }
  MenuItemView* mouse_menu = NULL;
  if (part.type == MenuPart::MENU_ITEM) {
    if (!part.menu)
      part.menu = source->GetMenuItem();
    else
      mouse_menu = part.menu;
    SetSelection(part.menu ? part.menu : state_.item, SELECTION_OPEN_SUBMENU);
  } else if (part.type == MenuPart::NONE) {
    ShowSiblingMenu(source, event.location());
  }
  UpdateActiveMouseView(source, event, mouse_menu);
}

void MenuController::OnMouseReleased(SubmenuView* source,
                                     const ui::MouseEvent& event) {
  if (!blocking_run_)
    return;

  DCHECK(state_.item);
  possible_drag_ = false;
  DCHECK(blocking_run_);
  MenuPart part = GetMenuPart(source, event.location());
  if (event.IsRightMouseButton() && part.type == MenuPart::MENU_ITEM) {
    MenuItemView* menu = part.menu;
    // |menu| is NULL means this event is from an empty menu or a separator.
    // If it is from an empty menu, use parent context menu instead of that.
    if (menu == NULL &&
        part.submenu->child_count() == 1 &&
        part.submenu->child_at(0)->id() == MenuItemView::kEmptyMenuItemViewID) {
      menu = part.parent;
    }

    if (menu != NULL && ShowContextMenu(menu, source, event,
                                        ui::MENU_SOURCE_MOUSE))
      return;
  }

  // We can use Ctrl+click or the middle mouse button to recursively open urls
  // for selected folder menu items. If it's only a left click, show the
  // contents of the folder.
  if (!part.is_scroll() && part.menu &&
      !(part.menu->HasSubmenu() &&
        (event.flags() & ui::EF_LEFT_MOUSE_BUTTON))) {
    if (GetActiveMouseView()) {
      SendMouseReleaseToActiveView(source, event);
      return;
    }
    // If a mouse release was received quickly after showing.
    base::TimeDelta time_shown = base::TimeTicks::Now() - menu_start_time_;
    if (time_shown.InMilliseconds() < menu_selection_hold_time_ms) {
      // And it wasn't far from the mouse press location.
      gfx::Point screen_loc(event.location());
      View::ConvertPointToScreen(source->GetScrollViewContainer(), &screen_loc);
      gfx::Vector2d moved = screen_loc - menu_start_mouse_press_loc_;
      if (moved.Length() < kMaximumLengthMovedToActivate) {
        // Ignore the mouse release as it was likely this menu was shown under
        // the mouse and the action was just a normal click.
        return;
      }
    }
    if (part.menu->GetDelegate()->ShouldExecuteCommandWithoutClosingMenu(
            part.menu->GetCommand(), event)) {
      part.menu->GetDelegate()->ExecuteCommand(part.menu->GetCommand(),
                                               event.flags());
      return;
    }
    if (!part.menu->NonIconChildViewsCount() &&
        part.menu->GetDelegate()->IsTriggerableEvent(part.menu, event)) {
      base::TimeDelta shown_time = base::TimeTicks::Now() - menu_start_time_;
      if (!state_.context_menu || !View::ShouldShowContextMenuOnMousePress() ||
          shown_time.InMilliseconds() > menu_selection_hold_time_ms) {
        Accept(part.menu, event.flags());
      }
      return;
    }
  } else if (part.type == MenuPart::MENU_ITEM) {
    // User either clicked on empty space, or a menu that has children.
    SetSelection(part.menu ? part.menu : state_.item,
                 SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
  }
  SendMouseCaptureLostToActiveView();
}

void MenuController::OnMouseMoved(SubmenuView* source,
                                  const ui::MouseEvent& event) {
  HandleMouseLocation(source, event.location());
}

void MenuController::OnMouseEntered(SubmenuView* source,
                                    const ui::MouseEvent& event) {
  // MouseEntered is always followed by a mouse moved, so don't need to
  // do anything here.
}

bool MenuController::OnMouseWheel(SubmenuView* source,
                                  const ui::MouseWheelEvent& event) {
  MenuPart part = GetMenuPart(source, event.location());
  return part.submenu && part.submenu->OnMouseWheel(event);
}

void MenuController::OnGestureEvent(SubmenuView* source,
                                    ui::GestureEvent* event) {
  MenuPart part = GetMenuPart(source, event->location());
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    SetSelectionOnPointerDown(source, *event);
    event->StopPropagation();
  } else if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
    if (part.type == MenuPart::MENU_ITEM && part.menu) {
      if (ShowContextMenu(part.menu, source, *event, ui::MENU_SOURCE_TOUCH))
        event->StopPropagation();
    }
  } else if (event->type() == ui::ET_GESTURE_TAP) {
    if (!part.is_scroll() && part.menu &&
        !(part.menu->HasSubmenu())) {
      if (part.menu->GetDelegate()->IsTriggerableEvent(
          part.menu, *event)) {
        Accept(part.menu, event->flags());
        item_selected_by_touch_ = true;
      }
      event->StopPropagation();
    } else if (part.type == MenuPart::MENU_ITEM) {
      // User either tapped on empty space, or a menu that has children.
      SetSelection(part.menu ? part.menu : state_.item,
                   SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
      event->StopPropagation();
    }
  } else if (event->type() == ui::ET_GESTURE_TAP_CANCEL &&
             part.menu &&
             part.type == MenuPart::MENU_ITEM) {
    // Move the selection to the parent menu so that the selection in the
    // current menu is unset. Make sure the submenu remains open by sending the
    // appropriate SetSelectionTypes flags.
    SetSelection(part.menu->GetParentMenuItem(),
        SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
    event->StopPropagation();
  }

  if (event->stopped_propagation())
      return;

  if (!part.submenu)
    return;
  part.submenu->OnGestureEvent(event);
}

bool MenuController::GetDropFormats(
      SubmenuView* source,
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) {
  return source->GetMenuItem()->GetDelegate()->GetDropFormats(
      source->GetMenuItem(), formats, custom_formats);
}

bool MenuController::AreDropTypesRequired(SubmenuView* source) {
  return source->GetMenuItem()->GetDelegate()->AreDropTypesRequired(
      source->GetMenuItem());
}

bool MenuController::CanDrop(SubmenuView* source, const OSExchangeData& data) {
  return source->GetMenuItem()->GetDelegate()->CanDrop(source->GetMenuItem(),
                                                       data);
}

void MenuController::OnDragEntered(SubmenuView* source,
                                   const ui::DropTargetEvent& event) {
  valid_drop_coordinates_ = false;
}

int MenuController::OnDragUpdated(SubmenuView* source,
                                  const ui::DropTargetEvent& event) {
  StopCancelAllTimer();

  gfx::Point screen_loc(event.location());
  View::ConvertPointToScreen(source, &screen_loc);
  if (valid_drop_coordinates_ && screen_loc == drop_pt_)
    return last_drop_operation_;
  drop_pt_ = screen_loc;
  valid_drop_coordinates_ = true;

  MenuItemView* menu_item = GetMenuItemAt(source, event.x(), event.y());
  bool over_empty_menu = false;
  if (!menu_item) {
    // See if we're over an empty menu.
    menu_item = GetEmptyMenuItemAt(source, event.x(), event.y());
    if (menu_item)
      over_empty_menu = true;
  }
  MenuDelegate::DropPosition drop_position = MenuDelegate::DROP_NONE;
  int drop_operation = ui::DragDropTypes::DRAG_NONE;
  if (menu_item) {
    gfx::Point menu_item_loc(event.location());
    View::ConvertPointToTarget(source, menu_item, &menu_item_loc);
    MenuItemView* query_menu_item;
    if (!over_empty_menu) {
      int menu_item_height = menu_item->height();
      if (menu_item->HasSubmenu() &&
          (menu_item_loc.y() > kDropBetweenPixels &&
           menu_item_loc.y() < (menu_item_height - kDropBetweenPixels))) {
        drop_position = MenuDelegate::DROP_ON;
      } else {
        drop_position = (menu_item_loc.y() < menu_item_height / 2) ?
            MenuDelegate::DROP_BEFORE : MenuDelegate::DROP_AFTER;
      }
      query_menu_item = menu_item;
    } else {
      query_menu_item = menu_item->GetParentMenuItem();
      drop_position = MenuDelegate::DROP_ON;
    }
    drop_operation = menu_item->GetDelegate()->GetDropOperation(
        query_menu_item, event, &drop_position);

    // If the menu has a submenu, schedule the submenu to open.
    SetSelection(menu_item, menu_item->HasSubmenu() ? SELECTION_OPEN_SUBMENU :
                 SELECTION_DEFAULT);

    if (drop_position == MenuDelegate::DROP_NONE ||
        drop_operation == ui::DragDropTypes::DRAG_NONE)
      menu_item = NULL;
  } else {
    SetSelection(source->GetMenuItem(), SELECTION_OPEN_SUBMENU);
  }
  SetDropMenuItem(menu_item, drop_position);
  last_drop_operation_ = drop_operation;
  return drop_operation;
}

void MenuController::OnDragExited(SubmenuView* source) {
  StartCancelAllTimer();

  if (drop_target_) {
    StopShowTimer();
    SetDropMenuItem(NULL, MenuDelegate::DROP_NONE);
  }
}

int MenuController::OnPerformDrop(SubmenuView* source,
                                  const ui::DropTargetEvent& event) {
  DCHECK(drop_target_);
  // NOTE: the delegate may delete us after invoking OnPerformDrop, as such
  // we don't call cancel here.

  MenuItemView* item = state_.item;
  DCHECK(item);

  MenuItemView* drop_target = drop_target_;
  MenuDelegate::DropPosition drop_position = drop_position_;

  // Close all menus, including any nested menus.
  SetSelection(NULL, SELECTION_UPDATE_IMMEDIATELY | SELECTION_EXIT);
  CloseAllNestedMenus();

  // Set state such that we exit.
  showing_ = false;
  SetExitType(EXIT_ALL);

  // If over an empty menu item, drop occurs on the parent.
  if (drop_target->id() == MenuItemView::kEmptyMenuItemViewID)
    drop_target = drop_target->GetParentMenuItem();

  if (!IsBlockingRun()) {
    delegate_->DropMenuClosed(
        internal::MenuControllerDelegate::DONT_NOTIFY_DELEGATE,
        item->GetRootMenuItem());
  }

  // WARNING: the call to MenuClosed deletes us.

  return drop_target->GetDelegate()->OnPerformDrop(
      drop_target, drop_position, event);
}

void MenuController::OnDragEnteredScrollButton(SubmenuView* source,
                                               bool is_up) {
  MenuPart part;
  part.type = is_up ? MenuPart::SCROLL_UP : MenuPart::SCROLL_DOWN;
  part.submenu = source;
  UpdateScrolling(part);

  // Do this to force the selection to hide.
  SetDropMenuItem(source->GetMenuItemAt(0), MenuDelegate::DROP_NONE);

  StopCancelAllTimer();
}

void MenuController::OnDragExitedScrollButton(SubmenuView* source) {
  StartCancelAllTimer();
  SetDropMenuItem(NULL, MenuDelegate::DROP_NONE);
  StopScrolling();
}

void MenuController::UpdateSubmenuSelection(SubmenuView* submenu) {
  if (submenu->IsShowing()) {
    gfx::Point point = GetScreen()->GetCursorScreenPoint();
    const SubmenuView* root_submenu =
        submenu->GetMenuItem()->GetRootMenuItem()->GetSubmenu();
    View::ConvertPointFromScreen(
        root_submenu->GetWidget()->GetRootView(), &point);
    HandleMouseLocation(submenu, point);
  }
}

void MenuController::OnWidgetDestroying(Widget* widget) {
  DCHECK_EQ(owner_, widget);
  owner_->RemoveObserver(this);
  owner_ = NULL;
  message_loop_->ClearOwner();
}

// static
void MenuController::TurnOffMenuSelectionHoldForTest() {
  menu_selection_hold_time_ms = -1;
}

void MenuController::SetSelection(MenuItemView* menu_item,
                                  int selection_types) {
  size_t paths_differ_at = 0;
  std::vector<MenuItemView*> current_path;
  std::vector<MenuItemView*> new_path;
  BuildPathsAndCalculateDiff(pending_state_.item, menu_item, &current_path,
                             &new_path, &paths_differ_at);

  size_t current_size = current_path.size();
  size_t new_size = new_path.size();

  bool pending_item_changed = pending_state_.item != menu_item;
  if (pending_item_changed && pending_state_.item) {
    CustomButton* button = GetFirstHotTrackedView(pending_state_.item);
    if (button)
      button->SetHotTracked(false);
  }

  // Notify the old path it isn't selected.
  MenuDelegate* current_delegate =
      current_path.empty() ? NULL : current_path.front()->GetDelegate();
  for (size_t i = paths_differ_at; i < current_size; ++i) {
    if (current_delegate &&
        current_path[i]->GetType() == MenuItemView::SUBMENU) {
      current_delegate->WillHideMenu(current_path[i]);
    }
    current_path[i]->SetSelected(false);
  }

  // Notify the new path it is selected.
  for (size_t i = paths_differ_at; i < new_size; ++i) {
    new_path[i]->ScrollRectToVisible(new_path[i]->GetLocalBounds());
    new_path[i]->SetSelected(true);
  }

  if (menu_item && menu_item->GetDelegate())
    menu_item->GetDelegate()->SelectionChanged(menu_item);

  DCHECK(menu_item || (selection_types & SELECTION_EXIT) != 0);

  pending_state_.item = menu_item;
  pending_state_.submenu_open = (selection_types & SELECTION_OPEN_SUBMENU) != 0;

  // Stop timers.
  StopCancelAllTimer();
  // Resets show timer only when pending menu item is changed.
  if (pending_item_changed)
    StopShowTimer();

  if (selection_types & SELECTION_UPDATE_IMMEDIATELY)
    CommitPendingSelection();
  else if (pending_item_changed)
    StartShowTimer();

  // Notify an accessibility focus event on all menu items except for the root.
  if (menu_item &&
      (MenuDepth(menu_item) != 1 ||
       menu_item->GetType() != MenuItemView::SUBMENU)) {
    menu_item->NotifyAccessibilityEvent(
        ui::AX_EVENT_FOCUS, true);
  }
}

void MenuController::SetSelectionOnPointerDown(SubmenuView* source,
                                               const ui::LocatedEvent& event) {
  if (!blocking_run_)
    return;

  DCHECK(!GetActiveMouseView());

  MenuPart part = GetMenuPart(source, event.location());
  if (part.is_scroll())
    return;  // Ignore presses on scroll buttons.

  // When this menu is opened through a touch event, a simulated right-click
  // is sent before the menu appears.  Ignore it.
  if ((event.flags() & ui::EF_RIGHT_MOUSE_BUTTON) &&
      (event.flags() & ui::EF_FROM_TOUCH))
    return;

  if (part.type == MenuPart::NONE ||
      (part.type == MenuPart::MENU_ITEM && part.menu &&
       part.menu->GetRootMenuItem() != state_.item->GetRootMenuItem())) {
    // Remember the time when we repost the event. The owner can then use this
    // to figure out if this menu was finished with the same click which is
    // sent to it thereafter. Note that the time stamp front he event cannot be
    // used since the reposting will set a new timestamp when the event gets
    // processed. As such it is better to take the current time which will be
    // closer to the time when it arrives again in the menu handler.
    closing_event_time_ = ui::EventTimeForNow();

    // Mouse wasn't pressed over any menu, or the active menu, cancel.

#if defined(OS_WIN)
    // We're going to close and we own the mouse capture. We need to repost the
    // mouse down, otherwise the window the user clicked on won't get the event.
    RepostEvent(source, event);
#endif

    // And close.
    ExitType exit_type = EXIT_ALL;
    if (!menu_stack_.empty()) {
      // We're running nested menus. Only exit all if the mouse wasn't over one
      // of the menus from the last run.
      gfx::Point screen_loc(event.location());
      View::ConvertPointToScreen(source->GetScrollViewContainer(), &screen_loc);
      MenuPart last_part = GetMenuPartByScreenCoordinateUsingMenu(
          menu_stack_.back().item, screen_loc);
      if (last_part.type != MenuPart::NONE)
        exit_type = EXIT_OUTERMOST;
    }
    Cancel(exit_type);

#if defined(OS_CHROMEOS)
    // We're going to exit the menu and want to repost the event so that is
    // is handled normally after the context menu has exited. We call
    // RepostEvent after Cancel so that mouse capture has been released so
    // that finding the event target is unaffected by the current capture.
    RepostEvent(source, event);
#endif
    // Do not repost events for Linux Aura because this behavior is more
    // consistent with the behavior of other Linux apps.
    return;
  }

  // On a press we immediately commit the selection, that way a submenu
  // pops up immediately rather than after a delay.
  int selection_types = SELECTION_UPDATE_IMMEDIATELY;
  if (!part.menu) {
    part.menu = part.parent;
    selection_types |= SELECTION_OPEN_SUBMENU;
  } else {
    if (part.menu->GetDelegate()->CanDrag(part.menu)) {
      possible_drag_ = true;
      press_pt_ = event.location();
    }
    if (part.menu->HasSubmenu())
      selection_types |= SELECTION_OPEN_SUBMENU;
  }
  SetSelection(part.menu, selection_types);
}

void MenuController::StartDrag(SubmenuView* source,
                               const gfx::Point& location) {
  MenuItemView* item = state_.item;
  DCHECK(item);
  // Points are in the coordinates of the submenu, need to map to that of
  // the selected item. Additionally source may not be the parent of
  // the selected item, so need to map to screen first then to item.
  gfx::Point press_loc(location);
  View::ConvertPointToScreen(source->GetScrollViewContainer(), &press_loc);
  View::ConvertPointFromScreen(item, &press_loc);
  gfx::Point widget_loc(press_loc);
  View::ConvertPointToWidget(item, &widget_loc);
  scoped_ptr<gfx::Canvas> canvas(GetCanvasForDragImage(
      source->GetWidget(), gfx::Size(item->width(), item->height())));
  item->PaintButton(canvas.get(), MenuItemView::PB_FOR_DRAG);

  OSExchangeData data;
  item->GetDelegate()->WriteDragData(item, &data);
  drag_utils::SetDragImageOnDataObject(*canvas,
                                       press_loc.OffsetFromOrigin(),
                                       &data);
  StopScrolling();
  int drag_ops = item->GetDelegate()->GetDragOperations(item);
  drag_in_progress_ = true;
  // TODO(varunjain): Properly determine and send DRAG_EVENT_SOURCE below.
  item->GetWidget()->RunShellDrag(NULL, data, widget_loc, drag_ops,
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  drag_in_progress_ = false;

  if (GetActiveInstance() == this) {
    if (showing_) {
      // We're still showing, close all menus.
      CloseAllNestedMenus();
      Cancel(EXIT_ALL);
    }  // else case, drop was on us.
  }  // else case, someone canceled us, don't do anything
}

bool MenuController::OnKeyDown(ui::KeyboardCode key_code) {
  DCHECK(blocking_run_);

  switch (key_code) {
    case ui::VKEY_UP:
      IncrementSelection(-1);
      break;

    case ui::VKEY_DOWN:
      IncrementSelection(1);
      break;

    // Handling of VK_RIGHT and VK_LEFT is different depending on the UI
    // layout.
    case ui::VKEY_RIGHT:
      if (base::i18n::IsRTL())
        CloseSubmenu();
      else
        OpenSubmenuChangeSelectionIfCan();
      break;

    case ui::VKEY_LEFT:
      if (base::i18n::IsRTL())
        OpenSubmenuChangeSelectionIfCan();
      else
        CloseSubmenu();
      break;

    case ui::VKEY_SPACE:
      if (SendAcceleratorToHotTrackedView() == ACCELERATOR_PROCESSED_EXIT)
        return false;
      break;

    case ui::VKEY_F4:
      if (!is_combobox_)
        break;
      // Fallthrough to accept on F4, so combobox menus match Windows behavior.
    case ui::VKEY_RETURN:
      if (pending_state_.item) {
        if (pending_state_.item->HasSubmenu()) {
          OpenSubmenuChangeSelectionIfCan();
        } else {
          SendAcceleratorResultType result = SendAcceleratorToHotTrackedView();
          if (result == ACCELERATOR_NOT_PROCESSED &&
              pending_state_.item->enabled()) {
            Accept(pending_state_.item, 0);
            return false;
          } else if (result == ACCELERATOR_PROCESSED_EXIT) {
            return false;
          }
        }
      }
      break;

    case ui::VKEY_ESCAPE:
      if (!state_.item->GetParentMenuItem() ||
          (!state_.item->GetParentMenuItem()->GetParentMenuItem() &&
           (!state_.item->HasSubmenu() ||
            !state_.item->GetSubmenu()->IsShowing()))) {
        // User pressed escape and only one menu is shown, cancel it.
        Cancel(EXIT_OUTERMOST);
        return false;
      }
      CloseSubmenu();
      break;

    default:
      break;
  }
  return true;
}

MenuController::MenuController(ui::NativeTheme* theme,
                               bool blocking,
                               internal::MenuControllerDelegate* delegate)
    : blocking_run_(blocking),
      showing_(false),
      exit_type_(EXIT_NONE),
      did_capture_(false),
      result_(NULL),
      accept_event_flags_(0),
      drop_target_(NULL),
      drop_position_(MenuDelegate::DROP_UNKNOWN),
      owner_(NULL),
      possible_drag_(false),
      drag_in_progress_(false),
      valid_drop_coordinates_(false),
      last_drop_operation_(MenuDelegate::DROP_UNKNOWN),
      showing_submenu_(false),
      menu_button_(NULL),
      active_mouse_view_id_(ViewStorage::GetInstance()->CreateStorageID()),
      delegate_(delegate),
      message_loop_depth_(0),
      menu_config_(theme),
      closing_event_time_(base::TimeDelta()),
      menu_start_time_(base::TimeTicks()),
      is_combobox_(false),
      item_selected_by_touch_(false),
      message_loop_(MenuMessageLoop::Create()) {
  active_instance_ = this;
}

MenuController::~MenuController() {
  DCHECK(!showing_);
  if (owner_)
    owner_->RemoveObserver(this);
  if (active_instance_ == this)
    active_instance_ = NULL;
  StopShowTimer();
  StopCancelAllTimer();
}

void MenuController::RunMessageLoop(bool nested_menu) {
  message_loop_->Run(this, owner_, nested_menu);
}

MenuController::SendAcceleratorResultType
    MenuController::SendAcceleratorToHotTrackedView() {
  CustomButton* hot_view = GetFirstHotTrackedView(pending_state_.item);
  if (!hot_view)
    return ACCELERATOR_NOT_PROCESSED;

  ui::Accelerator accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  hot_view->AcceleratorPressed(accelerator);
  CustomButton* button = static_cast<CustomButton*>(hot_view);
  button->SetHotTracked(true);
  return (exit_type_ == EXIT_NONE) ?
      ACCELERATOR_PROCESSED : ACCELERATOR_PROCESSED_EXIT;
}

void MenuController::UpdateInitialLocation(const gfx::Rect& bounds,
                                           MenuAnchorPosition position,
                                           bool context_menu) {
  pending_state_.context_menu = context_menu;
  pending_state_.initial_bounds = bounds;
  if (bounds.height() > 1) {
    // Inset the bounds slightly, otherwise drag coordinates don't line up
    // nicely and menus close prematurely.
    pending_state_.initial_bounds.Inset(0, 1);
  }

  // Reverse anchor position for RTL languages.
  if (base::i18n::IsRTL() &&
      (position == MENU_ANCHOR_TOPRIGHT || position == MENU_ANCHOR_TOPLEFT)) {
    pending_state_.anchor = position == MENU_ANCHOR_TOPRIGHT
                                ? MENU_ANCHOR_TOPLEFT
                                : MENU_ANCHOR_TOPRIGHT;
  } else {
    pending_state_.anchor = position;
  }

  // Calculate the bounds of the monitor we'll show menus on. Do this once to
  // avoid repeated system queries for the info.
  pending_state_.monitor_bounds = GetScreen()->GetDisplayNearestPoint(
      bounds.origin()).work_area();

  if (!pending_state_.monitor_bounds.Contains(bounds)) {
    // Use the monitor area if the work area doesn't contain the bounds. This
    // handles showing a menu from the launcher.
    gfx::Rect monitor_area = GetScreen()->GetDisplayNearestPoint(
        bounds.origin()).bounds();
    if (monitor_area.Contains(bounds))
      pending_state_.monitor_bounds = monitor_area;
  }
}

void MenuController::Accept(MenuItemView* item, int event_flags) {
  DCHECK(IsBlockingRun());
  result_ = item;
  if (item && !menu_stack_.empty() &&
      !item->GetDelegate()->ShouldCloseAllMenusOnExecute(item->GetCommand())) {
    SetExitType(EXIT_OUTERMOST);
  } else {
    SetExitType(EXIT_ALL);
  }
  accept_event_flags_ = event_flags;
}

bool MenuController::ShowSiblingMenu(SubmenuView* source,
                                     const gfx::Point& mouse_location) {
  if (!menu_stack_.empty() || !menu_button_)
    return false;

  View* source_view = source->GetScrollViewContainer();
  if (mouse_location.x() >= 0 &&
      mouse_location.x() < source_view->width() &&
      mouse_location.y() >= 0 &&
      mouse_location.y() < source_view->height()) {
    // The mouse is over the menu, no need to continue.
    return false;
  }

  gfx::NativeWindow window_under_mouse = GetScreen()->GetWindowUnderCursor();
  // TODO(oshima): Replace with views only API.
  if (!owner_ || window_under_mouse != owner_->GetNativeWindow())
    return false;

  // The user moved the mouse outside the menu and over the owning window. See
  // if there is a sibling menu we should show.
  gfx::Point screen_point(mouse_location);
  View::ConvertPointToScreen(source_view, &screen_point);
  MenuAnchorPosition anchor;
  bool has_mnemonics;
  MenuButton* button = NULL;
  MenuItemView* alt_menu = source->GetMenuItem()->GetDelegate()->
      GetSiblingMenu(source->GetMenuItem()->GetRootMenuItem(),
                     screen_point, &anchor, &has_mnemonics, &button);
  if (!alt_menu || (state_.item && state_.item->GetRootMenuItem() == alt_menu))
    return false;

  delegate_->SiblingMenuCreated(alt_menu);

  if (!button) {
    // If the delegate returns a menu, they must also return a button.
    NOTREACHED();
    return false;
  }

  // There is a sibling menu, update the button state, hide the current menu
  // and show the new one.
  menu_button_->SetState(CustomButton::STATE_NORMAL);
  menu_button_->SchedulePaint();
  menu_button_ = button;
  menu_button_->SetState(CustomButton::STATE_PRESSED);
  menu_button_->SchedulePaint();

  // Need to reset capture when we show the menu again, otherwise we aren't
  // going to get any events.
  did_capture_ = false;
  gfx::Point screen_menu_loc;
  View::ConvertPointToScreen(button, &screen_menu_loc);

  // It is currently not possible to show a submenu recursively in a bubble.
  DCHECK(!MenuItemView::IsBubble(anchor));
  // Subtract 1 from the height to make the popup flush with the button border.
  UpdateInitialLocation(gfx::Rect(screen_menu_loc.x(), screen_menu_loc.y(),
                                  button->width(), button->height() - 1),
                        anchor, state_.context_menu);
  alt_menu->PrepareForRun(
      false, has_mnemonics,
      source->GetMenuItem()->GetRootMenuItem()->show_mnemonics_);
  alt_menu->controller_ = this;
  SetSelection(alt_menu, SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
  return true;
}

bool MenuController::ShowContextMenu(MenuItemView* menu_item,
                                     SubmenuView* source,
                                     const ui::LocatedEvent& event,
                                     ui::MenuSourceType source_type) {
  // Set the selection immediately, making sure the submenu is only open
  // if it already was.
  int selection_types = SELECTION_UPDATE_IMMEDIATELY;
  if (state_.item == pending_state_.item && state_.submenu_open)
    selection_types |= SELECTION_OPEN_SUBMENU;
  SetSelection(pending_state_.item, selection_types);
  gfx::Point loc(event.location());
  View::ConvertPointToScreen(source->GetScrollViewContainer(), &loc);

  if (menu_item->GetDelegate()->ShowContextMenu(
          menu_item, menu_item->GetCommand(), loc, source_type)) {
    SendMouseCaptureLostToActiveView();
    return true;
  }
  return false;
}

void MenuController::CloseAllNestedMenus() {
  for (std::list<State>::iterator i = menu_stack_.begin();
       i != menu_stack_.end(); ++i) {
    MenuItemView* last_item = i->item;
    for (MenuItemView* item = last_item; item;
         item = item->GetParentMenuItem()) {
      CloseMenu(item);
      last_item = item;
    }
    i->submenu_open = false;
    i->item = last_item;
  }
}

MenuItemView* MenuController::GetMenuItemAt(View* source, int x, int y) {
  // Walk the view hierarchy until we find a menu item (or the root).
  View* child_under_mouse = source->GetEventHandlerForPoint(gfx::Point(x, y));
  while (child_under_mouse &&
         child_under_mouse->id() != MenuItemView::kMenuItemViewID) {
    child_under_mouse = child_under_mouse->parent();
  }
  if (child_under_mouse && child_under_mouse->enabled() &&
      child_under_mouse->id() == MenuItemView::kMenuItemViewID) {
    return static_cast<MenuItemView*>(child_under_mouse);
  }
  return NULL;
}

MenuItemView* MenuController::GetEmptyMenuItemAt(View* source, int x, int y) {
  View* child_under_mouse = source->GetEventHandlerForPoint(gfx::Point(x, y));
  if (child_under_mouse &&
      child_under_mouse->id() == MenuItemView::kEmptyMenuItemViewID) {
    return static_cast<MenuItemView*>(child_under_mouse);
  }
  return NULL;
}

bool MenuController::IsScrollButtonAt(SubmenuView* source,
                                      int x,
                                      int y,
                                      MenuPart::Type* part) {
  MenuScrollViewContainer* scroll_view = source->GetScrollViewContainer();
  View* child_under_mouse =
      scroll_view->GetEventHandlerForPoint(gfx::Point(x, y));
  if (child_under_mouse && child_under_mouse->enabled()) {
    if (child_under_mouse == scroll_view->scroll_up_button()) {
      *part = MenuPart::SCROLL_UP;
      return true;
    }
    if (child_under_mouse == scroll_view->scroll_down_button()) {
      *part = MenuPart::SCROLL_DOWN;
      return true;
    }
  }
  return false;
}

MenuController::MenuPart MenuController::GetMenuPart(
    SubmenuView* source,
    const gfx::Point& source_loc) {
  gfx::Point screen_loc(source_loc);
  View::ConvertPointToScreen(source->GetScrollViewContainer(), &screen_loc);
  return GetMenuPartByScreenCoordinateUsingMenu(state_.item, screen_loc);
}

MenuController::MenuPart MenuController::GetMenuPartByScreenCoordinateUsingMenu(
    MenuItemView* item,
    const gfx::Point& screen_loc) {
  MenuPart part;
  for (; item; item = item->GetParentMenuItem()) {
    if (item->HasSubmenu() && item->GetSubmenu()->IsShowing() &&
        GetMenuPartByScreenCoordinateImpl(item->GetSubmenu(), screen_loc,
                                          &part)) {
      return part;
    }
  }
  return part;
}

bool MenuController::GetMenuPartByScreenCoordinateImpl(
    SubmenuView* menu,
    const gfx::Point& screen_loc,
    MenuPart* part) {
  // Is the mouse over the scroll buttons?
  gfx::Point scroll_view_loc = screen_loc;
  View* scroll_view_container = menu->GetScrollViewContainer();
  View::ConvertPointFromScreen(scroll_view_container, &scroll_view_loc);
  if (scroll_view_loc.x() < 0 ||
      scroll_view_loc.x() >= scroll_view_container->width() ||
      scroll_view_loc.y() < 0 ||
      scroll_view_loc.y() >= scroll_view_container->height()) {
    // Point isn't contained in menu.
    return false;
  }
  if (IsScrollButtonAt(menu, scroll_view_loc.x(), scroll_view_loc.y(),
                       &(part->type))) {
    part->submenu = menu;
    return true;
  }

  // Not over the scroll button. Check the actual menu.
  if (DoesSubmenuContainLocation(menu, screen_loc)) {
    gfx::Point menu_loc = screen_loc;
    View::ConvertPointFromScreen(menu, &menu_loc);
    part->menu = GetMenuItemAt(menu, menu_loc.x(), menu_loc.y());
    part->type = MenuPart::MENU_ITEM;
    part->submenu = menu;
    if (!part->menu)
      part->parent = menu->GetMenuItem();
    return true;
  }

  // While the mouse isn't over a menu item or the scroll buttons of menu, it
  // is contained by menu and so we return true. If we didn't return true other
  // menus would be searched, even though they are likely obscured by us.
  return true;
}

bool MenuController::DoesSubmenuContainLocation(SubmenuView* submenu,
                                                const gfx::Point& screen_loc) {
  gfx::Point view_loc = screen_loc;
  View::ConvertPointFromScreen(submenu, &view_loc);
  gfx::Rect vis_rect = submenu->GetVisibleBounds();
  return vis_rect.Contains(view_loc.x(), view_loc.y());
}

void MenuController::CommitPendingSelection() {
  StopShowTimer();

  size_t paths_differ_at = 0;
  std::vector<MenuItemView*> current_path;
  std::vector<MenuItemView*> new_path;
  BuildPathsAndCalculateDiff(state_.item, pending_state_.item, &current_path,
                             &new_path, &paths_differ_at);

  // Hide the old menu.
  for (size_t i = paths_differ_at; i < current_path.size(); ++i) {
    if (current_path[i]->HasSubmenu()) {
      current_path[i]->GetSubmenu()->Hide();
    }
  }

  // Copy pending to state_, making sure to preserve the direction menus were
  // opened.
  std::list<bool> pending_open_direction;
  state_.open_leading.swap(pending_open_direction);
  state_ = pending_state_;
  state_.open_leading.swap(pending_open_direction);

  int menu_depth = MenuDepth(state_.item);
  if (menu_depth == 0) {
    state_.open_leading.clear();
  } else {
    int cached_size = static_cast<int>(state_.open_leading.size());
    DCHECK_GE(menu_depth, 0);
    while (cached_size-- >= menu_depth)
      state_.open_leading.pop_back();
  }

  if (!state_.item) {
    // Nothing to select.
    StopScrolling();
    return;
  }

  // Open all the submenus preceeding the last menu item (last menu item is
  // handled next).
  if (new_path.size() > 1) {
    for (std::vector<MenuItemView*>::iterator i = new_path.begin();
         i != new_path.end() - 1; ++i) {
      OpenMenu(*i);
    }
  }

  if (state_.submenu_open) {
    // The submenu should be open, open the submenu if the item has a submenu.
    if (state_.item->HasSubmenu()) {
      OpenMenu(state_.item);
    } else {
      state_.submenu_open = false;
    }
  } else if (state_.item->HasSubmenu() &&
             state_.item->GetSubmenu()->IsShowing()) {
    state_.item->GetSubmenu()->Hide();
  }

  if (scroll_task_.get() && scroll_task_->submenu()) {
    // Stop the scrolling if none of the elements of the selection contain
    // the menu being scrolled.
    bool found = false;
    for (MenuItemView* item = state_.item; item && !found;
         item = item->GetParentMenuItem()) {
      found = (item->HasSubmenu() && item->GetSubmenu()->IsShowing() &&
               item->GetSubmenu() == scroll_task_->submenu());
    }
    if (!found)
      StopScrolling();
  }
}

void MenuController::CloseMenu(MenuItemView* item) {
  DCHECK(item);
  if (!item->HasSubmenu())
    return;
  item->GetSubmenu()->Hide();
}

void MenuController::OpenMenu(MenuItemView* item) {
  DCHECK(item);
  if (item->GetSubmenu()->IsShowing()) {
    return;
  }

  OpenMenuImpl(item, true);
  did_capture_ = true;
}

void MenuController::OpenMenuImpl(MenuItemView* item, bool show) {
  // TODO(oshima|sky): Don't show the menu if drag is in progress and
  // this menu doesn't support drag drop. See crbug.com/110495.
  if (show) {
    int old_count = item->GetSubmenu()->child_count();
    item->GetDelegate()->WillShowMenu(item);
    if (old_count != item->GetSubmenu()->child_count()) {
      // If the number of children changed then we may need to add empty items.
      item->AddEmptyMenus();
    }
  }
  bool prefer_leading =
      state_.open_leading.empty() ? true : state_.open_leading.back();
  bool resulting_direction;
  gfx::Rect bounds = MenuItemView::IsBubble(state_.anchor) ?
      CalculateBubbleMenuBounds(item, prefer_leading, &resulting_direction) :
      CalculateMenuBounds(item, prefer_leading, &resulting_direction);
  state_.open_leading.push_back(resulting_direction);
  bool do_capture = (!did_capture_ && blocking_run_);
  showing_submenu_ = true;
  if (show) {
    // Menus are the only place using kGroupingPropertyKey, so any value (other
    // than 0) is fine.
    const int kGroupingId = 1001;
    item->GetSubmenu()->ShowAt(owner_, bounds, do_capture);
    item->GetSubmenu()->GetWidget()->SetNativeWindowProperty(
        TooltipManager::kGroupingPropertyKey,
        reinterpret_cast<void*>(kGroupingId));
  } else {
    item->GetSubmenu()->Reposition(bounds);
  }
  showing_submenu_ = false;
}

void MenuController::MenuChildrenChanged(MenuItemView* item) {
  DCHECK(item);
  // Menu shouldn't be updated during drag operation.
  DCHECK(!GetActiveMouseView());

  // If the current item or pending item is a descendant of the item
  // that changed, move the selection back to the changed item.
  const MenuItemView* ancestor = state_.item;
  while (ancestor && ancestor != item)
    ancestor = ancestor->GetParentMenuItem();
  if (!ancestor) {
    ancestor = pending_state_.item;
    while (ancestor && ancestor != item)
      ancestor = ancestor->GetParentMenuItem();
    if (!ancestor)
      return;
  }
  SetSelection(item, SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
  if (item->HasSubmenu())
    OpenMenuImpl(item, false);
}

void MenuController::BuildPathsAndCalculateDiff(
    MenuItemView* old_item,
    MenuItemView* new_item,
    std::vector<MenuItemView*>* old_path,
    std::vector<MenuItemView*>* new_path,
    size_t* first_diff_at) {
  DCHECK(old_path && new_path && first_diff_at);
  BuildMenuItemPath(old_item, old_path);
  BuildMenuItemPath(new_item, new_path);

  size_t common_size = std::min(old_path->size(), new_path->size());

  // Find the first difference between the two paths, when the loop
  // returns, diff_i is the first index where the two paths differ.
  for (size_t i = 0; i < common_size; ++i) {
    if ((*old_path)[i] != (*new_path)[i]) {
      *first_diff_at = i;
      return;
    }
  }

  *first_diff_at = common_size;
}

void MenuController::BuildMenuItemPath(MenuItemView* item,
                                       std::vector<MenuItemView*>* path) {
  if (!item)
    return;
  BuildMenuItemPath(item->GetParentMenuItem(), path);
  path->push_back(item);
}

void MenuController::StartShowTimer() {
  show_timer_.Start(FROM_HERE,
                    TimeDelta::FromMilliseconds(menu_config_.show_delay),
                    this, &MenuController::CommitPendingSelection);
}

void MenuController::StopShowTimer() {
  show_timer_.Stop();
}

void MenuController::StartCancelAllTimer() {
  cancel_all_timer_.Start(FROM_HERE,
                          TimeDelta::FromMilliseconds(kCloseOnExitTime),
                          this, &MenuController::CancelAll);
}

void MenuController::StopCancelAllTimer() {
  cancel_all_timer_.Stop();
}

gfx::Rect MenuController::CalculateMenuBounds(MenuItemView* item,
                                              bool prefer_leading,
                                              bool* is_leading) {
  DCHECK(item);

  SubmenuView* submenu = item->GetSubmenu();
  DCHECK(submenu);

  gfx::Size pref = submenu->GetScrollViewContainer()->GetPreferredSize();

  // Don't let the menu go too wide.
  pref.set_width(std::min(pref.width(),
                            item->GetDelegate()->GetMaxWidthForMenu(item)));
  if (!state_.monitor_bounds.IsEmpty())
    pref.set_width(std::min(pref.width(), state_.monitor_bounds.width()));

  // Assume we can honor prefer_leading.
  *is_leading = prefer_leading;

  int x, y;

  const MenuConfig& menu_config = item->GetMenuConfig();

  if (!item->GetParentMenuItem()) {
    // First item, position relative to initial location.
    x = state_.initial_bounds.x();

    // Offsets for context menu prevent menu items being selected by
    // simply opening the menu (bug 142992).
    if (menu_config.offset_context_menus && state_.context_menu)
      x += 1;

    y = state_.initial_bounds.bottom();
    if (state_.anchor == MENU_ANCHOR_TOPRIGHT) {
      x = x + state_.initial_bounds.width() - pref.width();
      if (menu_config.offset_context_menus && state_.context_menu)
        x -= 1;
    } else if (state_.anchor == MENU_ANCHOR_BOTTOMCENTER) {
      x = x - (pref.width() - state_.initial_bounds.width()) / 2;
      if (pref.height() >
          state_.initial_bounds.y() + kCenteredContextMenuYOffset) {
        // Menu does not fit above the anchor. We move it to below.
        y = state_.initial_bounds.y() - kCenteredContextMenuYOffset;
      } else {
        y = std::max(0, state_.initial_bounds.y() - pref.height()) +
            kCenteredContextMenuYOffset;
      }
    }

    if (!state_.monitor_bounds.IsEmpty() &&
        y + pref.height() > state_.monitor_bounds.bottom()) {
      // The menu doesn't fit fully below the button on the screen. The menu
      // position with respect to the bounds will be preserved if it has
      // already been drawn. When the requested positioning is below the bounds
      // it will shrink the menu to make it fit below.
      // If the requested positioning is best fit, it will first try to fit the
      // menu below. If that does not fit it will try to place it above. If
      // that will not fit it will place it at the bottom of the work area and
      // moving it off the initial_bounds region to avoid overlap.
      // In all other requested position styles it will be flipped above and
      // the height will be shrunken to the usable height.
      if (item->actual_menu_position() == MenuItemView::POSITION_BELOW_BOUNDS) {
        pref.set_height(std::min(pref.height(),
                                 state_.monitor_bounds.bottom() - y));
      } else if (item->actual_menu_position() ==
                 MenuItemView::POSITION_BEST_FIT) {
        MenuItemView::MenuPosition orientation =
            MenuItemView::POSITION_BELOW_BOUNDS;
        if (state_.monitor_bounds.height() < pref.height()) {
          // Handle very tall menus.
          pref.set_height(state_.monitor_bounds.height());
          y = state_.monitor_bounds.y();
        } else if (state_.monitor_bounds.y() + pref.height() <
            state_.initial_bounds.y()) {
          // Flipping upwards if there is enough space.
          y = state_.initial_bounds.y() - pref.height();
          orientation = MenuItemView::POSITION_ABOVE_BOUNDS;
        } else {
          // It is allowed to move the menu a bit around in order to get the
          // best fit and to avoid showing scroll elements.
          y = state_.monitor_bounds.bottom() - pref.height();
        }
        if (orientation == MenuItemView::POSITION_BELOW_BOUNDS) {
          // The menu should never overlap the owning button. So move it.
          // We use the anchor view style to determine the preferred position
          // relative to the owning button.
          if (state_.anchor == MENU_ANCHOR_TOPLEFT) {
            // The menu starts with the same x coordinate as the owning button.
            if (x + state_.initial_bounds.width() + pref.width() >
                state_.monitor_bounds.right())
              x -= pref.width();  // Move the menu to the left of the button.
            else
              x += state_.initial_bounds.width(); // Move the menu right.
          } else {
            // The menu should end with the same x coordinate as the owning
            // button.
            if (state_.monitor_bounds.x() >
                state_.initial_bounds.x() - pref.width())
              x = state_.initial_bounds.right();  // Move right of the button.
            else
              x = state_.initial_bounds.x() - pref.width(); // Move left.
          }
        }
        item->set_actual_menu_position(orientation);
      } else {
        pref.set_height(std::min(pref.height(),
            state_.initial_bounds.y() - state_.monitor_bounds.y()));
        y = state_.initial_bounds.y() - pref.height();
        item->set_actual_menu_position(MenuItemView::POSITION_ABOVE_BOUNDS);
      }
    } else if (item->actual_menu_position() ==
               MenuItemView::POSITION_ABOVE_BOUNDS) {
      pref.set_height(std::min(pref.height(),
          state_.initial_bounds.y() - state_.monitor_bounds.y()));
      y = state_.initial_bounds.y() - pref.height();
    } else {
      item->set_actual_menu_position(MenuItemView::POSITION_BELOW_BOUNDS);
    }
    if (state_.monitor_bounds.width() != 0 &&
        menu_config.offset_context_menus && state_.context_menu) {
      if (x + pref.width() > state_.monitor_bounds.right())
        x = state_.initial_bounds.x() - pref.width() - 1;
      if (x < state_.monitor_bounds.x())
        x = state_.monitor_bounds.x();
    }
  } else {
    // Not the first menu; position it relative to the bounds of the menu
    // item.
    gfx::Point item_loc;
    View::ConvertPointToScreen(item, &item_loc);

    // We must make sure we take into account the UI layout. If the layout is
    // RTL, then a 'leading' menu is positioned to the left of the parent menu
    // item and not to the right.
    bool layout_is_rtl = base::i18n::IsRTL();
    bool create_on_the_right = (prefer_leading && !layout_is_rtl) ||
                               (!prefer_leading && layout_is_rtl);
    int submenu_horizontal_inset = menu_config.submenu_horizontal_inset;

    if (create_on_the_right) {
      x = item_loc.x() + item->width() - submenu_horizontal_inset;
      if (state_.monitor_bounds.width() != 0 &&
          x + pref.width() > state_.monitor_bounds.right()) {
        if (layout_is_rtl)
          *is_leading = true;
        else
          *is_leading = false;
        x = item_loc.x() - pref.width() + submenu_horizontal_inset;
      }
    } else {
      x = item_loc.x() - pref.width() + submenu_horizontal_inset;
      if (state_.monitor_bounds.width() != 0 && x < state_.monitor_bounds.x()) {
        if (layout_is_rtl)
          *is_leading = false;
        else
          *is_leading = true;
        x = item_loc.x() + item->width() - submenu_horizontal_inset;
      }
    }
    y = item_loc.y() - menu_config.menu_vertical_border_size;
    if (state_.monitor_bounds.width() != 0) {
      pref.set_height(std::min(pref.height(), state_.monitor_bounds.height()));
      if (y + pref.height() > state_.monitor_bounds.bottom())
        y = state_.monitor_bounds.bottom() - pref.height();
      if (y < state_.monitor_bounds.y())
        y = state_.monitor_bounds.y();
    }
  }

  if (state_.monitor_bounds.width() != 0) {
    if (x + pref.width() > state_.monitor_bounds.right())
      x = state_.monitor_bounds.right() - pref.width();
    if (x < state_.monitor_bounds.x())
      x = state_.monitor_bounds.x();
  }
  return gfx::Rect(x, y, pref.width(), pref.height());
}

gfx::Rect MenuController::CalculateBubbleMenuBounds(MenuItemView* item,
                                                    bool prefer_leading,
                                                    bool* is_leading) {
  DCHECK(item);
  DCHECK(!item->GetParentMenuItem());

  // Assume we can honor prefer_leading.
  *is_leading = prefer_leading;

  SubmenuView* submenu = item->GetSubmenu();
  DCHECK(submenu);

  gfx::Size pref = submenu->GetScrollViewContainer()->GetPreferredSize();
  const gfx::Rect& owner_bounds = pending_state_.initial_bounds;

  // First the size gets reduced to the possible space.
  if (!state_.monitor_bounds.IsEmpty()) {
    int max_width = state_.monitor_bounds.width();
    int max_height = state_.monitor_bounds.height();
    // In case of bubbles, the maximum width is limited by the space
    // between the display corner and the target area + the tip size.
    if (state_.anchor == MENU_ANCHOR_BUBBLE_LEFT) {
      max_width = owner_bounds.x() - state_.monitor_bounds.x() +
                  kBubbleTipSizeLeftRight;
    } else if (state_.anchor == MENU_ANCHOR_BUBBLE_RIGHT) {
      max_width = state_.monitor_bounds.right() - owner_bounds.right() +
                  kBubbleTipSizeLeftRight;
    } else if (state_.anchor == MENU_ANCHOR_BUBBLE_ABOVE) {
      max_height = owner_bounds.y() - state_.monitor_bounds.y() +
                   kBubbleTipSizeTopBottom;
    } else if (state_.anchor == MENU_ANCHOR_BUBBLE_BELOW) {
      max_height = state_.monitor_bounds.bottom() - owner_bounds.bottom() +
                   kBubbleTipSizeTopBottom;
    }
    // The space for the menu to cover should never get empty.
    DCHECK_GE(max_width, kBubbleTipSizeLeftRight);
    DCHECK_GE(max_height, kBubbleTipSizeTopBottom);
    pref.set_width(std::min(pref.width(), max_width));
    pref.set_height(std::min(pref.height(), max_height));
  }
  // Also make sure that the menu does not go too wide.
  pref.set_width(std::min(pref.width(),
                          item->GetDelegate()->GetMaxWidthForMenu(item)));

  int x, y;
  if (state_.anchor == MENU_ANCHOR_BUBBLE_ABOVE ||
      state_.anchor == MENU_ANCHOR_BUBBLE_BELOW) {
    if (state_.anchor == MENU_ANCHOR_BUBBLE_ABOVE)
      y = owner_bounds.y() - pref.height() + kBubbleTipSizeTopBottom;
    else
      y = owner_bounds.bottom() - kBubbleTipSizeTopBottom;

    x = owner_bounds.CenterPoint().x() - pref.width() / 2;
    int x_old = x;
    if (x < state_.monitor_bounds.x()) {
      x = state_.monitor_bounds.x();
    } else if (x + pref.width() > state_.monitor_bounds.right()) {
      x = state_.monitor_bounds.right() - pref.width();
    }
    submenu->GetScrollViewContainer()->SetBubbleArrowOffset(
        pref.width() / 2 - x + x_old);
  } else {
    if (state_.anchor == MENU_ANCHOR_BUBBLE_RIGHT)
      x = owner_bounds.right() - kBubbleTipSizeLeftRight;
    else
      x = owner_bounds.x() - pref.width() + kBubbleTipSizeLeftRight;

    y = owner_bounds.CenterPoint().y() - pref.height() / 2;
    int y_old = y;
    if (y < state_.monitor_bounds.y()) {
      y = state_.monitor_bounds.y();
    } else if (y + pref.height() > state_.monitor_bounds.bottom()) {
      y = state_.monitor_bounds.bottom() - pref.height();
    }
    submenu->GetScrollViewContainer()->SetBubbleArrowOffset(
        pref.height() / 2 - y + y_old);
  }
  return gfx::Rect(x, y, pref.width(), pref.height());
}

// static
int MenuController::MenuDepth(MenuItemView* item) {
  return item ? (MenuDepth(item->GetParentMenuItem()) + 1) : 0;
}

void MenuController::IncrementSelection(int delta) {
  MenuItemView* item = pending_state_.item;
  DCHECK(item);
  if (pending_state_.submenu_open && item->HasSubmenu() &&
      item->GetSubmenu()->IsShowing()) {
    // A menu is selected and open, but none of its children are selected,
    // select the first menu item.
    if (item->GetSubmenu()->GetMenuItemCount()) {
      SetSelection(item->GetSubmenu()->GetMenuItemAt(0), SELECTION_DEFAULT);
      return;
    }
  }

  if (item->has_children()) {
    CustomButton* button = GetFirstHotTrackedView(item);
    if (button) {
      button->SetHotTracked(false);
      View* to_make_hot = GetNextFocusableView(item, button, delta == 1);
      CustomButton* button_hot = CustomButton::AsCustomButton(to_make_hot);
      if (button_hot) {
        button_hot->SetHotTracked(true);
        return;
      }
    } else {
      View* to_make_hot = GetInitialFocusableView(item, delta == 1);
      CustomButton* button_hot = CustomButton::AsCustomButton(to_make_hot);
      if (button_hot) {
        button_hot->SetHotTracked(true);
        return;
      }
    }
  }

  MenuItemView* parent = item->GetParentMenuItem();
  if (parent) {
    int parent_count = parent->GetSubmenu()->GetMenuItemCount();
    if (parent_count > 1) {
      for (int i = 0; i < parent_count; ++i) {
        if (parent->GetSubmenu()->GetMenuItemAt(i) == item) {
          MenuItemView* to_select =
              FindNextSelectableMenuItem(parent, i, delta);
          if (!to_select)
            break;
          SetSelection(to_select, SELECTION_DEFAULT);
          View* to_make_hot = GetInitialFocusableView(to_select, delta == 1);
          CustomButton* button_hot = CustomButton::AsCustomButton(to_make_hot);
          if (button_hot)
            button_hot->SetHotTracked(true);
          break;
        }
      }
    }
  }
}

MenuItemView* MenuController::FindNextSelectableMenuItem(MenuItemView* parent,
                                                         int index,
                                                         int delta) {
  int start_index = index;
  int parent_count = parent->GetSubmenu()->GetMenuItemCount();
  // Loop through the menu items skipping any invisible menus. The loop stops
  // when we wrap or find a visible child.
  do {
    index = (index + delta + parent_count) % parent_count;
    if (index == start_index)
      return NULL;
    MenuItemView* child = parent->GetSubmenu()->GetMenuItemAt(index);
    if (child->visible())
      return child;
  } while (index != start_index);
  return NULL;
}

void MenuController::OpenSubmenuChangeSelectionIfCan() {
  MenuItemView* item = pending_state_.item;
  if (item->HasSubmenu() && item->enabled()) {
    if (item->GetSubmenu()->GetMenuItemCount() > 0) {
      SetSelection(item->GetSubmenu()->GetMenuItemAt(0),
                   SELECTION_UPDATE_IMMEDIATELY);
    } else {
      // No menu items, just show the sub-menu.
      SetSelection(item, SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
    }
  }
}

void MenuController::CloseSubmenu() {
  MenuItemView* item = state_.item;
  DCHECK(item);
  if (!item->GetParentMenuItem())
    return;
  if (item->HasSubmenu() && item->GetSubmenu()->IsShowing())
    SetSelection(item, SELECTION_UPDATE_IMMEDIATELY);
  else if (item->GetParentMenuItem()->GetParentMenuItem())
    SetSelection(item->GetParentMenuItem(), SELECTION_UPDATE_IMMEDIATELY);
}

MenuController::SelectByCharDetails MenuController::FindChildForMnemonic(
    MenuItemView* parent,
    base::char16 key,
    bool (*match_function)(MenuItemView* menu, base::char16 mnemonic)) {
  SubmenuView* submenu = parent->GetSubmenu();
  DCHECK(submenu);
  SelectByCharDetails details;

  for (int i = 0, menu_item_count = submenu->GetMenuItemCount();
       i < menu_item_count; ++i) {
    MenuItemView* child = submenu->GetMenuItemAt(i);
    if (child->enabled() && child->visible()) {
      if (child == pending_state_.item)
        details.index_of_item = i;
      if (match_function(child, key)) {
        if (details.first_match == -1)
          details.first_match = i;
        else
          details.has_multiple = true;
        if (details.next_match == -1 && details.index_of_item != -1 &&
            i > details.index_of_item)
          details.next_match = i;
      }
    }
  }
  return details;
}

bool MenuController::AcceptOrSelect(MenuItemView* parent,
                                    const SelectByCharDetails& details) {
  // This should only be invoked if there is a match.
  DCHECK(details.first_match != -1);
  DCHECK(parent->HasSubmenu());
  SubmenuView* submenu = parent->GetSubmenu();
  DCHECK(submenu);
  if (!details.has_multiple) {
    // There's only one match, activate it (or open if it has a submenu).
    if (submenu->GetMenuItemAt(details.first_match)->HasSubmenu()) {
      SetSelection(submenu->GetMenuItemAt(details.first_match),
                   SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);
    } else {
      Accept(submenu->GetMenuItemAt(details.first_match), 0);
      return true;
    }
  } else if (details.index_of_item == -1 || details.next_match == -1) {
    SetSelection(submenu->GetMenuItemAt(details.first_match),
                 SELECTION_DEFAULT);
  } else {
    SetSelection(submenu->GetMenuItemAt(details.next_match),
                 SELECTION_DEFAULT);
  }
  return false;
}

bool MenuController::SelectByChar(base::char16 character) {
  base::char16 char_array[] = { character, 0 };
  base::char16 key = base::i18n::ToLower(char_array)[0];
  MenuItemView* item = pending_state_.item;
  if (!item->HasSubmenu() || !item->GetSubmenu()->IsShowing())
    item = item->GetParentMenuItem();
  DCHECK(item);
  DCHECK(item->HasSubmenu());
  DCHECK(item->GetSubmenu());
  if (item->GetSubmenu()->GetMenuItemCount() == 0)
    return false;

  // Look for matches based on mnemonic first.
  SelectByCharDetails details =
      FindChildForMnemonic(item, key, &MatchesMnemonic);
  if (details.first_match != -1)
    return AcceptOrSelect(item, details);

  if (is_combobox_) {
    item->GetSubmenu()->GetTextInputClient()->InsertChar(character, 0);
  } else {
    // If no mnemonics found, look at first character of titles.
    details = FindChildForMnemonic(item, key, &TitleMatchesMnemonic);
    if (details.first_match != -1)
      return AcceptOrSelect(item, details);
  }

  return false;
}

void MenuController::RepostEvent(SubmenuView* source,
                                 const ui::LocatedEvent& event) {
  if (!event.IsMouseEvent()) {
    // TODO(rbyers): Gesture event repost is tricky to get right
    // crbug.com/170987.
    DCHECK(event.IsGestureEvent());
    return;
  }

#if defined(OS_WIN)
  if (!state_.item) {
    // We some times get an event after closing all the menus. Ignore it. Make
    // sure the menu is in fact not visible. If the menu is visible, then
    // we're in a bad state where we think the menu isn't visibile but it is.
    DCHECK(!source->GetWidget()->IsVisible());
    return;
  }

  state_.item->GetRootMenuItem()->GetSubmenu()->ReleaseCapture();
#endif

  gfx::Point screen_loc(event.location());
  View::ConvertPointToScreen(source->GetScrollViewContainer(), &screen_loc);
  gfx::NativeView native_view = source->GetWidget()->GetNativeView();
  if (!native_view)
    return;

  gfx::Screen* screen = gfx::Screen::GetScreenFor(native_view);
  gfx::NativeWindow window = screen->GetWindowAtScreenPoint(screen_loc);

#if defined(OS_WIN)
  // Convert screen_loc to pixels for the Win32 API's like WindowFromPoint,
  // PostMessage/SendMessage to work correctly. These API's expect the
  // coordinates to be in pixels.
  // PostMessage() to metro windows isn't allowed (access will be denied). Don't
  // try to repost with Win32 if the window under the mouse press is in metro.
  if (!ViewsDelegate::views_delegate ||
      !ViewsDelegate::views_delegate->IsWindowInMetro(window)) {
    gfx::Point screen_loc_pixels = gfx::win::DIPToScreenPoint(screen_loc);
    HWND target_window = window ? HWNDForNativeWindow(window) :
                                  WindowFromPoint(screen_loc_pixels.ToPOINT());
    HWND source_window = HWNDForNativeView(native_view);
    if (!target_window || !source_window ||
        GetWindowThreadProcessId(source_window, NULL) !=
        GetWindowThreadProcessId(target_window, NULL)) {
      // Even though we have mouse capture, windows generates a mouse event if
      // the other window is in a separate thread. Only repost an event if
      // |target_window| and |source_window| were created on the same thread,
      // else double events can occur and lead to bad behavior.
      return;
    }

    // Determine whether the click was in the client area or not.
    // NOTE: WM_NCHITTEST coordinates are relative to the screen.
    LPARAM coords = MAKELPARAM(screen_loc_pixels.x(), screen_loc_pixels.y());
    LRESULT nc_hit_result = SendMessage(target_window, WM_NCHITTEST, 0, coords);
    const bool client_area = nc_hit_result == HTCLIENT;

    // TODO(sky): this isn't right. The event to generate should correspond with
    // the event we just got. MouseEvent only tells us what is down, which may
    // differ. Need to add ability to get changed button from MouseEvent.
    int event_type;
    int flags = event.flags();
    if (flags & ui::EF_LEFT_MOUSE_BUTTON) {
      event_type = client_area ? WM_LBUTTONDOWN : WM_NCLBUTTONDOWN;
    } else if (flags & ui::EF_MIDDLE_MOUSE_BUTTON) {
      event_type = client_area ? WM_MBUTTONDOWN : WM_NCMBUTTONDOWN;
    } else if (flags & ui::EF_RIGHT_MOUSE_BUTTON) {
      event_type = client_area ? WM_RBUTTONDOWN : WM_NCRBUTTONDOWN;
    } else {
      NOTREACHED();
      return;
    }

    int window_x = screen_loc_pixels.x();
    int window_y = screen_loc_pixels.y();
    if (client_area) {
      POINT pt = { window_x, window_y };
      ScreenToClient(target_window, &pt);
      window_x = pt.x;
      window_y = pt.y;
    }

    WPARAM target = client_area ? event.native_event().wParam : nc_hit_result;
    LPARAM window_coords = MAKELPARAM(window_x, window_y);
    PostMessage(target_window, event_type, target, window_coords);
    return;
  }
#endif
  // Non-Windows Aura or |window| is in metro mode.
  if (!window)
    return;

  message_loop_->RepostEventToWindow(event, window, screen_loc);
}

void MenuController::SetDropMenuItem(
    MenuItemView* new_target,
    MenuDelegate::DropPosition new_position) {
  if (new_target == drop_target_ && new_position == drop_position_)
    return;

  if (drop_target_) {
    drop_target_->GetParentMenuItem()->GetSubmenu()->SetDropMenuItem(
        NULL, MenuDelegate::DROP_NONE);
  }
  drop_target_ = new_target;
  drop_position_ = new_position;
  if (drop_target_) {
    drop_target_->GetParentMenuItem()->GetSubmenu()->SetDropMenuItem(
        drop_target_, drop_position_);
  }
}

void MenuController::UpdateScrolling(const MenuPart& part) {
  if (!part.is_scroll() && !scroll_task_.get())
    return;

  if (!scroll_task_.get())
    scroll_task_.reset(new MenuScrollTask());
  scroll_task_->Update(part);
}

void MenuController::StopScrolling() {
  scroll_task_.reset(NULL);
}

void MenuController::UpdateActiveMouseView(SubmenuView* event_source,
                                           const ui::MouseEvent& event,
                                           View* target_menu) {
  View* target = NULL;
  gfx::Point target_menu_loc(event.location());
  if (target_menu && target_menu->has_children()) {
    // Locate the deepest child view to send events to.  This code assumes we
    // don't have to walk up the tree to find a view interested in events. This
    // is currently true for the cases we are embedding views, but if we embed
    // more complex hierarchies it'll need to change.
    View::ConvertPointToScreen(event_source->GetScrollViewContainer(),
                               &target_menu_loc);
    View::ConvertPointFromScreen(target_menu, &target_menu_loc);
    target = target_menu->GetEventHandlerForPoint(target_menu_loc);
    if (target == target_menu || !target->enabled())
      target = NULL;
  }
  View* active_mouse_view = GetActiveMouseView();
  if (target != active_mouse_view) {
    SendMouseCaptureLostToActiveView();
    active_mouse_view = target;
    SetActiveMouseView(active_mouse_view);
    if (active_mouse_view) {
      gfx::Point target_point(target_menu_loc);
      View::ConvertPointToTarget(
          target_menu, active_mouse_view, &target_point);
      ui::MouseEvent mouse_entered_event(ui::ET_MOUSE_ENTERED,
                                         target_point, target_point,
                                         0, 0);
      active_mouse_view->OnMouseEntered(mouse_entered_event);

      ui::MouseEvent mouse_pressed_event(ui::ET_MOUSE_PRESSED,
                                         target_point, target_point,
                                         event.flags(),
                                         event.changed_button_flags());
      active_mouse_view->OnMousePressed(mouse_pressed_event);
    }
  }

  if (active_mouse_view) {
    gfx::Point target_point(target_menu_loc);
    View::ConvertPointToTarget(target_menu, active_mouse_view, &target_point);
    ui::MouseEvent mouse_dragged_event(ui::ET_MOUSE_DRAGGED,
                                       target_point, target_point,
                                       event.flags(),
                                       event.changed_button_flags());
    active_mouse_view->OnMouseDragged(mouse_dragged_event);
  }
}

void MenuController::SendMouseReleaseToActiveView(SubmenuView* event_source,
                                                  const ui::MouseEvent& event) {
  View* active_mouse_view = GetActiveMouseView();
  if (!active_mouse_view)
    return;

  gfx::Point target_loc(event.location());
  View::ConvertPointToScreen(event_source->GetScrollViewContainer(),
                             &target_loc);
  View::ConvertPointFromScreen(active_mouse_view, &target_loc);
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, target_loc, target_loc,
                               event.flags(), event.changed_button_flags());
  // Reset active mouse view before sending mouse released. That way if it calls
  // back to us, we aren't in a weird state.
  SetActiveMouseView(NULL);
  active_mouse_view->OnMouseReleased(release_event);
}

void MenuController::SendMouseCaptureLostToActiveView() {
  View* active_mouse_view = GetActiveMouseView();
  if (!active_mouse_view)
    return;

  // Reset the active_mouse_view_ before sending mouse capture lost. That way if
  // it calls back to us, we aren't in a weird state.
  SetActiveMouseView(NULL);
  active_mouse_view->OnMouseCaptureLost();
}

void MenuController::SetActiveMouseView(View* view) {
  if (view)
    ViewStorage::GetInstance()->StoreView(active_mouse_view_id_, view);
  else
    ViewStorage::GetInstance()->RemoveView(active_mouse_view_id_);
}

View* MenuController::GetActiveMouseView() {
  return ViewStorage::GetInstance()->RetrieveView(active_mouse_view_id_);
}

void MenuController::SetExitType(ExitType type) {
  exit_type_ = type;
  // Exit nested message loops as soon as possible. We do this as
  // MessagePumpDispatcher is only invoked before native events, which means
  // its entirely possible for a Widget::CloseNow() task to be processed before
  // the next native message. We quite the nested message loop as soon as
  // possible to avoid having deleted views classes (such as widgets and
  // rootviews) on the stack when the nested message loop stops.
  //
  // It's safe to invoke QuitNestedMessageLoop() multiple times, it only effects
  // the current loop.
  bool quit_now = message_loop_->ShouldQuitNow() && exit_type_ != EXIT_NONE &&
      message_loop_depth_;
  if (quit_now)
    TerminateNestedMessageLoop();
}

void MenuController::TerminateNestedMessageLoop() {
  message_loop_->QuitNow();
}

void MenuController::HandleMouseLocation(SubmenuView* source,
                                         const gfx::Point& mouse_location) {
  if (showing_submenu_)
    return;

  // Ignore mouse events if we're closing the menu.
  if (exit_type_ != EXIT_NONE)
    return;

  MenuPart part = GetMenuPart(source, mouse_location);

  UpdateScrolling(part);

  if (!blocking_run_)
    return;

  if (part.type == MenuPart::NONE && ShowSiblingMenu(source, mouse_location))
    return;

  if (part.type == MenuPart::MENU_ITEM && part.menu) {
    SetSelection(part.menu, SELECTION_OPEN_SUBMENU);
  } else if (!part.is_scroll() && pending_state_.item &&
             pending_state_.item->GetParentMenuItem() &&
             (!pending_state_.item->HasSubmenu() ||
              !pending_state_.item->GetSubmenu()->IsShowing())) {
    // On exit if the user hasn't selected an item with a submenu, move the
    // selection back to the parent menu item.
    SetSelection(pending_state_.item->GetParentMenuItem(),
                 SELECTION_OPEN_SUBMENU);
  }
}

gfx::Screen* MenuController::GetScreen() {
  Widget* root = owner_ ? owner_->GetTopLevelWidget() : NULL;
  return root ? gfx::Screen::GetScreenFor(root->GetNativeView())
              : gfx::Screen::GetNativeScreen();
}

}  // namespace views
