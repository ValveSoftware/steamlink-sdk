/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * 3. Neither the name of Google Inc. nor the names of its contributors
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

#ifndef V0CustomElementRegistrationContext_h
#define V0CustomElementRegistrationContext_h

#include "core/dom/QualifiedName.h"
#include "core/dom/custom/V0CustomElementDescriptor.h"
#include "core/dom/custom/V0CustomElementRegistry.h"
#include "core/dom/custom/V0CustomElementUpgradeCandidateMap.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class CustomElementsRegistry;

class V0CustomElementRegistrationContext final : public GarbageCollectedFinalized<V0CustomElementRegistrationContext> {
public:
    static V0CustomElementRegistrationContext* create()
    {
        return new V0CustomElementRegistrationContext();
    }

    ~V0CustomElementRegistrationContext() { }
    void documentWasDetached() { m_registry.documentWasDetached(); }

    // Definitions
    void registerElement(Document*, V0CustomElementConstructorBuilder*, const AtomicString& type, V0CustomElement::NameSet validNames, ExceptionState&);

    Element* createCustomTagElement(Document&, const QualifiedName&);
    static void setIsAttributeAndTypeExtension(Element*, const AtomicString& type);
    static void setTypeExtension(Element*, const AtomicString& type);

    void resolve(Element*, const V0CustomElementDescriptor&);

    bool nameIsDefined(const AtomicString& name) const;
    void setV1(const CustomElementsRegistry*);

    DECLARE_TRACE();

protected:
    V0CustomElementRegistrationContext();

    // Instance creation
    void didGiveTypeExtension(Element*, const AtomicString& type);

private:
    void resolveOrScheduleResolution(Element*, const AtomicString& typeExtension);

    V0CustomElementRegistry m_registry;

    // Element creation
    Member<V0CustomElementUpgradeCandidateMap> m_candidates;
};

} // namespace blink

#endif // V0CustomElementRegistrationContext_h
