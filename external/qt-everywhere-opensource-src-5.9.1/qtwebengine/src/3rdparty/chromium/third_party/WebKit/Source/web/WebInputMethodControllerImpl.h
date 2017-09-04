// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebInputMethodControllerImpl_h
#define WebInputMethodControllerImpl_h

#include "platform/heap/Handle.h"
#include "public/web/WebCompositionUnderline.h"
#include "public/web/WebInputMethodController.h"

namespace blink {

class InputMethodController;
class LocalFrame;
class WebLocalFrameImpl;
class WebPlugin;
class WebString;

class WebInputMethodControllerImpl : public WebInputMethodController {
  WTF_MAKE_NONCOPYABLE(WebInputMethodControllerImpl);

 public:
  explicit WebInputMethodControllerImpl(WebLocalFrameImpl* ownerFrame);
  ~WebInputMethodControllerImpl() override;

  static WebInputMethodControllerImpl* fromFrame(LocalFrame*);

  // WebInputMethodController overrides.
  bool setComposition(const WebString& text,
                      const WebVector<WebCompositionUnderline>& underlines,
                      int selectionStart,
                      int selectionEnd) override;

  // Used to ask the WebInputMethodController to either delete and ongoing
  // composition, or insert the specified text, or move the caret according to
  // relativeCaretPosition.
  bool commitText(const WebString& text, int relativeCaretPosition) override;

  // Called to ask the WebInputMethodController to confirm an ongoing
  // composition.
  bool finishComposingText(
      ConfirmCompositionBehavior selectionBehavior) override;

  void setSuppressNextKeypressEvent(bool suppress) {
    m_suppressNextKeypressEvent = suppress;
  }

  DECLARE_TRACE();

 private:
  LocalFrame* frame() const;
  InputMethodController& inputMethodController() const;
  WebPlugin* focusedPluginIfInputMethodSupported() const;

  WeakMember<WebLocalFrameImpl> m_webLocalFrame;
  bool m_suppressNextKeypressEvent;
};
}  // namespace blink

#endif
