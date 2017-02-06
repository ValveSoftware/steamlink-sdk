// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorInstalledApp_h
#define NavigatorInstalledApp_h

#include "core/frame/DOMWindowProperty.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class Navigator;
class ScriptPromise;
class ScriptState;
class InstalledAppController;

class NavigatorInstalledApp final : public GarbageCollected<NavigatorInstalledApp>, public Supplement<Navigator>, public DOMWindowProperty {
    USING_GARBAGE_COLLECTED_MIXIN(NavigatorInstalledApp);
public:
    static NavigatorInstalledApp* from(Document&);
    static NavigatorInstalledApp& from(Navigator&);

    static ScriptPromise getInstalledRelatedApps(ScriptState*, Navigator&);
    ScriptPromise getInstalledRelatedApps(ScriptState*);

    InstalledAppController* controller();

    DECLARE_VIRTUAL_TRACE();

private:
    explicit NavigatorInstalledApp(LocalFrame*);
    static const char* supplementName();
};

} // namespace blink

#endif // NavigatorInstalledApp_h
