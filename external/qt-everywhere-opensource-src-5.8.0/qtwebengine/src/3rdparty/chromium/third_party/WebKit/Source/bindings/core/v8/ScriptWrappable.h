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

#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/TypeTraits.h"
#include <v8.h>

namespace blink {

/**
 * ScriptWrappable wraps a V8 object and its WrapperTypeInfo.
 *
 * ScriptWrappable acts much like a v8::Persistent<> in that it keeps a
 * V8 object alive.
 *
 *  The state transitions are:
 *  - new: an empty ScriptWrappable.
 *  - setWrapper: install a v8::Persistent (or empty)
 *  - disposeWrapper (via setWeakCallback, triggered by V8 garbage collecter):
 *        remove v8::Persistent and become empty.
 */
class CORE_EXPORT ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(ScriptWrappable);
public:
    ScriptWrappable() { }

    template<typename T>
    T* toImpl()
    {
        // All ScriptWrappables are managed by the Blink GC heap; check that
        // |T| is a garbage collected type.
        static_assert(sizeof(T) && WTF::IsGarbageCollectedType<T>::value, "Classes implementing ScriptWrappable must be garbage collected.");

        // Check if T* is castable to ScriptWrappable*, which means T doesn't
        // have two or more ScriptWrappable as superclasses. If T has two
        // ScriptWrappable as superclasses, conversions from T* to
        // ScriptWrappable* are ambiguous.
#if !COMPILER(MSVC)
        // MSVC 2013 doesn't support static_assert + constexpr well.
        static_assert(!static_cast<ScriptWrappable*>(static_cast<T*>(nullptr)),
            "Class T must not have two or more ScriptWrappable as its superclasses.");
#endif
        return static_cast<T*>(this);
    }

    // Returns the WrapperTypeInfo of the instance.
    //
    // This method must be overridden by DEFINE_WRAPPERTYPEINFO macro.
    virtual const WrapperTypeInfo* wrapperTypeInfo() const = 0;

    // Creates and returns a new wrapper object.
    virtual v8::Local<v8::Object> wrap(v8::Isolate*, v8::Local<v8::Object> creationContext);

    // Associates the instance with the given |wrapper| if this instance is not
    // yet associated with any wrapper.  Returns the wrapper already associated
    // or |wrapper| if not yet associated.
    // The caller should always use the returned value rather than |wrapper|.
    virtual v8::Local<v8::Object> associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Local<v8::Object> wrapper) WARN_UNUSED_RETURN;

    // Associates this instance with the given |wrapper| if this instance is not
    // yet associated with any wrapper.  Returns true if the given wrapper is
    // associated with this instance, or false if this instance is already
    // associated with a wrapper.  In the latter case, |wrapper| will be updated
    // to the existing wrapper.
    bool setWrapper(v8::Isolate* isolate, const WrapperTypeInfo* wrapperTypeInfo, v8::Local<v8::Object>& wrapper) WARN_UNUSED_RETURN
    {
        ASSERT(!wrapper.IsEmpty());
        if (UNLIKELY(containsWrapper())) {
            wrapper = mainWorldWrapper(isolate);
            return false;
        }
        m_mainWorldWrapper.Reset(isolate, wrapper);
        wrapperTypeInfo->configureWrapper(&m_mainWorldWrapper);
        m_mainWorldWrapper.SetWeak();
        ASSERT(containsWrapper());
        return true;
    }

    bool isEqualTo(const v8::Local<v8::Object>& other) const
    {
        return m_mainWorldWrapper == other;
    }

    // Provides a way to convert Node* to ScriptWrappable* without including
    // "core/dom/Node.h".
    //
    // Example:
    //   void foo(const void*) { ... }       // [1]
    //   void foo(ScriptWrappable*) { ... }  // [2]
    //   class Node;
    //   Node* node;
    //   foo(node);  // This calls [1] because there is no definition of Node
    //               // and compilers do not know that Node is a subclass of
    //               // ScriptWrappable.
    //   foo(ScriptWrappable::fromNode(node));  // This calls [2] as expected.
    //
    // The definition of fromNode is placed in Node.h because we'd like to
    // inline calls to fromNode as much as possible.
    static ScriptWrappable* fromNode(Node*);

    bool setReturnValue(v8::ReturnValue<v8::Value> returnValue)
    {
        returnValue.Set(m_mainWorldWrapper);
        return containsWrapper();
    }

    void setReference(const v8::Persistent<v8::Object>& parent, v8::Isolate* isolate)
    {
        isolate->SetReference(parent, m_mainWorldWrapper);
    }

    bool containsWrapper() const { return !m_mainWorldWrapper.IsEmpty(); }

    /**
     *  Mark wrapper of this ScriptWrappable as alive in V8. Only marks
     *  wrapper in the main world. To mark wrappers in all worlds call
     *  ScriptWrappableVisitor::markWrapper(ScriptWrappable*, v8::Isolate*)
     */
    void markWrapper(const WrapperVisitor*) const;

    DECLARE_VIRTUAL_TRACE_WRAPPERS() {};

    // With Oilpan we don't need a ScriptWrappable destructor.
    //
    // 'RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!containsWrapper())' is not needed
    // because Oilpan is not using reference counting at all. If containsWrapper() is true,
    // it means that ScriptWrappable still has a wrapper. In this case, the destructor
    // must not be called since the wrapper has a persistent handle back to this ScriptWrappable object.
    // Assuming that Oilpan's GC is correct (If we cannot assume this, a lot of more things are
    // already broken), we must not hit the RELEASE_ASSERT.

private:
    // These classes are exceptionally allowed to use mainWorldWrapper().
    friend class DOMDataStore;
    friend class V8HiddenValue;
    friend class V8PrivateProperty;
    friend class WebGLRenderingContextBase;

    v8::Local<v8::Object> mainWorldWrapper(v8::Isolate* isolate) const
    {
        return v8::Local<v8::Object>::New(isolate, m_mainWorldWrapper);
    }

    v8::Persistent<v8::Object> m_mainWorldWrapper;
};

// Defines 'wrapperTypeInfo' virtual method which returns the WrapperTypeInfo of
// the instance. Also declares a static member of type WrapperTypeInfo, of which
// the definition is given by the IDL code generator.
//
// All the derived classes of ScriptWrappable, regardless of directly or
// indirectly, must write this macro in the class definition as long as the
// class has a corresponding .idl file.
#define DEFINE_WRAPPERTYPEINFO() \
public: \
    const WrapperTypeInfo* wrapperTypeInfo() const override \
    { \
        return &s_wrapperTypeInfo; \
    } \
private: \
    static const WrapperTypeInfo& s_wrapperTypeInfo

// Declares 'wrapperTypeInfo' method without definition.
//
// This macro is used for template classes. e.g. DOMTypedArray<>.
// To export such a template class X, we need to instantiate X with EXPORT_API,
// i.e. "extern template class EXPORT_API X;"
// However, once we instantiate X, we cannot specialize X after
// the instantiation. i.e. we will see "error: explicit specialization of ...
// after instantiation". So we cannot define X's s_wrapperTypeInfo in generated
// code by using specialization. Instead, we need to implement wrapperTypeInfo
// in X's cpp code, and instantiate X, i.e. "template class X;".
#define DECLARE_WRAPPERTYPEINFO() \
public: \
    const WrapperTypeInfo* wrapperTypeInfo() const override; \
private: \
    typedef void end_of_define_wrappertypeinfo_not_reached_t

} // namespace blink

#endif // ScriptWrappable_h
