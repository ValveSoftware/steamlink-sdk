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

#ifndef ScriptWrappable_h
#define ScriptWrappable_h

#include "bindings/v8/WrapperTypeInfo.h"
#include "platform/heap/Handle.h"
#include <v8.h>

// Helper to call webCoreInitializeScriptWrappableForInterface in the global namespace.
template <class C> inline void initializeScriptWrappableHelper(C* object)
{
    void webCoreInitializeScriptWrappableForInterface(C*);
    webCoreInitializeScriptWrappableForInterface(object);
}

namespace WebCore {

/**
 * ScriptWrappable wraps a V8 object and its WrapperTypeInfo.
 *
 * ScriptWrappable acts much like a v8::Persistent<> in that it keeps a
 * V8 object alive. Under the hood, however, it keeps either a TypeInfo
 * object or an actual v8 persistent (or is empty).
 *
 * The physical state space of ScriptWrappable is:
 * - uintptr_t m_wrapperOrTypeInfo;
 *   - if 0: the ScriptWrappable is uninitialized/empty.
 *   - if even: a pointer to WebCore::TypeInfo
 *   - if odd: a pointer to v8::Persistent<v8::Object> + 1.
 *
 * In other words, one integer represents one of two object pointers,
 * depending on its least signficiant bit, plus an uninitialized state.
 * This class is meant to mask the logistics behind this.
 *
 * typeInfo() and newLocalWrapper will return appropriate values (possibly
 * 0/empty) in all physical states.
 *
 *  The state transitions are:
 *  - new: an empty and invalid ScriptWrappable.
 *  - init (to be called by all subclasses in their constructor):
 *        needs to call setTypeInfo
 *  - setTypeInfo: install a WrapperTypeInfo
 *  - setWrapper: install a v8::Persistent (or empty)
 *  - disposeWrapper (via setWeakCallback, triggered by V8 garbage collecter):
 *        remove v8::Persistent and install a TypeInfo of the previous value.
 */
class ScriptWrappable {
public:
    ScriptWrappable() : m_wrapperOrTypeInfo(0) { }

    // Wrappables need to be initialized with their most derrived type for which
    // bindings exist, in much the same way that certain other types need to be
    // adopted and so forth. The overloaded initializeScriptWrappableForInterface()
    // functions are implemented by the generated V8 bindings code. Declaring the
    // extern function in the template avoids making a centralized header of all
    // the bindings in the universe. C++11's extern template feature may provide
    // a cleaner solution someday.
    template <class C> static void init(C* object)
    {
        initializeScriptWrappableHelper(object);
    }

    void setWrapper(v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperConfiguration& configuration)
    {
        ASSERT(!containsWrapper());
        if (!*wrapper) {
            m_wrapperOrTypeInfo = 0;
            return;
        }
        v8::Persistent<v8::Object> persistent(isolate, wrapper);
        configuration.configureWrapper(&persistent);
        persistent.SetWeak(this, &setWeakCallback);
        m_wrapperOrTypeInfo = reinterpret_cast<uintptr_t>(persistent.ClearAndLeak()) | 1;
        ASSERT(containsWrapper());
    }

    v8::Local<v8::Object> newLocalWrapper(v8::Isolate* isolate) const
    {
        v8::Persistent<v8::Object> persistent;
        getPersistent(&persistent);
        return v8::Local<v8::Object>::New(isolate, persistent);
    }

    const WrapperTypeInfo* typeInfo()
    {
        if (containsTypeInfo())
            return reinterpret_cast<const WrapperTypeInfo*>(m_wrapperOrTypeInfo);

        if (containsWrapper()) {
            v8::Persistent<v8::Object> persistent;
            getPersistent(&persistent);
            return toWrapperTypeInfo(persistent);
        }

        return 0;
    }

    void setTypeInfo(const WrapperTypeInfo* typeInfo)
    {
        m_wrapperOrTypeInfo = reinterpret_cast<uintptr_t>(typeInfo);
        ASSERT(containsTypeInfo());
    }

    bool isEqualTo(const v8::Local<v8::Object>& other) const
    {
        v8::Persistent<v8::Object> persistent;
        getPersistent(&persistent);
        return persistent == other;
    }

    static bool wrapperCanBeStoredInObject(const void*) { return false; }
    static bool wrapperCanBeStoredInObject(const ScriptWrappable*) { return true; }

    static ScriptWrappable* fromObject(const void*)
    {
        ASSERT_NOT_REACHED();
        return 0;
    }

    static ScriptWrappable* fromObject(ScriptWrappable* object)
    {
        return object;
    }

    bool setReturnValue(v8::ReturnValue<v8::Value> returnValue)
    {
        v8::Persistent<v8::Object> persistent;
        getPersistent(&persistent);
        returnValue.Set(persistent);
        return containsWrapper();
    }

    void markAsDependentGroup(ScriptWrappable* groupRoot, v8::Isolate* isolate)
    {
        ASSERT(containsWrapper());
        ASSERT(groupRoot && groupRoot->containsWrapper());

        v8::UniqueId groupId(groupRoot->m_wrapperOrTypeInfo);
        v8::Persistent<v8::Object> wrapper;
        getPersistent(&wrapper);
        wrapper.MarkPartiallyDependent();
        isolate->SetObjectGroupId(v8::Persistent<v8::Value>::Cast(wrapper), groupId);
    }

    void setReference(const v8::Persistent<v8::Object>& parent, v8::Isolate* isolate)
    {
        v8::Persistent<v8::Object> persistent;
        getPersistent(&persistent);
        isolate->SetReference(parent, persistent);
    }

    template<typename V8T, typename T>
    static void assertWrapperSanity(v8::Local<v8::Object> object, T* objectAsT)
    {
        ASSERT(objectAsT);
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(object.IsEmpty()
            || object->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex) == V8T::toInternalPointer(objectAsT));
    }

    template<typename V8T, typename T>
    static void assertWrapperSanity(void* object, T* objectAsT)
    {
        ASSERT_NOT_REACHED();
    }

    template<typename V8T, typename T>
    static void assertWrapperSanity(ScriptWrappable* object, T* objectAsT)
    {
        ASSERT(object);
        ASSERT(objectAsT);
        v8::Object* value = object->getRawValue();
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(value == 0
            || value->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex) == V8T::toInternalPointer(objectAsT));
    }

    inline bool containsWrapper() const { return (m_wrapperOrTypeInfo & 1); }
    inline bool containsTypeInfo() const { return m_wrapperOrTypeInfo && !(m_wrapperOrTypeInfo & 1); }

protected:
    ~ScriptWrappable()
    {
        // We must not get deleted as long as we contain a wrapper. If this happens, we screwed up ref
        // counting somewhere. Crash here instead of crashing during a later gc cycle.
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!containsWrapper());
        ASSERT(m_wrapperOrTypeInfo); // Assert initialization via init() even if not subsequently wrapped.
        m_wrapperOrTypeInfo = 0; // Break UAF attempts to wrap.
    }

private:
    void getPersistent(v8::Persistent<v8::Object>* persistent) const
    {
        ASSERT(persistent);

        // Horrible and super unsafe: Cast the Persistent to an Object*, so
        // that we can inject the wrapped value. This only works because
        // we previously 'stole' the object pointer from a Persistent in
        // the setWrapper() method.
        *reinterpret_cast<v8::Object**>(persistent) = getRawValue();
    }

    inline v8::Object* getRawValue() const
    {
        v8::Object* object = containsWrapper() ? reinterpret_cast<v8::Object*>(m_wrapperOrTypeInfo & ~1) : 0;
        return object;
    }

    inline void disposeWrapper(v8::Local<v8::Object> wrapper)
    {
        ASSERT(containsWrapper());

        v8::Persistent<v8::Object> persistent;
        getPersistent(&persistent);

        ASSERT(wrapper == persistent);
        persistent.Reset();
        setTypeInfo(toWrapperTypeInfo(wrapper));
    }

    // If zero, then this contains nothing, otherwise:
    //   If the bottom bit it set, then this contains a pointer to a wrapper object in the remainging bits.
    //   If the bottom bit is clear, then this contains a pointer to the wrapper type info in the remaining bits.
    uintptr_t m_wrapperOrTypeInfo;

    static void setWeakCallback(const v8::WeakCallbackData<v8::Object, ScriptWrappable>& data)
    {
        v8::Persistent<v8::Object> persistent;
        data.GetParameter()->getPersistent(&persistent);
        ASSERT(persistent == data.GetValue());
        data.GetParameter()->disposeWrapper(data.GetValue());

        // FIXME: I noticed that 50%~ of minor GC cycle times can be consumed
        // inside data.GetParameter()->deref(), which causes Node destructions. We should
        // make Node destructions incremental.
        releaseObject(data.GetValue());
    }
};

} // namespace WebCore

#endif // ScriptWrappable_h
