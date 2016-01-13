// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu.h"

#include "base/i18n/rtl.h"
#include "ui/gfx/image/image_skia.h"

namespace views {

bool Menu::Delegate::IsItemChecked(int id) const {
  return false;
}

bool Menu::Delegate::IsItemDefault(int id) const {
  return false;
}

base::string16 Menu::Delegate::GetLabel(int id) const {
  return base::string16();
}

bool Menu::Delegate::GetAcceleratorInfo(int id, ui::Accelerator* accel) {
  return false;
}

const gfx::ImageSkia& Menu::Delegate::GetIcon(int id) const {
  return GetEmptyIcon();
}

int Menu::Delegate::GetItemCount() const {
  return 0;
}

bool Menu::Delegate::IsItemSeparator(int id) const {
  return false;
}

bool Menu::Delegate::HasIcon(int id) const {
  return false;
}

bool Menu::Delegate::SupportsCommand(int id) const {
  return true;
}

bool Menu::Delegate::IsCommandEnabled(int id) const {
  return true;
}

bool Menu::Delegate::GetContextualLabel(int id, base::string16* out) const {
  return false;
}

bool Menu::Delegate::IsRightToLeftUILayout() const {
  return base::i18n::IsRTL();
}

const gfx::ImageSkia& Menu::Delegate::GetEmptyIcon() const {
  static const gfx::ImageSkia* empty_icon = new gfx::ImageSkia();
  return *empty_icon;
}

Menu::Menu(Delegate* delegate, AnchorPoint anchor)
    : delegate_(delegate),
      anchor_(anchor) {
}

Menu::Menu(Menu* parent)
    : delegate_(parent->delegate_),
      anchor_(parent->anchor_) {
}

Menu::~Menu() {
}

void Menu::AppendMenuItem(int item_id,
                          const base::string16& label,
                          MenuItemType type) {
  AddMenuItem(-1, item_id, label, type);
}

void Menu::AddMenuItem(int index,
                       int item_id,
                       const base::string16& label,
                       MenuItemType type) {
  if (type == SEPARATOR)
    AddSeparator(index);
  else
    AddMenuItemInternal(index, item_id, label, gfx::ImageSkia(), type);
}

Menu* Menu::AppendSubMenu(int item_id, const base::string16& label) {
  return AddSubMenu(-1, item_id, label);
}

Menu* Menu::AddSubMenu(int index, int item_id, const base::string16& label) {
  return AddSubMenuWithIcon(index, item_id, label, gfx::ImageSkia());
}

Menu* Menu::AppendSubMenuWithIcon(int item_id,
                                  const base::string16& label,
                                  const gfx::ImageSkia& icon) {
  return AddSubMenuWithIcon(-1, item_id, label, icon);
}

void Menu::AppendMenuItemWithLabel(int item_id, const base::string16& label) {
  AddMenuItemWithLabel(-1, item_id, label);
}

void Menu::AddMenuItemWithLabel(int index,
                                int item_id,
                                const base::string16& label) {
  AddMenuItem(index, item_id, label, Menu::NORMAL);
}

void Menu::AppendDelegateMenuItem(int item_id) {
  AddDelegateMenuItem(-1, item_id);
}

void Menu::AddDelegateMenuItem(int index, int item_id) {
  AddMenuItem(index, item_id, base::string16(), Menu::NORMAL);
}

void Menu::AppendSeparator() {
  AddSeparator(-1);
}

void Menu::AppendMenuItemWithIcon(int item_id,
                                  const base::string16& label,
                                  const gfx::ImageSkia& icon) {
  AddMenuItemWithIcon(-1, item_id, label, icon);
}

void Menu::AddMenuItemWithIcon(int index,
                               int item_id,
                               const base::string16& label,
                               const gfx::ImageSkia& icon) {
  AddMenuItemInternal(index, item_id, label, icon, Menu::NORMAL);
}

Menu::Menu() : delegate_(NULL), anchor_(TOPLEFT) {
}

}  // namespace views
