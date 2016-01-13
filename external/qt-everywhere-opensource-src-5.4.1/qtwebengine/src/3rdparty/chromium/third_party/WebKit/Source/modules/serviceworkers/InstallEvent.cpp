/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "InstallEvent.h"

#include "bindings/v8/ScriptPromiseResolver.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/NotImplemented.h"
#include "wtf/RefPtr.h"
#include <v8.h>

namespace WebCore {

PassRefPtrWillBeRawPtr<InstallEvent> InstallEvent::create()
{
    return adoptRefWillBeNoop(new InstallEvent());
}

PassRefPtrWillBeRawPtr<InstallEvent> InstallEvent::create(const AtomicString& type, const EventInit& initializer, PassRefPtr<WaitUntilObserver> observer)
{
    return adoptRefWillBeNoop(new InstallEvent(type, initializer, observer));
}

void InstallEvent::replace()
{
    // FIXME: implement.
    notImplemented();
}

ScriptPromise InstallEvent::reloadAll(ScriptState* scriptState)
{
    // FIXME: implement.
    notImplemented();

    // For now this just returns a promise which is already rejected.
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    resolver->reject(ScriptValue(scriptState, v8::Null(scriptState->isolate())));
    return promise;
}

const AtomicString& InstallEvent::interfaceName() const
{
    return EventNames::InstallEvent;
}

InstallEvent::InstallEvent()
{
    ScriptWrappable::init(this);
}

InstallEvent::InstallEvent(const AtomicString& type, const EventInit& initializer, PassRefPtr<WaitUntilObserver> observer)
    : InstallPhaseEvent(type, initializer, observer)
{
    ScriptWrappable::init(this);
}

void InstallEvent::trace(Visitor* visitor)
{
    InstallPhaseEvent::trace(visitor);
}

} // namespace WebCore
