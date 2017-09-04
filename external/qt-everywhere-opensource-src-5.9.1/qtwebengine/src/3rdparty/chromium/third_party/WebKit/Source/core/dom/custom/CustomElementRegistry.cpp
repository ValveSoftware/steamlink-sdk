// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementRegistry.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptCustomElementDefinitionBuilder.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/HTMLElementTypeHelpers.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementDefinitionOptions.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/custom/CEReactionsScope.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/CustomElementDefinitionBuilder.h"
#include "core/dom/custom/CustomElementDescriptor.h"
#include "core/dom/custom/CustomElementUpgradeReaction.h"
#include "core/dom/custom/CustomElementUpgradeSorter.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "platform/tracing/TraceEvent.h"
#include "wtf/Allocator.h"

namespace blink {

// Returns true if |name| is invalid.
static bool throwIfInvalidName(const AtomicString& name,
                               ExceptionState& exceptionState) {
  if (CustomElement::isValidName(name))
    return false;
  exceptionState.throwDOMException(
      SyntaxError, "\"" + name + "\" is not a valid custom element name");
  return true;
}

// Returns true if |name| is valid.
static bool throwIfValidName(const AtomicString& name,
                             ExceptionState& exceptionState) {
  if (!CustomElement::isValidName(name))
    return false;
  exceptionState.throwDOMException(
      NotSupportedError, "\"" + name + "\" is a valid custom element name");
  return true;
}

class CustomElementRegistry::ElementDefinitionIsRunning final {
  STACK_ALLOCATED();
  DISALLOW_IMPLICIT_CONSTRUCTORS(ElementDefinitionIsRunning);

 public:
  ElementDefinitionIsRunning(bool& flag) : m_flag(flag) {
    DCHECK(!m_flag);
    m_flag = true;
  }

  ~ElementDefinitionIsRunning() {
    DCHECK(m_flag);
    m_flag = false;
  }

 private:
  bool& m_flag;
};

CustomElementRegistry* CustomElementRegistry::create(
    const LocalDOMWindow* owner) {
  CustomElementRegistry* registry = new CustomElementRegistry(owner);
  Document* document = owner->document();
  if (V0CustomElementRegistrationContext* v0 =
          document ? document->registrationContext() : nullptr)
    registry->entangle(v0);
  return registry;
}

CustomElementRegistry::CustomElementRegistry(const LocalDOMWindow* owner)
    : m_elementDefinitionIsRunning(false),
      m_owner(owner),
      m_v0(new V0RegistrySet()),
      m_upgradeCandidates(new UpgradeCandidateMap()) {}

DEFINE_TRACE(CustomElementRegistry) {
  visitor->trace(m_definitions);
  visitor->trace(m_owner);
  visitor->trace(m_v0);
  visitor->trace(m_upgradeCandidates);
  visitor->trace(m_whenDefinedPromiseMap);
}

CustomElementDefinition* CustomElementRegistry::define(
    ScriptState* scriptState,
    const AtomicString& name,
    const ScriptValue& constructor,
    const ElementDefinitionOptions& options,
    ExceptionState& exceptionState) {
  ScriptCustomElementDefinitionBuilder builder(scriptState, this, constructor,
                                               exceptionState);
  return define(name, builder, options, exceptionState);
}

// http://w3c.github.io/webcomponents/spec/custom/#dfn-element-definition
CustomElementDefinition* CustomElementRegistry::define(
    const AtomicString& name,
    CustomElementDefinitionBuilder& builder,
    const ElementDefinitionOptions& options,
    ExceptionState& exceptionState) {
  TRACE_EVENT1("blink", "CustomElementRegistry::define", "name", name.utf8());
  if (!builder.checkConstructorIntrinsics())
    return nullptr;

  if (throwIfInvalidName(name, exceptionState))
    return nullptr;

  if (nameIsDefined(name) || v0NameIsDefined(name)) {
    exceptionState.throwDOMException(
        NotSupportedError,
        "this name has already been used with this registry");
    return nullptr;
  }

  if (!builder.checkConstructorNotRegistered())
    return nullptr;

  AtomicString localName = name;

  // Step 7. customized built-in elements definition
  // element interface extends option checks
  if (RuntimeEnabledFeatures::customElementsBuiltinEnabled() &&
      options.hasExtends()) {
    // 7.1. If element interface is valid custom element name, throw exception
    const AtomicString& extends = AtomicString(options.extends());
    if (throwIfValidName(AtomicString(options.extends()), exceptionState))
      return nullptr;
    // 7.2. If element interface is undefined element, throw exception
    if (htmlElementTypeForTag(extends) ==
        HTMLElementType::kHTMLUnknownElement) {
      exceptionState.throwDOMException(
          NotSupportedError, "\"" + extends + "\" is an HTMLUnknownElement");
      return nullptr;
    }
    // 7.3. Set localName to extends
    localName = extends;
  }

  // TODO(dominicc): Add a test where the prototype getter destroys
  // the context.

  // 8. If this CustomElementRegistry's element definition is
  // running flag is set, then throw a "NotSupportedError"
  // DOMException and abort these steps.
  if (m_elementDefinitionIsRunning) {
    exceptionState.throwDOMException(
        NotSupportedError, "an element definition is already being processed");
    return nullptr;
  }

  {
    // 9. Set this CustomElementRegistry's element definition is
    // running flag.
    ElementDefinitionIsRunning defining(m_elementDefinitionIsRunning);

    // 10.1-2
    if (!builder.checkPrototype())
      return nullptr;

    // 10.3-6
    if (!builder.rememberOriginalProperties())
      return nullptr;

    // "Then, perform the following substep, regardless of whether
    // the above steps threw an exception or not: Unset this
    // CustomElementRegistry's element definition is running
    // flag."
    // (ElementDefinitionIsRunning destructor does this.)
  }

  CustomElementDescriptor descriptor(name, localName);
  CustomElementDefinition* definition = builder.build(descriptor);
  CHECK(!exceptionState.hadException());
  CHECK(definition->descriptor() == descriptor);
  DefinitionMap::AddResult result =
      m_definitions.add(descriptor.name(), definition);
  CHECK(result.isNewEntry);

  HeapVector<Member<Element>> candidates;
  collectCandidates(descriptor, &candidates);
  for (Element* candidate : candidates)
    definition->enqueueUpgradeReaction(candidate);

  // 16: when-defined promise processing
  const auto& entry = m_whenDefinedPromiseMap.find(name);
  if (entry != m_whenDefinedPromiseMap.end()) {
    entry->value->resolve();
    m_whenDefinedPromiseMap.remove(entry);
  }
  return definition;
}

// https://html.spec.whatwg.org/multipage/scripting.html#dom-customelementsregistry-get
ScriptValue CustomElementRegistry::get(const AtomicString& name) {
  CustomElementDefinition* definition = definitionForName(name);
  if (!definition) {
    // Binding layer converts |ScriptValue()| to script specific value,
    // e.g. |undefined| for v8.
    return ScriptValue();
  }
  return definition->getConstructorForScript();
}

// https://html.spec.whatwg.org/multipage/scripting.html#look-up-a-custom-element-definition
// At this point, what the spec calls 'is' is 'name' from desc
CustomElementDefinition* CustomElementRegistry::definitionFor(
    const CustomElementDescriptor& desc) const {
  // 4&5. If there is a definition in registry with name equal to is/localName
  // Autonomous elements have the same name and local name
  CustomElementDefinition* definition = definitionForName(desc.name());
  // 4&5. and name equal to localName, return that definition
  if (definition and definition->descriptor().localName() == desc.localName())
    return definition;
  // 6. Return null
  return nullptr;
}

bool CustomElementRegistry::nameIsDefined(const AtomicString& name) const {
  return m_definitions.contains(name);
}

void CustomElementRegistry::entangle(V0CustomElementRegistrationContext* v0) {
  m_v0->add(v0);
  v0->setV1(this);
}

bool CustomElementRegistry::v0NameIsDefined(const AtomicString& name) {
  for (const auto& v0 : *m_v0) {
    if (v0->nameIsDefined(name))
      return true;
  }
  return false;
}

CustomElementDefinition* CustomElementRegistry::definitionForName(
    const AtomicString& name) const {
  return m_definitions.get(name);
}

void CustomElementRegistry::addCandidate(Element* candidate) {
  const AtomicString& name = candidate->localName();
  if (nameIsDefined(name) || v0NameIsDefined(name))
    return;
  UpgradeCandidateMap::iterator it = m_upgradeCandidates->find(name);
  UpgradeCandidateSet* set;
  if (it != m_upgradeCandidates->end()) {
    set = it->value;
  } else {
    set = m_upgradeCandidates->add(name, new UpgradeCandidateSet())
              .storedValue->value;
  }
  set->add(candidate);
}

// https://html.spec.whatwg.org/multipage/scripting.html#dom-customelementsregistry-whendefined
ScriptPromise CustomElementRegistry::whenDefined(
    ScriptState* scriptState,
    const AtomicString& name,
    ExceptionState& exceptionState) {
  if (throwIfInvalidName(name, exceptionState))
    return ScriptPromise();
  CustomElementDefinition* definition = definitionForName(name);
  if (definition)
    return ScriptPromise::castUndefined(scriptState);
  ScriptPromiseResolver* resolver = m_whenDefinedPromiseMap.get(name);
  if (resolver)
    return resolver->promise();
  ScriptPromiseResolver* newResolver =
      ScriptPromiseResolver::create(scriptState);
  m_whenDefinedPromiseMap.add(name, newResolver);
  return newResolver->promise();
}

void CustomElementRegistry::collectCandidates(
    const CustomElementDescriptor& desc,
    HeapVector<Member<Element>>* elements) {
  UpgradeCandidateMap::iterator it = m_upgradeCandidates->find(desc.name());
  if (it == m_upgradeCandidates->end())
    return;
  CustomElementUpgradeSorter sorter;
  for (Element* element : *it.get()->value) {
    if (!element || !desc.matches(*element))
      continue;
    sorter.add(element);
  }

  m_upgradeCandidates->remove(it);

  Document* document = m_owner->document();
  if (!document)
    return;

  sorter.sorted(elements, document);
}

}  // namespace blink
