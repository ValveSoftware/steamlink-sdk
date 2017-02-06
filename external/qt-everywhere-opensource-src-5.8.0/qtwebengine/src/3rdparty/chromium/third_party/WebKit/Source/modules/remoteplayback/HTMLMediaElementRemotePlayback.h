// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementRemotePlayback_h
#define HTMLMediaElementRemotePlayback_h

#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLMediaElement;
class QualifiedName;
class RemotePlayback;

// Class used to implement the Remote Playback API. It is a supplement to
// HTMLMediaElement.
class HTMLMediaElementRemotePlayback final : public GarbageCollected<HTMLMediaElementRemotePlayback>, public Supplement<HTMLMediaElement> {
    USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElementRemotePlayback);
public:
    static bool fastHasAttribute(const QualifiedName&, const HTMLMediaElement&);
    static void setBooleanAttribute(const QualifiedName&, HTMLMediaElement&, bool);

    static HTMLMediaElementRemotePlayback& from(HTMLMediaElement&);
    static RemotePlayback* remote(HTMLMediaElement&);

    DECLARE_VIRTUAL_TRACE();

private:
    static const char* supplementName();

    Member<RemotePlayback> m_remote;
};

} // namespace blink

#endif // HTMLMediaElementRemotePlayback_h
