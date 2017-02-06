// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_service.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/dummy_input_method.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace arc {

namespace {

class FakeArcImeBridge : public ArcImeBridge {
 public:
  FakeArcImeBridge() : count_send_insert_text_(0) {}

  void SendSetCompositionText(const ui::CompositionText& composition) override {
  }
  void SendConfirmCompositionText() override {
  }
  void SendInsertText(const base::string16& text) override {
    count_send_insert_text_++;
  }
  void SendOnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override {
  }
  void SendExtendSelectionAndDelete(size_t before, size_t after) override {
  }

  int count_send_insert_text() const { return count_send_insert_text_; }

 private:
  int count_send_insert_text_;
};

class FakeInputMethod : public ui::DummyInputMethod {
 public:
  FakeInputMethod() : client_(nullptr),
                      count_show_ime_if_needed_(0),
                      count_cancel_composition_(0) {}

  void SetFocusedTextInputClient(ui::TextInputClient* client) override {
    client_ = client;
  }

  ui::TextInputClient* GetTextInputClient() const override {
    return client_;
  }

  void ShowImeIfNeeded() override {
    count_show_ime_if_needed_++;
  }

  void CancelComposition(const ui::TextInputClient* client) override {
    if (client == client_)
      count_cancel_composition_++;
  }

  int count_show_ime_if_needed() const {
    return count_show_ime_if_needed_;
  }

  int count_cancel_composition() const {
    return count_cancel_composition_;
  }

 private:
  ui::TextInputClient* client_;
  int count_show_ime_if_needed_;
  int count_cancel_composition_;
};

}  // namespace

class ArcImeServiceTest : public testing::Test {
 public:
  ArcImeServiceTest() {}

 protected:
  std::unique_ptr<FakeArcBridgeService> fake_arc_bridge_service_;
  std::unique_ptr<FakeInputMethod> fake_input_method_;
  std::unique_ptr<ArcImeService> instance_;
  FakeArcImeBridge* fake_arc_ime_bridge_;  // Owned by |instance_|

 private:
  void SetUp() override {
    fake_arc_bridge_service_.reset(new FakeArcBridgeService);
    instance_.reset(new ArcImeService(fake_arc_bridge_service_.get()));
    fake_arc_ime_bridge_ = new FakeArcImeBridge;
    instance_->SetImeBridgeForTesting(base::WrapUnique(fake_arc_ime_bridge_));

    fake_input_method_.reset(new FakeInputMethod);
    instance_->SetInputMethodForTesting(fake_input_method_.get());
  }

  void TearDown() override {
    fake_arc_ime_bridge_ = nullptr;
    instance_.reset();
    fake_arc_bridge_service_.reset();
  }
};

TEST_F(ArcImeServiceTest, HasCompositionText) {
  ui::CompositionText composition;
  composition.text = base::UTF8ToUTF16("nonempty text");

  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->ClearCompositionText();
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->ConfirmCompositionText();
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->InsertText(base::UTF8ToUTF16("another text"));
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->SetCompositionText(ui::CompositionText());
  EXPECT_FALSE(instance_->HasCompositionText());
}

TEST_F(ArcImeServiceTest, ShowImeIfNeeded) {
  fake_input_method_->SetFocusedTextInputClient(instance_.get());
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_NONE);
  ASSERT_EQ(0, fake_input_method_->count_show_ime_if_needed());

  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(1, fake_input_method_->count_show_ime_if_needed());

  // The type is not changing, hence no call.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(1, fake_input_method_->count_show_ime_if_needed());

  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_SEARCH);
  EXPECT_EQ(2, fake_input_method_->count_show_ime_if_needed());

  // Change to NONE should not trigger the showing event.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_EQ(2, fake_input_method_->count_show_ime_if_needed());
}

TEST_F(ArcImeServiceTest, CancelComposition) {
  // The bridge should forward the cancel event to the input method.
  fake_input_method_->SetFocusedTextInputClient(instance_.get());
  instance_->OnCancelComposition();
  EXPECT_EQ(1, fake_input_method_->count_cancel_composition());
}

TEST_F(ArcImeServiceTest, InsertChar) {
  fake_input_method_->SetFocusedTextInputClient(instance_.get());

  // When text input type is NONE, the event is not forwarded.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_NONE);
  instance_->InsertChar(ui::KeyEvent('a', ui::VKEY_A, 0));
  EXPECT_EQ(0, fake_arc_ime_bridge_->count_send_insert_text());

  // When the bridge is accepting text inputs, forward the event.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT);
  instance_->InsertChar(ui::KeyEvent('a', ui::VKEY_A, 0));
  EXPECT_EQ(1, fake_arc_ime_bridge_->count_send_insert_text());
}

}  // namespace arc
