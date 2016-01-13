// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/views_export.h"

namespace base {
class TimeDelta;
}

namespace gfx {
class Rect;
}

namespace ui {
class MenuModel;
}

namespace views {

class MenuButton;
class MenuItemView;
class MenuModelAdapter;
class MenuRunnerHandler;
class Widget;

namespace internal {
class DisplayChangeListener;
class MenuRunnerImpl;
}

namespace test {
class MenuRunnerTestAPI;
}

// MenuRunner is responsible for showing (running) the menu and additionally
// owning the MenuItemView. RunMenuAt() runs a nested message loop. It is safe
// to delete MenuRunner at any point, but MenuRunner internally only deletes the
// MenuItemView *after* the nested message loop completes. If MenuRunner is
// deleted while the menu is showing the delegate of the menu is reset. This is
// done to ensure delegates aren't notified after they may have been deleted.
//
// NOTE: while you can delete a MenuRunner at any point, the nested message loop
// won't return immediately. This means if you delete the object that owns
// the MenuRunner while the menu is running, your object is effectively still
// on the stack. A return value of MENU_DELETED indicated this. In most cases
// if RunMenuAt() returns MENU_DELETED, you should return immediately.
//
// Similarly you should avoid creating MenuRunner on the stack. Doing so means
// MenuRunner may not be immediately destroyed if your object is destroyed,
// resulting in possible callbacks to your now deleted object. Instead you
// should define MenuRunner as a scoped_ptr in your class so that when your
// object is destroyed MenuRunner initiates the proper cleanup and ensures your
// object isn't accessed again.
class VIEWS_EXPORT MenuRunner {
 public:
  enum RunTypes {
    // The menu has mnemonics.
    HAS_MNEMONICS = 1 << 0,

    // The menu is a nested context menu. For example, click a folder on the
    // bookmark bar, then right click an entry to get its context menu.
    IS_NESTED     = 1 << 1,

    // Used for showing a menu during a drop operation. This does NOT block the
    // caller, instead the delegate is notified when the menu closes via the
    // DropMenuClosed method.
    FOR_DROP      = 1 << 2,

    // The menu is a context menu (not necessarily nested), for example right
    // click on a link on a website in the browser.
    CONTEXT_MENU  = 1 << 3,

    // The menu should behave like a Windows native Combobox dropdow menu.
    // This behavior includes accepting the pending item and closing on F4.
    COMBOBOX  = 1 << 4,
  };

  enum RunResult {
    // Indicates RunMenuAt is returning because the MenuRunner was deleted.
    MENU_DELETED,

    // Indicates RunMenuAt returned and MenuRunner was not deleted.
    NORMAL_EXIT
  };

  // Creates a new MenuRunner.
  explicit MenuRunner(ui::MenuModel* menu_model);
  explicit MenuRunner(MenuItemView* menu);
  ~MenuRunner();

  // Returns the menu.
  MenuItemView* GetMenu();

  // Takes ownership of |menu|, deleting it when MenuRunner is deleted. You
  // only need call this if you create additional menus from
  // MenuDelegate::GetSiblingMenu.
  void OwnMenu(MenuItemView* menu);

  // Runs the menu. |types| is a bitmask of RunTypes. If this returns
  // MENU_DELETED the method is returning because the MenuRunner was deleted.
  // Typically callers should NOT do any processing if this returns
  // MENU_DELETED.
  // If |anchor| uses a |BUBBLE_..| type, the bounds will get determined by
  // using |bounds| as the thing to point at in screen coordinates.
  RunResult RunMenuAt(Widget* parent,
                      MenuButton* button,
                      const gfx::Rect& bounds,
                      MenuAnchorPosition anchor,
                      ui::MenuSourceType source_type,
                      int32 types) WARN_UNUSED_RESULT;

  // Returns true if we're in a nested message loop running the menu.
  bool IsRunning() const;

  // Hides and cancels the menu. This does nothing if the menu is not open.
  void Cancel();

  // Returns the time from the event which closed the menu - or 0.
  base::TimeDelta closing_event_time() const;

 private:
  friend class test::MenuRunnerTestAPI;

  // Sets an implementation of RunMenuAt. This is intended to be used at test.
  void SetRunnerHandler(scoped_ptr<MenuRunnerHandler> runner_handler);

  scoped_ptr<MenuModelAdapter> menu_model_adapter_;

  internal::MenuRunnerImpl* holder_;

  // An implementation of RunMenuAt. This is usually NULL and ignored. If this
  // is not NULL, this implementation will be used.
  scoped_ptr<MenuRunnerHandler> runner_handler_;

  scoped_ptr<internal::DisplayChangeListener> display_change_listener_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunner);
};

namespace internal {

// DisplayChangeListener is intended to listen for changes in the display size
// and cancel the menu. DisplayChangeListener is created when the menu is
// shown.
class DisplayChangeListener {
 public:
  virtual ~DisplayChangeListener() {}

  // Creates the platform specified DisplayChangeListener, or NULL if there
  // isn't one. Caller owns the returned value.
  static DisplayChangeListener* Create(Widget* parent,
                                       MenuRunner* runner);

 protected:
  DisplayChangeListener() {}
};

}

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_H_
