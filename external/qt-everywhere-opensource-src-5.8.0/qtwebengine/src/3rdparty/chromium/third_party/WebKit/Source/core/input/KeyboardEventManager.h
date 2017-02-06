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
#include "public/platform/WebInputEventResult.h"
#include "wtf/Allocator.h"

namespace blink {

class KeyboardEvent;
class LocalFrame;
class PlatformKeyboardEvent;
class ScrollManager;

class CORE_EXPORT KeyboardEventManager {
    WTF_MAKE_NONCOPYABLE(KeyboardEventManager);
    DISALLOW_NEW();
public:
    explicit KeyboardEventManager(LocalFrame*, ScrollManager*);
    ~KeyboardEventManager();
    DECLARE_TRACE();

    bool handleAccessKey(const PlatformKeyboardEvent&);
    WebInputEventResult keyEvent(const PlatformKeyboardEvent&);
    void defaultKeyboardEventHandler(KeyboardEvent*, Node*);

    void capsLockStateMayHaveChanged();

private:

    void defaultSpaceEventHandler(KeyboardEvent*, Node*);
    void defaultBackspaceEventHandler(KeyboardEvent*);
    void defaultTabEventHandler(KeyboardEvent*);
    void defaultEscapeEventHandler(KeyboardEvent*);
    void defaultArrowEventHandler(WebFocusType, KeyboardEvent*);

    const Member<LocalFrame> m_frame;

    ScrollManager* m_scrollManager;
};

} // namespace blink

#endif // KeyboardEventManager_h
