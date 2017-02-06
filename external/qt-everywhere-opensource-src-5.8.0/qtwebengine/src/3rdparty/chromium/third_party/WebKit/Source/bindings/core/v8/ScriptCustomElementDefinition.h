// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptCustomElementDefinition_h
#define ScriptCustomElementDefinition_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/CoreExport.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "v8.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"

namespace blink {

class CustomElementDescriptor;
class CustomElementsRegistry;

class CORE_EXPORT ScriptCustomElementDefinition final :
    public CustomElementDefinition {
    WTF_MAKE_NONCOPYABLE(ScriptCustomElementDefinition);
public:
    static ScriptCustomElementDefinition* forConstructor(
        ScriptState*,
        CustomElementsRegistry*,
        const v8::Local<v8::Value>& constructor);

    static ScriptCustomElementDefinition* create(
        ScriptState*,
        CustomElementsRegistry*,
        const CustomElementDescriptor&,
        const v8::Local<v8::Object>& constructor,
        const v8::Local<v8::Object>& prototype,
        const v8::Local<v8::Function>& connectedCallback,
        const v8::Local<v8::Function>& disconnectedCallback,
        const v8::Local<v8::Function>& attributeChangedCallback,
        const HashSet<AtomicString>& observedAttributes);

    virtual ~ScriptCustomElementDefinition() = default;

    v8::Local<v8::Object> constructor() const;
    v8::Local<v8::Object> prototype() const;

    HTMLElement* createElementSync(Document&, const QualifiedName&) override;
    HTMLElement* createElementSync(Document&, const QualifiedName&, ExceptionState&) override;

    bool hasConnectedCallback() const override;
    bool hasDisconnectedCallback() const override;

    void runConnectedCallback(Element*) override;
    void runDisconnectedCallback(Element*) override;
    void runAttributeChangedCallback(Element*, const QualifiedName&,
        const AtomicString& oldValue, const AtomicString& newValue) override;

private:
    ScriptCustomElementDefinition(
        ScriptState*,
        const CustomElementDescriptor&,
        const v8::Local<v8::Object>& constructor,
        const v8::Local<v8::Object>& prototype,
        const v8::Local<v8::Function>& connectedCallback,
        const v8::Local<v8::Function>& disconnectedCallback,
        const v8::Local<v8::Function>& attributeChangedCallback,
        const HashSet<AtomicString>& observedAttributes);

    // Implementations of |CustomElementDefinition|
    ScriptValue getConstructorForScript() final;
    bool runConstructor(Element*) override;
    Element* runConstructor();

    void runCallback(v8::Local<v8::Function>, Element*,
        int argc = 0, v8::Local<v8::Value> argv[] = nullptr);

    RefPtr<ScriptState> m_scriptState;
    ScopedPersistent<v8::Object> m_constructor;
    ScopedPersistent<v8::Object> m_prototype;
    ScopedPersistent<v8::Function> m_connectedCallback;
    ScopedPersistent<v8::Function> m_disconnectedCallback;
    ScopedPersistent<v8::Function> m_attributeChangedCallback;
};

} // namespace blink

#endif // ScriptCustomElementDefinition_h
