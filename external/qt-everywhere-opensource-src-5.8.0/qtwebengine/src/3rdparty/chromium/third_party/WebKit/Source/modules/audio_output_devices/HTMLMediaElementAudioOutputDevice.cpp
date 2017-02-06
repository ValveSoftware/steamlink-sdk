// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/audio_output_devices/HTMLMediaElementAudioOutputDevice.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExecutionContext.h"
#include "modules/audio_output_devices/AudioOutputDeviceClient.h"
#include "modules/audio_output_devices/SetSinkIdCallbacks.h"
#include "public/platform/WebSecurityOrigin.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

class SetSinkIdResolver : public ScriptPromiseResolver {
    WTF_MAKE_NONCOPYABLE(SetSinkIdResolver);
public:
    static SetSinkIdResolver* create(ScriptState*, HTMLMediaElement&, const String& sinkId);
    ~SetSinkIdResolver() override = default;
    void startAsync();

    DECLARE_VIRTUAL_TRACE();

private:
    SetSinkIdResolver(ScriptState*, HTMLMediaElement&, const String& sinkId);
    void timerFired(Timer<SetSinkIdResolver>*);

    Member<HTMLMediaElement> m_element;
    String m_sinkId;
    Timer<SetSinkIdResolver> m_timer;
};

SetSinkIdResolver* SetSinkIdResolver::create(ScriptState* scriptState, HTMLMediaElement& element, const String& sinkId)
{
    SetSinkIdResolver* resolver = new SetSinkIdResolver(scriptState, element, sinkId);
    resolver->suspendIfNeeded();
    resolver->keepAliveWhilePending();
    return resolver;
}

SetSinkIdResolver::SetSinkIdResolver(ScriptState* scriptState, HTMLMediaElement& element, const String& sinkId)
    : ScriptPromiseResolver(scriptState)
    , m_element(element)
    , m_sinkId(sinkId)
    , m_timer(this, &SetSinkIdResolver::timerFired)
{
}

void SetSinkIdResolver::startAsync()
{
    m_timer.startOneShot(0, BLINK_FROM_HERE);
}

void SetSinkIdResolver::timerFired(Timer<SetSinkIdResolver>* timer)
{
    ExecutionContext* context = getExecutionContext();
    ASSERT(context && context->isDocument());
    std::unique_ptr<SetSinkIdCallbacks> callbacks = wrapUnique(new SetSinkIdCallbacks(this, *m_element, m_sinkId));
    WebMediaPlayer* webMediaPlayer = m_element->webMediaPlayer();
    if (webMediaPlayer) {
        // Using release() to transfer ownership because |webMediaPlayer| is a platform object that takes raw pointers
        webMediaPlayer->setSinkId(m_sinkId, WebSecurityOrigin(context->getSecurityOrigin()), callbacks.release());
    } else {
        if (AudioOutputDeviceClient* client = AudioOutputDeviceClient::from(context)) {
            client->checkIfAudioSinkExistsAndIsAuthorized(context, m_sinkId, std::move(callbacks));
        } else {
            // The context has been detached. Impossible to get a security origin to check.
            ASSERT(context->activeDOMObjectsAreStopped());
            reject(DOMException::create(SecurityError, "Impossible to authorize device for detached context"));
        }
    }
}

DEFINE_TRACE(SetSinkIdResolver)
{
    visitor->trace(m_element);
    ScriptPromiseResolver::trace(visitor);
}

} // namespace

HTMLMediaElementAudioOutputDevice::HTMLMediaElementAudioOutputDevice()
    : m_sinkId("")
{
}

String HTMLMediaElementAudioOutputDevice::sinkId(HTMLMediaElement& element)
{
    HTMLMediaElementAudioOutputDevice& aodElement = HTMLMediaElementAudioOutputDevice::from(element);
    return aodElement.m_sinkId;
}

void HTMLMediaElementAudioOutputDevice::setSinkId(const String& sinkId)
{
    m_sinkId = sinkId;
}

ScriptPromise HTMLMediaElementAudioOutputDevice::setSinkId(ScriptState* scriptState, HTMLMediaElement& element, const String& sinkId)
{
    SetSinkIdResolver* resolver = SetSinkIdResolver::create(scriptState, element, sinkId);
    ScriptPromise promise = resolver->promise();
    if (sinkId == HTMLMediaElementAudioOutputDevice::sinkId(element))
        resolver->resolve();
    else
        resolver->startAsync();

    return promise;
}

const char* HTMLMediaElementAudioOutputDevice::supplementName()
{
    return "HTMLMediaElementAudioOutputDevice";
}

HTMLMediaElementAudioOutputDevice& HTMLMediaElementAudioOutputDevice::from(HTMLMediaElement& element)
{
    HTMLMediaElementAudioOutputDevice* supplement = static_cast<HTMLMediaElementAudioOutputDevice*>(Supplement<HTMLMediaElement>::from(element, supplementName()));
    if (!supplement) {
        supplement = new HTMLMediaElementAudioOutputDevice();
        provideTo(element, supplementName(), supplement);
    }
    return *supplement;
}

DEFINE_TRACE(HTMLMediaElementAudioOutputDevice)
{
    Supplement<HTMLMediaElement>::trace(visitor);
}

} // namespace blink
