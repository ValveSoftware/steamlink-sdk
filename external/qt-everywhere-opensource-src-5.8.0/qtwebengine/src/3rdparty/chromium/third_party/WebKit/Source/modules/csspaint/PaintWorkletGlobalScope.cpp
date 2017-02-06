// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorkletGlobalScope.h"

#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/CSSPropertyNames.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/dom/ExceptionCode.h"
#include "core/inspector/MainThreadDebugger.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"

namespace blink {

// static
PaintWorkletGlobalScope* PaintWorkletGlobalScope::create(LocalFrame* frame, const KURL& url, const String& userAgent, PassRefPtr<SecurityOrigin> securityOrigin, v8::Isolate* isolate)
{
    PaintWorkletGlobalScope* paintWorkletGlobalScope = new PaintWorkletGlobalScope(frame, url, userAgent, securityOrigin, isolate);
    paintWorkletGlobalScope->scriptController()->initializeContextIfNeeded();
    MainThreadDebugger::instance()->contextCreated(paintWorkletGlobalScope->scriptController()->getScriptState(), paintWorkletGlobalScope->frame(), paintWorkletGlobalScope->getSecurityOrigin());
    return paintWorkletGlobalScope;
}

PaintWorkletGlobalScope::PaintWorkletGlobalScope(LocalFrame* frame, const KURL& url, const String& userAgent, PassRefPtr<SecurityOrigin> securityOrigin, v8::Isolate* isolate)
    : MainThreadWorkletGlobalScope(frame, url, userAgent, securityOrigin, isolate)
{
}

PaintWorkletGlobalScope::~PaintWorkletGlobalScope()
{
}

void PaintWorkletGlobalScope::dispose()
{
    // Explicitly clear the paint defininitions to break a reference cycle
    // between them and this global scope.
    m_paintDefinitions.clear();

    WorkletGlobalScope::dispose();
}

void PaintWorkletGlobalScope::registerPaint(const String& name, const ScriptValue& ctorValue, ExceptionState& exceptionState)
{
    if (m_paintDefinitions.contains(name)) {
        exceptionState.throwDOMException(NotSupportedError, "A class with name:'" + name + "' is already registered.");
        return;
    }

    if (name.isEmpty()) {
        exceptionState.throwTypeError("The empty string is not a valid name.");
        return;
    }

    v8::Isolate* isolate = scriptController()->getScriptState()->isolate();
    v8::Local<v8::Context> context = scriptController()->context();

    ASSERT(ctorValue.v8Value()->IsFunction());
    v8::Local<v8::Function> constructor = v8::Local<v8::Function>::Cast(ctorValue.v8Value());

    v8::Local<v8::Value> inputPropertiesValue;
    if (!v8Call(constructor->Get(context, v8String(isolate, "inputProperties")), inputPropertiesValue))
        return;

    Vector<CSSPropertyID> nativeInvalidationProperties;
    Vector<AtomicString> customInvalidationProperties;

    if (!isUndefinedOrNull(inputPropertiesValue)) {
        Vector<String> properties = toImplArray<Vector<String>>(inputPropertiesValue, 0, isolate, exceptionState);

        if (exceptionState.hadException())
            return;

        for (const auto& property : properties) {
            CSSPropertyID propertyID = cssPropertyID(property);
            if (propertyID == CSSPropertyInvalid) {
                if (CSSVariableParser::isValidVariableName(property))
                    customInvalidationProperties.append(property);
            } else {
                nativeInvalidationProperties.append(propertyID);
            }
        }
    }

    v8::Local<v8::Value> prototypeValue;
    if (!v8Call(constructor->Get(context, v8String(isolate, "prototype")), prototypeValue))
        return;

    if (isUndefinedOrNull(prototypeValue)) {
        exceptionState.throwTypeError("The 'prototype' object on the class does not exist.");
        return;
    }

    if (!prototypeValue->IsObject()) {
        exceptionState.throwTypeError("The 'prototype' property on the class is not an object.");
        return;
    }

    v8::Local<v8::Object> prototype = v8::Local<v8::Object>::Cast(prototypeValue);

    v8::Local<v8::Value> paintValue;
    if (!v8Call(prototype->Get(context, v8String(isolate, "paint")), paintValue))
        return;

    if (isUndefinedOrNull(paintValue)) {
        exceptionState.throwTypeError("The 'paint' function on the prototype does not exist.");
        return;
    }

    if (!paintValue->IsFunction()) {
        exceptionState.throwTypeError("The 'paint' property on the prototype is not a function.");
        return;
    }

    v8::Local<v8::Function> paint = v8::Local<v8::Function>::Cast(paintValue);

    CSSPaintDefinition* definition = CSSPaintDefinition::create(scriptController()->getScriptState(), constructor, paint, nativeInvalidationProperties, customInvalidationProperties);
    m_paintDefinitions.set(name, definition);

    // Set the definition on any pending generators.
    GeneratorHashSet* set = m_pendingGenerators.get(name);
    if (set) {
        for (const auto& generator : *set) {
            if (generator) {
                generator->setDefinition(definition);
            }
        }
    }
    m_pendingGenerators.remove(name);
}

CSSPaintDefinition* PaintWorkletGlobalScope::findDefinition(const String& name)
{
    return m_paintDefinitions.get(name);
}

void PaintWorkletGlobalScope::addPendingGenerator(const String& name, CSSPaintImageGeneratorImpl* generator)
{
    Member<GeneratorHashSet>& set = m_pendingGenerators.add(name, nullptr).storedValue->value;
    if (!set)
        set = new GeneratorHashSet;
    set->add(generator);
}

DEFINE_TRACE(PaintWorkletGlobalScope)
{
    visitor->trace(m_paintDefinitions);
    visitor->trace(m_pendingGenerators);
    MainThreadWorkletGlobalScope::trace(visitor);
}

} // namespace blink
