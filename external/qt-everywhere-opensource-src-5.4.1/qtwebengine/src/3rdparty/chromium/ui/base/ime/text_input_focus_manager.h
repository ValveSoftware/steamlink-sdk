// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_TEXT_INPUT_FOCUS_MANAGER_H_
#define UI_BASE_IME_TEXT_INPUT_FOCUS_MANAGER_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "ui/base/ui_base_export.h"

template <typename T> struct DefaultSingletonTraits;

namespace ui {

class TextInputClient;

// Manages the focused TextInputClient across windows and their contents.
class UI_BASE_EXPORT TextInputFocusManager {
 public:
  static TextInputFocusManager* GetInstance();

  // Returns the currently focused text input client or NULL.
  TextInputClient* GetFocusedTextInputClient();

  // Changes the text input focus to |text_input_client|.
  void FocusTextInputClient(TextInputClient* text_input_client);

  // Removes the text input focus from |text_input_client|.  If
  // |text_input_client| was not focused, does nothing.
  void BlurTextInputClient(TextInputClient* text_input_client);

 private:
  friend struct DefaultSingletonTraits<TextInputFocusManager>;

  TextInputFocusManager();
  ~TextInputFocusManager();

  TextInputClient* focused_text_input_client_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(TextInputFocusManager);
};

}  // namespace ui

#endif  // UI_BASE_IME_TEXT_INPUT_FOCUS_MANAGER_H_
