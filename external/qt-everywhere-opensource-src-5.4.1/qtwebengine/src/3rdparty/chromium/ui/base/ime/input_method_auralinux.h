// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_AURALINUX_H_
#define UI_BASE_IME_INPUT_METHOD_AURALINUX_H_

#include "base/memory/scoped_ptr.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/linux/linux_input_method_context.h"

namespace ui {

// A ui::InputMethod implementation for Aura on Linux platforms. The
// implementation details are separated to ui::LinuxInputMethodContext
// interface.
class InputMethodAuraLinux : public InputMethodBase,
                             public LinuxInputMethodContextDelegate {
 public:
  explicit InputMethodAuraLinux(internal::InputMethodDelegate* delegate);
  virtual ~InputMethodAuraLinux();

  // Overriden from InputMethod.
  virtual void Init(bool focused) OVERRIDE;
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

  // Overriden from ui::LinuxInputMethodContextDelegate
  virtual void OnCommit(const base::string16& text) OVERRIDE;
  virtual void OnPreeditChanged(const CompositionText& composition_text)
      OVERRIDE;
  virtual void OnPreeditEnd() OVERRIDE;
  virtual void OnPreeditStart() OVERRIDE;

 protected:
  // Overridden from InputMethodBase.
  virtual void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                        TextInputClient* focused) OVERRIDE;

 private:
  // Allows to fire a VKEY_PROCESSKEY key event.
  void AllowToFireProcessKey(const ui::KeyEvent& event);
  // Fires a VKEY_PROCESSKEY key event if allowed.
  void MaybeFireProcessKey();
  // Stops firing VKEY_PROCESSKEY key events.
  void StopFiringProcessKey();

  scoped_ptr<LinuxInputMethodContext> input_method_context_;

  // IBus in async mode eagerly consumes all the key events first regardless of
  // whether the underlying IME consumes the key event or not, and makes
  // gtk_im_context_filter_keypress() always return true, and later pushes
  // the key event back to the GDK event queue when it turns out that the
  // underlying IME doesn't consume the key event.
  //
  // Thus we have to defer a decision whether or not to dispatch a
  // VKEY_PROCESSKEY key event.  Unlike other InputMethod's subclasses,
  // DispatchKeyEvent() in this class does not directly dispatch a
  // VKEY_PROCESSKEY event, OnCommit or OnPreedit{Start,Changed,End} dispatch
  // a VKEY_PROCESSKEY event instead.
  //
  // Because of this hack, there could be chances that we accidentally dispatch
  // VKEY_PROCESSKEY events and other key events in out of order.
  //
  // |allowed_to_fire_vkey_process_key_| is used not to dispatch a
  // VKEY_PROCESSKEY event twice for a single key event.
  bool allowed_to_fire_vkey_process_key_;
  int vkey_processkey_flags_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodAuraLinux);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_AURALINUX_H_
