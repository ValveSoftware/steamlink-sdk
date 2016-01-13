// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_CHROMEOS_H_
#define UI_BASE_IME_INPUT_METHOD_CHROMEOS_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/ime/chromeos/character_composer.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method_base.h"

namespace ui {

// A ui::InputMethod implementation based on IBus.
class UI_BASE_EXPORT InputMethodChromeOS
    : public InputMethodBase,
      public chromeos::IMEInputContextHandlerInterface {
 public:
  explicit InputMethodChromeOS(internal::InputMethodDelegate* delegate);
  virtual ~InputMethodChromeOS();

  // Overridden from InputMethod:
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) OVERRIDE;
  virtual bool DispatchKeyEvent(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE;
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE;
  virtual void CancelComposition(const TextInputClient* client) OVERRIDE;
  virtual void OnInputLocaleChanged() OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual bool IsActive() OVERRIDE;
  virtual bool IsCandidatePopupOpen() const OVERRIDE;

 protected:
  // Converts |text| into CompositionText.
  void ExtractCompositionText(const chromeos::CompositionText& text,
                              uint32 cursor_position,
                              CompositionText* out_composition) const;

  // Process a key returned from the input method.
  virtual void ProcessKeyEventPostIME(const ui::KeyEvent& event,
                                      bool handled);

  // Resets context and abandon all pending results and key events.
  void ResetContext();

 private:
  class PendingKeyEvent;

  // Overridden from InputMethodBase:
  virtual void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                         TextInputClient* focused) OVERRIDE;
  virtual void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                        TextInputClient* focused) OVERRIDE;

  // Asks the client to confirm current composition text.
  void ConfirmCompositionText();

  // Checks the availability of focused text input client and update focus
  // state.
  void UpdateContextFocusState();

  // Processes a key event that was already filtered by the input method.
  // A VKEY_PROCESSKEY may be dispatched to the focused View.
  void ProcessFilteredKeyPressEvent(const ui::KeyEvent& event);

  // Processes a key event that was not filtered by the input method.
  void ProcessUnfilteredKeyPressEvent(const ui::KeyEvent& event);

  // Sends input method result caused by the given key event to the focused text
  // input client.
  void ProcessInputMethodResult(const ui::KeyEvent& event, bool filtered);

  // Checks if the pending input method result needs inserting into the focused
  // text input client as a single character.
  bool NeedInsertChar() const;

  // Checks if there is pending input method result.
  bool HasInputMethodResult() const;

  // Sends a fake key event for IME composing without physical key events.
  void SendFakeProcessKeyEvent(bool pressed) const;

  // Abandons all pending key events. It usually happends when we lose keyboard
  // focus, the text input type is changed or we are destroyed.
  void AbandonAllPendingKeyEvents();

  // Passes keyevent and executes character composition if necessary. Returns
  // true if character composer comsumes key event.
  bool ExecuteCharacterComposer(const ui::KeyEvent& event);

  // chromeos::IMEInputContextHandlerInterface overrides:
  virtual void CommitText(const std::string& text) OVERRIDE;
  virtual void UpdateCompositionText(const chromeos::CompositionText& text,
                                     uint32 cursor_pos,
                                     bool visible) OVERRIDE;
  virtual void DeleteSurroundingText(int32 offset, uint32 length) OVERRIDE;

  // Hides the composition text.
  void HidePreeditText();

  // Callback function for IMEEngineHandlerInterface::ProcessKeyEvent.
  void ProcessKeyEventDone(uint32 id, ui::KeyEvent* event, bool is_handled);

  // Returns whether an input field is focused. Note that password field is not
  // considered as an input field.
  bool IsInputFieldFocused();

  // All pending key events. Note: we do not own these object, we just save
  // pointers to these object so that we can abandon them when necessary.
  // They will be deleted in ProcessKeyEventDone().
  std::set<uint32> pending_key_events_;

  // Pending composition text generated by the current pending key event.
  // It'll be sent to the focused text input client as soon as we receive the
  // processing result of the pending key event.
  CompositionText composition_;

  // Pending result text generated by the current pending key event.
  // It'll be sent to the focused text input client as soon as we receive the
  // processing result of the pending key event.
  base::string16 result_text_;

  base::string16 previous_surrounding_text_;
  gfx::Range previous_selection_range_;

  // Indicates if there is an ongoing composition text.
  bool composing_text_;

  // Indicates if the composition text is changed or deleted.
  bool composition_changed_;

  // The latest id of key event.
  uint32 current_keyevent_id_;

  // An object to compose a character from a sequence of key presses
  // including dead key etc.
  CharacterComposer character_composer_;

  // Used for making callbacks.
  base::WeakPtrFactory<InputMethodChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodChromeOS);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_CHROMEOS_H_
