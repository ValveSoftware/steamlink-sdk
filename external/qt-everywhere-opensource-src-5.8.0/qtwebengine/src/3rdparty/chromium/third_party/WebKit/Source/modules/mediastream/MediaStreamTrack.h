/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamTrack_h
#define MediaStreamTrack_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/mediastream/SourceInfo.h"
#include "platform/mediastream/MediaStreamDescriptor.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "public/platform/WebMediaConstraints.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class AudioSourceProvider;
class ExceptionState;
class MediaTrackConstraints;
class MediaStreamTrackSourcesCallback;
class MediaTrackSettings;

class MODULES_EXPORT MediaStreamTrack
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ActiveDOMObject
    , public MediaStreamSource::Observer {
    USING_GARBAGE_COLLECTED_MIXIN(MediaStreamTrack);
    DEFINE_WRAPPERTYPEINFO();
public:
    static MediaStreamTrack* create(ExecutionContext*, MediaStreamComponent*);
    ~MediaStreamTrack() override;

    String kind() const;
    String id() const;
    String label() const;
    bool remote() const;

    bool enabled() const;
    void setEnabled(bool);

    bool muted() const;

    String readyState() const;

    static void getSources(ExecutionContext*, MediaStreamTrackSourcesCallback*, ExceptionState&);
    void stopTrack(ExceptionState&);
    virtual MediaStreamTrack* clone(ExecutionContext*);

    void getConstraints(MediaTrackConstraints&);

    // This function is called when constrains have been successfully applied.
    // Called from UserMediaRequest when it succeeds. It is not IDL-exposed.
    void setConstraints(const WebMediaConstraints&);

    void getSettings(MediaTrackSettings&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(mute);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(unmute);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(ended);

    MediaStreamComponent* component() { return m_component; }
    bool ended() const;

    void registerMediaStream(MediaStream*);
    void unregisterMediaStream(MediaStream*);

    // EventTarget
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    // ActiveScriptWrappable
    bool hasPendingActivity() const final;

    // ActiveDOMObject
    void stop() override;

    std::unique_ptr<AudioSourceProvider> createWebAudioSource();

    DECLARE_VIRTUAL_TRACE();

private:
    friend class CanvasCaptureMediaStreamTrack;

    MediaStreamTrack(ExecutionContext*, MediaStreamComponent*);

    // MediaStreamSourceObserver
    void sourceChangedState() override;

    void propagateTrackEnded();

    MediaStreamSource::ReadyState m_readyState;
    HeapHashSet<Member<MediaStream>> m_registeredMediaStreams;
    bool m_isIteratingRegisteredMediaStreams;
    bool m_stopped;
    Member<MediaStreamComponent> m_component;
    WebMediaConstraints m_constraints;
};

typedef HeapVector<Member<MediaStreamTrack>> MediaStreamTrackVector;

} // namespace blink

#endif // MediaStreamTrack_h
