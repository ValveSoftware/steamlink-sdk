// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorVR_h
#define NavigatorVR_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/frame/DOMWindowProperty.h"
#include "modules/ModulesExport.h"
#include "modules/vr/VRDisplay.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebVector.h"
#include "wtf/Noncopyable.h"

namespace blink {

class Document;
class Navigator;
class VRController;
class VRDisplayCollection;

class MODULES_EXPORT NavigatorVR final : public GarbageCollectedFinalized<NavigatorVR>, public Supplement<Navigator>, public DOMWindowProperty {
    USING_GARBAGE_COLLECTED_MIXIN(NavigatorVR);
    WTF_MAKE_NONCOPYABLE(NavigatorVR);
public:
    static NavigatorVR* from(Document&);
    static NavigatorVR& from(Navigator&);
    virtual ~NavigatorVR();

    static ScriptPromise getVRDisplays(ScriptState*, Navigator&);
    ScriptPromise getVRDisplays(ScriptState*);

    VRController* controller();
    Document* document();

    DECLARE_VIRTUAL_TRACE();

private:
    friend class VRDisplay;
    friend class VRGetDevicesCallback;

    explicit NavigatorVR(LocalFrame*);

    static const char* supplementName();

    Member<VRDisplayCollection> m_displays;
};

} // namespace blink

#endif // NavigatorVR_h
