// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_MODEL_ADAPTER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_MODEL_ADAPTER_H_

#include <map>

#include "ui/views/controls/menu/menu_delegate.h"

namespace ui {
class MenuModel;
}

namespace views {
class MenuItemView;

// This class wraps an instance of ui::MenuModel with the
// views::MenuDelegate interface required by views::MenuItemView.
class VIEWS_EXPORT MenuModelAdapter : public MenuDelegate {
 public:
  // The caller retains ownership of the ui::MenuModel instance and
  // must ensure it exists for the lifetime of the adapter.
  explicit MenuModelAdapter(ui::MenuModel* menu_model);
  virtual ~MenuModelAdapter();

  // Populate a MenuItemView menu with the ui::MenuModel items
  // (including submenus).
  virtual void BuildMenu(MenuItemView* menu);

  // Convenience for creating and populating a menu. The caller owns the
  // returned MenuItemView.
  MenuItemView* CreateMenu();

  void set_triggerable_event_flags(int triggerable_event_flags) {
    triggerable_event_flags_ = triggerable_event_flags;
  }
  int triggerable_event_flags() const { return triggerable_event_flags_; }

  // Creates a menu item for the specified entry in the model and adds it as
  // a child to |menu| at the specified |menu_index|.
  static MenuItemView* AddMenuItemFromModelAt(ui::MenuModel* model,
                                              int model_index,
                                              MenuItemView* menu,
                                              int menu_index,
                                              int item_id);

  // Creates a menu item for the specified entry in the model and appends it as
  // a child to |menu|.
  static MenuItemView* AppendMenuItemFromModel(ui::MenuModel* model,
                                               int model_index,
                                               MenuItemView* menu,
                                               int item_id);

 protected:
  // Create and add a menu item to |menu| for the item at index |index| in
  // |model|. Subclasses override this to allow custom items to be added to the
  // menu.
  virtual MenuItemView* AppendMenuItem(MenuItemView* menu,
                                       ui::MenuModel* model,
                                       int index);

  // views::MenuDelegate implementation.
  virtual void ExecuteCommand(int id) OVERRIDE;
  virtual void ExecuteCommand(int id, int mouse_event_flags) OVERRIDE;
  virtual bool IsTriggerableEvent(MenuItemView* source,
                                  const ui::Event& e) OVERRIDE;
  virtual bool GetAccelerator(int id,
                              ui::Accelerator* accelerator) const OVERRIDE;
  virtual base::string16 GetLabel(int id) const OVERRIDE;
  virtual const gfx::FontList* GetLabelFontList(int id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual bool IsCommandVisible(int id) const OVERRIDE;
  virtual bool IsItemChecked(int id) const OVERRIDE;
  virtual void SelectionChanged(MenuItemView* menu) OVERRIDE;
  virtual void WillShowMenu(MenuItemView* menu) OVERRIDE;
  virtual void WillHideMenu(MenuItemView* menu) OVERRIDE;

 private:
  // Implementation of BuildMenu().
  void BuildMenuImpl(MenuItemView* menu, ui::MenuModel* model);

  // Container of ui::MenuModel pointers as encountered by preorder
  // traversal.  The first element is always the top-level model
  // passed to the constructor.
  ui::MenuModel* menu_model_;

  // Mouse event flags which can trigger menu actions.
  int triggerable_event_flags_;

  // Map MenuItems to MenuModels.  Used to implement WillShowMenu().
  std::map<MenuItemView*, ui::MenuModel*> menu_map_;

  DISALLOW_COPY_AND_ASSIGN(MenuModelAdapter);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_MODEL_ADAPTER_H_
