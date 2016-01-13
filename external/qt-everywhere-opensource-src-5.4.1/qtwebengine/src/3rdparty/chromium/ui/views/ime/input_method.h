// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_INPUT_METHOD_H_
#define UI_VIEWS_IME_INPUT_METHOD_H_

#include <string>

#include "base/basictypes.h"
#include "base/event_types.h"
#include "base/i18n/rtl.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/views/views_export.h"

namespace ui {
class KeyEvent;
class TextInputClient;
}  // namespace ui

namespace views {

namespace internal {
class InputMethodDelegate;
}  // namespace internal

class View;
class Widget;

// An interface implemented by an object that encapsulates a native input method
// service provided by the underlying operation system. Input method services
// are typically bound to individual native windows (HWND, aura::Window, etc.).
// In Views, only the top-level Widgets get keyboard focus, so this API is
// designed to be bound to top-level Widgets.
class VIEWS_EXPORT InputMethod {
 public:

#if defined(OS_WIN)
  typedef LRESULT NativeEventResult;
#else
  typedef int32 NativeEventResult;
#endif

  virtual ~InputMethod() {}

  // Sets the delegate used by this InputMethod instance.
  // This should only be called by the owner Widget or testing code.
  virtual void SetDelegate(internal::InputMethodDelegate* delegate) = 0;

  // Initialize the InputMethod object and attach it to the given |widget|.
  // The |widget| must already be initialized.
  virtual void Init(Widget* widget) = 0;

  // Called when the top-level Widget gains or loses keyboard focus.
  // These should only be called by the Widget that owns this InputMethod.
  virtual void OnFocus() = 0;
  virtual void OnBlur() = 0;

  // Called when the focused window receives native IME messages that are not
  // translated into other predefined event callbacks. Currently this method is
  // used only for IME functionalities specific to Windows.
  // TODO(ime): Break down these messages into platform-neutral methods.
  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) = 0;

  // Dispatch a key event to the input method. The key event will be dispatched
  // back to the caller via InputMethodDelegate::DispatchKeyEventPostIME(), once
  // it has been processed by the input method. It should only be called by the
  // top-level Widget that owns this InputMethod instance, or other related
  // platform-specific code, such as a message dispatcher.
  virtual void DispatchKeyEvent(const ui::KeyEvent& key) = 0;

  // Called by the focused |view| whenever its text input type has changed.
  // Before calling this method, the focused |view| must confirm or clear any
  // existing composition text and call InputMethod::CancelComposition() when
  // necessary. This method has no effect if |view| is not focused.
  virtual void OnTextInputTypeChanged(View* view) = 0;

  // Called by the focused |view| whenever its caret bounds have changed.
  // This method has no effect if |view| is not focused.
  virtual void OnCaretBoundsChanged(View* view) = 0;

  // Called by the focused |view| to cancel the ongoing composition session.
  // This method has no effect if |view| is not focused.
  virtual void CancelComposition(View* view) = 0;

  // Called by the focused client whenever its input locale is changed.
  // This method is currently used only on Windows.
  // This method does not take a parameter of View for historical reasons.
  // TODO(ime): Consider to take a parameter of View.
  virtual void OnInputLocaleChanged() = 0;

  // Returns the locale of current keyboard layout or input method, as a BCP-47
  // tag, or an empty string if the input method cannot provide it.
  virtual std::string GetInputLocale() = 0;

  // Returns true if the input method is ready to process keyboard events and
  // generate composition or text results. It is not necessary to notify
  // inactive input methods of caret bounds or text input type changes.
  // Note: TextInputClient::InsertChar() may be called to send input to the text
  // input client even if the input method is not active.
  virtual bool IsActive() = 0;

  // Returns the focused text input client, or NULL if the Widget is not active,
  // has no focused View, or if the focused View does not support text input.
  virtual ui::TextInputClient* GetTextInputClient() const = 0;

  // Gets the text input type of the focused text input client. Returns
  // ui::TEXT_INPUT_TYPE_NONE if there is no focused text input client.
  virtual ui::TextInputType GetTextInputType() const = 0;

  // Returns true if we know for sure that a candidate window (or IME suggest,
  // etc.) is open.  Returns false if no popup window is open or the detection
  // of IME popups is not supported.
  virtual bool IsCandidatePopupOpen() const = 0;

  // Displays an on screen keyboard if enabled.
  virtual void ShowImeIfNeeded() = 0;

  // Returns true if the input method is a mock instance used for testing.
  virtual bool IsMock() const = 0;

  // TODO(suzhe): Support mouse/touch event.
};

}  // namespace views

#endif  // UI_VIEWS_IME_INPUT_METHOD_H_
