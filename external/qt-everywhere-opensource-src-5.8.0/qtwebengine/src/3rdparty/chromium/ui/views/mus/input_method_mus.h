// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#ifndef UI_VIEWS_MUS_INPUT_METHOD_MUS_H_
#define UI_VIEWS_MUS_INPUT_METHOD_MUS_H_

#include "base/macros.h"
#include "ui/views/mus/mus_export.h"

namespace mus {
class Window;
}  // namespace mojo

namespace views {

class VIEWS_MUS_EXPORT InputMethodMUS : public ui::InputMethodBase {
 public:
  InputMethodMUS(ui::internal::InputMethodDelegate* delegate,
                 mus::Window* window);
  ~InputMethodMUS() override;

 private:
  // Overridden from ui::InputMethod:
  void OnFocus() override;
  void OnBlur() override;
  bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                NativeEventResult* result) override;
  void DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnTextInputTypeChanged(const ui::TextInputClient* client) override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void CancelComposition(const ui::TextInputClient* client) override;
  void OnInputLocaleChanged() override;
  std::string GetInputLocale() override;
  bool IsCandidatePopupOpen() const override;

  // Overridden from ui::InputMethodBase:
  void OnDidChangeFocusedClient(ui::TextInputClient* focused_before,
                                ui::TextInputClient* focused) override;

  void UpdateTextInputType();

  // The toplevel window which is not owned by this class.
  mus::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMUS);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_INPUT_METHOD_MUS_H_
