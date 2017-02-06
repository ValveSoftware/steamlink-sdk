// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementAttributeChangedCallbackReaction_h
#define CustomElementAttributeChangedCallbackReaction_h

#include "core/CoreExport.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/custom/CustomElementReaction.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CORE_EXPORT CustomElementAttributeChangedCallbackReaction final
    : public CustomElementReaction {
    WTF_MAKE_NONCOPYABLE(CustomElementAttributeChangedCallbackReaction);
public:
    CustomElementAttributeChangedCallbackReaction(CustomElementDefinition*,
        const QualifiedName&,
        const AtomicString& oldValue, const AtomicString& newValue);

private:
    void invoke(Element*) override;

    QualifiedName m_name;
    AtomicString m_oldValue;
    AtomicString m_newValue;
};

} // namespace blink

#endif // CustomElementAttributeChangedCallbackReaction_h
