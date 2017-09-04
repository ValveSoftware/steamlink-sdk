// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDefinition_h
#define CustomElementDefinition_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/custom/CustomElementDescriptor.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class Document;
class Element;
class ExceptionState;
class HTMLElement;
class QualifiedName;

class CORE_EXPORT CustomElementDefinition
    : public GarbageCollectedFinalized<CustomElementDefinition> {
  WTF_MAKE_NONCOPYABLE(CustomElementDefinition);

 public:
  CustomElementDefinition(const CustomElementDescriptor&);
  CustomElementDefinition(const CustomElementDescriptor&,
                          const HashSet<AtomicString>&);
  virtual ~CustomElementDefinition();

  DECLARE_VIRTUAL_TRACE();

  const CustomElementDescriptor& descriptor() { return m_descriptor; }

  // TODO(yosin): To support Web Modules, introduce an abstract
  // class |CustomElementConstructor| to allow us to have JavaScript
  // and C++ constructors and ask the binding layer to convert
  // |CustomElementConstructor| to |ScriptValue|. Replace
  // |getConstructorForScript()| by |getConstructor() ->
  // CustomElementConstructor|.
  virtual ScriptValue getConstructorForScript() = 0;

  using ConstructionStack = HeapVector<Member<Element>, 1>;
  ConstructionStack& constructionStack() { return m_constructionStack; }

  HTMLElement* createElementForConstructor(Document&);
  virtual HTMLElement* createElementSync(Document&, const QualifiedName&) = 0;
  HTMLElement* createElementAsync(Document&, const QualifiedName&);

  void upgrade(Element*);

  virtual bool hasConnectedCallback() const = 0;
  virtual bool hasDisconnectedCallback() const = 0;
  virtual bool hasAdoptedCallback() const = 0;
  bool hasAttributeChangedCallback(const QualifiedName&) const;
  bool hasStyleAttributeChangedCallback() const;

  virtual void runConnectedCallback(Element*) = 0;
  virtual void runDisconnectedCallback(Element*) = 0;
  virtual void runAdoptedCallback(Element*,
                                  Document* oldOwner,
                                  Document* newOwner) = 0;
  virtual void runAttributeChangedCallback(Element*,
                                           const QualifiedName&,
                                           const AtomicString& oldValue,
                                           const AtomicString& newValue) = 0;

  void enqueueUpgradeReaction(Element*);
  void enqueueConnectedCallback(Element*);
  void enqueueDisconnectedCallback(Element*);
  void enqueueAdoptedCallback(Element*, Document* oldOwner, Document* newOwner);
  void enqueueAttributeChangedCallback(Element*,
                                       const QualifiedName&,
                                       const AtomicString& oldValue,
                                       const AtomicString& newValue);

  class CORE_EXPORT ConstructionStackScope final {
    STACK_ALLOCATED();
    DISALLOW_COPY_AND_ASSIGN(ConstructionStackScope);

   public:
    ConstructionStackScope(CustomElementDefinition*, Element*);
    ~ConstructionStackScope();

   private:
    ConstructionStack& m_constructionStack;
    Member<Element> m_element;
    size_t m_depth;
  };

 protected:
  virtual bool runConstructor(Element*) = 0;

  static void checkConstructorResult(Element*,
                                     Document&,
                                     const QualifiedName&,
                                     ExceptionState&);

 private:
  const CustomElementDescriptor m_descriptor;
  ConstructionStack m_constructionStack;
  HashSet<AtomicString> m_observedAttributes;
  bool m_hasStyleAttributeChangedCallback;

  void enqueueAttributeChangedCallbackForAllAttributes(Element*);
};

}  // namespace blink

#endif  // CustomElementDefinition_h
