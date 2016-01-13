// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/remote_input_method_win.h"

#include <InputScope.h>

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/remote_input_method_delegate_win.h"
#include "ui/events/event.h"

namespace ui {
namespace {

class MockTextInputClient : public DummyTextInputClient {
 public:
  MockTextInputClient()
      : text_input_type_(TEXT_INPUT_TYPE_NONE),
        text_input_mode_(TEXT_INPUT_MODE_DEFAULT),
        call_count_set_composition_text_(0),
        call_count_insert_char_(0),
        call_count_insert_text_(0),
        emulate_pepper_flash_(false),
        is_candidate_window_shown_called_(false),
        is_candidate_window_hidden_called_(false) {
  }

  size_t call_count_set_composition_text() const {
    return call_count_set_composition_text_;
  }
  const base::string16& inserted_text() const {
    return inserted_text_;
  }
  size_t call_count_insert_char() const {
    return call_count_insert_char_;
  }
  size_t call_count_insert_text() const {
    return call_count_insert_text_;
  }
  bool is_candidate_window_shown_called() const {
    return is_candidate_window_shown_called_;
  }
  bool is_candidate_window_hidden_called() const {
    return is_candidate_window_hidden_called_;
  }
  void Reset() {
    text_input_type_ = TEXT_INPUT_TYPE_NONE;
    text_input_mode_ = TEXT_INPUT_MODE_DEFAULT;
    call_count_set_composition_text_ = 0;
    inserted_text_.clear();
    call_count_insert_char_ = 0;
    call_count_insert_text_ = 0;
    caret_bounds_ = gfx::Rect();
    composition_character_bounds_.clear();
    emulate_pepper_flash_ = false;
    is_candidate_window_shown_called_ = false;
    is_candidate_window_hidden_called_ = false;
  }
  void set_text_input_type(ui::TextInputType type) {
    text_input_type_ = type;
  }
  void set_text_input_mode(ui::TextInputMode mode) {
    text_input_mode_ = mode;
  }
  void set_caret_bounds(const gfx::Rect& caret_bounds) {
    caret_bounds_ = caret_bounds;
  }
  void set_composition_character_bounds(
      const std::vector<gfx::Rect>& composition_character_bounds) {
    composition_character_bounds_ = composition_character_bounds;
  }
  void set_emulate_pepper_flash(bool enabled) {
    emulate_pepper_flash_ = enabled;
  }

 private:
  // Overriden from DummyTextInputClient.
  virtual void SetCompositionText(
      const ui::CompositionText& composition) OVERRIDE {
    ++call_count_set_composition_text_;
  }
  virtual void InsertChar(base::char16 ch, int flags) OVERRIDE {
    inserted_text_.append(1, ch);
    ++call_count_insert_char_;
  }
  virtual void InsertText(const base::string16& text) OVERRIDE {
    inserted_text_.append(text);
    ++call_count_insert_text_;
  }
  virtual ui::TextInputType GetTextInputType() const OVERRIDE {
    return text_input_type_;
  }
  virtual ui::TextInputMode GetTextInputMode() const OVERRIDE {
    return text_input_mode_;
  }
  virtual gfx::Rect GetCaretBounds() const {
    return caret_bounds_;
  }
  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) const OVERRIDE {
    // Emulate the situation of crbug.com/328237.
    if (emulate_pepper_flash_)
      return false;
    if (!rect || composition_character_bounds_.size() <= index)
      return false;
    *rect = composition_character_bounds_[index];
    return true;
  }
  virtual bool HasCompositionText() const OVERRIDE {
    return !composition_character_bounds_.empty();
  }
  virtual bool GetCompositionTextRange(gfx::Range* range) const OVERRIDE {
    if (composition_character_bounds_.empty())
      return false;
    *range = gfx::Range(0, composition_character_bounds_.size());
    return true;
  }
  virtual void OnCandidateWindowShown() OVERRIDE {
    is_candidate_window_shown_called_ = true;
  }
  virtual void OnCandidateWindowHidden() OVERRIDE {
    is_candidate_window_hidden_called_ = true;
  }

  ui::TextInputType text_input_type_;
  ui::TextInputMode text_input_mode_;
  gfx::Rect caret_bounds_;
  std::vector<gfx::Rect> composition_character_bounds_;
  base::string16 inserted_text_;
  size_t call_count_set_composition_text_;
  size_t call_count_insert_char_;
  size_t call_count_insert_text_;
  bool emulate_pepper_flash_;
  bool is_candidate_window_shown_called_;
  bool is_candidate_window_hidden_called_;
  DISALLOW_COPY_AND_ASSIGN(MockTextInputClient);
};

class MockInputMethodDelegate : public internal::InputMethodDelegate {
 public:
  MockInputMethodDelegate() {}

  const std::vector<ui::KeyboardCode>& fabricated_key_events() const {
    return fabricated_key_events_;
  }
  void Reset() {
    fabricated_key_events_.clear();
  }

 private:
  virtual bool DispatchKeyEventPostIME(const ui::KeyEvent& event) OVERRIDE {
    EXPECT_FALSE(event.HasNativeEvent());
    fabricated_key_events_.push_back(event.key_code());
    return true;
  }

  std::vector<ui::KeyboardCode> fabricated_key_events_;
  DISALLOW_COPY_AND_ASSIGN(MockInputMethodDelegate);
};

class MockRemoteInputMethodDelegateWin
    : public internal::RemoteInputMethodDelegateWin {
 public:
  MockRemoteInputMethodDelegateWin()
      : cancel_composition_called_(false),
        text_input_client_updated_called_(false) {
  }

  bool cancel_composition_called() const {
    return cancel_composition_called_;
  }
  bool text_input_client_updated_called() const {
    return text_input_client_updated_called_;
  }
  const std::vector<int32>& input_scopes() const {
    return input_scopes_;
  }
  const std::vector<gfx::Rect>& composition_character_bounds() const {
    return composition_character_bounds_;
  }
  void Reset() {
    cancel_composition_called_ = false;
    text_input_client_updated_called_ = false;
    input_scopes_.clear();
    composition_character_bounds_.clear();
  }

 private:
  virtual void CancelComposition() OVERRIDE {
    cancel_composition_called_ = true;
  }

  virtual void OnTextInputClientUpdated(
      const std::vector<int32>& input_scopes,
      const std::vector<gfx::Rect>& composition_character_bounds) OVERRIDE {
    text_input_client_updated_called_ = true;
    input_scopes_ = input_scopes;
    composition_character_bounds_ = composition_character_bounds;
  }

  bool cancel_composition_called_;
  bool text_input_client_updated_called_;
  std::vector<int32> input_scopes_;
  std::vector<gfx::Rect> composition_character_bounds_;
  DISALLOW_COPY_AND_ASSIGN(MockRemoteInputMethodDelegateWin);
};

class MockInputMethodObserver : public InputMethodObserver {
 public:
  MockInputMethodObserver()
      : on_text_input_state_changed_(0),
        on_input_method_destroyed_changed_(0) {
  }
  virtual ~MockInputMethodObserver() {
  }
  void Reset() {
    on_text_input_state_changed_ = 0;
    on_input_method_destroyed_changed_ = 0;
  }
  size_t on_text_input_state_changed() const {
    return on_text_input_state_changed_;
  }
  size_t on_input_method_destroyed_changed() const {
    return on_input_method_destroyed_changed_;
  }

 private:
  // Overriden from InputMethodObserver.
  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE {
  }
  virtual void OnFocus() OVERRIDE {
  }
  virtual void OnBlur() OVERRIDE {
  }
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE {
  }
  virtual void OnTextInputStateChanged(const TextInputClient* client) OVERRIDE {
    ++on_text_input_state_changed_;
  }
  virtual void OnInputMethodDestroyed(const InputMethod* client) OVERRIDE {
    ++on_input_method_destroyed_changed_;
  }
  virtual void OnShowImeIfNeeded() {
  }

  size_t on_text_input_state_changed_;
  size_t on_input_method_destroyed_changed_;
  DISALLOW_COPY_AND_ASSIGN(MockInputMethodObserver);
};

typedef ScopedObserver<InputMethod, InputMethodObserver>
    InputMethodScopedObserver;

TEST(RemoteInputMethodWinTest, RemoteInputMethodPrivateWin) {
  InputMethod* other_ptr = static_cast<InputMethod*>(NULL) + 1;

  // Use typed NULL to make EXPECT_NE happy until nullptr becomes available.
  RemoteInputMethodPrivateWin* kNull =
      static_cast<RemoteInputMethodPrivateWin*>(NULL);
  EXPECT_EQ(kNull, RemoteInputMethodPrivateWin::Get(other_ptr));

  MockInputMethodDelegate delegate_;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));
  EXPECT_NE(kNull, RemoteInputMethodPrivateWin::Get(input_method.get()));

  InputMethod* dangling_ptr = input_method.get();
  input_method.reset(NULL);
  EXPECT_EQ(kNull, RemoteInputMethodPrivateWin::Get(dangling_ptr));
}

TEST(RemoteInputMethodWinTest, OnInputSourceChanged) {
  MockInputMethodDelegate delegate_;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));
  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);

  private_ptr->OnInputSourceChanged(
      MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN), true);
  EXPECT_EQ("ja-JP", input_method->GetInputLocale());

  private_ptr->OnInputSourceChanged(
      MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_QATAR), true);
  EXPECT_EQ("ar-QA", input_method->GetInputLocale());
}

TEST(RemoteInputMethodWinTest, OnCandidatePopupChanged) {
  MockInputMethodDelegate delegate_;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));
  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);

  // Initial value
  EXPECT_FALSE(input_method->IsCandidatePopupOpen());

  // RemoteInputMethodWin::OnCandidatePopupChanged can be called even when the
  // focused text input client is NULL.
  ASSERT_TRUE(input_method->GetTextInputClient() == NULL);
  private_ptr->OnCandidatePopupChanged(false);
  private_ptr->OnCandidatePopupChanged(true);

  MockTextInputClient mock_text_input_client;
  input_method->SetFocusedTextInputClient(&mock_text_input_client);

  ASSERT_FALSE(mock_text_input_client.is_candidate_window_shown_called());
  ASSERT_FALSE(mock_text_input_client.is_candidate_window_hidden_called());
  mock_text_input_client.Reset();

  private_ptr->OnCandidatePopupChanged(true);
  EXPECT_TRUE(input_method->IsCandidatePopupOpen());
  EXPECT_TRUE(mock_text_input_client.is_candidate_window_shown_called());
  EXPECT_FALSE(mock_text_input_client.is_candidate_window_hidden_called());

  private_ptr->OnCandidatePopupChanged(false);
  EXPECT_FALSE(input_method->IsCandidatePopupOpen());
  EXPECT_TRUE(mock_text_input_client.is_candidate_window_shown_called());
  EXPECT_TRUE(mock_text_input_client.is_candidate_window_hidden_called());
}

TEST(RemoteInputMethodWinTest, CancelComposition) {
  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));

  // This must not cause a crash.
  input_method->CancelComposition(&mock_text_input_client);

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  input_method->CancelComposition(&mock_text_input_client);
  EXPECT_FALSE(mock_remote_delegate.cancel_composition_called());

  input_method->SetFocusedTextInputClient(&mock_text_input_client);
  input_method->CancelComposition(&mock_text_input_client);
  EXPECT_TRUE(mock_remote_delegate.cancel_composition_called());
}

TEST(RemoteInputMethodWinTest, SetFocusedTextInputClient) {
  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));

  mock_text_input_client.set_caret_bounds(gfx::Rect(10, 0, 10, 20));
  mock_text_input_client.set_text_input_type(ui::TEXT_INPUT_TYPE_URL);
  input_method->SetFocusedTextInputClient(&mock_text_input_client);

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  // Initial state must be synced.
  EXPECT_TRUE(mock_remote_delegate.text_input_client_updated_called());
  ASSERT_EQ(1, mock_remote_delegate.composition_character_bounds().size());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 20),
            mock_remote_delegate.composition_character_bounds()[0]);
  ASSERT_EQ(1, mock_remote_delegate.input_scopes().size());
  EXPECT_EQ(IS_URL, mock_remote_delegate.input_scopes()[0]);

  // State must be cleared by SetFocusedTextInputClient(NULL).
  mock_remote_delegate.Reset();
  input_method->SetFocusedTextInputClient(NULL);
  EXPECT_TRUE(mock_remote_delegate.text_input_client_updated_called());
  EXPECT_TRUE(mock_remote_delegate.composition_character_bounds().empty());
  EXPECT_TRUE(mock_remote_delegate.input_scopes().empty());
}

TEST(RemoteInputMethodWinTest, DetachTextInputClient) {
  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));

  mock_text_input_client.set_caret_bounds(gfx::Rect(10, 0, 10, 20));
  mock_text_input_client.set_text_input_type(ui::TEXT_INPUT_TYPE_URL);
  input_method->SetFocusedTextInputClient(&mock_text_input_client);

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  // Initial state must be synced.
  EXPECT_TRUE(mock_remote_delegate.text_input_client_updated_called());
  ASSERT_EQ(1, mock_remote_delegate.composition_character_bounds().size());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 20),
    mock_remote_delegate.composition_character_bounds()[0]);
  ASSERT_EQ(1, mock_remote_delegate.input_scopes().size());
  EXPECT_EQ(IS_URL, mock_remote_delegate.input_scopes()[0]);

  // State must be cleared by DetachTextInputClient
  mock_remote_delegate.Reset();
  input_method->DetachTextInputClient(&mock_text_input_client);
  EXPECT_TRUE(mock_remote_delegate.text_input_client_updated_called());
  EXPECT_TRUE(mock_remote_delegate.composition_character_bounds().empty());
  EXPECT_TRUE(mock_remote_delegate.input_scopes().empty());
}

TEST(RemoteInputMethodWinTest, OnCaretBoundsChanged) {
  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));

  // This must not cause a crash.
  input_method->OnCaretBoundsChanged(&mock_text_input_client);

  mock_text_input_client.set_caret_bounds(gfx::Rect(10, 0, 10, 20));
  input_method->SetFocusedTextInputClient(&mock_text_input_client);

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  // Initial state must be synced.
  EXPECT_TRUE(mock_remote_delegate.text_input_client_updated_called());
  ASSERT_EQ(1, mock_remote_delegate.composition_character_bounds().size());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 20),
      mock_remote_delegate.composition_character_bounds()[0]);

  // Redundant OnCaretBoundsChanged must be ignored.
  mock_remote_delegate.Reset();
  input_method->OnCaretBoundsChanged(&mock_text_input_client);
  EXPECT_FALSE(mock_remote_delegate.text_input_client_updated_called());

  // Check OnCaretBoundsChanged is handled. (w/o composition)
  mock_remote_delegate.Reset();
  mock_text_input_client.Reset();
  mock_text_input_client.set_caret_bounds(gfx::Rect(10, 20, 30, 40));
  input_method->OnCaretBoundsChanged(&mock_text_input_client);
  EXPECT_TRUE(mock_remote_delegate.text_input_client_updated_called());
  ASSERT_EQ(1, mock_remote_delegate.composition_character_bounds().size());
  EXPECT_EQ(gfx::Rect(10, 20, 30, 40),
      mock_remote_delegate.composition_character_bounds()[0]);

  // Check OnCaretBoundsChanged is handled. (w/ composition)
  {
    mock_remote_delegate.Reset();
    mock_text_input_client.Reset();

    std::vector<gfx::Rect> bounds;
    bounds.push_back(gfx::Rect(10, 20, 30, 40));
    bounds.push_back(gfx::Rect(40, 30, 20, 10));
    mock_text_input_client.set_composition_character_bounds(bounds);
    input_method->OnCaretBoundsChanged(&mock_text_input_client);
    EXPECT_TRUE(mock_remote_delegate.text_input_client_updated_called());
    EXPECT_EQ(bounds, mock_remote_delegate.composition_character_bounds());
  }
}

// Test case against crbug.com/328237.
TEST(RemoteInputMethodWinTest, OnCaretBoundsChangedForPepperFlash) {
  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));
  input_method->SetFocusedTextInputClient(&mock_text_input_client);

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  mock_remote_delegate.Reset();
  mock_text_input_client.Reset();
  mock_text_input_client.set_emulate_pepper_flash(true);

  std::vector<gfx::Rect> caret_bounds;
  caret_bounds.push_back(gfx::Rect(5, 15, 25, 35));
  mock_text_input_client.set_caret_bounds(caret_bounds[0]);

  std::vector<gfx::Rect> composition_bounds;
  composition_bounds.push_back(gfx::Rect(10, 20, 30, 40));
  composition_bounds.push_back(gfx::Rect(40, 30, 20, 10));
  mock_text_input_client.set_composition_character_bounds(composition_bounds);
  input_method->OnCaretBoundsChanged(&mock_text_input_client);
  EXPECT_TRUE(mock_remote_delegate.text_input_client_updated_called());
  // The caret bounds must be used when
  // TextInputClient::GetCompositionCharacterBounds failed.
  EXPECT_EQ(caret_bounds, mock_remote_delegate.composition_character_bounds());
}

TEST(RemoteInputMethodWinTest, OnTextInputTypeChanged) {
  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));

  // This must not cause a crash.
  input_method->OnCaretBoundsChanged(&mock_text_input_client);

  mock_text_input_client.set_text_input_type(ui::TEXT_INPUT_TYPE_URL);
  input_method->SetFocusedTextInputClient(&mock_text_input_client);

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  // Initial state must be synced.
  EXPECT_TRUE(mock_remote_delegate.text_input_client_updated_called());
  ASSERT_EQ(1, mock_remote_delegate.input_scopes().size());
  EXPECT_EQ(IS_URL, mock_remote_delegate.input_scopes()[0]);

  // Check TEXT_INPUT_TYPE_NONE is handled.
  mock_remote_delegate.Reset();
  mock_text_input_client.Reset();
  mock_text_input_client.set_text_input_type(ui::TEXT_INPUT_TYPE_NONE);
  mock_text_input_client.set_text_input_mode(ui::TEXT_INPUT_MODE_KATAKANA);
  input_method->OnTextInputTypeChanged(&mock_text_input_client);
  EXPECT_TRUE(mock_remote_delegate.text_input_client_updated_called());
  EXPECT_TRUE(mock_remote_delegate.input_scopes().empty());

  // Redundant OnTextInputTypeChanged must be ignored.
  mock_remote_delegate.Reset();
  input_method->OnTextInputTypeChanged(&mock_text_input_client);
  EXPECT_FALSE(mock_remote_delegate.text_input_client_updated_called());

  mock_remote_delegate.Reset();
  mock_text_input_client.Reset();
  mock_text_input_client.set_caret_bounds(gfx::Rect(10, 20, 30, 40));
  input_method->OnCaretBoundsChanged(&mock_text_input_client);
}

TEST(RemoteInputMethodWinTest, DispatchKeyEvent_NativeKeyEvent) {
  // Basically RemoteInputMethodWin does not handle native keydown event.

  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));

  const MSG wm_keydown = { NULL, WM_KEYDOWN, ui::VKEY_A };
  ui::KeyEvent native_keydown(wm_keydown, false);

  // This must not cause a crash.
  EXPECT_FALSE(input_method->DispatchKeyEvent(native_keydown));
  EXPECT_TRUE(mock_text_input_client.inserted_text().empty());
  EXPECT_TRUE(delegate_.fabricated_key_events().empty());
  delegate_.Reset();
  mock_text_input_client.Reset();

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  // TextInputClient is not focused yet here.

  EXPECT_FALSE(input_method->DispatchKeyEvent(native_keydown));
  EXPECT_TRUE(mock_text_input_client.inserted_text().empty());
  EXPECT_TRUE(delegate_.fabricated_key_events().empty());
  delegate_.Reset();
  mock_text_input_client.Reset();

  input_method->SetFocusedTextInputClient(&mock_text_input_client);

  // TextInputClient is now focused here.

  EXPECT_FALSE(input_method->DispatchKeyEvent(native_keydown));
  EXPECT_TRUE(mock_text_input_client.inserted_text().empty());
  EXPECT_TRUE(delegate_.fabricated_key_events().empty());
  delegate_.Reset();
  mock_text_input_client.Reset();
}

TEST(RemoteInputMethodWinTest, DispatchKeyEvent_NativeCharEvent) {
  // RemoteInputMethodWin handles native char event if possible.

  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));

  const MSG wm_char = { NULL, WM_CHAR, 'A', 0 };
  ui::KeyEvent native_char(wm_char, true);

  // This must not cause a crash.
  EXPECT_FALSE(input_method->DispatchKeyEvent(native_char));
  EXPECT_TRUE(mock_text_input_client.inserted_text().empty());
  EXPECT_TRUE(delegate_.fabricated_key_events().empty());
  delegate_.Reset();
  mock_text_input_client.Reset();

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  // TextInputClient is not focused yet here.

  EXPECT_FALSE(input_method->DispatchKeyEvent(native_char));
  EXPECT_TRUE(mock_text_input_client.inserted_text().empty());
  EXPECT_TRUE(delegate_.fabricated_key_events().empty());
  delegate_.Reset();
  mock_text_input_client.Reset();

  input_method->SetFocusedTextInputClient(&mock_text_input_client);

  // TextInputClient is now focused here.

  EXPECT_TRUE(input_method->DispatchKeyEvent(native_char));
  EXPECT_EQ(L"A", mock_text_input_client.inserted_text());
  EXPECT_TRUE(delegate_.fabricated_key_events().empty());
  delegate_.Reset();
  mock_text_input_client.Reset();
}

TEST(RemoteInputMethodWinTest, DispatchKeyEvent_FabricatedKeyDown) {
  // Fabricated non-char event will be delegated to
  // InputMethodDelegate::DispatchFabricatedKeyEventPostIME as long as the
  // delegate is installed.

  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));

  ui::KeyEvent fabricated_keydown(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, false);
  fabricated_keydown.set_character(L'A');

  // This must not cause a crash.
  EXPECT_TRUE(input_method->DispatchKeyEvent(fabricated_keydown));
  EXPECT_TRUE(mock_text_input_client.inserted_text().empty());
  ASSERT_EQ(1, delegate_.fabricated_key_events().size());
  EXPECT_EQ(L'A', delegate_.fabricated_key_events()[0]);
  delegate_.Reset();
  mock_text_input_client.Reset();

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  // TextInputClient is not focused yet here.

  EXPECT_TRUE(input_method->DispatchKeyEvent(fabricated_keydown));
  EXPECT_TRUE(mock_text_input_client.inserted_text().empty());
  ASSERT_EQ(1, delegate_.fabricated_key_events().size());
  EXPECT_EQ(L'A', delegate_.fabricated_key_events()[0]);
  delegate_.Reset();
  mock_text_input_client.Reset();

  input_method->SetFocusedTextInputClient(&mock_text_input_client);
  // TextInputClient is now focused here.

  EXPECT_TRUE(input_method->DispatchKeyEvent(fabricated_keydown));
  EXPECT_TRUE(mock_text_input_client.inserted_text().empty());
  ASSERT_EQ(1, delegate_.fabricated_key_events().size());
  EXPECT_EQ(L'A', delegate_.fabricated_key_events()[0]);
  delegate_.Reset();
  mock_text_input_client.Reset();

  input_method->SetDelegate(NULL);
  // RemoteInputMethodDelegateWin is no longer set here.

  EXPECT_FALSE(input_method->DispatchKeyEvent(fabricated_keydown));
  EXPECT_TRUE(mock_text_input_client.inserted_text().empty());
}

TEST(RemoteInputMethodWinTest, DispatchKeyEvent_FabricatedChar) {
  // Note: RemoteInputMethodWin::DispatchKeyEvent should always return true
  // for fabricated character events.

  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));

  ui::KeyEvent fabricated_char(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, true);
  fabricated_char.set_character(L'A');

  // This must not cause a crash.
  EXPECT_TRUE(input_method->DispatchKeyEvent(fabricated_char));
  EXPECT_TRUE(mock_text_input_client.inserted_text().empty());
  EXPECT_TRUE(delegate_.fabricated_key_events().empty());
  delegate_.Reset();
  mock_text_input_client.Reset();

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  // TextInputClient is not focused yet here.

  EXPECT_TRUE(input_method->DispatchKeyEvent(fabricated_char));
  EXPECT_TRUE(mock_text_input_client.inserted_text().empty());
  EXPECT_TRUE(delegate_.fabricated_key_events().empty());
  delegate_.Reset();
  mock_text_input_client.Reset();

  input_method->SetFocusedTextInputClient(&mock_text_input_client);

  // TextInputClient is now focused here.

  EXPECT_TRUE(input_method->DispatchKeyEvent(fabricated_char));
  EXPECT_EQ(L"A", mock_text_input_client.inserted_text());
  EXPECT_TRUE(delegate_.fabricated_key_events().empty());
  delegate_.Reset();
  mock_text_input_client.Reset();
}

TEST(RemoteInputMethodWinTest, OnCompositionChanged) {
  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  CompositionText composition_text;

  // TextInputClient is not focused yet here.

  private_ptr->OnCompositionChanged(composition_text);
  EXPECT_EQ(0, mock_text_input_client.call_count_set_composition_text());
  delegate_.Reset();
  mock_text_input_client.Reset();

  input_method->SetFocusedTextInputClient(&mock_text_input_client);

  // TextInputClient is now focused here.

  private_ptr->OnCompositionChanged(composition_text);
  EXPECT_EQ(1, mock_text_input_client.call_count_set_composition_text());
  delegate_.Reset();
  mock_text_input_client.Reset();
}

TEST(RemoteInputMethodWinTest, OnTextCommitted) {
  MockInputMethodDelegate delegate_;
  MockTextInputClient mock_text_input_client;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));

  RemoteInputMethodPrivateWin* private_ptr =
      RemoteInputMethodPrivateWin::Get(input_method.get());
  ASSERT_TRUE(private_ptr != NULL);
  MockRemoteInputMethodDelegateWin mock_remote_delegate;
  private_ptr->SetRemoteDelegate(&mock_remote_delegate);

  base::string16 committed_text = L"Hello";

  // TextInputClient is not focused yet here.

  mock_text_input_client.set_text_input_type(TEXT_INPUT_TYPE_TEXT);
  private_ptr->OnTextCommitted(committed_text);
  EXPECT_EQ(0, mock_text_input_client.call_count_insert_char());
  EXPECT_EQ(0, mock_text_input_client.call_count_insert_text());
  EXPECT_EQ(L"", mock_text_input_client.inserted_text());
  delegate_.Reset();
  mock_text_input_client.Reset();

  input_method->SetFocusedTextInputClient(&mock_text_input_client);

  // TextInputClient is now focused here.

  mock_text_input_client.set_text_input_type(TEXT_INPUT_TYPE_TEXT);
  private_ptr->OnTextCommitted(committed_text);
  EXPECT_EQ(0, mock_text_input_client.call_count_insert_char());
  EXPECT_EQ(1, mock_text_input_client.call_count_insert_text());
  EXPECT_EQ(committed_text, mock_text_input_client.inserted_text());
  delegate_.Reset();
  mock_text_input_client.Reset();

  // When TextInputType is TEXT_INPUT_TYPE_NONE, TextInputClient::InsertText
  // should not be used.
  mock_text_input_client.set_text_input_type(TEXT_INPUT_TYPE_NONE);
  private_ptr->OnTextCommitted(committed_text);
  EXPECT_EQ(committed_text.size(),
            mock_text_input_client.call_count_insert_char());
  EXPECT_EQ(0, mock_text_input_client.call_count_insert_text());
  EXPECT_EQ(committed_text, mock_text_input_client.inserted_text());
  delegate_.Reset();
  mock_text_input_client.Reset();
}

TEST(RemoteInputMethodWinTest, OnTextInputStateChanged_Observer) {
  DummyTextInputClient text_input_client;
  DummyTextInputClient text_input_client_the_other;

  MockInputMethodObserver input_method_observer;
  MockInputMethodDelegate delegate_;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));
  InputMethodScopedObserver scoped_observer(&input_method_observer);
  scoped_observer.Add(input_method.get());

  input_method->SetFocusedTextInputClient(&text_input_client);
  ASSERT_EQ(&text_input_client, input_method->GetTextInputClient());
  EXPECT_EQ(1u, input_method_observer.on_text_input_state_changed());
  input_method_observer.Reset();

  input_method->SetFocusedTextInputClient(&text_input_client);
  ASSERT_EQ(&text_input_client, input_method->GetTextInputClient());
  EXPECT_EQ(0u, input_method_observer.on_text_input_state_changed());
  input_method_observer.Reset();

  input_method->SetFocusedTextInputClient(&text_input_client_the_other);
  ASSERT_EQ(&text_input_client_the_other, input_method->GetTextInputClient());
  EXPECT_EQ(1u, input_method_observer.on_text_input_state_changed());
  input_method_observer.Reset();

  input_method->DetachTextInputClient(&text_input_client_the_other);
  ASSERT_TRUE(input_method->GetTextInputClient() == NULL);
  EXPECT_EQ(1u, input_method_observer.on_text_input_state_changed());
  input_method_observer.Reset();
}

TEST(RemoteInputMethodWinTest, OnInputMethodDestroyed_Observer) {
  DummyTextInputClient text_input_client;
  DummyTextInputClient text_input_client_the_other;

  MockInputMethodObserver input_method_observer;
  InputMethodScopedObserver scoped_observer(&input_method_observer);

  MockInputMethodDelegate delegate_;
  scoped_ptr<InputMethod> input_method(CreateRemoteInputMethodWin(&delegate_));
  input_method->AddObserver(&input_method_observer);

  EXPECT_EQ(0u, input_method_observer.on_input_method_destroyed_changed());
  input_method.reset();
  EXPECT_EQ(1u, input_method_observer.on_input_method_destroyed_changed());
}

}  // namespace
}  // namespace ui
