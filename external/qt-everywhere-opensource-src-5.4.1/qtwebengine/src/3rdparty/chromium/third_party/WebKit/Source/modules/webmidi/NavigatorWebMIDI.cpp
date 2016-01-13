/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/webmidi/NavigatorWebMIDI.h"

#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMError.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/webmidi/MIDIAccessInitializer.h"
#include "modules/webmidi/MIDIOptions.h"

namespace WebCore {

NavigatorWebMIDI::NavigatorWebMIDI(LocalFrame* frame)
    : DOMWindowProperty(frame)
{
}

NavigatorWebMIDI::~NavigatorWebMIDI()
{
}

const char* NavigatorWebMIDI::supplementName()
{
    return "NavigatorWebMIDI";
}

NavigatorWebMIDI& NavigatorWebMIDI::from(Navigator& navigator)
{
    NavigatorWebMIDI* supplement = static_cast<NavigatorWebMIDI*>(WillBeHeapSupplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorWebMIDI(navigator.frame());
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

ScriptPromise NavigatorWebMIDI::requestMIDIAccess(ScriptState* scriptState, Navigator& navigator, const Dictionary& options)
{
    return NavigatorWebMIDI::from(navigator).requestMIDIAccess(scriptState, options);
}

ScriptPromise NavigatorWebMIDI::requestMIDIAccess(ScriptState* scriptState, const Dictionary& options)
{
    if (!frame() || frame()->document()->activeDOMObjectsAreStopped()) {
        RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
        ScriptPromise promise = resolver->promise();
        // FIXME: Currently this rejection does not work because the context is stopped.
        resolver->reject(DOMError::create("AbortError"));
        return promise;
    }

    return MIDIAccessInitializer::start(scriptState, MIDIOptions(options));
}

} // namespace WebCore
