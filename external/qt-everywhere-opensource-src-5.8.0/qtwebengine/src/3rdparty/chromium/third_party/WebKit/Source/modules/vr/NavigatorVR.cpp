// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/NavigatorVR.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/page/Page.h"
#include "modules/vr/VRController.h"
#include "modules/vr/VRDisplay.h"
#include "modules/vr/VRDisplayCollection.h"
#include "modules/vr/VRGetDevicesCallback.h"
#include "modules/vr/VRPose.h"
#include "wtf/PtrUtil.h"

namespace blink {

NavigatorVR* NavigatorVR::from(Document& document)
{
    if (!document.frame() || !document.frame()->domWindow())
        return 0;
    Navigator& navigator = *document.frame()->domWindow()->navigator();
    return &from(navigator);
}

NavigatorVR& NavigatorVR::from(Navigator& navigator)
{
    NavigatorVR* supplement = static_cast<NavigatorVR*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorVR(navigator.frame());
        provideTo(navigator, supplementName(), supplement);
    }
    return *supplement;
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* scriptState, Navigator& navigator)
{
    return NavigatorVR::from(navigator).getVRDisplays(scriptState);
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    Document* document = m_frame ? m_frame->document() : 0;

    if (!document || !controller()) {
        DOMException* exception = DOMException::create(InvalidStateError, "The object is no longer associated to a document.");
        resolver->reject(exception);
        return promise;
    }

    controller()->getDisplays(WTF::wrapUnique(new VRGetDevicesCallback(resolver, m_displays.get())));

    return promise;
}

VRController* NavigatorVR::controller()
{
    if (!frame())
        return 0;

    return VRController::from(*frame());
}

Document* NavigatorVR::document()
{
    return m_frame ? m_frame->document() : 0;
}

DEFINE_TRACE(NavigatorVR)
{
    visitor->trace(m_displays);

    Supplement<Navigator>::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

NavigatorVR::NavigatorVR(LocalFrame* frame)
    : DOMWindowProperty(frame)
{
    m_displays = new VRDisplayCollection(this);
}

NavigatorVR::~NavigatorVR()
{
}

const char* NavigatorVR::supplementName()
{
    return "NavigatorVR";
}

} // namespace blink
