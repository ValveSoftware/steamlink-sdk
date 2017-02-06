// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDefinitionBuilder_h
#define CustomElementDefinitionBuilder_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CustomElementDefinition;
class CustomElementDescriptor;
class CustomElementsRegistry;
class ExceptionState;
class ScriptState;
class ScriptValue;

// Implement CustomElementDefinitionBuilder to provide
// technology-specific steps for CustomElementsRegistry.define.
// https://html.spec.whatwg.org/multipage/scripting.html#dom-customelementsregistry-define
class CORE_EXPORT CustomElementDefinitionBuilder {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(CustomElementDefinitionBuilder);
public:
    CustomElementDefinitionBuilder() { }

    // This API necessarily sounds JavaScript specific; this implements
    // some steps of the CustomElementsRegistry.define process, which
    // are defined in terms of JavaScript.

    // Check the constructor is valid. Return false if processing
    // should not proceed.
    virtual bool checkConstructorIntrinsics() = 0;

    // Check the constructor is not already registered in the calling
    // registry. Return false if processing should not proceed.
    virtual bool checkConstructorNotRegistered() = 0;

    // Checking the prototype may destroy the window. Return false if
    // processing should not proceed.
    virtual bool checkPrototype() = 0;

    // Cache properties for build to use. Return false if processing
    // should not proceed.
    virtual bool rememberOriginalProperties() = 0;

    // Produce the definition. This must produce a definition.
    virtual CustomElementDefinition* build(const CustomElementDescriptor&) = 0;
};

} // namespace blink

#endif // CustomElementDefinitionBuilder_h
