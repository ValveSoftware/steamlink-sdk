/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef V8DOMWrapper_h
#define V8DOMWrapper_h

#include "bindings/v8/DOMDataStore.h"
#include <v8.h>
#include "wtf/PassRefPtr.h"
#include "wtf/RawPtr.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

struct WrapperTypeInfo;

    class V8DOMWrapper {
    public:
        static v8::Local<v8::Object> createWrapper(v8::Handle<v8::Object> creationContext, const WrapperTypeInfo*, void*, v8::Isolate*);

        template<typename V8T, typename T>
        static inline v8::Handle<v8::Object> associateObjectWithWrapper(PassRefPtr<T>, const WrapperTypeInfo*, v8::Handle<v8::Object>, v8::Isolate*, WrapperConfiguration::Lifetime);
        template<typename V8T, typename T>
        static inline v8::Handle<v8::Object> associateObjectWithWrapper(RawPtr<T> object, const WrapperTypeInfo* wrapperTypeInfo, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, WrapperConfiguration::Lifetime lifetime)
        {
            return associateObjectWithWrapper<V8T, T>(object.get(), wrapperTypeInfo, wrapper, isolate, lifetime);
        }
        template<typename V8T, typename T>
        static inline v8::Handle<v8::Object> associateObjectWithWrapper(T*, const WrapperTypeInfo*, v8::Handle<v8::Object>, v8::Isolate*, WrapperConfiguration::Lifetime);
        static inline void setNativeInfo(v8::Handle<v8::Object>, const WrapperTypeInfo*, void*);
        static inline void setNativeInfoForHiddenWrapper(v8::Handle<v8::Object>, const WrapperTypeInfo*, void*);
        static inline void setNativeInfoWithPersistentHandle(v8::Handle<v8::Object>, const WrapperTypeInfo*, void*, PersistentNode*);
        static inline void clearNativeInfo(v8::Handle<v8::Object>, const WrapperTypeInfo*);

        static bool isDOMWrapper(v8::Handle<v8::Value>);
    };

    inline void V8DOMWrapper::setNativeInfo(v8::Handle<v8::Object> wrapper, const WrapperTypeInfo* wrapperTypeInfo, void* object)
    {
        ASSERT(wrapper->InternalFieldCount() >= 2);
        ASSERT(object);
        ASSERT(wrapperTypeInfo);
#if ENABLE(OILPAN)
        ASSERT(wrapperTypeInfo->gcType == RefCountedObject);
#else
        ASSERT(wrapperTypeInfo->gcType == RefCountedObject || wrapperTypeInfo->gcType == WillBeGarbageCollectedObject);
#endif
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperObjectIndex, object);
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperTypeIndex, const_cast<WrapperTypeInfo*>(wrapperTypeInfo));
    }

    inline void V8DOMWrapper::setNativeInfoForHiddenWrapper(v8::Handle<v8::Object> wrapper, const WrapperTypeInfo* wrapperTypeInfo, void* object)
    {
        // see V8WindowShell::installDOMWindow() comment for why this version is needed and safe.
        ASSERT(wrapper->InternalFieldCount() >= 2);
        ASSERT(object);
        ASSERT(wrapperTypeInfo);
#if ENABLE(OILPAN)
        ASSERT(wrapperTypeInfo->gcType != RefCountedObject);
#else
        ASSERT(wrapperTypeInfo->gcType == RefCountedObject || wrapperTypeInfo->gcType == WillBeGarbageCollectedObject);
#endif

        // Clear out the last internal field, which is assumed to contain a valid persistent pointer value.
        if (wrapperTypeInfo->gcType == GarbageCollectedObject) {
            wrapper->SetAlignedPointerInInternalField(wrapper->InternalFieldCount() - 1, 0);
        } else if (wrapperTypeInfo->gcType == WillBeGarbageCollectedObject) {
#if ENABLE(OILPAN)
            wrapper->SetAlignedPointerInInternalField(wrapper->InternalFieldCount() - 1, 0);
#endif
        }
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperObjectIndex, object);
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperTypeIndex, const_cast<WrapperTypeInfo*>(wrapperTypeInfo));
    }

    inline void V8DOMWrapper::setNativeInfoWithPersistentHandle(v8::Handle<v8::Object> wrapper, const WrapperTypeInfo* wrapperTypeInfo, void* object, PersistentNode* handle)
    {
        ASSERT(wrapper->InternalFieldCount() >= 3);
        ASSERT(object);
        ASSERT(wrapperTypeInfo);
        ASSERT(wrapperTypeInfo->gcType != RefCountedObject);
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperObjectIndex, object);
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperTypeIndex, const_cast<WrapperTypeInfo*>(wrapperTypeInfo));
        // Persistent handle is stored in the last internal field.
        wrapper->SetAlignedPointerInInternalField(wrapper->InternalFieldCount() - 1, handle);
    }

    inline void V8DOMWrapper::clearNativeInfo(v8::Handle<v8::Object> wrapper, const WrapperTypeInfo* wrapperTypeInfo)
    {
        ASSERT(wrapper->InternalFieldCount() >= 2);
        ASSERT(wrapperTypeInfo);
        // clearNativeInfo() is used only by NP objects, which are not garbage collected.
        ASSERT(wrapperTypeInfo->gcType == RefCountedObject);
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperTypeIndex, const_cast<WrapperTypeInfo*>(wrapperTypeInfo));
        wrapper->SetAlignedPointerInInternalField(v8DOMWrapperObjectIndex, 0);
    }

    template<typename V8T, typename T>
    inline v8::Handle<v8::Object> V8DOMWrapper::associateObjectWithWrapper(PassRefPtr<T> object, const WrapperTypeInfo* wrapperTypeInfo, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, WrapperConfiguration::Lifetime lifetime)
    {
        setNativeInfo(wrapper, wrapperTypeInfo, V8T::toInternalPointer(object.get()));
        ASSERT(isDOMWrapper(wrapper));
        WrapperConfiguration configuration = buildWrapperConfiguration(object.get(), lifetime);
        DOMDataStore::setWrapper<V8T>(object.leakRef(), wrapper, isolate, configuration);
        return wrapper;
    }

    template<typename V8T, typename T>
    inline v8::Handle<v8::Object> V8DOMWrapper::associateObjectWithWrapper(T* object, const WrapperTypeInfo* wrapperTypeInfo, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, WrapperConfiguration::Lifetime lifetime)
    {
        setNativeInfoWithPersistentHandle(wrapper, wrapperTypeInfo, V8T::toInternalPointer(object), new Persistent<T>(object));
        ASSERT(isDOMWrapper(wrapper));
        WrapperConfiguration configuration = buildWrapperConfiguration(object, lifetime);
        DOMDataStore::setWrapper<V8T>(object, wrapper, isolate, configuration);
        return wrapper;
    }

    class V8WrapperInstantiationScope {
    public:
        V8WrapperInstantiationScope(v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
            : m_didEnterContext(false)
            , m_context(isolate->GetCurrentContext())
        {
            // creationContext should not be empty. Because if we have an
            // empty creationContext, we will end up creating
            // a new object in the context currently entered. This is wrong.
            RELEASE_ASSERT(!creationContext.IsEmpty());
            v8::Handle<v8::Context> contextForWrapper = creationContext->CreationContext();
            // For performance, we enter the context only if the currently running context
            // is different from the context that we are about to enter.
            if (contextForWrapper == m_context)
                return;
            m_context = v8::Local<v8::Context>::New(isolate, contextForWrapper);
            m_didEnterContext = true;
            m_context->Enter();
        }

        ~V8WrapperInstantiationScope()
        {
            if (!m_didEnterContext)
                return;
            m_context->Exit();
        }

        v8::Handle<v8::Context> context() const { return m_context; }

    private:
        bool m_didEnterContext;
        v8::Handle<v8::Context> m_context;
    };

}

#endif // V8DOMWrapper_h
