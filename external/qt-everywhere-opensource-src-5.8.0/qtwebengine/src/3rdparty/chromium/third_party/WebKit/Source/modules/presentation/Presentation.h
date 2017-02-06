// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Presentation_h
#define Presentation_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/frame/DOMWindowProperty.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"

namespace blink {

class LocalFrame;
class PresentationReceiver;
class PresentationRequest;

// Implements the main entry point of the Presentation API corresponding to the Presentation.idl
// See https://w3c.github.io/presentation-api/#navigatorpresentation for details.
class Presentation final
    : public GarbageCollected<Presentation>
    , public ScriptWrappable
    , public DOMWindowProperty {
    USING_GARBAGE_COLLECTED_MIXIN(Presentation);
    DEFINE_WRAPPERTYPEINFO();
public:
    static Presentation* create(LocalFrame*);

    DECLARE_VIRTUAL_TRACE();

    PresentationRequest* defaultRequest() const;
    void setDefaultRequest(PresentationRequest*);

    PresentationReceiver* receiver();

private:
    explicit Presentation(LocalFrame*);

    // Default PresentationRequest used by the embedder.
    Member<PresentationRequest> m_defaultRequest;

    // PresentationReceiver instance. It will always be nullptr if the Blink
    // instance is not running as a presentation receiver.
    Member<PresentationReceiver> m_receiver;
};

} // namespace blink

#endif // Presentation_h
