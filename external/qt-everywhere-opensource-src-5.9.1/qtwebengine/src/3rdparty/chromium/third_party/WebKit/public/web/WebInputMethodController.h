// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebInputMethodController_h
#define WebInputMethodController_h

#include "WebCompositionUnderline.h"
#include "WebWidget.h"

namespace blink {

class WebString;
template <typename T>
class WebVector;

class WebInputMethodController {
 public:
  enum ConfirmCompositionBehavior {
    DoNotKeepSelection,
    KeepSelection,
  };

  virtual ~WebInputMethodController() {}
  // Called to inform the WebInputMethodController of a new composition text.
  // If selectionStart and selectionEnd has the same value, then it indicates
  // the input caret position. If the text is empty, then the existing
  // composition text will be canceled.
  // Returns true if the composition text was set successfully.
  virtual bool setComposition(
      const WebString& text,
      const WebVector<WebCompositionUnderline>& underlines,
      int selectionStart,
      int selectionEnd) = 0;

  // Called to inform the WebWidget that deleting the ongoing composition if
  // any, inserting the specified text, and moving the caret according to
  // relativeCaretPosition.
  virtual bool commitText(const WebString& text, int relativeCaretPosition) = 0;

  // Called to inform the WebWidget to confirm an ongoing composition.
  virtual bool finishComposingText(
      ConfirmCompositionBehavior selectionBehavior) = 0;
};

}  // namespace blink
#endif
