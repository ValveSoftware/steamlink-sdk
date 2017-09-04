// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptCustomElementDefinitionBuilder_h
#define ScriptCustomElementDefinitionBuilder_h

#include "core/CoreExport.h"
#include "core/dom/custom/CustomElementDefinitionBuilder.h"
#include "platform/heap/Handle.h"
#include "v8.h"
#include "wtf/Allocator.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringView.h"

namespace blink {

class CustomElementRegistry;
class ExceptionState;
class ScriptState;
class ScriptValue;

class CORE_EXPORT ScriptCustomElementDefinitionBuilder
    : public CustomElementDefinitionBuilder {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ScriptCustomElementDefinitionBuilder);

 public:
  ScriptCustomElementDefinitionBuilder(
      ScriptState*,
      CustomElementRegistry*,
      const ScriptValue& constructorScriptValue,
      ExceptionState&);
  ~ScriptCustomElementDefinitionBuilder();

  bool checkConstructorIntrinsics() override;
  bool checkConstructorNotRegistered() override;
  bool checkPrototype() override;
  bool rememberOriginalProperties() override;
  CustomElementDefinition* build(const CustomElementDescriptor&) override;

 private:
  static ScriptCustomElementDefinitionBuilder* s_stack;

  ScriptCustomElementDefinitionBuilder* m_prev;
  RefPtr<ScriptState> m_scriptState;
  Member<CustomElementRegistry> m_registry;
  v8::Local<v8::Value> m_constructorValue;
  v8::Local<v8::Object> m_constructor;
  v8::Local<v8::Object> m_prototype;
  v8::Local<v8::Function> m_connectedCallback;
  v8::Local<v8::Function> m_disconnectedCallback;
  v8::Local<v8::Function> m_adoptedCallback;
  v8::Local<v8::Function> m_attributeChangedCallback;
  HashSet<AtomicString> m_observedAttributes;
  ExceptionState& m_exceptionState;

  bool valueForName(const v8::Local<v8::Object>&,
                    const StringView&,
                    v8::Local<v8::Value>&) const;
  bool callableForName(const StringView&, v8::Local<v8::Function>&) const;
  bool retrieveObservedAttributes();
};

}  // namespace blink

#endif  // ScriptCustomElementDefinitionBuilder_h
