// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientation_h
#define ScreenOrientation_h

#include "core/frame/DOMWindowProperty.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebScreenOrientationLockType.h"
#include "public/platform/WebScreenOrientationType.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Document;
class ScriptPromise;
class ScriptState;
class Screen;

class ScreenOrientation FINAL : public NoBaseWillBeGarbageCollectedFinalized<ScreenOrientation>, public WillBeHeapSupplement<Screen>, DOMWindowProperty {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientation);
public:
    static ScreenOrientation& from(Screen&);
    virtual ~ScreenOrientation();

    static const AtomicString& orientation(Screen&);
    static ScriptPromise lockOrientation(ScriptState*, Screen&, const AtomicString& orientation);
    static void unlockOrientation(Screen&);

    // Helper being used by this class and LockOrientationCallback.
    static const AtomicString& orientationTypeToString(blink::WebScreenOrientationType);

    virtual void trace(Visitor* visitor) OVERRIDE { WillBeHeapSupplement<Screen>::trace(visitor); }

private:
    explicit ScreenOrientation(Screen&);

    static const char* supplementName();
    Document* document() const;
};

} // namespace WebCore

#endif // ScreenOrientation_h
