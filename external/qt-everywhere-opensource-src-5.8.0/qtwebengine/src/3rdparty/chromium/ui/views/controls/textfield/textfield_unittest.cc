// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/render_text.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/textfield/textfield_model.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/base/ime/linux/text_edit_key_bindings_delegate_auralinux.h"
#endif

#if defined(USE_X11)
#include "ui/events/event_utils.h"
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::WideToUTF16;

#define EXPECT_STR_EQ(ascii, utf16) EXPECT_EQ(ASCIIToUTF16(ascii), utf16)

namespace {

const base::char16 kHebrewLetterSamekh = 0x05E1;

class MockInputMethod : public ui::InputMethodBase {
 public:
  MockInputMethod();
  ~MockInputMethod() override;

  // Overridden from InputMethod:
  bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                NativeEventResult* result) override;
  void DispatchKeyEvent(ui::KeyEvent* key) override;
  void OnTextInputTypeChanged(const ui::TextInputClient* client) override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void CancelComposition(const ui::TextInputClient* client) override;
  void OnInputLocaleChanged() override {}
  std::string GetInputLocale() override;
  bool IsCandidatePopupOpen() const override;
  void ShowImeIfNeeded() override {}

  bool untranslated_ime_message_called() const {
    return untranslated_ime_message_called_;
  }
  bool text_input_type_changed() const { return text_input_type_changed_; }
  bool cancel_composition_called() const { return cancel_composition_called_; }

  // Clears all internal states and result.
  void Clear();

  void SetCompositionTextForNextKey(const ui::CompositionText& composition);
  void SetResultTextForNextKey(const base::string16& result);

 private:
  // Overridden from InputMethodBase.
  void OnWillChangeFocusedClient(ui::TextInputClient* focused_before,
                                 ui::TextInputClient* focused) override;

  // Clears boolean states defined below.
  void ClearStates();

  // Whether a mock composition or result is scheduled for the next key event.
  bool HasComposition();

  // Clears only composition information and result text.
  void ClearComposition();

  // Composition information for the next key event. It'll be cleared
  // automatically after dispatching the next key event.
  ui::CompositionText composition_;

  // Result text for the next key event. It'll be cleared automatically after
  // dispatching the next key event.
  base::string16 result_text_;

  // Record call state of corresponding methods. They will be set to false
  // automatically before dispatching a key event.
  bool untranslated_ime_message_called_;
  bool text_input_type_changed_;
  bool cancel_composition_called_;

  DISALLOW_COPY_AND_ASSIGN(MockInputMethod);
};

MockInputMethod::MockInputMethod()
    : untranslated_ime_message_called_(false),
      text_input_type_changed_(false),
      cancel_composition_called_(false) {
}

MockInputMethod::~MockInputMethod() {
}

bool MockInputMethod::OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                               NativeEventResult* result) {
  if (result)
    *result = NativeEventResult();
  return false;
}

void MockInputMethod::DispatchKeyEvent(ui::KeyEvent* key) {
// On Mac, emulate InputMethodMac behavior for character events. Composition
// still needs to be mocked, since it's not possible to generate test events
// which trigger the appropriate NSResponder action messages for composition.
#if defined(OS_MACOSX)
  if (key->is_char()) {
    ignore_result(DispatchKeyEventPostIME(key));
    return;
  }
#endif

  // Checks whether the key event is from EventGenerator on Windows which will
  // generate key event for WM_CHAR.
  // The MockInputMethod will insert char on WM_KEYDOWN so ignore WM_CHAR here.
  if (key->is_char() && key->HasNativeEvent()) {
    key->SetHandled();
    return;
  }

  bool handled = !IsTextInputTypeNone() && HasComposition();
  ClearStates();
  if (handled) {
    DCHECK(!key->is_char());
    ui::KeyEvent mock_key(ui::ET_KEY_PRESSED,
                          ui::VKEY_PROCESSKEY,
                          key->flags());
    DispatchKeyEventPostIME(&mock_key);
  } else {
    DispatchKeyEventPostIME(key);
  }

  ui::TextInputClient* client = GetTextInputClient();
  if (client) {
    if (handled) {
      if (result_text_.length())
        client->InsertText(result_text_);
      if (composition_.text.length())
        client->SetCompositionText(composition_);
      else
        client->ClearCompositionText();
    } else if (key->type() == ui::ET_KEY_PRESSED) {
      base::char16 ch = key->GetCharacter();
      if (ch)
        client->InsertChar(*key);
    }
  }

  ClearComposition();
}

void MockInputMethod::OnTextInputTypeChanged(
    const ui::TextInputClient* client) {
  if (IsTextInputClientFocused(client))
    text_input_type_changed_ = true;
  InputMethodBase::OnTextInputTypeChanged(client);
}

void MockInputMethod::CancelComposition(const ui::TextInputClient* client) {
  if (IsTextInputClientFocused(client)) {
    cancel_composition_called_ = true;
    ClearComposition();
  }
}

std::string MockInputMethod::GetInputLocale() {
  return "en-US";
}

bool MockInputMethod::IsCandidatePopupOpen() const {
  return false;
}

void MockInputMethod::OnWillChangeFocusedClient(
    ui::TextInputClient* focused_before,
    ui::TextInputClient* focused) {
  ui::TextInputClient* client = GetTextInputClient();
  if (client && client->HasCompositionText())
    client->ConfirmCompositionText();
  ClearComposition();
}

void MockInputMethod::Clear() {
  ClearStates();
  ClearComposition();
}

void MockInputMethod::SetCompositionTextForNextKey(
    const ui::CompositionText& composition) {
  composition_ = composition;
}

void MockInputMethod::SetResultTextForNextKey(const base::string16& result) {
  result_text_ = result;
}

void MockInputMethod::ClearStates() {
  untranslated_ime_message_called_ = false;
  text_input_type_changed_ = false;
  cancel_composition_called_ = false;
}

bool MockInputMethod::HasComposition() {
  return composition_.text.length() || result_text_.length();
}

void MockInputMethod::ClearComposition() {
  composition_.Clear();
  result_text_.clear();
}

// A Textfield wrapper to intercept OnKey[Pressed|Released]() ressults.
class TestTextfield : public views::Textfield {
 public:
  TestTextfield()
     : Textfield(),
       key_handled_(false),
       key_received_(false),
       weak_ptr_factory_(this) {}

  bool OnKeyPressed(const ui::KeyEvent& e) override {
    key_received_ = true;

    // Since OnKeyPressed() might destroy |this|, get a weak pointer and
    // verify it isn't null before writing the bool value to key_handled_.
    base::WeakPtr<TestTextfield> textfield(weak_ptr_factory_.GetWeakPtr());
    bool key = views::Textfield::OnKeyPressed(e);

    if (!textfield)
      return key;

    key_handled_ = key;

    return key_handled_;
  }

  bool OnKeyReleased(const ui::KeyEvent& e) override {
    key_received_ = true;
    key_handled_ = views::Textfield::OnKeyReleased(e);
    EXPECT_FALSE(key_handled_);  // Textfield doesn't override OnKeyReleased.
    return key_handled_;
  }

  // ui::TextInputClient overrides:
  void InsertChar(const ui::KeyEvent& e) override {
    views::Textfield::InsertChar(e);
#if defined(OS_MACOSX)
    // On Mac, characters are inserted directly rather than attempting to get a
    // unicode character from the ui::KeyEvent (which isn't always possible).
    key_received_ = true;
#endif
  }

  bool key_handled() const { return key_handled_; }
  bool key_received() const { return key_received_; }

  void clear() { key_received_ = key_handled_ = false; }

 private:
  bool key_handled_;
  bool key_received_;

  base::WeakPtrFactory<TestTextfield> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestTextfield);
};

// Convenience to make constructing a GestureEvent simpler.
class GestureEventForTest : public ui::GestureEvent {
 public:
  GestureEventForTest(int x, int y, ui::GestureEventDetails details)
      : GestureEvent(x, y, 0, base::TimeTicks(), details) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureEventForTest);
};

// This controller will happily destroy the target textfield passed on
// construction when a key event is triggered.
class TextfieldDestroyerController : public views::TextfieldController {
 public:
  explicit TextfieldDestroyerController(views::Textfield* target)
      : target_(target) {
    target_->set_controller(this);
  }

  views::Textfield* target() { return target_.get(); }

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (target_)
      target_->OnBlur();
    target_.reset();
    return false;
  }

 private:
  std::unique_ptr<views::Textfield> target_;
};

base::string16 GetClipboardText(ui::ClipboardType type) {
  base::string16 text;
  ui::Clipboard::GetForCurrentThread()->ReadText(type, &text);
  return text;
}

void SetClipboardText(ui::ClipboardType type, const std::string& text) {
  ui::ScopedClipboardWriter(type).WriteText(ASCIIToUTF16(text));
}

}  // namespace

namespace views {

class TextfieldTest : public ViewsTestBase, public TextfieldController {
 public:
  TextfieldTest()
      : widget_(NULL),
        textfield_(NULL),
        model_(NULL),
        input_method_(NULL),
        on_before_user_action_(0),
        on_after_user_action_(0),
        copied_to_clipboard_(ui::CLIPBOARD_TYPE_LAST) {
    input_method_ = new MockInputMethod();
    ui::SetUpInputMethodForTesting(input_method_);
  }

  // ::testing::Test:
  void TearDown() override {
    if (widget_)
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  ui::ClipboardType GetAndResetCopiedToClipboard() {
    ui::ClipboardType clipboard_type = copied_to_clipboard_;
    copied_to_clipboard_ = ui::CLIPBOARD_TYPE_LAST;
    return clipboard_type;
  }

  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const base::string16& new_contents) override {
    // Paste calls TextfieldController::ContentsChanged() explicitly even if the
    // paste action did not change the content. So |new_contents| may match
    // |last_contents_|. For more info, see http://crbug.com/79002
    last_contents_ = new_contents;
  }

  void OnBeforeUserAction(Textfield* sender) override {
    ++on_before_user_action_;
  }

  void OnAfterUserAction(Textfield* sender) override {
    ++on_after_user_action_;
  }

  void OnAfterCutOrCopy(ui::ClipboardType clipboard_type) override {
    copied_to_clipboard_ = clipboard_type;
  }

  void InitTextfield() {
    InitTextfields(1);
  }

  void InitTextfields(int count) {
    ASSERT_FALSE(textfield_);
    textfield_ = new TestTextfield();
    textfield_->set_controller(this);
    widget_ = new Widget();

    // The widget type must be an activatable type, and we don't want to worry
    // about the non-client view, which leaves just TYPE_WINDOW_FRAMELESS.
    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);

    params.bounds = gfx::Rect(100, 100, 100, 100);
    widget_->Init(params);
    input_method_->SetDelegate(
        test::WidgetTest::GetInputMethodDelegateForWidget(widget_));
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(textfield_);
    textfield_->SetBoundsRect(params.bounds);
    textfield_->set_id(1);
    test_api_.reset(new TextfieldTestApi(textfield_));

    for (int i = 1; i < count; i++) {
      Textfield* textfield = new Textfield();
      container->AddChildView(textfield);
      textfield->set_id(i + 1);
    }

    model_ = test_api_->model();
    model_->ClearEditHistory();

    // Since the window type is activatable, showing the widget will also
    // activate it. Calling Activate directly is insufficient, since that does
    // not also _focus_ an aura::Window (i.e. using the FocusClient). Both the
    // widget and the textfield must have focus to properly handle input.
    widget_->Show();
    textfield_->RequestFocus();

    event_generator_.reset(
        new ui::test::EventGenerator(widget_->GetNativeWindow()));
  }
  ui::MenuModel* GetContextMenuModel() {
    test_api_->UpdateContextMenu();
    return test_api_->context_menu_contents();
  }

  // True if native Mac keystrokes should be used (to avoid ifdef litter).
  bool TestingNativeMac() {
#if defined(OS_MACOSX)
    return true;
#else
    return false;
#endif
  }

  bool TestingNativeCrOs() const {
#if defined(OS_CHROMEOS)
    return true;
#else
    return false;
#endif  // defined(OS_CHROMEOS)
  }

 protected:
  void SendKeyPress(ui::KeyboardCode key_code, int flags) {
    event_generator_->PressKey(key_code, flags);
  }

  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool alt,
                    bool shift,
                    bool control_or_command,
                    bool caps_lock) {
    bool control = control_or_command;
    bool command = false;

    // By default, swap control and command for native events on Mac. This
    // handles most cases.
    if (TestingNativeMac())
      std::swap(control, command);

    int flags = (shift ? ui::EF_SHIFT_DOWN : 0) |
                (control ? ui::EF_CONTROL_DOWN : 0) |
                (alt ? ui::EF_ALT_DOWN : 0) |
                (command ? ui::EF_COMMAND_DOWN : 0) |
                (caps_lock ? ui::EF_CAPS_LOCK_ON : 0);

    SendKeyPress(key_code, flags);
  }

  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool shift,
                    bool control_or_command) {
    SendKeyEvent(key_code, false, shift, control_or_command, false);
  }

  void SendKeyEvent(ui::KeyboardCode key_code) {
    SendKeyEvent(key_code, false, false);
  }

  void SendKeyEvent(base::char16 ch) {
    if (ch < 0x80) {
      ui::KeyboardCode code =
          ch == ' ' ? ui::VKEY_SPACE :
          static_cast<ui::KeyboardCode>(ui::VKEY_A + ch - 'a');
      SendKeyEvent(code);
    } else {
      // For unicode characters, assume they come from IME rather than the
      // keyboard. So they are dispatched directly to the input method. But on
      // Mac, key events don't pass through InputMethod. Hence they are
      // dispatched regularly.
      ui::KeyEvent event(ch, ui::VKEY_UNKNOWN, ui::EF_NONE);
#if defined(OS_MACOSX)
      event_generator_->Dispatch(&event);
#else
      input_method_->DispatchKeyEvent(&event);
#endif
    }
  }

  // Sends a platform-specific move (and select) to the logical start of line.
  // Eg. this should move (and select) to the right end of line for RTL text.
  void SendHomeEvent(bool shift) {
    if (TestingNativeMac()) {
      // [NSResponder moveToBeginningOfLine:] is the correct way to do this on
      // Mac, but that doesn't have a default key binding. Since
      // views::Textfield doesn't currently support multiple lines, the same
      // effect can be achieved by Cmd+Up which maps to
      // [NSResponder moveToBeginningOfDocument:].
      SendKeyEvent(ui::VKEY_UP, shift /* shift */, true /* command */);
      return;
    }
    SendKeyEvent(ui::VKEY_HOME, shift /* shift */, false /* control */);
  }

  // Sends a platform-specific move (and select) to the logical end of line.
  void SendEndEvent(bool shift) {
    if (TestingNativeMac()) {
      SendKeyEvent(ui::VKEY_DOWN, shift, true);  // Cmd+Down.
      return;
    }
    SendKeyEvent(ui::VKEY_END, shift, false);
  }

  // Sends {delete, move, select} word {forward, backward}.
  void SendWordEvent(ui::KeyboardCode key, bool shift) {
    bool alt = false;
    bool control = true;
    bool caps = false;
    if (TestingNativeMac()) {
      // Use Alt+Left/Right/Backspace on native Mac.
      alt = true;
      control = false;
    }
    SendKeyEvent(key, alt, shift, control, caps);
  }

  // Sends Shift+Delete if supported, otherwise Cmd+X again.
  void SendAlternateCut() {
    if (TestingNativeMac())
      SendKeyEvent(ui::VKEY_X, false, true);
    else
      SendKeyEvent(ui::VKEY_DELETE, true, false);
  }

  // Sends Ctrl+Insert if supported, otherwise Cmd+C again.
  void SendAlternateCopy() {
    if (TestingNativeMac())
      SendKeyEvent(ui::VKEY_C, false, true);
    else
      SendKeyEvent(ui::VKEY_INSERT, false, true);
  }

  // Sends Shift+Insert if supported, otherwise Cmd+V again.
  void SendAlternatePaste() {
    if (TestingNativeMac())
      SendKeyEvent(ui::VKEY_V, false, true);
    else
      SendKeyEvent(ui::VKEY_INSERT, true, false);
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  int GetCursorPositionX(int cursor_pos) {
    return test_api_->GetRenderText()->GetCursorBounds(
        gfx::SelectionModel(cursor_pos, gfx::CURSOR_FORWARD), false).x();
  }

  // Get the current cursor bounds.
  gfx::Rect GetCursorBounds() {
    return test_api_->GetRenderText()->GetUpdatedCursorBounds();
  }

  // Get the cursor bounds of |sel|.
  gfx::Rect GetCursorBounds(const gfx::SelectionModel& sel) {
    return test_api_->GetRenderText()->GetCursorBounds(sel, true);
  }

  gfx::Rect GetDisplayRect() {
    return test_api_->GetRenderText()->display_rect();
  }

  // Mouse click on the point whose x-axis is |bound|'s x plus |x_offset| and
  // y-axis is in the middle of |bound|'s vertical range.
  void MouseClick(const gfx::Rect bound, int x_offset) {
    gfx::Point point(bound.x() + x_offset, bound.y() + bound.height() / 2);
    ui::MouseEvent click(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMousePressed(click);
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, point, point,
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMouseReleased(release);
  }

  // This is to avoid double/triple click.
  void NonClientMouseClick() {
    ui::MouseEvent click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_NON_CLIENT,
                         ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMousePressed(click);
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(),
                           ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_NON_CLIENT,
                           ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMouseReleased(release);
  }

  void VerifyTextfieldContextMenuContents(bool textfield_has_selection,
                                          bool can_undo,
                                          ui::MenuModel* menu) {
    EXPECT_EQ(can_undo, menu->IsEnabledAt(0 /* UNDO */));
    EXPECT_TRUE(menu->IsEnabledAt(1 /* Separator */));
    EXPECT_EQ(textfield_has_selection, menu->IsEnabledAt(2 /* CUT */));
    EXPECT_EQ(textfield_has_selection, menu->IsEnabledAt(3 /* COPY */));
    EXPECT_NE(GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE).empty(),
              menu->IsEnabledAt(4 /* PASTE */));
    EXPECT_EQ(textfield_has_selection, menu->IsEnabledAt(5 /* DELETE */));
    EXPECT_TRUE(menu->IsEnabledAt(6 /* Separator */));
    EXPECT_TRUE(menu->IsEnabledAt(7 /* SELECT ALL */));
  }

  void PressLeftMouseButton(int extra_flags) {
    ui::MouseEvent click(ui::ET_MOUSE_PRESSED, mouse_position_, mouse_position_,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON | extra_flags);
    textfield_->OnMousePressed(click);
  }

  void PressLeftMouseButton() {
    PressLeftMouseButton(0);
  }

  void ReleaseLeftMouseButton() {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, mouse_position_,
                           mouse_position_, ui::EventTimeForNow(),
                           ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMouseReleased(release);
  }

  void ClickLeftMouseButton(int extra_flags) {
    PressLeftMouseButton(extra_flags);
    ReleaseLeftMouseButton();
  }

  void ClickLeftMouseButton() {
    ClickLeftMouseButton(0);
  }

  void DragMouseTo(const gfx::Point& where) {
    mouse_position_ = where;
    ui::MouseEvent drag(ui::ET_MOUSE_DRAGGED, where, where,
                        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    textfield_->OnMouseDragged(drag);
  }

  // Textfield does not listen to OnMouseMoved, so this function does not send
  // an event when it updates the cursor position.
  void MoveMouseTo(const gfx::Point& where) { mouse_position_ = where; }

  // We need widget to populate wrapper class.
  Widget* widget_;

  TestTextfield* textfield_;
  std::unique_ptr<TextfieldTestApi> test_api_;
  TextfieldModel* model_;

  // The string from Controller::ContentsChanged callback.
  base::string16 last_contents_;

  // For testing input method related behaviors.
  MockInputMethod* input_method_;

  // Indicates how many times OnBeforeUserAction() is called.
  int on_before_user_action_;

  // Indicates how many times OnAfterUserAction() is called.
  int on_after_user_action_;

 private:
  // Position of the mouse for synthetic mouse events.
  gfx::Point mouse_position_;
  ui::ClipboardType copied_to_clipboard_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  DISALLOW_COPY_AND_ASSIGN(TextfieldTest);
};

TEST_F(TextfieldTest, ModelChangesTest) {
  InitTextfield();

  // TextfieldController::ContentsChanged() shouldn't be called when changing
  // text programmatically.
  last_contents_.clear();
  textfield_->SetText(ASCIIToUTF16("this is"));

  EXPECT_STR_EQ("this is", model_->text());
  EXPECT_STR_EQ("this is", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());

  textfield_->AppendText(ASCIIToUTF16(" a test"));
  EXPECT_STR_EQ("this is a test", model_->text());
  EXPECT_STR_EQ("this is a test", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());

  EXPECT_EQ(base::string16(), textfield_->GetSelectedText());
  textfield_->SelectAll(false);
  EXPECT_STR_EQ("this is a test", textfield_->GetSelectedText());
  EXPECT_TRUE(last_contents_.empty());
}

TEST_F(TextfieldTest, KeyTest) {
  InitTextfield();
  // Event flags:  key,    alt,   shift, ctrl,  caps-lock.
  SendKeyEvent(ui::VKEY_T, false, true,  false, false);
  SendKeyEvent(ui::VKEY_E, false, false, false, false);
  SendKeyEvent(ui::VKEY_X, false, true,  false, true);
  SendKeyEvent(ui::VKEY_T, false, false, false, true);
  SendKeyEvent(ui::VKEY_1, false, true,  false, false);
  SendKeyEvent(ui::VKEY_1, false, false, false, false);
  SendKeyEvent(ui::VKEY_1, false, true,  false, true);
  SendKeyEvent(ui::VKEY_1, false, false, false, true);

  // On Mac, Caps+Shift remains uppercase.
  if (TestingNativeMac())
    EXPECT_STR_EQ("TeXT!1!1", textfield_->text());
  else
    EXPECT_STR_EQ("TexT!1!1", textfield_->text());
}

TEST_F(TextfieldTest, KeysWithModifiersTest) {
  InitTextfield();
  const int ctrl = ui::EF_CONTROL_DOWN;
  const int alt = ui::EF_ALT_DOWN;
  const int command = ui::EF_COMMAND_DOWN;
  const int altgr = ui::EF_ALTGR_DOWN;
  const int shift = ui::EF_SHIFT_DOWN;

  SendKeyPress(ui::VKEY_T, shift | alt | altgr);
  SendKeyPress(ui::VKEY_H, alt);
  SendKeyPress(ui::VKEY_E, altgr);
  SendKeyPress(ui::VKEY_T, shift);
  SendKeyPress(ui::VKEY_E, shift | altgr);
  SendKeyPress(ui::VKEY_X, 0);
  SendKeyPress(ui::VKEY_T, ctrl);  // This causes transpose on Mac.
  SendKeyPress(ui::VKEY_1, alt);
  SendKeyPress(ui::VKEY_2, command);
  SendKeyPress(ui::VKEY_3, 0);
  SendKeyPress(ui::VKEY_4, 0);
  SendKeyPress(ui::VKEY_OEM_PLUS, ctrl);
  SendKeyPress(ui::VKEY_OEM_PLUS, ctrl | shift);
  SendKeyPress(ui::VKEY_OEM_MINUS, ctrl);
  SendKeyPress(ui::VKEY_OEM_MINUS, ctrl | shift);

  if (TestingNativeCrOs())
    EXPECT_STR_EQ("TeTEx34", textfield_->text());
  else if (TestingNativeMac())
    EXPECT_STR_EQ("TheTxE134", textfield_->text());
  else
    EXPECT_STR_EQ("TeTEx234", textfield_->text());
}

TEST_F(TextfieldTest, ControlAndSelectTest) {
  // Insert a test string in a textfield.
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("one two three"));
  SendHomeEvent(false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);

  EXPECT_STR_EQ("one", textfield_->GetSelectedText());

  // Test word select.
  SendWordEvent(ui::VKEY_RIGHT, true);
  EXPECT_STR_EQ("one two", textfield_->GetSelectedText());
  SendWordEvent(ui::VKEY_RIGHT, true);
  EXPECT_STR_EQ("one two three", textfield_->GetSelectedText());
  SendWordEvent(ui::VKEY_LEFT, true);
  EXPECT_STR_EQ("one two ", textfield_->GetSelectedText());
  SendWordEvent(ui::VKEY_LEFT, true);
  EXPECT_STR_EQ("one ", textfield_->GetSelectedText());

  // Replace the selected text.
  SendKeyEvent(ui::VKEY_Z, true, false);
  SendKeyEvent(ui::VKEY_E, true, false);
  SendKeyEvent(ui::VKEY_R, true, false);
  SendKeyEvent(ui::VKEY_O, true, false);
  SendKeyEvent(ui::VKEY_SPACE, false, false);
  EXPECT_STR_EQ("ZERO two three", textfield_->text());

  SendEndEvent(true);
  EXPECT_STR_EQ("two three", textfield_->GetSelectedText());
  SendHomeEvent(true);
  EXPECT_STR_EQ("ZERO ", textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, InsertionDeletionTest) {
  // Insert a test string in a textfield.
  InitTextfield();
  for (size_t i = 0; i < 10; i++)
    SendKeyEvent(static_cast<ui::KeyboardCode>(ui::VKEY_A + i));
  EXPECT_STR_EQ("abcdefghij", textfield_->text());

  // Test the delete and backspace keys.
  textfield_->SelectRange(gfx::Range(5));
  for (int i = 0; i < 3; i++)
    SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("abfghij", textfield_->text());
  for (int i = 0; i < 3; i++)
    SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("abij", textfield_->text());

  // Select all and replace with "k".
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_K);
  EXPECT_STR_EQ("k", textfield_->text());

  // Delete the previous word from cursor.
  bool shift = false;
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  SendEndEvent(shift);
  SendWordEvent(ui::VKEY_BACK, shift);
  EXPECT_STR_EQ("one two three ", textfield_->text());

  // Delete to a line break on Linux and ChromeOS, to a word break on Windows
  // and Mac.
  SendWordEvent(ui::VKEY_LEFT, shift);
  shift = true;
  SendWordEvent(ui::VKEY_BACK, shift);
#if defined(OS_LINUX)
  EXPECT_STR_EQ("three ", textfield_->text());
#else
  EXPECT_STR_EQ("one three ", textfield_->text());
#endif

  // Delete the next word from cursor.
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  shift = false;
  SendHomeEvent(shift);
  SendWordEvent(ui::VKEY_DELETE, shift);
  EXPECT_STR_EQ(" two three four", textfield_->text());

  // Delete to a line break on Linux and ChromeOS, to a word break on Windows
  // and Mac.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  shift = true;
  SendWordEvent(ui::VKEY_DELETE, shift);
#if defined(OS_LINUX)
  EXPECT_STR_EQ(" two", textfield_->text());
#else
  EXPECT_STR_EQ(" two four", textfield_->text());
#endif
}

// Test that deletion operations behave correctly with an active selection.
TEST_F(TextfieldTest, DeletionWithSelection) {
  struct {
    ui::KeyboardCode key;
    bool shift;
  } cases[] = {
      {ui::VKEY_BACK, false},
      {ui::VKEY_BACK, true},
      {ui::VKEY_DELETE, false},
      {ui::VKEY_DELETE, true},
  };

  InitTextfield();
  // [Ctrl] ([Alt] on Mac) + [Delete]/[Backspace] should delete the active
  // selection, regardless of [Shift].
  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    textfield_->SetText(ASCIIToUTF16("one two three"));
    textfield_->SelectRange(gfx::Range(2, 6));
    // Make selection as - on|e tw|o three.
    SendWordEvent(cases[i].key, cases[i].shift);
    // Verify state is on|o three.
    EXPECT_STR_EQ("ono three", textfield_->text());
    EXPECT_EQ(gfx::Range(2), textfield_->GetSelectedRange());
  }
}

TEST_F(TextfieldTest, PasswordTest) {
  InitTextfield();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());
  EXPECT_TRUE(textfield_->enabled());
  EXPECT_TRUE(textfield_->IsFocusable());

  last_contents_.clear();
  textfield_->SetText(ASCIIToUTF16("password"));
  // Ensure text() and the callback returns the actual text instead of "*".
  EXPECT_STR_EQ("password", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());
  model_->SelectAll(false);
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "foo");

  // Cut and copy should be disabled.
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_->ExecuteCommand(IDS_APP_CUT, 0);
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_->ExecuteCommand(IDS_APP_COPY, 0);
  SendKeyEvent(ui::VKEY_C, false, true);
  SendAlternateCopy();
  EXPECT_STR_EQ("foo", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("password", textfield_->text());
  // [Shift]+[Delete] should just delete without copying text to the clipboard.
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_DELETE, true, false);

  // Paste should work normally.
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_->ExecuteCommand(IDS_APP_PASTE, 0);
  SendKeyEvent(ui::VKEY_V, false, true);
  SendAlternatePaste();
  EXPECT_STR_EQ("foo", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("foofoofoo", textfield_->text());
}

// Check that text insertion works appropriately for password and read-only
// textfields.
TEST_F(TextfieldTest, TextInputType_InsertionTest) {
  InitTextfield();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());

  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(kHebrewLetterSamekh);
  SendKeyEvent(ui::VKEY_B);

  EXPECT_EQ(WideToUTF16(L"a\x05E1"
                        L"b"),
            textfield_->text());

  textfield_->SetReadOnly(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, textfield_->GetTextInputType());
  SendKeyEvent(ui::VKEY_C);

  // No text should be inserted for read only textfields.
  EXPECT_EQ(WideToUTF16(L"a\x05E1"
                        L"b"),
            textfield_->text());
}

TEST_F(TextfieldTest, TextInputType) {
  InitTextfield();

  // Defaults to TEXT
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, textfield_->GetTextInputType());

  // And can be set.
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_URL, textfield_->GetTextInputType());
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());

  // Readonly textfields have type NONE
  textfield_->SetReadOnly(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, textfield_->GetTextInputType());

  textfield_->SetReadOnly(false);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());

  // As do disabled textfields
  textfield_->SetEnabled(false);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, textfield_->GetTextInputType());

  textfield_->SetEnabled(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());
}

TEST_F(TextfieldTest, OnKeyPress) {
  InitTextfield();

  // Character keys are handled by the input method.
  SendKeyEvent(ui::VKEY_A);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  // Arrow keys and home/end are handled by the textfield.
  SendKeyEvent(ui::VKEY_LEFT);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  const bool shift = false;
  SendHomeEvent(shift);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  SendEndEvent(shift);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  // F20 key won't be handled.
  SendKeyEvent(ui::VKEY_F20);
#if defined(OS_MACOSX)
  // On Mac, key combinations that don't map to editing commands are forwarded
  // on to the next responder, usually ending up at the window, which will beep.
  EXPECT_FALSE(textfield_->key_received());
#else
  EXPECT_TRUE(textfield_->key_received());
#endif
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  // Up/Down keys won't be handled except on Mac where they map to move
  // commands.
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_TRUE(textfield_->key_received());
#if defined(OS_MACOSX)
  EXPECT_TRUE(textfield_->key_handled());
#else
  EXPECT_FALSE(textfield_->key_handled());
#endif
  textfield_->clear();

  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(textfield_->key_received());
#if defined(OS_MACOSX)
  EXPECT_TRUE(textfield_->key_handled());
#else
  EXPECT_FALSE(textfield_->key_handled());
#endif
  textfield_->clear();
}

// Tests that default key bindings are handled even with a delegate installed.
TEST_F(TextfieldTest, OnKeyPressBinding) {
  InitTextfield();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Install a TextEditKeyBindingsDelegateAuraLinux that does nothing.
  class TestDelegate : public ui::TextEditKeyBindingsDelegateAuraLinux {
   public:
    TestDelegate() {}
    ~TestDelegate() override {}

    bool MatchEvent(
        const ui::Event& event,
        std::vector<ui::TextEditCommandAuraLinux>* commands) override {
      return false;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(TestDelegate);
  };

  TestDelegate delegate;
  ui::SetTextEditKeyBindingsDelegate(&delegate);
#endif

  SendKeyEvent(ui::VKEY_A, false, false);
  EXPECT_STR_EQ("a", textfield_->text());
  textfield_->clear();

  // Undo/Redo command keys are handled by the textfield.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_TRUE(textfield_->text().empty());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_STR_EQ("a", textfield_->text());
  textfield_->clear();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  ui::SetTextEditKeyBindingsDelegate(NULL);
#endif
}

TEST_F(TextfieldTest, CursorMovement) {
  InitTextfield();

  // Test with trailing whitespace.
  textfield_->SetText(ASCIIToUTF16("one two hre "));

  // Send the cursor at the end.
  SendKeyEvent(ui::VKEY_END);

  // Ctrl+Left should move the cursor just before the last word.
  const bool shift = false;
  SendWordEvent(ui::VKEY_LEFT, shift);
  SendKeyEvent(ui::VKEY_T);
  EXPECT_STR_EQ("one two thre ", textfield_->text());
  EXPECT_STR_EQ("one two thre ", last_contents_);

  // Ctrl+Right should move the cursor to the end of the last word.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  SendKeyEvent(ui::VKEY_E);
  EXPECT_STR_EQ("one two three ", textfield_->text());
  EXPECT_STR_EQ("one two three ", last_contents_);

  // Ctrl+Right again should move the cursor to the end.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("one two three", textfield_->text());
  EXPECT_STR_EQ("one two three", last_contents_);

  // Test with leading whitespace.
  textfield_->SetText(ASCIIToUTF16(" ne two"));

  // Send the cursor at the beginning.
  SendHomeEvent(shift);

  // Ctrl+Right, then Ctrl+Left should move the cursor to the beginning of the
  // first word.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  SendWordEvent(ui::VKEY_LEFT, shift);
  SendKeyEvent(ui::VKEY_O);
  EXPECT_STR_EQ(" one two", textfield_->text());
  EXPECT_STR_EQ(" one two", last_contents_);

  // Ctrl+Left to move the cursor to the beginning of the first word.
  SendWordEvent(ui::VKEY_LEFT, shift);
  // Ctrl+Left again should move the cursor back to the very beginning.
  SendWordEvent(ui::VKEY_LEFT, shift);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("one two", textfield_->text());
  EXPECT_STR_EQ("one two", last_contents_);
}

TEST_F(TextfieldTest, FocusTraversalTest) {
  InitTextfields(3);
  textfield_->RequestFocus();

  EXPECT_EQ(1, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(2, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(3, GetFocusedView()->id());
  // Cycle back to the first textfield.
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(1, GetFocusedView()->id());

  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(2, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(1, GetFocusedView()->id());
  // Cycle back to the last textfield.
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->id());

  // Request focus should still work.
  textfield_->RequestFocus();
  EXPECT_EQ(1, GetFocusedView()->id());

  // Test if clicking on textfield view sets the focus.
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->id());
  MoveMouseTo(gfx::Point());
  ClickLeftMouseButton();
  EXPECT_EQ(1, GetFocusedView()->id());

  // Tab/Shift+Tab should also cycle focus, not insert a tab character.
  SendKeyEvent(ui::VKEY_TAB, false, false);
  EXPECT_EQ(2, GetFocusedView()->id());
  SendKeyEvent(ui::VKEY_TAB, false, false);
  EXPECT_EQ(3, GetFocusedView()->id());
  // Cycle back to the first textfield.
  SendKeyEvent(ui::VKEY_TAB, false, false);
  EXPECT_EQ(1, GetFocusedView()->id());

  SendKeyEvent(ui::VKEY_TAB, true, false);
  EXPECT_EQ(3, GetFocusedView()->id());
  SendKeyEvent(ui::VKEY_TAB, true, false);
  EXPECT_EQ(2, GetFocusedView()->id());
  SendKeyEvent(ui::VKEY_TAB, true, false);
  EXPECT_EQ(1, GetFocusedView()->id());
  // Cycle back to the last textfield.
  SendKeyEvent(ui::VKEY_TAB, true, false);
  EXPECT_EQ(3, GetFocusedView()->id());
}

TEST_F(TextfieldTest, ContextMenuDisplayTest) {
  InitTextfield();
  EXPECT_TRUE(textfield_->context_menu_controller());
  textfield_->SetText(ASCIIToUTF16("hello world"));
  ui::Clipboard::GetForCurrentThread()->Clear(ui::CLIPBOARD_TYPE_COPY_PASTE);
  textfield_->ClearEditHistory();
  EXPECT_TRUE(GetContextMenuModel());
  VerifyTextfieldContextMenuContents(false, false, GetContextMenuModel());

  textfield_->SelectAll(false);
  VerifyTextfieldContextMenuContents(true, false, GetContextMenuModel());

  SendKeyEvent(ui::VKEY_T);
  VerifyTextfieldContextMenuContents(false, true, GetContextMenuModel());

  textfield_->SelectAll(false);
  VerifyTextfieldContextMenuContents(true, true, GetContextMenuModel());

  // Exercise the "paste enabled?" check in the verifier.
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "Test");
  VerifyTextfieldContextMenuContents(true, true, GetContextMenuModel());
}

TEST_F(TextfieldTest, DoubleAndTripleClickTest) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  // Test for double click.
  MoveMouseTo(gfx::Point());
  ClickLeftMouseButton();
  EXPECT_TRUE(textfield_->GetSelectedText().empty());
  ClickLeftMouseButton(ui::EF_IS_DOUBLE_CLICK);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());

  // Test for triple click.
  ClickLeftMouseButton();
  EXPECT_STR_EQ("hello world", textfield_->GetSelectedText());

  // Another click should reset back to double click.
  ClickLeftMouseButton();
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, DragToSelect) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  const int kStart = GetCursorPositionX(5);
  const int kEnd = 500;
  gfx::Point start_point(kStart, 0);
  gfx::Point end_point(kEnd, 0);

  MoveMouseTo(start_point);
  PressLeftMouseButton();
  EXPECT_TRUE(textfield_->GetSelectedText().empty());

  // Check that dragging left selects the beginning of the string.
  DragMouseTo(gfx::Point());
  base::string16 text_left = textfield_->GetSelectedText();
  EXPECT_STR_EQ("hello", text_left);

  // Check that dragging right selects the rest of the string.
  DragMouseTo(end_point);
  base::string16 text_right = textfield_->GetSelectedText();
  EXPECT_STR_EQ(" world", text_right);

  // Check that releasing in the same location does not alter the selection.
  ReleaseLeftMouseButton();
  EXPECT_EQ(text_right, textfield_->GetSelectedText());

  // Check that dragging from beyond the text length works too.
  MoveMouseTo(end_point);
  PressLeftMouseButton();
  DragMouseTo(gfx::Point());
  ReleaseLeftMouseButton();
  EXPECT_EQ(textfield_->text(), textfield_->GetSelectedText());
}

// This test checks that dragging above the textfield selects to the beginning
// and dragging below the textfield selects to the end, but only on platforms
// where that is the expected behavior.
TEST_F(TextfieldTest, DragUpOrDownSelectsToEnd) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  const base::string16 expected_up = base::ASCIIToUTF16(
      PlatformStyle::kTextfieldDragVerticallyDragsToEnd ? "hello" : "lo");
  const base::string16 expected_down = base::ASCIIToUTF16(
      PlatformStyle::kTextfieldDragVerticallyDragsToEnd ? " world" : " w");
  const int kStartX = GetCursorPositionX(5);
  const int kDownX = GetCursorPositionX(7);
  const int kUpX = GetCursorPositionX(3);
  gfx::Point start_point(kStartX, 0);
  gfx::Point down_point(kDownX, 500);
  gfx::Point up_point(kUpX, -500);

  MoveMouseTo(start_point);
  PressLeftMouseButton();
  DragMouseTo(up_point);
  ReleaseLeftMouseButton();
  EXPECT_EQ(textfield_->GetSelectedText(), expected_up);

  // Click at |up_point|. This is important because drags do not count as clicks
  // for the purpose of double-click detection, so if this test doesn't click
  // somewhere other than |start_point| before the code below runs, the second
  // click at |start_point| will be interpreted as a double-click instead of the
  // start of a drag.
  ClickLeftMouseButton();

  MoveMouseTo(start_point);
  PressLeftMouseButton();
  DragMouseTo(down_point);
  ReleaseLeftMouseButton();
  EXPECT_EQ(textfield_->GetSelectedText(), expected_down);
}

#if defined(OS_WIN)
TEST_F(TextfieldTest, DragAndDrop_AcceptDrop) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  ui::OSExchangeData data;
  base::string16 string(ASCIIToUTF16("string "));
  data.SetString(string);
  int formats = 0;
  std::set<ui::Clipboard::FormatType> format_types;

  // Ensure that disabled textfields do not accept drops.
  textfield_->SetEnabled(false);
  EXPECT_FALSE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(0, formats);
  EXPECT_TRUE(format_types.empty());
  EXPECT_FALSE(textfield_->CanDrop(data));
  textfield_->SetEnabled(true);

  // Ensure that read-only textfields do not accept drops.
  textfield_->SetReadOnly(true);
  EXPECT_FALSE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(0, formats);
  EXPECT_TRUE(format_types.empty());
  EXPECT_FALSE(textfield_->CanDrop(data));
  textfield_->SetReadOnly(false);

  // Ensure that enabled and editable textfields do accept drops.
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(format_types.empty());
  EXPECT_TRUE(textfield_->CanDrop(data));
  gfx::Point drop_point(GetCursorPositionX(6), 0);
  ui::DropTargetEvent drop(data, drop_point, drop_point,
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE,
            textfield_->OnDragUpdated(drop));
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, textfield_->OnPerformDrop(drop));
  EXPECT_STR_EQ("hello string world", textfield_->text());

  // Ensure that textfields do not accept non-OSExchangeData::STRING types.
  ui::OSExchangeData bad_data;
  bad_data.SetFilename(base::FilePath(FILE_PATH_LITERAL("x")));
  ui::Clipboard::FormatType fmt = ui::Clipboard::GetBitmapFormatType();
  bad_data.SetPickledData(fmt, base::Pickle());
  bad_data.SetFileContents(base::FilePath(L"x"), "x");
  bad_data.SetHtml(base::string16(ASCIIToUTF16("x")), GURL("x.org"));
  ui::OSExchangeData::DownloadFileInfo download(base::FilePath(), NULL);
  bad_data.SetDownloadFileInfo(download);
  EXPECT_FALSE(textfield_->CanDrop(bad_data));
}
#endif

TEST_F(TextfieldTest, DragAndDrop_InitiateDrag) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello string world"));

  // Ensure the textfield will provide selected text for drag data.
  base::string16 string;
  ui::OSExchangeData data;
  const gfx::Range kStringRange(6, 12);
  textfield_->SelectRange(kStringRange);
  const gfx::Point kStringPoint(GetCursorPositionX(9), 0);
  textfield_->WriteDragDataForView(NULL, kStringPoint, &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);

  // Ensure that disabled textfields do not support drag operations.
  textfield_->SetEnabled(false);
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(NULL, kStringPoint));
  textfield_->SetEnabled(true);
  // Ensure that textfields without selections do not support drag operations.
  textfield_->ClearSelection();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(NULL, kStringPoint));
  textfield_->SelectRange(kStringRange);
  // Ensure that password textfields do not support drag operations.
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(NULL, kStringPoint));
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  MoveMouseTo(kStringPoint);
  PressLeftMouseButton();
  // Ensure that textfields only initiate drag operations inside the selection.
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(NULL, gfx::Point()));
  EXPECT_FALSE(textfield_->CanStartDragForView(NULL, gfx::Point(),
                                               gfx::Point()));
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY,
            textfield_->GetDragOperationsForView(NULL, kStringPoint));
  EXPECT_TRUE(textfield_->CanStartDragForView(NULL, kStringPoint,
                                              gfx::Point()));
  // Ensure that textfields support local moves.
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
      textfield_->GetDragOperationsForView(textfield_, kStringPoint));
}

TEST_F(TextfieldTest, DragAndDrop_ToTheRight) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  base::string16 string;
  ui::OSExchangeData data;
  int formats = 0;
  int operations = 0;
  std::set<ui::Clipboard::FormatType> format_types;

  // Start dragging "ello".
  textfield_->SelectRange(gfx::Range(1, 5));
  gfx::Point point(GetCursorPositionX(3), 0);
  MoveMouseTo(point);
  PressLeftMouseButton();
  EXPECT_TRUE(textfield_->CanStartDragForView(textfield_, point, point));
  operations = textfield_->GetDragOperationsForView(textfield_, point);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
            operations);
  textfield_->WriteDragDataForView(nullptr, point, &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(format_types.empty());

  // Drop "ello" after "w".
  const gfx::Point kDropPoint(GetCursorPositionX(7), 0);
  EXPECT_TRUE(textfield_->CanDrop(data));
  ui::DropTargetEvent drop_a(data, kDropPoint, kDropPoint, operations);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop_a));
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnPerformDrop(drop_a));
  EXPECT_STR_EQ("h welloorld", textfield_->text());
  textfield_->OnDragDone();

  // Undo/Redo the drag&drop change.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("h welloorld", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("h welloorld", textfield_->text());
}

TEST_F(TextfieldTest, DragAndDrop_ToTheLeft) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  base::string16 string;
  ui::OSExchangeData data;
  int formats = 0;
  int operations = 0;
  std::set<ui::Clipboard::FormatType> format_types;

  // Start dragging " worl".
  textfield_->SelectRange(gfx::Range(5, 10));
  gfx::Point point(GetCursorPositionX(7), 0);
  MoveMouseTo(point);
  PressLeftMouseButton();
  EXPECT_TRUE(textfield_->CanStartDragForView(textfield_, point, gfx::Point()));
  operations = textfield_->GetDragOperationsForView(textfield_, point);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
            operations);
  textfield_->WriteDragDataForView(nullptr, point, &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(format_types.empty());

  // Drop " worl" after "h".
  EXPECT_TRUE(textfield_->CanDrop(data));
  gfx::Point drop_point(GetCursorPositionX(1), 0);
  ui::DropTargetEvent drop_a(data, drop_point, drop_point, operations);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop_a));
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnPerformDrop(drop_a));
  EXPECT_STR_EQ("h worlellod", textfield_->text());
  textfield_->OnDragDone();

  // Undo/Redo the drag&drop change.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("h worlellod", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("h worlellod", textfield_->text());
}

TEST_F(TextfieldTest, DragAndDrop_Canceled) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  // Start dragging "worl".
  textfield_->SelectRange(gfx::Range(6, 10));
  gfx::Point point(GetCursorPositionX(8), 0);
  MoveMouseTo(point);
  PressLeftMouseButton();
  ui::OSExchangeData data;
  textfield_->WriteDragDataForView(nullptr, point, &data);
  EXPECT_TRUE(textfield_->CanDrop(data));
  // Drag the text over somewhere valid, outside the current selection.
  gfx::Point drop_point(GetCursorPositionX(2), 0);
  ui::DropTargetEvent drop(data, drop_point, drop_point,
                           ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop));
  // "Cancel" the drag, via move and release over the selection, and OnDragDone.
  gfx::Point drag_point(GetCursorPositionX(9), 0);
  DragMouseTo(drag_point);
  ReleaseLeftMouseButton();
  EXPECT_EQ(ASCIIToUTF16("hello world"), textfield_->text());
}

TEST_F(TextfieldTest, ReadOnlyTest) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("read only"));
  textfield_->SetReadOnly(true);
  EXPECT_TRUE(textfield_->enabled());
  EXPECT_TRUE(textfield_->IsFocusable());

  bool shift = false;
  SendHomeEvent(shift);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());
  SendEndEvent(shift);
  EXPECT_EQ(9U, textfield_->GetCursorPosition());

  SendKeyEvent(ui::VKEY_LEFT, shift, false);
  EXPECT_EQ(8U, textfield_->GetCursorPosition());
  SendWordEvent(ui::VKEY_LEFT, shift);
  EXPECT_EQ(5U, textfield_->GetCursorPosition());
  shift = true;
  SendWordEvent(ui::VKEY_LEFT, shift);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());
  EXPECT_STR_EQ("read ", textfield_->GetSelectedText());
  textfield_->SelectAll(false);
  EXPECT_STR_EQ("read only", textfield_->GetSelectedText());

  // Cut should be disabled.
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "Test");
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_->ExecuteCommand(IDS_APP_CUT, 0);
  SendKeyEvent(ui::VKEY_X, false, true);
  SendAlternateCut();
  EXPECT_STR_EQ("Test", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("read only", textfield_->text());

  // Paste should be disabled.
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_->ExecuteCommand(IDS_APP_PASTE, 0);
  SendKeyEvent(ui::VKEY_V, false, true);
  SendAlternatePaste();
  EXPECT_STR_EQ("read only", textfield_->text());

  // Copy should work normally.
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "Test");
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_->ExecuteCommand(IDS_APP_COPY, 0);
  EXPECT_STR_EQ("read only", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "Test");
  SendKeyEvent(ui::VKEY_C, false, true);
  EXPECT_STR_EQ("read only", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "Test");
  SendAlternateCopy();
  EXPECT_STR_EQ("read only", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));

  // SetText should work even in read only mode.
  textfield_->SetText(ASCIIToUTF16(" four five six "));
  EXPECT_STR_EQ(" four five six ", textfield_->text());

  textfield_->SelectAll(false);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());

  // Text field is unmodifiable and selection shouldn't change.
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_T);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, TextInputClientTest) {
  InitTextfield();
  ui::TextInputClient* client = textfield_;
  EXPECT_TRUE(client);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, client->GetTextInputType());

  textfield_->SetText(ASCIIToUTF16("0123456789"));
  gfx::Range range;
  EXPECT_TRUE(client->GetTextRange(&range));
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(10U, range.end());

  EXPECT_TRUE(client->SetSelectionRange(gfx::Range(1, 4)));
  EXPECT_TRUE(client->GetSelectionRange(&range));
  EXPECT_EQ(gfx::Range(1, 4), range);

  base::string16 substring;
  EXPECT_TRUE(client->GetTextFromRange(range, &substring));
  EXPECT_STR_EQ("123", substring);

  EXPECT_TRUE(client->DeleteRange(range));
  EXPECT_STR_EQ("0456789", textfield_->text());

  ui::CompositionText composition;
  composition.text = UTF8ToUTF16("321");
  // Set composition through input method.
  input_method_->Clear();
  input_method_->SetCompositionTextForNextKey(composition);
  textfield_->clear();

  on_before_user_action_ = on_after_user_action_ = 0;

  // Send a key to trigger MockInputMethod::DispatchKeyEvent(). Note the
  // specific VKEY isn't used (MockInputMethod will mock a ui::VKEY_PROCESSKEY
  // whenever it has a test composition). However, on Mac, it can't be a letter
  // (e.g. VKEY_A) since all native character events on Mac are unicode events
  // and don't have a meaningful ui::KeyEvent that would trigger
  // DispatchKeyEvent().
  SendKeyEvent(ui::VKEY_RETURN);

  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  EXPECT_TRUE(client->HasCompositionText());
  EXPECT_TRUE(client->GetCompositionTextRange(&range));
  EXPECT_STR_EQ("0321456789", textfield_->text());
  EXPECT_EQ(gfx::Range(1, 4), range);
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  input_method_->SetResultTextForNextKey(UTF8ToUTF16("123"));
  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  SendKeyEvent(ui::VKEY_RETURN);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  EXPECT_FALSE(client->HasCompositionText());
  EXPECT_FALSE(input_method_->cancel_composition_called());
  EXPECT_STR_EQ("0123456789", textfield_->text());
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  input_method_->Clear();
  input_method_->SetCompositionTextForNextKey(composition);
  textfield_->clear();
  SendKeyEvent(ui::VKEY_RETURN);
  EXPECT_TRUE(client->HasCompositionText());
  EXPECT_STR_EQ("0123321456789", textfield_->text());

  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_FALSE(client->HasCompositionText());
  EXPECT_TRUE(input_method_->cancel_composition_called());
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_STR_EQ("0123321456789", textfield_->text());
  EXPECT_EQ(8U, textfield_->GetCursorPosition());
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  textfield_->clear();
  textfield_->SetText(ASCIIToUTF16("0123456789"));
  EXPECT_TRUE(client->SetSelectionRange(gfx::Range(5, 5)));
  client->ExtendSelectionAndDelete(4, 2);
  EXPECT_STR_EQ("0789", textfield_->text());

  // On{Before,After}UserAction should be called by whatever user action
  // triggers clearing or setting a selection if appropriate.
  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  textfield_->ClearSelection();
  textfield_->SelectAll(false);
  EXPECT_EQ(0, on_before_user_action_);
  EXPECT_EQ(0, on_after_user_action_);

  input_method_->Clear();

  // Changing the Textfield to readonly shouldn't change the input client, since
  // it's still required for selections and clipboard copy.
  ui::TextInputClient* text_input_client = textfield_;
  EXPECT_TRUE(text_input_client);
  EXPECT_NE(ui::TEXT_INPUT_TYPE_NONE, text_input_client->GetTextInputType());
  textfield_->SetReadOnly(true);
  EXPECT_TRUE(input_method_->text_input_type_changed());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, text_input_client->GetTextInputType());

  input_method_->Clear();
  textfield_->SetReadOnly(false);
  EXPECT_TRUE(input_method_->text_input_type_changed());
  EXPECT_NE(ui::TEXT_INPUT_TYPE_NONE, text_input_client->GetTextInputType());

  input_method_->Clear();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_TRUE(input_method_->text_input_type_changed());
}

TEST_F(TextfieldTest, UndoRedoTest) {
  InitTextfield();
  SendKeyEvent(ui::VKEY_A);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("a", textfield_->text());

  // AppendText
  textfield_->AppendText(ASCIIToUTF16("b"));
  last_contents_.clear();  // AppendText doesn't call ContentsChanged.
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("ab", textfield_->text());

  // SetText
  SendKeyEvent(ui::VKEY_C);
  // Undo'ing append moves the cursor to the end for now.
  // A no-op SetText won't add a new edit; see TextfieldModel::SetText.
  EXPECT_STR_EQ("abc", textfield_->text());
  textfield_->SetText(ASCIIToUTF16("abc"));
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("abc", textfield_->text());
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SetText(ASCIIToUTF16("123"));
  EXPECT_STR_EQ("123", textfield_->text());
  SendKeyEvent(ui::VKEY_END, false, false);
  SendKeyEvent(ui::VKEY_4, false, false);
  EXPECT_STR_EQ("1234", textfield_->text());
  last_contents_.clear();
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("123", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  // the insert edit "c" and set edit "123" are merged to single edit,
  // so text becomes "ab" after undo.
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("123", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("1234", textfield_->text());

  // Undoing to the same text shouldn't call ContentsChanged.
  SendKeyEvent(ui::VKEY_A, false, true);  // select all
  SendKeyEvent(ui::VKEY_A);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("1234", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("abc", textfield_->text());

  // Delete/Backspace
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendHomeEvent(false);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("b", textfield_->text());
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("b", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("b", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("", textfield_->text());
}

// Most platforms support Ctrl+Y as an alternative to Ctrl+Shift+Z, but on Mac
// that is bound to "Show full history", so is not mapped as an editing
// command. So, on Mac, send Cmd+Shift+Z.
#if !defined(OS_MACOSX)

// Test that Ctrl+Y works for Redo, as well as Ctrl+Shift+Z.
TEST_F(TextfieldTest, RedoWithCtrlY) {
  InitTextfield();
  SendKeyEvent(ui::VKEY_A);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("a", textfield_->text());
}

#endif  // !defined(OS_MACOSX)

TEST_F(TextfieldTest, CutCopyPaste) {
  InitTextfield();

  // Ensure IDS_APP_CUT cuts.
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SelectAll(false);
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_->ExecuteCommand(IDS_APP_CUT, 0);
  EXPECT_STR_EQ("123", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[x] cuts and [Ctrl]+[Alt][x] does nothing.
  textfield_->SetText(ASCIIToUTF16("456"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_X, true, false, true, false);
  EXPECT_STR_EQ("123", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("456", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_STR_EQ("456", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Ensure [Shift]+[Delete] cuts.
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SelectAll(false);
  SendAlternateCut();
  EXPECT_STR_EQ("123", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Reset clipboard text.
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "");

  // Ensure [Shift]+[Delete] is a no-op in case there is no selection.
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SelectRange(gfx::Range(0));
  SendAlternateCut();
  EXPECT_STR_EQ("", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("123", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());

  // Ensure IDS_APP_COPY copies.
  textfield_->SetText(ASCIIToUTF16("789"));
  textfield_->SelectAll(false);
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_->ExecuteCommand(IDS_APP_COPY, 0);
  EXPECT_STR_EQ("789", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[c] copies and [Ctrl]+[Alt][c] does nothing.
  textfield_->SetText(ASCIIToUTF16("012"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_C, true, false, true, false);
  EXPECT_STR_EQ("789", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_C, false, true);
  EXPECT_STR_EQ("012", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[Insert] copies.
  textfield_->SetText(ASCIIToUTF16("345"));
  textfield_->SelectAll(false);
  SendAlternateCopy();
  EXPECT_STR_EQ("345", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("345", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_COPY_PASTE, GetAndResetCopiedToClipboard());

  // Ensure IDS_APP_PASTE, [Ctrl]+[V], and [Shift]+[Insert] pastes;
  // also ensure that [Ctrl]+[Alt]+[V] does nothing.
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "abc");
  textfield_->SetText(base::string16());
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_->ExecuteCommand(IDS_APP_PASTE, 0);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_V, false, true);
  EXPECT_STR_EQ("abcabc", textfield_->text());
  SendAlternatePaste();
  EXPECT_STR_EQ("abcabcabc", textfield_->text());
  SendKeyEvent(ui::VKEY_V, true, false, true, false);
  EXPECT_STR_EQ("abcabcabc", textfield_->text());

  // Ensure [Ctrl]+[Shift]+[Insert] is a no-op.
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_INSERT, true, true);
  EXPECT_STR_EQ("abc", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_STR_EQ("abcabcabc", textfield_->text());
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());
}

TEST_F(TextfieldTest, OvertypeMode) {
  InitTextfield();
  // Overtype mode should be disabled (no-op [Insert]).
  textfield_->SetText(ASCIIToUTF16("2"));
  const bool shift = false;
  SendHomeEvent(shift);
  // Note: On Mac, there is no insert key. Insert sends kVK_Help. Currently,
  // since there is no overtype on toolkit-views, the behavior happens to match.
  // However, there's no enable-overtype equivalent key combination on OSX.
  SendKeyEvent(ui::VKEY_INSERT);
  SendKeyEvent(ui::VKEY_1, false, false);
  EXPECT_STR_EQ("12", textfield_->text());
}

TEST_F(TextfieldTest, TextCursorDisplayTest) {
  InitTextfield();
  // LTR-RTL string in LTR context.
  SendKeyEvent('a');
  EXPECT_STR_EQ("a", textfield_->text());
  int x = GetCursorBounds().x();
  int prev_x = x;

  SendKeyEvent('b');
  EXPECT_STR_EQ("ab", textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
  prev_x = x;

  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1\x5E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);

  // RTL-LTR string in LTR context.
  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(GetDisplayRect().x(), x);
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"\x05E1\x05E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  SendKeyEvent('a');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"a"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"ab"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
}

TEST_F(TextfieldTest, TextCursorDisplayInRTLTest) {
  std::string locale = base::i18n::GetConfiguredLocale();
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield();
  // LTR-RTL string in RTL context.
  SendKeyEvent('a');
  EXPECT_STR_EQ("a", textfield_->text());
  int x = GetCursorBounds().x();
  EXPECT_EQ(GetDisplayRect().right() - 1, x);
  int prev_x = x;

  SendKeyEvent('b');
  EXPECT_STR_EQ("ab", textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1\x5E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);

  // RTL-LTR string in RTL context.
  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"\x05E1\x05E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);
  prev_x = x;

  SendKeyEvent('a');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"a"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"ab"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextfieldTest, HitInsideTextAreaTest) {
  InitTextfield();
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));
  std::vector<gfx::Rect> cursor_bounds;

  // Save each cursor bound.
  gfx::SelectionModel sel(0, gfx::CURSOR_FORWARD);
  cursor_bounds.push_back(GetCursorBounds(sel));

  sel = gfx::SelectionModel(1, gfx::CURSOR_BACKWARD);
  gfx::Rect bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(1, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  // Check that a cursor at the end of the Latin portion of the text is at the
  // same position as a cursor placed at the end of the RTL Hebrew portion.
  sel = gfx::SelectionModel(2, gfx::CURSOR_BACKWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(4, gfx::CURSOR_BACKWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  sel = gfx::SelectionModel(3, gfx::CURSOR_BACKWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(3, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  sel = gfx::SelectionModel(2, gfx::CURSOR_FORWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(4, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  // Expected cursor position when clicking left and right of each character.
  size_t cursor_pos_expected[] = {0, 1, 1, 2, 4, 3, 3, 2};

  int index = 0;
  for (int i = 0; i < static_cast<int>(cursor_bounds.size() - 1); ++i) {
    int half_width = (cursor_bounds[i + 1].x() - cursor_bounds[i].x()) / 2;
    MouseClick(cursor_bounds[i], half_width / 2);
    EXPECT_EQ(cursor_pos_expected[index++], textfield_->GetCursorPosition());

    // To avoid trigger double click. Not using sleep() since it takes longer
    // for the test to run if using sleep().
    NonClientMouseClick();

    MouseClick(cursor_bounds[i + 1], - (half_width / 2));
    EXPECT_EQ(cursor_pos_expected[index++], textfield_->GetCursorPosition());

    NonClientMouseClick();
  }
}

TEST_F(TextfieldTest, HitOutsideTextAreaTest) {
  InitTextfield();

  // LTR-RTL string in LTR context.
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));

  const bool shift = false;
  SendHomeEvent(shift);
  gfx::Rect bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendEndEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  NonClientMouseClick();

  // RTL-LTR string in LTR context.
  textfield_->SetText(WideToUTF16(L"\x05E1\x5E2" L"ab"));

  SendHomeEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendEndEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());
}

TEST_F(TextfieldTest, HitOutsideTextAreaInRTLTest) {
  std::string locale = base::i18n::GetConfiguredLocale();
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield();

  // RTL-LTR string in RTL context.
  textfield_->SetText(WideToUTF16(L"\x05E1\x5E2" L"ab"));
  const bool shift = false;
  SendHomeEvent(shift);
  gfx::Rect bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendEndEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  NonClientMouseClick();

  // LTR-RTL string in RTL context.
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));
  SendHomeEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendEndEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextfieldTest, OverflowTest) {
  InitTextfield();

  base::string16 str;
  for (int i = 0; i < 500; ++i)
    SendKeyEvent('a');
  SendKeyEvent(kHebrewLetterSamekh);
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  // Test mouse pointing.
  MouseClick(GetCursorBounds(), -1);
  EXPECT_EQ(500U, textfield_->GetCursorPosition());

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);

  for (int i = 0; i < 500; ++i)
    SendKeyEvent(kHebrewLetterSamekh);
  SendKeyEvent('a');
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), -1);
  EXPECT_EQ(501U, textfield_->GetCursorPosition());
}

TEST_F(TextfieldTest, OverflowInRTLTest) {
  std::string locale = base::i18n::GetConfiguredLocale();
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield();

  base::string16 str;
  for (int i = 0; i < 500; ++i)
    SendKeyEvent('a');
  SendKeyEvent(kHebrewLetterSamekh);
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), 1);
  EXPECT_EQ(501U, textfield_->GetCursorPosition());

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);

  for (int i = 0; i < 500; ++i)
    SendKeyEvent(kHebrewLetterSamekh);
  SendKeyEvent('a');
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), 1);
  EXPECT_EQ(500U, textfield_->GetCursorPosition());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextfieldTest, GetCompositionCharacterBoundsTest) {
  InitTextfield();
  ui::CompositionText composition;
  composition.text = UTF8ToUTF16("abc123");
  const uint32_t char_count = static_cast<uint32_t>(composition.text.length());
  ui::TextInputClient* client = textfield_;

  // Compare the composition character bounds with surrounding cursor bounds.
  for (uint32_t i = 0; i < char_count; ++i) {
    composition.selection = gfx::Range(i);
    client->SetCompositionText(composition);
    gfx::Point cursor_origin = GetCursorBounds().origin();
    views::View::ConvertPointToScreen(textfield_, &cursor_origin);

    composition.selection = gfx::Range(i + 1);
    client->SetCompositionText(composition);
    gfx::Point next_cursor_bottom_left = GetCursorBounds().bottom_left();
    views::View::ConvertPointToScreen(textfield_, &next_cursor_bottom_left);

    gfx::Rect character;
    EXPECT_TRUE(client->GetCompositionCharacterBounds(i, &character));
    EXPECT_EQ(character.origin(), cursor_origin) << " i=" << i;
    EXPECT_EQ(character.bottom_right(), next_cursor_bottom_left) << " i=" << i;
  }

  // Return false if the index is out of range.
  gfx::Rect rect;
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count, &rect));
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count + 1, &rect));
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count + 100, &rect));
}

TEST_F(TextfieldTest, GetCompositionCharacterBounds_ComplexText) {
  InitTextfield();

  const base::char16 kUtf16Chars[] = {
    // U+0020 SPACE
    0x0020,
    // U+1F408 (CAT) as surrogate pair
    0xd83d, 0xdc08,
    // U+5642 as Ideographic Variation Sequences
    0x5642, 0xDB40, 0xDD00,
    // U+260E (BLACK TELEPHONE) as Emoji Variation Sequences
    0x260E, 0xFE0F,
    // U+0020 SPACE
    0x0020,
  };
  const size_t kUtf16CharsCount = arraysize(kUtf16Chars);

  ui::CompositionText composition;
  composition.text.assign(kUtf16Chars, kUtf16Chars + kUtf16CharsCount);
  ui::TextInputClient* client = textfield_;
  client->SetCompositionText(composition);

  // Make sure GetCompositionCharacterBounds never fails for index.
  gfx::Rect rects[kUtf16CharsCount];
  for (uint32_t i = 0; i < kUtf16CharsCount; ++i)
    EXPECT_TRUE(client->GetCompositionCharacterBounds(i, &rects[i]));

  // Here we might expect the following results but it actually depends on how
  // Uniscribe or HarfBuzz treats them with given font.
  // - rects[1] == rects[2]
  // - rects[3] == rects[4] == rects[5]
  // - rects[6] == rects[7]
}

// The word we select by double clicking should remain selected regardless of
// where we drag the mouse afterwards without releasing the left button.
TEST_F(TextfieldTest, KeepInitiallySelectedWord) {
  InitTextfield();

  textfield_->SetText(ASCIIToUTF16("abc def ghi"));

  textfield_->SelectRange(gfx::Range(5, 5));
  const gfx::Rect middle_cursor = GetCursorBounds();
  textfield_->SelectRange(gfx::Range(0, 0));
  const gfx::Point beginning = GetCursorBounds().origin();

  // Double click, but do not release the left button.
  MouseClick(middle_cursor, 0);
  const gfx::Point middle(middle_cursor.x(),
                          middle_cursor.y() + middle_cursor.height() / 2);
  MoveMouseTo(middle);
  PressLeftMouseButton();
  EXPECT_EQ(gfx::Range(4, 7), textfield_->GetSelectedRange());

  // Drag the mouse to the beginning of the textfield.
  DragMouseTo(beginning);
  EXPECT_EQ(gfx::Range(7, 0), textfield_->GetSelectedRange());
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// flaky: http://crbug.com/396477
TEST_F(TextfieldTest, DISABLED_SelectionClipboard) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("0123"));
  gfx::Point point_1(GetCursorPositionX(1), 0);
  gfx::Point point_2(GetCursorPositionX(2), 0);
  gfx::Point point_3(GetCursorPositionX(3), 0);
  gfx::Point point_4(GetCursorPositionX(4), 0);

  // Text selected by the mouse should be placed on the selection clipboard.
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, point_1, point_1,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(press);
  ui::MouseEvent drag(ui::ET_MOUSE_DRAGGED, point_3, point_3,
                      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                      ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseDragged(drag);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, point_3, point_3,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release);
  EXPECT_EQ(gfx::Range(1, 3), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("12", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));

  // Select-all should update the selection clipboard.
  SendKeyEvent(ui::VKEY_A, false, true);
  EXPECT_EQ(gfx::Range(0, 4), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("0123", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());

  // Shift-click selection modifications should update the clipboard.
  NonClientMouseClick();
  ui::MouseEvent press_2(ui::ET_MOUSE_PRESSED, point_2, point_2,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  press_2.set_flags(press_2.flags() | ui::EF_SHIFT_DOWN);
#if defined(USE_X11)
  ui::UpdateX11EventForFlags(&press_2);
#endif
  textfield_->OnMousePressed(press_2);
  ui::MouseEvent release_2(ui::ET_MOUSE_RELEASED, point_2, point_2,
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release_2);
  EXPECT_EQ(gfx::Range(0, 2), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("01", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());

  // Shift-Left/Right should update the selection clipboard.
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  EXPECT_STR_EQ("012", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_LEFT, true, false);
  EXPECT_STR_EQ("01", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_RIGHT, true, true);
  EXPECT_STR_EQ("0123", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());

  // Moving the cursor without a selection should not change the clipboard.
  SendKeyEvent(ui::VKEY_LEFT, false, false);
  EXPECT_EQ(gfx::Range(0, 0), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("0123", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());

  // Middle clicking should paste at the mouse (not cursor) location.
  // The cursor should be placed at the end of the pasted text.
  ui::MouseEvent middle(ui::ET_MOUSE_PRESSED, point_4, point_4,
                        ui::EventTimeForNow(), ui::EF_MIDDLE_MOUSE_BUTTON,
                        ui::EF_MIDDLE_MOUSE_BUTTON);
  textfield_->OnMousePressed(middle);
  EXPECT_STR_EQ("01230123", textfield_->text());
  EXPECT_EQ(gfx::Range(8, 8), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("0123", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));

  // Middle clicking on an unfocused textfield should focus it and paste.
  textfield_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(textfield_->HasFocus());
  textfield_->OnMousePressed(middle);
  EXPECT_TRUE(textfield_->HasFocus());
  EXPECT_STR_EQ("012301230123", textfield_->text());
  EXPECT_EQ(gfx::Range(8, 8), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("0123", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));

  // Middle clicking with an empty selection clipboard should still focus.
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, std::string());
  textfield_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(textfield_->HasFocus());
  textfield_->OnMousePressed(middle);
  EXPECT_TRUE(textfield_->HasFocus());
  EXPECT_STR_EQ("012301230123", textfield_->text());
  EXPECT_EQ(gfx::Range(4, 4), textfield_->GetSelectedRange());
  EXPECT_TRUE(GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION).empty());

  // Middle clicking in the selection should clear the clipboard and selection.
  SetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE, "foo");
  textfield_->SelectRange(gfx::Range(2, 6));
  textfield_->OnMousePressed(middle);
  EXPECT_STR_EQ("012301230123", textfield_->text());
  EXPECT_EQ(gfx::Range(6, 6), textfield_->GetSelectedRange());
  EXPECT_TRUE(GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION).empty());

  // Double and triple clicking should update the clipboard contents.
  textfield_->SetText(ASCIIToUTF16("ab cd ef"));
  gfx::Point word(GetCursorPositionX(4), 0);
  ui::MouseEvent press_word(ui::ET_MOUSE_PRESSED, word, word,
                            ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                            ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(press_word);
  ui::MouseEvent release_word(ui::ET_MOUSE_RELEASED, word, word,
                              ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                              ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release_word);
  ui::MouseEvent double_click(ui::ET_MOUSE_PRESSED, word, word,
                              ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_DOUBLE_CLICK,
                              ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(double_click);
  textfield_->OnMouseReleased(release_word);
  EXPECT_EQ(gfx::Range(3, 5), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("cd", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());
  textfield_->OnMousePressed(press_word);
  textfield_->OnMouseReleased(release_word);
  EXPECT_EQ(gfx::Range(0, 8), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("ab cd ef", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_SELECTION, GetAndResetCopiedToClipboard());

  // Selecting a range of text without any user interaction should not change
  // the clipboard content.
  textfield_->SelectRange(gfx::Range(0, 3));
  EXPECT_STR_EQ("ab ", textfield_->GetSelectedText());
  EXPECT_STR_EQ("ab cd ef", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());

  SetClipboardText(ui::CLIPBOARD_TYPE_SELECTION, "other");
  textfield_->SelectAll(false);
  EXPECT_STR_EQ("other", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
  EXPECT_EQ(ui::CLIPBOARD_TYPE_LAST, GetAndResetCopiedToClipboard());
}
#endif

// Long_Press gesture in Textfield can initiate a drag and drop now.
TEST_F(TextfieldTest, TestLongPressInitiatesDragDrop) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("Hello string world"));

  // Ensure the textfield will provide selected text for drag data.
  textfield_->SelectRange(gfx::Range(6, 12));
  const gfx::Point kStringPoint(GetCursorPositionX(9), 0);

  // Enable touch-drag-drop to make long press effective.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);

  // Create a long press event in the selected region should start a drag.
  GestureEventForTest long_press(
      kStringPoint.x(),
      kStringPoint.y(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  textfield_->OnGestureEvent(&long_press);
  EXPECT_TRUE(textfield_->CanStartDragForView(NULL, kStringPoint,
                                              kStringPoint));
}

TEST_F(TextfieldTest, GetTextfieldBaseline_FontFallbackTest) {
  InitTextfield();
  textfield_->SetText(UTF8ToUTF16("abc"));
  const int old_baseline = textfield_->GetBaseline();

  // Set text which may fall back to a font which has taller baseline than
  // the default font.
  textfield_->SetText(UTF8ToUTF16("\xE0\xB9\x91"));
  const int new_baseline = textfield_->GetBaseline();

  // Regardless of the text, the baseline must be the same.
  EXPECT_EQ(new_baseline, old_baseline);
}

// Tests that a textfield view can be destroyed from OnKeyEvent() on its
// controller and it does not crash.
TEST_F(TextfieldTest, DestroyingTextfieldFromOnKeyEvent) {
  InitTextfield();

  // The controller assumes ownership of the textfield.
  TextfieldDestroyerController controller(textfield_);
  EXPECT_TRUE(controller.target());

  // Send a key to trigger OnKeyEvent().
  SendKeyEvent(ui::VKEY_RETURN);

  EXPECT_FALSE(controller.target());
}

class TextfieldTouchSelectionTest : public TextfieldTest {
 protected:
  // Simulates a complete tap.
  void Tap(const gfx::Point& point) {
    GestureEventForTest begin(
        point.x(), point.y(), ui::GestureEventDetails(ui::ET_GESTURE_BEGIN));
    textfield_->OnGestureEvent(&begin);

    GestureEventForTest tap_down(
        point.x(), point.y(), ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
    textfield_->OnGestureEvent(&tap_down);

    GestureEventForTest show_press(
        point.x(),
        point.y(),
        ui::GestureEventDetails(ui::ET_GESTURE_SHOW_PRESS));
    textfield_->OnGestureEvent(&show_press);

    ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP);
    tap_details.set_tap_count(1);
    GestureEventForTest tap(point.x(), point.y(), tap_details);
    textfield_->OnGestureEvent(&tap);

    GestureEventForTest end(
        point.x(), point.y(), ui::GestureEventDetails(ui::ET_GESTURE_END));
    textfield_->OnGestureEvent(&end);
  }

};

// Touch selection and dragging currently only works for chromeos.
#if defined(OS_CHROMEOS)
TEST_F(TextfieldTouchSelectionTest, TouchSelectionAndDraggingTest) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  EXPECT_FALSE(test_api_->touch_selection_controller());
  const int x = GetCursorPositionX(2);

  // Tapping on the textfield should turn on the TouchSelectionController.
  ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP);
  tap_details.set_tap_count(1);
  GestureEventForTest tap(x, 0, tap_details);
  textfield_->OnGestureEvent(&tap);
  EXPECT_TRUE(test_api_->touch_selection_controller());

  // Un-focusing the textfield should reset the TouchSelectionController
  textfield_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(test_api_->touch_selection_controller());
  textfield_->RequestFocus();

  // With touch editing enabled, long press should not show context menu.
  // Instead, select word and invoke TouchSelectionController.
  GestureEventForTest long_press_1(
      x, 0, ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  textfield_->OnGestureEvent(&long_press_1);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_TRUE(test_api_->touch_selection_controller());
  EXPECT_TRUE(long_press_1.handled());

  // With touch drag drop enabled, long pressing in the selected region should
  // start a drag and remove TouchSelectionController.
  ASSERT_TRUE(switches::IsTouchDragDropEnabled());
  GestureEventForTest long_press_2(
      x, 0, ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  textfield_->OnGestureEvent(&long_press_2);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_FALSE(test_api_->touch_selection_controller());
  EXPECT_FALSE(long_press_2.handled());

  // After disabling touch drag drop, long pressing again in the selection
  // region should not do anything.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableTouchDragDrop);
  ASSERT_FALSE(switches::IsTouchDragDropEnabled());
  GestureEventForTest long_press_3(
      x, 0, ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  textfield_->OnGestureEvent(&long_press_3);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_FALSE(test_api_->touch_selection_controller());
  EXPECT_FALSE(long_press_3.handled());
}
#endif

TEST_F(TextfieldTouchSelectionTest, TouchSelectionInUnfocusableTextfield) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  gfx::Point touch_point(GetCursorPositionX(2), 0);

  // Disable textfield and tap on it. Touch text selection should not get
  // activated.
  textfield_->SetEnabled(false);
  Tap(touch_point);
  EXPECT_FALSE(test_api_->touch_selection_controller());
  textfield_->SetEnabled(true);

  // Make textfield unfocusable and tap on it. Touch text selection should not
  // get activated.
  textfield_->SetFocusBehavior(View::FocusBehavior::NEVER);
  Tap(touch_point);
  EXPECT_FALSE(textfield_->HasFocus());
  EXPECT_FALSE(test_api_->touch_selection_controller());
  textfield_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if defined(OS_MACOSX) && !defined(USE_AURA)
#define MAYBE_TapOnSelection DISABLED_TapOnSelection
#else
#define MAYBE_TapOnSelection TapOnSelection
#endif

TEST_F(TextfieldTouchSelectionTest, MAYBE_TapOnSelection) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  gfx::Range sel_range(2, 7);
  gfx::Range tap_range(5, 5);
  gfx::Rect tap_rect =
      GetCursorBounds(gfx::SelectionModel(tap_range, gfx::CURSOR_FORWARD));
  gfx::Point tap_point = tap_rect.CenterPoint();

  // Select range |sel_range| and check if touch selection handles are not
  // present and correct range is selected.
  textfield_->SetSelectionRange(sel_range);
  gfx::Range range;
  textfield_->GetSelectionRange(&range);
  EXPECT_FALSE(test_api_->touch_selection_controller());
  EXPECT_EQ(sel_range, range);

  // Tap on selection and check if touch selectoin handles are shown, but
  // selection range is not modified.
  Tap(tap_point);
  textfield_->GetSelectionRange(&range);
  EXPECT_TRUE(test_api_->touch_selection_controller());
  EXPECT_EQ(sel_range, range);

  // Tap again on selection and check if touch selection handles are still
  // present and selection is changed to a cursor at tap location.
  Tap(tap_point);
  textfield_->GetSelectionRange(&range);
  EXPECT_TRUE(test_api_->touch_selection_controller());
  EXPECT_EQ(tap_range, range);
}

TEST_F(TextfieldTest, AccessiblePasswordTest) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("password"));

  ui::AXViewState state_regular;
  textfield_->GetAccessibleState(&state_regular);
  EXPECT_EQ(ui::AX_ROLE_TEXT_FIELD, state_regular.role);
  EXPECT_EQ(ASCIIToUTF16("password"), state_regular.value);
  EXPECT_FALSE(state_regular.HasStateFlag(ui::AX_STATE_PROTECTED));

  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  ui::AXViewState state_protected;
  textfield_->GetAccessibleState(&state_protected);
  EXPECT_EQ(ui::AX_ROLE_TEXT_FIELD, state_protected.role);
  EXPECT_EQ(ASCIIToUTF16("********"), state_protected.value);
  EXPECT_TRUE(state_protected.HasStateFlag(ui::AX_STATE_PROTECTED));
}

}  // namespace views
