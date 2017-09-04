// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_INPUT_METHOD_MUS_H_
#define UI_AURA_MUS_INPUT_METHOD_MUS_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/ime.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/base/ime/input_method_base.h"

namespace ui {
namespace mojom {
enum class EventResult;
}
}

namespace aura {

class TextInputClientImpl;
class Window;

class AURA_EXPORT InputMethodMus : public ui::InputMethodBase {
 public:
  InputMethodMus(ui::internal::InputMethodDelegate* delegate, Window* window);
  ~InputMethodMus() override;

  void Init(service_manager::Connector* connector);
  void DispatchKeyEvent(
      ui::KeyEvent* event,
      std::unique_ptr<base::Callback<void(ui::mojom::EventResult)>>
          ack_callback);

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
  bool IsCandidatePopupOpen() const override;

 private:
  friend TextInputClientImpl;

  // Overridden from ui::InputMethodBase:
  void OnDidChangeFocusedClient(ui::TextInputClient* focused_before,
                                ui::TextInputClient* focused) override;

  void UpdateTextInputType();
  void ProcessKeyEventCallback(
      const ui::KeyEvent& event,
      std::unique_ptr<base::Callback<void(ui::mojom::EventResult)>>
          ack_callback,
      bool handled);

  // The toplevel window which is not owned by this class. This may be null
  // for tests.
  Window* window_;

  ui::mojom::IMEServerPtr ime_server_;
  ui::mojom::InputMethodPtr input_method_;
  std::unique_ptr<TextInputClientImpl> text_input_client_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_INPUT_METHOD_MUS_H_
