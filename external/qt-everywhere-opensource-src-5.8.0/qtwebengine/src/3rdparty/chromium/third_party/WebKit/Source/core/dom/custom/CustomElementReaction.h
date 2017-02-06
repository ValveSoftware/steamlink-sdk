// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementReaction_h
#define CustomElementReaction_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CustomElementDefinition;
class Element;

class CORE_EXPORT CustomElementReaction
    : public GarbageCollectedFinalized<CustomElementReaction> {
    WTF_MAKE_NONCOPYABLE(CustomElementReaction);
public:
    CustomElementReaction(CustomElementDefinition*);
    virtual ~CustomElementReaction() = default;

    virtual void invoke(Element*) = 0;

    DECLARE_VIRTUAL_TRACE();

protected:
    Member<CustomElementDefinition> m_definition;
};

} // namespace blink

#endif // CustomElementReaction_h
