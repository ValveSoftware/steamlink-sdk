// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_

#include "build/build_config.h"

#include <list>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace base {
class MessagePumpDispatcher;
}
namespace gfx {
class Screen;
}
namespace ui {
class NativeTheme;
class OSExchangeData;
class ScopedEventDispatcher;
}
namespace views {

class MenuButton;
class MenuHostRootView;
class MenuItemView;
class MenuMessageLoop;
class MouseEvent;
class SubmenuView;
class View;

namespace internal {
class MenuControllerDelegate;
class MenuEventDispatcher;
class MenuMessagePumpDispatcher;
class MenuRunnerImpl;
}

// MenuController -------------------------------------------------------------

// MenuController is used internally by the various menu classes to manage
// showing, selecting and drag/drop for menus. All relevant events are
// forwarded to the MenuController from SubmenuView and MenuHost.
class VIEWS_EXPORT MenuController : public WidgetObserver {
 public:
  // Enumeration of how the menu should exit.
  enum ExitType {
    // Don't exit.
    EXIT_NONE,

    // All menus, including nested, should be exited.
    EXIT_ALL,

    // Only the outermost menu should be exited.
    EXIT_OUTERMOST,

    // This is set if the menu is being closed as the result of one of the menus
    // being destroyed.
    EXIT_DESTROYED
  };

  // If a menu is currently active, this returns the controller for it.
  static MenuController* GetActiveInstance();

  // Runs the menu at the specified location. If the menu was configured to
  // block, the selected item is returned. If the menu does not block this
  // returns NULL immediately.
  MenuItemView* Run(Widget* parent,
                    MenuButton* button,
                    MenuItemView* root,
                    const gfx::Rect& bounds,
                    MenuAnchorPosition position,
                    bool context_menu,
                    int* event_flags);

  // Whether or not Run blocks.
  bool IsBlockingRun() const { return blocking_run_; }

  // Whether or not drag operation is in progress.
  bool drag_in_progress() const { return drag_in_progress_; }

  // Returns the owner of child windows.
  // WARNING: this may be NULL.
  Widget* owner() { return owner_; }

  // Get the anchor position wich is used to show this menu.
  MenuAnchorPosition GetAnchorPosition() { return state_.anchor; }

  // Cancels the current Run. See ExitType for a description of what happens
  // with the various parameters.
  void Cancel(ExitType type);

  // An alternative to Cancel(EXIT_ALL) that can be used with a OneShotTimer.
  void CancelAll() { Cancel(EXIT_ALL); }

  // Returns the current exit type. This returns a value other than EXIT_NONE if
  // the menu is being canceled.
  ExitType exit_type() const { return exit_type_; }

  // Returns the time from the event which closed the menu - or 0.
  base::TimeDelta closing_event_time() const { return closing_event_time_; }

  void set_is_combobox(bool is_combobox) { is_combobox_ = is_combobox; }

  // Various events, forwarded from the submenu.
  //
  // NOTE: the coordinates of the events are in that of the
  // MenuScrollViewContainer.
  void OnMousePressed(SubmenuView* source, const ui::MouseEvent& event);
  void OnMouseDragged(SubmenuView* source, const ui::MouseEvent& event);
  void OnMouseReleased(SubmenuView* source, const ui::MouseEvent& event);
  void OnMouseMoved(SubmenuView* source, const ui::MouseEvent& event);
  void OnMouseEntered(SubmenuView* source, const ui::MouseEvent& event);
  bool OnMouseWheel(SubmenuView* source, const ui::MouseWheelEvent& event);
  void OnGestureEvent(SubmenuView* source, ui::GestureEvent* event);

  bool GetDropFormats(
      SubmenuView* source,
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats);
  bool AreDropTypesRequired(SubmenuView* source);
  bool CanDrop(SubmenuView* source, const ui::OSExchangeData& data);
  void OnDragEntered(SubmenuView* source, const ui::DropTargetEvent& event);
  int OnDragUpdated(SubmenuView* source, const ui::DropTargetEvent& event);
  void OnDragExited(SubmenuView* source);
  int OnPerformDrop(SubmenuView* source, const ui::DropTargetEvent& event);

  // Invoked from the scroll buttons of the MenuScrollViewContainer.
  void OnDragEnteredScrollButton(SubmenuView* source, bool is_up);
  void OnDragExitedScrollButton(SubmenuView* source);

  // Update the submenu's selection based on the current mouse location
  void UpdateSubmenuSelection(SubmenuView* source);

  // WidgetObserver overrides:
  virtual void OnWidgetDestroying(Widget* widget) OVERRIDE;

  // Only used for testing.
  static void TurnOffMenuSelectionHoldForTest();

 private:
  friend class internal::MenuEventDispatcher;
  friend class internal::MenuMessagePumpDispatcher;
  friend class internal::MenuRunnerImpl;
  friend class MenuControllerTest;
  friend class MenuHostRootView;
  friend class MenuItemView;
  friend class SubmenuView;

  class MenuScrollTask;

  struct SelectByCharDetails;

  // Values supplied to SetSelection.
  enum SetSelectionTypes {
    SELECTION_DEFAULT               = 0,

    // If set submenus are opened immediately, otherwise submenus are only
    // openned after a timer fires.
    SELECTION_UPDATE_IMMEDIATELY    = 1 << 0,

    // If set and the menu_item has a submenu, the submenu is shown.
    SELECTION_OPEN_SUBMENU          = 1 << 1,

    // SetSelection is being invoked as the result exiting or cancelling the
    // menu. This is used for debugging.
    SELECTION_EXIT                  = 1 << 2,
  };

  // Result type for SendAcceleratorToHotTrackedView
  enum SendAcceleratorResultType {
    // Accelerator is not sent because of no hot tracked views.
    ACCELERATOR_NOT_PROCESSED,

    // Accelerator is sent to the hot tracked views.
    ACCELERATOR_PROCESSED,

    // Same as above and the accelerator causes the exit of the menu.
    ACCELERATOR_PROCESSED_EXIT
  };

  // Tracks selection information.
  struct State {
    State();
    ~State();

    // The selected menu item.
    MenuItemView* item;

    // If item has a submenu this indicates if the submenu is showing.
    bool submenu_open;

    // Bounds passed to the run menu. Used for positioning the first menu.
    gfx::Rect initial_bounds;

    // Position of the initial menu.
    MenuAnchorPosition anchor;

    // The direction child menus have opened in.
    std::list<bool> open_leading;

    // Bounds for the monitor we're showing on.
    gfx::Rect monitor_bounds;

    // Is the current menu a context menu.
    bool context_menu;
  };

  // Used by GetMenuPart to indicate the menu part at a particular location.
  struct MenuPart {
    // Type of part.
    enum Type {
      NONE,
      MENU_ITEM,
      SCROLL_UP,
      SCROLL_DOWN
    };

    MenuPart() : type(NONE), menu(NULL), parent(NULL), submenu(NULL) {}

    // Convenience for testing type == SCROLL_DOWN or type == SCROLL_UP.
    bool is_scroll() const { return type == SCROLL_DOWN || type == SCROLL_UP; }

    // Type of part.
    Type type;

    // If type is MENU_ITEM, this is the menu item the mouse is over, otherwise
    // this is NULL.
    // NOTE: if type is MENU_ITEM and the mouse is not over a valid menu item
    //       but is over a menu (for example, the mouse is over a separator or
    //       empty menu), this is NULL and parent is the menu the mouse was
    //       clicked on.
    MenuItemView* menu;

    // If type is MENU_ITEM but the mouse is not over a menu item this is the
    // parent of the menu item the user clicked on. Otherwise this is NULL.
    MenuItemView* parent;

    // This is the submenu the mouse is over.
    SubmenuView* submenu;
  };

  // Sets the selection to |menu_item|. A value of NULL unselects
  // everything. |types| is a bitmask of |SetSelectionTypes|.
  //
  // Internally this updates pending_state_ immediatley. state_ is only updated
  // immediately if SELECTION_UPDATE_IMMEDIATELY is set. If
  // SELECTION_UPDATE_IMMEDIATELY is not set CommitPendingSelection is invoked
  // to show/hide submenus and update state_.
  void SetSelection(MenuItemView* menu_item, int types);

  void SetSelectionOnPointerDown(SubmenuView* source,
                                 const ui::LocatedEvent& event);
  void StartDrag(SubmenuView* source, const gfx::Point& location);

  // Key processing. The return value of this is returned from Dispatch.
  // In other words, if this returns false (which happens if escape was
  // pressed, or a matching mnemonic was found) the message loop returns.
  bool OnKeyDown(ui::KeyboardCode key_code);

  // Creates a MenuController. If |blocking| is true a nested message loop is
  // started in |Run|.
  MenuController(ui::NativeTheme* theme,
                 bool blocking,
                 internal::MenuControllerDelegate* delegate);

  virtual ~MenuController();

  // Runs the platform specific bits of the message loop. If |nested_menu| is
  // true we're being asked to run a menu from within a menu (eg a context
  // menu).
  void RunMessageLoop(bool nested_menu);

  // AcceleratorPressed is invoked on the hot tracked view if it exists.
  SendAcceleratorResultType SendAcceleratorToHotTrackedView();

  void UpdateInitialLocation(const gfx::Rect& bounds,
                             MenuAnchorPosition position,
                             bool context_menu);

  // Invoked when the user accepts the selected item. This is only used
  // when blocking. This schedules the loop to quit.
  void Accept(MenuItemView* item, int event_flags);

  bool ShowSiblingMenu(SubmenuView* source, const gfx::Point& mouse_location);

  // Shows a context menu for |menu_item| as a result of a located event if
  // appropriate. This is invoked on long press and releasing the right mouse
  // button. Returns whether a context menu was shown.
  bool ShowContextMenu(MenuItemView* menu_item,
                       SubmenuView* source,
                       const ui::LocatedEvent& event,
                       ui::MenuSourceType source_type);

  // Closes all menus, including any menus of nested invocations of Run.
  void CloseAllNestedMenus();

  // Gets the enabled menu item at the specified location.
  // If over_any_menu is non-null it is set to indicate whether the location
  // is over any menu. It is possible for this to return NULL, but
  // over_any_menu to be true. For example, the user clicked on a separator.
  MenuItemView* GetMenuItemAt(View* menu, int x, int y);

  // If there is an empty menu item at the specified location, it is returned.
  MenuItemView* GetEmptyMenuItemAt(View* source, int x, int y);

  // Returns true if the coordinate is over the scroll buttons of the
  // SubmenuView's MenuScrollViewContainer. If true is returned, part is set to
  // indicate which scroll button the coordinate is.
  bool IsScrollButtonAt(SubmenuView* source,
                        int x,
                        int y,
                        MenuPart::Type* part);

  // Returns the target for the mouse event. The coordinates are in terms of
  // source's scroll view container.
  MenuPart GetMenuPart(SubmenuView* source, const gfx::Point& source_loc);

  // Returns the target for mouse events. The search is done through |item| and
  // all its parents.
  MenuPart GetMenuPartByScreenCoordinateUsingMenu(MenuItemView* item,
                                                  const gfx::Point& screen_loc);

  // Implementation of GetMenuPartByScreenCoordinate for a single menu. Returns
  // true if the supplied SubmenuView contains the location in terms of the
  // screen. If it does, part is set appropriately and true is returned.
  bool GetMenuPartByScreenCoordinateImpl(SubmenuView* menu,
                                         const gfx::Point& screen_loc,
                                         MenuPart* part);

  // Returns true if the SubmenuView contains the specified location. This does
  // NOT included the scroll buttons, only the submenu view.
  bool DoesSubmenuContainLocation(SubmenuView* submenu,
                                  const gfx::Point& screen_loc);

  // Opens/Closes the necessary menus such that state_ matches that of
  // pending_state_. This is invoked if submenus are not opened immediately,
  // but after a delay.
  void CommitPendingSelection();

  // If item has a submenu, it is closed. This does NOT update the selection
  // in anyway.
  void CloseMenu(MenuItemView* item);

  // If item has a submenu, it is opened. This does NOT update the selection
  // in anyway.
  void OpenMenu(MenuItemView* item);

  // Implementation of OpenMenu. If |show| is true, this invokes show on the
  // menu, otherwise Reposition is invoked.
  void OpenMenuImpl(MenuItemView* item, bool show);

  // Invoked when the children of a menu change and the menu is showing.
  // This closes any submenus and resizes the submenu.
  void MenuChildrenChanged(MenuItemView* item);

  // Builds the paths of the two menu items into the two paths, and
  // sets first_diff_at to the location of the first difference between the
  // two paths.
  void BuildPathsAndCalculateDiff(MenuItemView* old_item,
                                  MenuItemView* new_item,
                                  std::vector<MenuItemView*>* old_path,
                                  std::vector<MenuItemView*>* new_path,
                                  size_t* first_diff_at);

  // Builds the path for the specified item.
  void BuildMenuItemPath(MenuItemView* item, std::vector<MenuItemView*>* path);

  // Starts/stops the timer that commits the pending state to state
  // (opens/closes submenus).
  void StartShowTimer();
  void StopShowTimer();

  // Starts/stops the timer cancel the menu. This is used during drag and
  // drop when the drop enters/exits the menu.
  void StartCancelAllTimer();
  void StopCancelAllTimer();

  // Calculates the bounds of the menu to show. is_leading is set to match the
  // direction the menu opened in.
  gfx::Rect CalculateMenuBounds(MenuItemView* item,
                                bool prefer_leading,
                                bool* is_leading);

  // Calculates the bubble bounds of the menu to show. is_leading is set to
  // match the direction the menu opened in.
  gfx::Rect CalculateBubbleMenuBounds(MenuItemView* item,
                                      bool prefer_leading,
                                      bool* is_leading);

  // Returns the depth of the menu.
  static int MenuDepth(MenuItemView* item);

  // Selects the next/previous menu item.
  void IncrementSelection(int delta);

  // Returns the next selectable child menu item of |parent| starting at |index|
  // and incrementing index by |delta|. If there are no more selected menu items
  // NULL is returned.
  MenuItemView* FindNextSelectableMenuItem(MenuItemView* parent,
                                           int index,
                                           int delta);

  // If the selected item has a submenu and it isn't currently open, the
  // the selection is changed such that the menu opens immediately.
  void OpenSubmenuChangeSelectionIfCan();

  // If possible, closes the submenu.
  void CloseSubmenu();

  // Returns details about which menu items match the mnemonic |key|.
  // |match_function| is used to determine which menus match.
  SelectByCharDetails FindChildForMnemonic(
      MenuItemView* parent,
      base::char16 key,
      bool (*match_function)(MenuItemView* menu, base::char16 mnemonic));

  // Selects or accepts the appropriate menu item based on |details|. Returns
  // true if |Accept| was invoked (which happens if there aren't multiple item
  // with the same mnemonic and the item to select does not have a submenu).
  bool AcceptOrSelect(MenuItemView* parent, const SelectByCharDetails& details);

  // Selects by mnemonic, and if that doesn't work tries the first character of
  // the title. Returns true if a match was selected and the menu should exit.
  bool SelectByChar(base::char16 key);

  // For Windows and Aura we repost an event for some events that dismiss
  // the context menu. The event is then reprocessed to cause its result
  // if the context menu had not been present.
  // On non-aura Windows, a new mouse event is generated and posted to
  // the window (if there is one) at the location of the event. On
  // aura, the event is reposted on the RootWindow.
  void RepostEvent(SubmenuView* source, const ui::LocatedEvent& event);

  // Sets the drop target to new_item.
  void SetDropMenuItem(MenuItemView* new_item,
                       MenuDelegate::DropPosition position);

  // Starts/stops scrolling as appropriate. part gives the part the mouse is
  // over.
  void UpdateScrolling(const MenuPart& part);

  // Stops scrolling.
  void StopScrolling();

  // Updates active mouse view from the location of the event and sends it
  // the appropriate events. This is used to send mouse events to child views so
  // that they react to click-drag-release as if the user clicked on the view
  // itself.
  void UpdateActiveMouseView(SubmenuView* event_source,
                             const ui::MouseEvent& event,
                             View* target_menu);

  // Sends a mouse release event to the current active mouse view and sets
  // it to null.
  void SendMouseReleaseToActiveView(SubmenuView* event_source,
                                    const ui::MouseEvent& event);

  // Sends a mouse capture lost event to the current active mouse view and sets
  // it to null.
  void SendMouseCaptureLostToActiveView();

  // Sets/gets the active mouse view. See UpdateActiveMouseView() for details.
  void SetActiveMouseView(View* view);
  View* GetActiveMouseView();

  // Sets exit type. Calling this can terminate the active nested message-loop.
  void SetExitType(ExitType type);

  // Terminates the current nested message-loop.
  void TerminateNestedMessageLoop();

  // Returns true if SetExitType() should quit the message loop.
  bool ShouldQuitNow() const;

  // Handles the mouse location event on the submenu |source|.
  void HandleMouseLocation(SubmenuView* source,
                           const gfx::Point& mouse_location);

  // Retrieve an appropriate Screen.
  gfx::Screen* GetScreen();

  // The active instance.
  static MenuController* active_instance_;

  // If true, Run blocks. If false, Run doesn't block and this is used for
  // drag and drop. Note that the semantics for drag and drop are slightly
  // different: cancel timer is kicked off any time the drag moves outside the
  // menu, mouse events do nothing...
  bool blocking_run_;

  // If true, we're showing.
  bool showing_;

  // Indicates what to exit.
  ExitType exit_type_;

  // Whether we did a capture. We do a capture only if we're blocking and
  // the mouse was down when Run.
  bool did_capture_;

  // As the user drags the mouse around pending_state_ changes immediately.
  // When the user stops moving/dragging the mouse (or clicks the mouse)
  // pending_state_ is committed to state_, potentially resulting in
  // opening or closing submenus. This gives a slight delayed effect to
  // submenus as the user moves the mouse around. This is done so that as the
  // user moves the mouse all submenus don't immediately pop.
  State pending_state_;
  State state_;

  // If the user accepted the selection, this is the result.
  MenuItemView* result_;

  // The event flags when the user selected the menu.
  int accept_event_flags_;

  // If not empty, it means we're nested. When Run is invoked from within
  // Run, the current state (state_) is pushed onto menu_stack_. This allows
  // MenuController to restore the state when the nested run returns.
  std::list<State> menu_stack_;

  // As the mouse moves around submenus are not opened immediately. Instead
  // they open after this timer fires.
  base::OneShotTimer<MenuController> show_timer_;

  // Used to invoke CancelAll(). This is used during drag and drop to hide the
  // menu after the mouse moves out of the of the menu. This is necessitated by
  // the lack of an ability to detect when the drag has completed from the drop
  // side.
  base::OneShotTimer<MenuController> cancel_all_timer_;

  // Drop target.
  MenuItemView* drop_target_;
  MenuDelegate::DropPosition drop_position_;

  // Owner of child windows.
  // WARNING: this may be NULL.
  Widget* owner_;

  // Indicates a possible drag operation.
  bool possible_drag_;

  // True when drag operation is in progress.
  bool drag_in_progress_;

  // Location the mouse was pressed at. Used to detect d&d.
  gfx::Point press_pt_;

  // We get a slew of drag updated messages as the mouse is over us. To avoid
  // continually processing whether we can drop, we cache the coordinates.
  bool valid_drop_coordinates_;
  gfx::Point drop_pt_;
  int last_drop_operation_;

  // If true, we're in the middle of invoking ShowAt on a submenu.
  bool showing_submenu_;

  // Task for scrolling the menu. If non-null indicates a scroll is currently
  // underway.
  scoped_ptr<MenuScrollTask> scroll_task_;

  MenuButton* menu_button_;

  // ViewStorage id used to store the view mouse drag events are forwarded to.
  // See UpdateActiveMouseView() for details.
  const int active_mouse_view_id_;

  internal::MenuControllerDelegate* delegate_;

  // How deep we are in nested message loops. This should be at most 2 (when
  // showing a context menu from a menu).
  int message_loop_depth_;

  views::MenuConfig menu_config_;

  // The timestamp of the event which closed the menu - or 0 otherwise.
  base::TimeDelta closing_event_time_;

  // Time when the menu is first shown.
  base::TimeTicks menu_start_time_;

  // If a mouse press triggered this menu, this will have its location (in
  // screen coordinates). Otherwise this will be (0, 0).
  gfx::Point menu_start_mouse_press_loc_;

  // Controls behavior differences between a combobox and other types of menu
  // (like a context menu).
  bool is_combobox_;

  // Set to true if the menu item was selected by touch.
  bool item_selected_by_touch_;

  scoped_ptr<MenuMessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MenuController);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_
