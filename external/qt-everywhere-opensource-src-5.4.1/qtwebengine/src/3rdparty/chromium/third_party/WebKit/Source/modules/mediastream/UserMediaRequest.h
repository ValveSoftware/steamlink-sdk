/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef UserMediaRequest_h
#define UserMediaRequest_h

#include "core/dom/ActiveDOMObject.h"
#include "modules/mediastream/NavigatorUserMediaErrorCallback.h"
#include "modules/mediastream/NavigatorUserMediaSuccessCallback.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "public/platform/WebMediaConstraints.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Dictionary;
class Document;
class ExceptionState;
class MediaStreamDescriptor;
class UserMediaController;

class UserMediaRequest FINAL : public RefCountedWillBeGarbageCollectedFinalized<UserMediaRequest>, public ContextLifecycleObserver {
public:
    static PassRefPtrWillBeRawPtr<UserMediaRequest> create(ExecutionContext*, UserMediaController*, const Dictionary& options, PassOwnPtr<NavigatorUserMediaSuccessCallback>, PassOwnPtr<NavigatorUserMediaErrorCallback>, ExceptionState&);
    virtual ~UserMediaRequest();

    NavigatorUserMediaSuccessCallback* successCallback() const { return m_successCallback.get(); }
    NavigatorUserMediaErrorCallback* errorCallback() const { return m_errorCallback.get(); }
    Document* ownerDocument();

    void start();

    void succeed(PassRefPtr<MediaStreamDescriptor>);
    void failPermissionDenied(const String& message);
    void failConstraint(const String& constraintName, const String& message);
    void failUASpecific(const String& name, const String& message, const String& constraintName);

    bool audio() const;
    bool video() const;
    blink::WebMediaConstraints audioConstraints() const;
    blink::WebMediaConstraints videoConstraints() const;

    // ContextLifecycleObserver
    virtual void contextDestroyed() OVERRIDE;

    void trace(Visitor*) { }

private:
    UserMediaRequest(ExecutionContext*, UserMediaController*, blink::WebMediaConstraints audio, blink::WebMediaConstraints video, PassOwnPtr<NavigatorUserMediaSuccessCallback>, PassOwnPtr<NavigatorUserMediaErrorCallback>);

    blink::WebMediaConstraints m_audio;
    blink::WebMediaConstraints m_video;

    UserMediaController* m_controller;

    OwnPtr<NavigatorUserMediaSuccessCallback> m_successCallback;
    OwnPtr<NavigatorUserMediaErrorCallback> m_errorCallback;
};

} // namespace WebCore

#endif // UserMediaRequest_h
