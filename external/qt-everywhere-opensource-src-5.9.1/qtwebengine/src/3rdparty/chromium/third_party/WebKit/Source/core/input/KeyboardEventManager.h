// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KeyboardEventManager_h
#define KeyboardEventManager_h

#include "core/CoreExport.h"
#include "platform/PlatformEvent.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Visitor.h"
#include "public/platform/WebFocusType.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebInputEventResult.h"
#include "wtf/Allocator.h"

namespace blink {

class KeyboardEvent;
class LocalFrame;
class ScrollManager;

enum class OverrideCapsLockState { Default, On, Off };

class CORE_EXPORT KeyboardEventManager
    : public GarbageCollectedFinalized<KeyboardEventManager> {
  WTF_MAKE_NONCOPYABLE(KeyboardEventManager);

 public:
  static const int kAccessKeyModifiers =
// TODO(crbug.com/618397): Add a settings to control this behavior.
#if OS(MACOSX)
      WebInputEvent::ControlKey | WebInputEvent::AltKey;
#else
      WebInputEvent::AltKey;
#endif

  KeyboardEventManager(LocalFrame*, ScrollManager*);
  DECLARE_TRACE();

  bool handleAccessKey(const WebKeyboardEvent&);
  WebInputEventResult keyEvent(const WebKeyboardEvent&);
  void defaultKeyboardEventHandler(KeyboardEvent*, Node*);

  void capsLockStateMayHaveChanged();
  static WebInputEvent::Modifiers getCurrentModifierState();
  static bool currentCapsLockState();

 private:
  friend class Internals;
  // Allows overriding the current caps lock state for testing purposes.
  static void setCurrentCapsLockState(OverrideCapsLockState);

  void defaultSpaceEventHandler(KeyboardEvent*, Node*);
  void defaultBackspaceEventHandler(KeyboardEvent*);
  void defaultTabEventHandler(KeyboardEvent*);
  void defaultEscapeEventHandler(KeyboardEvent*);
  void defaultArrowEventHandler(KeyboardEvent*, Node*);

  const Member<LocalFrame> m_frame;

  Member<ScrollManager> m_scrollManager;
};

}  // namespace blink

#endif  // KeyboardEventManager_h
