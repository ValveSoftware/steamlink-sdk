// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_WEB_KEYBOARD_EVENT_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_WEB_KEYBOARD_EVENT_H_

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace ui {
class KeyEvent;
}

namespace content {

// Owns a platform specific event; used to pass own and pass event through
// platform independent code.
struct CONTENT_EXPORT NativeWebKeyboardEvent :
  NON_EXPORTED_BASE(public blink::WebKeyboardEvent) {
  NativeWebKeyboardEvent();

  explicit NativeWebKeyboardEvent(gfx::NativeEvent native_event);
#if defined(OS_ANDROID)
  NativeWebKeyboardEvent(blink::WebInputEvent::Type type,
                         int modifiers,
                         double time_secs,
                         int keycode,
                         int scancode,
                         int unicode_character,
                         bool is_system_key);
  // Takes ownership of android_key_event.
  NativeWebKeyboardEvent(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& android_key_event,
      blink::WebInputEvent::Type type,
      int modifiers,
      double time_secs,
      int keycode,
      int scancode,
      int unicode_character,
      bool is_system_key);
#else
  explicit NativeWebKeyboardEvent(const ui::KeyEvent& key_event);
#if defined(USE_AURA)
  // Create a legacy keypress event specified by |character|.
  NativeWebKeyboardEvent(const ui::KeyEvent& key_event, base::char16 character);
#endif
#endif

  NativeWebKeyboardEvent(const NativeWebKeyboardEvent& event);
  ~NativeWebKeyboardEvent();

  NativeWebKeyboardEvent& operator=(const NativeWebKeyboardEvent& event);

  gfx::NativeEvent os_event;

  // True if the browser should ignore this event if it's not handled by the
  // renderer. This happens for RawKeyDown events that are created while IME is
  // active and is necessary to prevent backspace from doing "history back" if
  // it is hit in ime mode.
  // Currently, it's only used by Linux and Mac ports.
  bool skip_in_browser;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_WEB_KEYBOARD_EVENT_H_
