/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef WrapperTypeInfo_h
#define WrapperTypeInfo_h

#include "gin/public/wrapper_info.h"
#include "platform/heap/Handle.h"
#include "wtf/Assertions.h"
#include <v8.h>

namespace WebCore {

    class ActiveDOMObject;
    class EventTarget;
    class Node;

    static const int v8DOMWrapperTypeIndex = static_cast<int>(gin::kWrapperInfoIndex);
    static const int v8DOMWrapperObjectIndex = static_cast<int>(gin::kEncodedValueIndex);
    static const int v8DefaultWrapperInternalFieldCount = static_cast<int>(gin::kNumberOfInternalFields);
    static const int v8PrototypeTypeIndex = 0;
    static const int v8PrototypeInternalFieldcount = 1;

    static const uint16_t v8DOMNodeClassId = 1;
    static const uint16_t v8DOMObjectClassId = 2;

    typedef v8::Handle<v8::FunctionTemplate> (*DomTemplateFunction)(v8::Isolate*);
    typedef void (*DerefObjectFunction)(void*);
    typedef ActiveDOMObject* (*ToActiveDOMObjectFunction)(v8::Handle<v8::Object>);
    typedef EventTarget* (*ToEventTargetFunction)(v8::Handle<v8::Object>);
    typedef void (*ResolveWrapperReachabilityFunction)(void*, const v8::Persistent<v8::Object>&, v8::Isolate*);
    typedef void (*InstallPerContextEnabledPrototypePropertiesFunction)(v8::Handle<v8::Object>, v8::Isolate*);

    enum WrapperTypePrototype {
        WrapperTypeObjectPrototype,
        WrapperTypeExceptionPrototype
    };

    enum GCType {
        GarbageCollectedObject,
        WillBeGarbageCollectedObject,
        RefCountedObject,
    };

    inline void setObjectGroup(void* object, const v8::Persistent<v8::Object>& wrapper, v8::Isolate* isolate)
    {
        isolate->SetObjectGroupId(wrapper, v8::UniqueId(reinterpret_cast<intptr_t>(object)));
    }

    // This struct provides a way to store a bunch of information that is helpful when unwrapping
    // v8 objects. Each v8 bindings class has exactly one static WrapperTypeInfo member, so
    // comparing pointers is a safe way to determine if types match.
    struct WrapperTypeInfo {

        static const WrapperTypeInfo* unwrap(v8::Handle<v8::Value> typeInfoWrapper)
        {
            return reinterpret_cast<const WrapperTypeInfo*>(v8::External::Cast(*typeInfoWrapper)->Value());
        }


        bool equals(const WrapperTypeInfo* that) const
        {
            return this == that;
        }

        bool isSubclass(const WrapperTypeInfo* that) const
        {
            for (const WrapperTypeInfo* current = this; current; current = current->parentClass) {
                if (current == that)
                    return true;
            }

            return false;
        }

        v8::Handle<v8::FunctionTemplate> domTemplate(v8::Isolate* isolate) const
        {
            return domTemplateFunction(isolate);
        }

        void installPerContextEnabledMethods(v8::Handle<v8::Object> prototypeTemplate, v8::Isolate* isolate) const
        {
            if (installPerContextEnabledMethodsFunction)
                installPerContextEnabledMethodsFunction(prototypeTemplate, isolate);
        }

        ActiveDOMObject* toActiveDOMObject(v8::Handle<v8::Object> object) const
        {
            if (!toActiveDOMObjectFunction)
                return 0;
            return toActiveDOMObjectFunction(object);
        }

        EventTarget* toEventTarget(v8::Handle<v8::Object> object) const
        {
            if (!toEventTargetFunction)
                return 0;
            return toEventTargetFunction(object);
        }

        void visitDOMWrapper(void* object, const v8::Persistent<v8::Object>& wrapper, v8::Isolate* isolate) const
        {
            if (!visitDOMWrapperFunction)
                setObjectGroup(object, wrapper, isolate);
            else
                visitDOMWrapperFunction(object, wrapper, isolate);
        }

        // This field must be the first member of the struct WrapperTypeInfo. This is also checked by a COMPILE_ASSERT() below.
        const gin::GinEmbedder ginEmbedder;

        const DomTemplateFunction domTemplateFunction;
        const DerefObjectFunction derefObjectFunction;
        const ToActiveDOMObjectFunction toActiveDOMObjectFunction;
        const ToEventTargetFunction toEventTargetFunction;
        const ResolveWrapperReachabilityFunction visitDOMWrapperFunction;
        const InstallPerContextEnabledPrototypePropertiesFunction installPerContextEnabledMethodsFunction;
        const WrapperTypeInfo* parentClass;
        const WrapperTypePrototype wrapperTypePrototype;
        const GCType gcType;
    };


    COMPILE_ASSERT(offsetof(struct WrapperTypeInfo, ginEmbedder) == offsetof(struct gin::WrapperInfo, embedder), wrapper_type_info_compatible_to_gin);

    template<typename T, int offset>
    inline T* getInternalField(const v8::Persistent<v8::Object>& persistent)
    {
        // This would be unsafe, but InternalFieldCount and GetAlignedPointerFromInternalField are guaranteed not to allocate
        const v8::Handle<v8::Object>& object = reinterpret_cast<const v8::Handle<v8::Object>&>(persistent);
        ASSERT(offset < object->InternalFieldCount());
        return static_cast<T*>(object->GetAlignedPointerFromInternalField(offset));
    }

    template<typename T, int offset>
    inline T* getInternalField(v8::Handle<v8::Object> wrapper)
    {
        ASSERT(offset < wrapper->InternalFieldCount());
        return static_cast<T*>(wrapper->GetAlignedPointerFromInternalField(offset));
    }

    inline void* toNative(const v8::Persistent<v8::Object>& wrapper)
    {
        return getInternalField<void, v8DOMWrapperObjectIndex>(wrapper);
    }

    inline void* toNative(v8::Handle<v8::Object> wrapper)
    {
        return getInternalField<void, v8DOMWrapperObjectIndex>(wrapper);
    }

    inline const WrapperTypeInfo* toWrapperTypeInfo(const v8::Persistent<v8::Object>& wrapper)
    {
        return getInternalField<WrapperTypeInfo, v8DOMWrapperTypeIndex>(wrapper);
    }

    inline const WrapperTypeInfo* toWrapperTypeInfo(v8::Handle<v8::Object> wrapper)
    {
        return getInternalField<WrapperTypeInfo, v8DOMWrapperTypeIndex>(wrapper);
    }

    inline const PersistentNode* toPersistentHandle(const v8::Handle<v8::Object>& wrapper)
    {
        // Persistent handle is stored in the last internal field.
        return static_cast<PersistentNode*>(wrapper->GetAlignedPointerFromInternalField(wrapper->InternalFieldCount() - 1));
    }

    inline void releaseObject(v8::Handle<v8::Object> wrapper)
    {
        const WrapperTypeInfo* typeInfo = toWrapperTypeInfo(wrapper);
        if (typeInfo->gcType == GarbageCollectedObject) {
            const PersistentNode* handle = toPersistentHandle(wrapper);
            // This will be null iff a wrapper for a hidden wrapper object,
            // see V8DOMWrapper::setNativeInfoForHiddenWrapper().
            delete handle;
        } else if (typeInfo->gcType == WillBeGarbageCollectedObject) {
#if ENABLE(OILPAN)
            const PersistentNode* handle = toPersistentHandle(wrapper);
            // This will be null iff a wrapper for a hidden wrapper object,
            // see V8DOMWrapper::setNativeInfoForHiddenWrapper().
            delete handle;
#else
            ASSERT(typeInfo->derefObjectFunction);
            typeInfo->derefObjectFunction(toNative(wrapper));
#endif
        } else {
            ASSERT(typeInfo->derefObjectFunction);
            typeInfo->derefObjectFunction(toNative(wrapper));
        }
    }

    struct WrapperConfiguration {

        enum Lifetime {
            Dependent, Independent
        };

        void configureWrapper(v8::PersistentBase<v8::Object>* wrapper) const
        {
            wrapper->SetWrapperClassId(classId);
            if (lifetime == Independent)
                wrapper->MarkIndependent();
        }

        const uint16_t classId;
        const Lifetime lifetime;
    };

    inline WrapperConfiguration buildWrapperConfiguration(void*, WrapperConfiguration::Lifetime lifetime)
    {
        WrapperConfiguration configuration = {v8DOMObjectClassId, lifetime};
        return configuration;
    }

    inline WrapperConfiguration buildWrapperConfiguration(Node*, WrapperConfiguration::Lifetime lifetime)
    {
        WrapperConfiguration configuration = {v8DOMNodeClassId, lifetime};
        return configuration;
    }
}

#endif // WrapperTypeInfo_h
