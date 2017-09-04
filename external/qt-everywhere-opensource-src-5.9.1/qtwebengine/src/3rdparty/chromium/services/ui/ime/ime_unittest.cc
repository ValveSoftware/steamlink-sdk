// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/ime.mojom.h"
#include "ui/events/event.h"

class TestTextInputClient : public ui::mojom::TextInputClient {
 public:
  explicit TestTextInputClient(ui::mojom::TextInputClientRequest request)
      : binding_(this, std::move(request)) {}

  ui::mojom::CompositionEventPtr WaitUntilCompositionEvent() {
    if (!receieved_composition_event_) {
      run_loop_.reset(new base::RunLoop);
      run_loop_->Run();
      run_loop_.reset();
    }

    return std::move(receieved_composition_event_);
  }

 private:
  void OnCompositionEvent(ui::mojom::CompositionEventPtr event) override {
    receieved_composition_event_ = std::move(event);
    if (run_loop_)
      run_loop_->Quit();
  }

  mojo::Binding<ui::mojom::TextInputClient> binding_;
  std::unique_ptr<base::RunLoop> run_loop_;
  ui::mojom::CompositionEventPtr receieved_composition_event_;

  DISALLOW_COPY_AND_ASSIGN(TestTextInputClient);
};

class IMEAppTest : public service_manager::test::ServiceTest {
 public:
  IMEAppTest() : ServiceTest("mus_ime_unittests") {}
  ~IMEAppTest() override {}

  // service_manager::test::ServiceTest:
  void SetUp() override {
    ServiceTest::SetUp();
    // test_ime_driver will register itself as the current IMEDriver.
    connector()->Connect("test_ime_driver");
    connector()->ConnectToInterface(ui::mojom::kServiceName, &ime_server_);
  }

  bool ProcessKeyEvent(ui::mojom::InputMethodPtr* input_method,
                       std::unique_ptr<ui::Event> event) {
    (*input_method)
        ->ProcessKeyEvent(std::move(event),
                          base::Bind(&IMEAppTest::ProcessKeyEventCallback,
                                     base::Unretained(this)));

    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    run_loop_.reset();

    return handled_;
  }

 protected:
  void ProcessKeyEventCallback(bool handled) {
    handled_ = handled;
    run_loop_->Quit();
  }

  ui::mojom::IMEServerPtr ime_server_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool handled_;

  DISALLOW_COPY_AND_ASSIGN(IMEAppTest);
};

// Tests sending a KeyEvent to the IMEDriver through the Mus IMEServer.
TEST_F(IMEAppTest, ProcessKeyEvent) {
  ui::mojom::TextInputClientPtr client_ptr;
  TestTextInputClient client(GetProxy(&client_ptr));

  ui::mojom::InputMethodPtr input_method;
  ime_server_->StartSession(std::move(client_ptr), GetProxy(&input_method));

  // Send character key event.
  ui::KeyEvent char_event('A', ui::VKEY_A, 0);
  EXPECT_TRUE(ProcessKeyEvent(&input_method, ui::Event::Clone(char_event)));

  ui::mojom::CompositionEventPtr composition_event =
      client.WaitUntilCompositionEvent();
  EXPECT_EQ(ui::mojom::CompositionEventType::INSERT_CHAR,
            composition_event->type);
  EXPECT_TRUE(composition_event->key_event);
  EXPECT_TRUE(composition_event->key_event.value()->IsKeyEvent());

  ui::KeyEvent* received_key_event =
      composition_event->key_event.value()->AsKeyEvent();
  EXPECT_EQ(ui::ET_KEY_PRESSED, received_key_event->type());
  EXPECT_TRUE(received_key_event->is_char());
  EXPECT_EQ(char_event.GetCharacter(), received_key_event->GetCharacter());

  // Send non-character key event.
  ui::KeyEvent nonchar_event(ui::ET_KEY_PRESSED, ui::VKEY_LEFT, 0);
  EXPECT_FALSE(ProcessKeyEvent(&input_method, ui::Event::Clone(nonchar_event)));
}
