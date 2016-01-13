// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_focus_manager.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/event.h"

namespace ui {
namespace {

class ClientChangeVerifier {
 public:
  ClientChangeVerifier()
     : previous_client_(NULL),
       next_client_(NULL),
       call_expected_(false),
       on_will_change_focused_client_called_(false),
       on_did_change_focused_client_called_(false),
       on_text_input_state_changed_(false) {
  }

  // Expects that focused text input client will not be changed.
  void ExpectClientDoesNotChange() {
    previous_client_ = NULL;
    next_client_ = NULL;
    call_expected_ = false;
    on_will_change_focused_client_called_ = false;
    on_did_change_focused_client_called_ = false;
    on_text_input_state_changed_ = false;
  }

  // Expects that focused text input client will be changed from
  // |previous_client| to |next_client|.
  void ExpectClientChange(TextInputClient* previous_client,
                          TextInputClient* next_client) {
    previous_client_ = previous_client;
    next_client_ = next_client;
    call_expected_ = true;
    on_will_change_focused_client_called_ = false;
    on_did_change_focused_client_called_ = false;
    on_text_input_state_changed_ = false;
  }

  // Verifies the result satisfies the expectation or not.
  void Verify() {
    if (switches::IsTextInputFocusManagerEnabled()) {
      EXPECT_FALSE(on_will_change_focused_client_called_);
      EXPECT_FALSE(on_did_change_focused_client_called_);
      EXPECT_FALSE(on_text_input_state_changed_);
    } else {
      EXPECT_EQ(call_expected_, on_will_change_focused_client_called_);
      EXPECT_EQ(call_expected_, on_did_change_focused_client_called_);
      EXPECT_EQ(call_expected_, on_text_input_state_changed_);
    }
  }

  void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                 TextInputClient* focused) {
    EXPECT_TRUE(call_expected_);

    // Check arguments
    EXPECT_EQ(previous_client_, focused_before);
    EXPECT_EQ(next_client_, focused);

    // Check call order
    EXPECT_FALSE(on_will_change_focused_client_called_);
    EXPECT_FALSE(on_did_change_focused_client_called_);
    EXPECT_FALSE(on_text_input_state_changed_);

    on_will_change_focused_client_called_ = true;
  }

  void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                TextInputClient* focused) {
    EXPECT_TRUE(call_expected_);

    // Check arguments
    EXPECT_EQ(previous_client_, focused_before);
    EXPECT_EQ(next_client_, focused);

    // Check call order
    EXPECT_TRUE(on_will_change_focused_client_called_);
    EXPECT_FALSE(on_did_change_focused_client_called_);
    EXPECT_FALSE(on_text_input_state_changed_);

    on_did_change_focused_client_called_ = true;
 }

  void OnTextInputStateChanged(const TextInputClient* client) {
    EXPECT_TRUE(call_expected_);

    // Check arguments
    EXPECT_EQ(next_client_, client);

    // Check call order
    EXPECT_TRUE(on_will_change_focused_client_called_);
    EXPECT_TRUE(on_did_change_focused_client_called_);
    EXPECT_FALSE(on_text_input_state_changed_);

    on_text_input_state_changed_ = true;
 }

 private:
  TextInputClient* previous_client_;
  TextInputClient* next_client_;
  bool call_expected_;
  bool on_will_change_focused_client_called_;
  bool on_did_change_focused_client_called_;
  bool on_text_input_state_changed_;

  DISALLOW_COPY_AND_ASSIGN(ClientChangeVerifier);
};

class InputMethodBaseTest : public testing::Test {
 protected:
  InputMethodBaseTest() {
  }
  virtual ~InputMethodBaseTest() {
  }

  virtual void SetUp() {
    message_loop_.reset(new base::MessageLoopForUI);
  }

  virtual void TearDown() {
    message_loop_.reset();
  }

 private:
  scoped_ptr<base::MessageLoop> message_loop_;
  DISALLOW_COPY_AND_ASSIGN(InputMethodBaseTest);
};

class MockInputMethodBase : public InputMethodBase {
 public:
  // Note: this class does not take the ownership of |verifier|.
  MockInputMethodBase(ClientChangeVerifier* verifier) : verifier_(verifier) {
  }
  virtual ~MockInputMethodBase() {
  }

 private:
  // Overriden from InputMethod.
  virtual bool OnUntranslatedIMEMessage(
      const base::NativeEvent& event,
      InputMethod::NativeEventResult* result) OVERRIDE {
    return false;
  }
  virtual bool DispatchKeyEvent(const ui::KeyEvent&) OVERRIDE {
    return false;
  }
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE {
  }
  virtual void CancelComposition(const TextInputClient* client) OVERRIDE {
  }
  virtual void OnInputLocaleChanged() OVERRIDE {
  }
  virtual std::string GetInputLocale() OVERRIDE{
    return "";
  }
  virtual bool IsActive() OVERRIDE {
    return false;
  }
  virtual bool IsCandidatePopupOpen() const OVERRIDE {
    return false;
  }
  // Overriden from InputMethodBase.
  virtual void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                         TextInputClient* focused) OVERRIDE {
    verifier_->OnWillChangeFocusedClient(focused_before, focused);
  }

  virtual void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                        TextInputClient* focused) OVERRIDE {
    verifier_->OnDidChangeFocusedClient(focused_before, focused);
  }

  ClientChangeVerifier* verifier_;

  FRIEND_TEST_ALL_PREFIXES(InputMethodBaseTest, CandidateWindowEvents);
  DISALLOW_COPY_AND_ASSIGN(MockInputMethodBase);
};

class MockInputMethodObserver : public InputMethodObserver {
 public:
  // Note: this class does not take the ownership of |verifier|.
  explicit MockInputMethodObserver(ClientChangeVerifier* verifier)
      : verifier_(verifier) {
  }
  virtual ~MockInputMethodObserver() {
  }

 private:
  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE {
  }
  virtual void OnFocus() OVERRIDE {
  }
  virtual void OnBlur() OVERRIDE {
  }
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE {
  }
  virtual void OnTextInputStateChanged(const TextInputClient* client) OVERRIDE {
    verifier_->OnTextInputStateChanged(client);
  }
  virtual void OnShowImeIfNeeded() OVERRIDE {
  }
  virtual void OnInputMethodDestroyed(const InputMethod* client) OVERRIDE {
  }

  ClientChangeVerifier* verifier_;
  DISALLOW_COPY_AND_ASSIGN(MockInputMethodObserver);
};

class MockTextInputClient : public DummyTextInputClient {
 public:
  MockTextInputClient()
      : shown_event_count_(0), updated_event_count_(0), hidden_event_count_(0) {
  }
  virtual ~MockTextInputClient() {
  }

  virtual void OnCandidateWindowShown() OVERRIDE {
    ++shown_event_count_;
  }
  virtual void OnCandidateWindowUpdated() OVERRIDE {
    ++updated_event_count_;
  }
  virtual void OnCandidateWindowHidden() OVERRIDE {
    ++hidden_event_count_;
  }

  int shown_event_count() const { return shown_event_count_; }
  int updated_event_count() const { return updated_event_count_; }
  int hidden_event_count() const { return hidden_event_count_; }

 private:
  int shown_event_count_;
  int updated_event_count_;
  int hidden_event_count_;
};

typedef ScopedObserver<InputMethod, InputMethodObserver>
    InputMethodScopedObserver;

void SetFocusedTextInputClient(InputMethod* input_method,
                               TextInputClient* text_input_client) {
  if (switches::IsTextInputFocusManagerEnabled()) {
    TextInputFocusManager::GetInstance()->FocusTextInputClient(
        text_input_client);
  } else {
    input_method->SetFocusedTextInputClient(text_input_client);
  }
}

TEST_F(InputMethodBaseTest, SetFocusedTextInputClient) {
  DummyTextInputClient text_input_client_1st;
  DummyTextInputClient text_input_client_2nd;

  ClientChangeVerifier verifier;
  MockInputMethodBase input_method(&verifier);
  MockInputMethodObserver input_method_observer(&verifier);
  InputMethodScopedObserver scoped_observer(&input_method_observer);
  scoped_observer.Add(&input_method);

  // Assume that the top-level-widget gains focus.
  input_method.OnFocus();

  {
    SCOPED_TRACE("Focus from NULL to 1st TextInputClient");

    ASSERT_EQ(NULL, input_method.GetTextInputClient());
    verifier.ExpectClientChange(NULL, &text_input_client_1st);
    SetFocusedTextInputClient(&input_method, &text_input_client_1st);
    EXPECT_EQ(&text_input_client_1st, input_method.GetTextInputClient());
    verifier.Verify();
  }

  {
    SCOPED_TRACE("Redundant focus events must be ignored");
    verifier.ExpectClientDoesNotChange();
    SetFocusedTextInputClient(&input_method, &text_input_client_1st);
    verifier.Verify();
  }

  {
    SCOPED_TRACE("Focus from 1st to 2nd TextInputClient");

    ASSERT_EQ(&text_input_client_1st, input_method.GetTextInputClient());
    verifier.ExpectClientChange(&text_input_client_1st,
                                &text_input_client_2nd);
    SetFocusedTextInputClient(&input_method, &text_input_client_2nd);
    EXPECT_EQ(&text_input_client_2nd, input_method.GetTextInputClient());
    verifier.Verify();
  }

  {
    SCOPED_TRACE("Focus from 2nd TextInputClient to NULL");

    ASSERT_EQ(&text_input_client_2nd, input_method.GetTextInputClient());
    verifier.ExpectClientChange(&text_input_client_2nd, NULL);
    SetFocusedTextInputClient(&input_method, NULL);
    EXPECT_EQ(NULL, input_method.GetTextInputClient());
    verifier.Verify();
  }

  {
    SCOPED_TRACE("Redundant focus events must be ignored");
    verifier.ExpectClientDoesNotChange();
    SetFocusedTextInputClient(&input_method, NULL);
    verifier.Verify();
  }
}

TEST_F(InputMethodBaseTest, DetachTextInputClient) {
  // DetachTextInputClient is not supported when IsTextInputFocusManagerEnabled.
  if (switches::IsTextInputFocusManagerEnabled())
    return;

  DummyTextInputClient text_input_client;
  DummyTextInputClient text_input_client_the_other;

  ClientChangeVerifier verifier;
  MockInputMethodBase input_method(&verifier);
  MockInputMethodObserver input_method_observer(&verifier);
  InputMethodScopedObserver scoped_observer(&input_method_observer);
  scoped_observer.Add(&input_method);

  // Assume that the top-level-widget gains focus.
  input_method.OnFocus();

  // Initialize for the next test.
  {
    verifier.ExpectClientChange(NULL, &text_input_client);
    input_method.SetFocusedTextInputClient(&text_input_client);
    verifier.Verify();
  }

  {
    SCOPED_TRACE("DetachTextInputClient must be ignored for other clients");
    ASSERT_EQ(&text_input_client, input_method.GetTextInputClient());
    verifier.ExpectClientDoesNotChange();
    input_method.DetachTextInputClient(&text_input_client_the_other);
    EXPECT_EQ(&text_input_client, input_method.GetTextInputClient());
    verifier.Verify();
  }

  {
    SCOPED_TRACE("DetachTextInputClient must succeed even after the "
                 "top-level loses the focus");

    ASSERT_EQ(&text_input_client, input_method.GetTextInputClient());
    input_method.OnBlur();
    input_method.OnFocus();
    verifier.ExpectClientChange(&text_input_client, NULL);
    input_method.DetachTextInputClient(&text_input_client);
    EXPECT_EQ(NULL, input_method.GetTextInputClient());
    verifier.Verify();
  }
}

TEST_F(InputMethodBaseTest, CandidateWindowEvents) {
  MockTextInputClient text_input_client;

  {
    ClientChangeVerifier verifier;
    MockInputMethodBase input_method_base(&verifier);
    input_method_base.OnFocus();

    verifier.ExpectClientChange(NULL, &text_input_client);
    SetFocusedTextInputClient(&input_method_base, &text_input_client);

    EXPECT_EQ(0, text_input_client.shown_event_count());
    EXPECT_EQ(0, text_input_client.updated_event_count());
    EXPECT_EQ(0, text_input_client.hidden_event_count());

    input_method_base.OnCandidateWindowShown();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, text_input_client.shown_event_count());
    EXPECT_EQ(0, text_input_client.updated_event_count());
    EXPECT_EQ(0, text_input_client.hidden_event_count());

    input_method_base.OnCandidateWindowUpdated();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, text_input_client.shown_event_count());
    EXPECT_EQ(1, text_input_client.updated_event_count());
    EXPECT_EQ(0, text_input_client.hidden_event_count());

    input_method_base.OnCandidateWindowHidden();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, text_input_client.shown_event_count());
    EXPECT_EQ(1, text_input_client.updated_event_count());
    EXPECT_EQ(1, text_input_client.hidden_event_count());

    input_method_base.OnCandidateWindowShown();
  }

  // If InputMethod is deleted immediately after an event happens, but before
  // its callback is invoked, the callback will be cancelled.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, text_input_client.shown_event_count());
  EXPECT_EQ(1, text_input_client.updated_event_count());
  EXPECT_EQ(1, text_input_client.hidden_event_count());
}

}  // namespace
}  // namespace ui
