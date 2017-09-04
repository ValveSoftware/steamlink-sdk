// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/input_method_mus.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/events/event.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace views {
namespace {

class TestInputMethodDelegate : public ui::internal::InputMethodDelegate {
 public:
  TestInputMethodDelegate() {}
  ~TestInputMethodDelegate() override {}

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* key_event) override {
    return ui::EventDispatchDetails();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInputMethodDelegate);
};

class TestTextInputClient : public ui::DummyTextInputClient {
 public:
  TestTextInputClient() {}
  ~TestTextInputClient() override {}

  ui::KeyEvent* WaitUntilInputReceieved() {
    run_loop_ = base::MakeUnique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();

    return received_event_->AsKeyEvent();
  }

  // ui::DummyTextInputClient:
  void InsertChar(const ui::KeyEvent& event) override {
    received_event_ = ui::Event::Clone(event);
    run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<ui::Event> received_event_;

  DISALLOW_COPY_AND_ASSIGN(TestTextInputClient);
};

}  // namespace

class InputMethodMusTest : public testing::Test {
 public:
  InputMethodMusTest() : message_loop_(base::MessageLoop::TYPE_UI) {}
  ~InputMethodMusTest() override {}

  service_manager::Connector* connector() {
    return WindowManagerConnection::Get()->connector();
  }

 private:
  base::MessageLoop message_loop_;
  ScopedViewsTestHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMusTest);
};

TEST_F(InputMethodMusTest, DispatchKeyEvent) {
  // test_ime_driver will register itself as the current IMEDriver. It echoes
  // back the character key events it receives.
  EXPECT_TRUE(connector()->Connect("test_ime_driver"));

  TestInputMethodDelegate input_method_delegate;
  InputMethodMus input_method(&input_method_delegate, nullptr);
  input_method.Init(connector());

  TestTextInputClient text_input_client;
  input_method.SetFocusedTextInputClient(&text_input_client);

  ui::KeyEvent key_event('A', ui::VKEY_A, 0);
  input_method.DispatchKeyEvent(&key_event);

  ui::KeyEvent* received_event = text_input_client.WaitUntilInputReceieved();
  EXPECT_EQ(ui::ET_KEY_PRESSED, received_event->type());
  EXPECT_TRUE(received_event->is_char());
  EXPECT_EQ(key_event.GetCharacter(), received_event->GetCharacter());
}

}  // namespace views
