// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementsRegistry_h
#define CustomElementsRegistry_h

#include "base/gtest_prod_util.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class CustomElementDefinition;
class CustomElementDefinitionBuilder;
class CustomElementDescriptor;
class Document;
class Element;
class ElementRegistrationOptions;
class ExceptionState;
class ScriptPromiseResolver;
class ScriptState;
class ScriptValue;
class V0CustomElementRegistrationContext;

class CORE_EXPORT CustomElementsRegistry final
    : public GarbageCollectedFinalized<CustomElementsRegistry>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(CustomElementsRegistry);
public:
    static CustomElementsRegistry* create(Document*);

    virtual ~CustomElementsRegistry() = default;

    void define(
        ScriptState*,
        const AtomicString& name,
        const ScriptValue& constructor,
        const ElementRegistrationOptions&,
        ExceptionState&);

    void define(
        const AtomicString& name,
        CustomElementDefinitionBuilder&,
        const ElementRegistrationOptions&,
        ExceptionState&);

    ScriptValue get(const AtomicString& name);
    bool nameIsDefined(const AtomicString& name) const;
    CustomElementDefinition* definitionForName(const AtomicString& name) const;

    // TODO(dominicc): Consider broadening this API when type extensions are
    // implemented.
    void addCandidate(Element*);
    ScriptPromise whenDefined(
        ScriptState*,
        const AtomicString& name,
        ExceptionState&);

    DECLARE_TRACE();

private:
    friend class CustomElementsRegistryTestBase;

    CustomElementsRegistry(Document*);

    V0CustomElementRegistrationContext* v0();
    bool v0NameIsDefined(const AtomicString& name);

    void collectCandidates(
        const CustomElementDescriptor&,
        HeapVector<Member<Element>>*);

    class NameIsBeingDefined;

    HashSet<AtomicString> m_namesBeingDefined;
    using DefinitionMap =
        HeapHashMap<AtomicString, Member<CustomElementDefinition>>;
    DefinitionMap m_definitions;

    Member<Document> m_document;

    using UpgradeCandidateSet = HeapHashSet<WeakMember<Element>>;
    using UpgradeCandidateMap = HeapHashMap<
        AtomicString,
        Member<UpgradeCandidateSet>>;
    Member<UpgradeCandidateMap> m_upgradeCandidates;

    using WhenDefinedPromiseMap =
        HeapHashMap<AtomicString, Member<ScriptPromiseResolver>>;
    WhenDefinedPromiseMap m_whenDefinedPromiseMap;
};

} // namespace blink

#endif // CustomElementsRegistry_h
