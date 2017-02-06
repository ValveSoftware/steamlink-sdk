// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WrapperVisitor_h
#define WrapperVisitor_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"

namespace v8 {
class Value;
class Object;
template <class T> class PersistentBase;
}

namespace blink {

template<typename T> class TraceTrait;
template<typename T> class Member;
class ScriptWrappable;
template<typename T> class ScopedPersistent;

// TODO(hlopko): Find a way to remove special-casing using templates
#define WRAPPER_VISITOR_SPECIAL_CLASSES(V)                           \
    V(DocumentStyleSheetCollection);                                 \
    V(ElementRareData);                                              \
    V(ElementShadow);                                                \
    V(HTMLImportsController)                                         \
    V(MutationObserverRegistration);                                 \
    V(NodeIntersectionObserverData)                                  \
    V(NodeListsNodeData);                                            \
    V(NodeMutationObserverData);                                     \
    V(NodeRareData);                                                 \
    V(StyleEngine);                                                  \
    V(V8AbstractEventListener);                                      \

#define FORWARD_DECLARE_SPECIAL_CLASSES(className)                   \
    class className;

WRAPPER_VISITOR_SPECIAL_CLASSES(FORWARD_DECLARE_SPECIAL_CLASSES);

#undef FORWARD_DECLARE_SPECIAL_CLASSES

/**
 * Declares non-virtual traceWrappers method. Should be used on
 * non-ScriptWrappable classes which should participate in wrapper tracing (e.g.
 * NodeRareData):
 *
 *     class NodeRareData {
 *     public:
 *         DECLARE_TRACE_WRAPPERS();
 *     }
 */
#define DECLARE_TRACE_WRAPPERS()                                     \
    void traceWrappers(const WrapperVisitor* visitor) const

/**
 * Declares virtual traceWrappers method. It is used in ScriptWrappable, can be
 * used to override the method in the subclasses, and can be used by
 * non-ScriptWrappable classes which expect to be inherited.
 */
#define DECLARE_VIRTUAL_TRACE_WRAPPERS()                             \
    virtual DECLARE_TRACE_WRAPPERS()

/**
 * Provides definition of traceWrappers method. Custom code will usually call
 * visitor->traceWrappers with all objects which could contribute to the set of
 * reachable wrappers:
 *
 *     DEFINE_TRACE_WRAPPERS(NodeRareData)
 *     {
 *         visitor->traceWrappers(m_nodeLists);
 *         visitor->traceWrappers(m_mutationObserverData);
 *     }
 */
#define DEFINE_TRACE_WRAPPERS(T)                                     \
    void T::traceWrappers(const WrapperVisitor* visitor) const

#define DECLARE_TRACE_WRAPPERS_AFTER_DISPATCH()                      \
    void traceWrappersAfterDispatch(const WrapperVisitor*) const

#define DEFINE_TRACE_WRAPPERS_AFTER_DISPATCH(T)                      \
    void T::traceWrappersAfterDispatch(const WrapperVisitor* visitor) const

#define DEFINE_INLINE_TRACE_WRAPPERS() DECLARE_TRACE_WRAPPERS()
#define DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() DECLARE_VIRTUAL_TRACE_WRAPPERS()

// ###########################################################################
// TODO(hlopko): Get rid of virtual calls using CRTP
class PLATFORM_EXPORT WrapperVisitor {
    USING_FAST_MALLOC(WrapperVisitor);
public:
    template<typename T>
    void traceWrappers(const T* traceable) const
    {
        static_assert(sizeof(T), "T must be fully defined");

        if (!traceable)
            return;

        if (TraceTrait<T>::heapObjectHeader(traceable)->isWrapperHeaderMarked()) {
            return;
        }

        pushToMarkingDeque(
            TraceTrait<T>::markWrapper,
            TraceTrait<T>::heapObjectHeader,
            traceable);
    }

    template<typename T>
    void traceWrappers(const Member<T>& t) const
    {
        traceWrappers(t.get());
    }

    virtual void traceWrappers(const ScopedPersistent<v8::Value>* persistent) const = 0;
    virtual void traceWrappers(const ScopedPersistent<v8::Object>* persistent) const = 0;
    virtual void markWrapper(const v8::PersistentBase<v8::Object>* persistent) const = 0;

    virtual void dispatchTraceWrappers(const ScriptWrappable*) const = 0;
#define DECLARE_DISPATCH_TRACE_WRAPPERS(className)                   \
    virtual void dispatchTraceWrappers(const className*) const = 0;

    WRAPPER_VISITOR_SPECIAL_CLASSES(DECLARE_DISPATCH_TRACE_WRAPPERS);

#undef DECLARE_DISPATCH_TRACE_WRAPPERS
    virtual void dispatchTraceWrappers(const void*) const = 0;

    virtual bool markWrapperHeader(HeapObjectHeader*) const = 0;
    virtual void markWrappersInAllWorlds(const ScriptWrappable*) const = 0;
    virtual void markWrappersInAllWorlds(const void*) const = 0;
    virtual void pushToMarkingDeque(
        void (*traceWrappersCallback)(const WrapperVisitor*, const void*),
        HeapObjectHeader* (*heapObjectHeaderCallback)(const void*),
        const void*) const = 0;
};

} // namespace blink

#endif // WrapperVisitor_h
