// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_ui.h"

#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/keyboard/keyboard_controller.h"

namespace keyboard {

KeyboardUI::KeyboardUI() : keyboard_controller_(nullptr) {}
KeyboardUI::~KeyboardUI() {}

void KeyboardUI::ShowKeyboardContainer(aura::Window* container) {
  if (HasKeyboardWindow()) {
    GetKeyboardWindow()->Show();
    container->Show();
  }
}

void KeyboardUI::HideKeyboardContainer(aura::Window* container) {
  if (HasKeyboardWindow()) {
    container->Hide();
    GetKeyboardWindow()->Hide();
  }
}

void KeyboardUI::EnsureCaretInWorkArea() {
  if (GetInputMethod()->GetTextInputClient()) {
    aura::Window* keyboard_window = GetKeyboardWindow();
    aura::Window* root_window = keyboard_window->GetRootWindow();
    gfx::Rect available_bounds = root_window->bounds();
    gfx::Rect keyboard_bounds = keyboard_window->bounds();
    available_bounds.set_height(available_bounds.height() -
        keyboard_bounds.height());
    GetInputMethod()->GetTextInputClient()->EnsureCaretInRect(available_bounds);
  }
}

void KeyboardUI::SetController(KeyboardController* controller) {
  keyboard_controller_ = controller;
}

}  // namespace keyboard
