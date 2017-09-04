// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ime/test_ime_driver/test_ime_driver.h"

#include "services/ui/public/interfaces/ime.mojom.h"

namespace ui {
namespace test {

class TestInputMethod : public mojom::InputMethod {
 public:
  explicit TestInputMethod(mojom::TextInputClientPtr client)
      : client_(std::move(client)) {}
  ~TestInputMethod() override {}

 private:
  // mojom::InputMethod:
  void OnTextInputModeChanged(mojom::TextInputMode text_input_mode) override {}
  void OnTextInputTypeChanged(mojom::TextInputType text_input_type) override {}
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override {}
  void ProcessKeyEvent(std::unique_ptr<Event> key_event,
                       const ProcessKeyEventCallback& callback) override {
    DCHECK(key_event->IsKeyEvent());

    if (key_event->AsKeyEvent()->is_char()) {
      mojom::CompositionEventPtr composition_event =
          mojom::CompositionEvent::New();
      composition_event->type = mojom::CompositionEventType::INSERT_CHAR;
      composition_event->key_event = std::move(key_event);
      client_->OnCompositionEvent(std::move(composition_event));
      callback.Run(true);
    } else {
      callback.Run(false);
    }
  }
  void CancelComposition() override {}

  mojom::TextInputClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethod);
};

TestIMEDriver::TestIMEDriver() {}

TestIMEDriver::~TestIMEDriver() {}

void TestIMEDriver::StartSession(
    int32_t session_id,
    mojom::TextInputClientPtr client,
    mojom::InputMethodRequest input_method_request) {
  input_method_bindings_[session_id].reset(
      new mojo::Binding<mojom::InputMethod>(
          new TestInputMethod(std::move(client)),
          std::move(input_method_request)));
}

void TestIMEDriver::CancelSession(int32_t session_id) {
  input_method_bindings_.erase(session_id);
}

}  // namespace test
}  // namespace ui
