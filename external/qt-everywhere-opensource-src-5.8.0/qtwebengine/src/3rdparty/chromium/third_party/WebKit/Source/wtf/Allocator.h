// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_Allocator_h
#define WTF_Allocator_h

#include "wtf/Assertions.h"
#include "wtf/StdLibExtras.h"
#include "wtf/allocator/Partitions.h"

namespace WTF {

// Classes that contain references to garbage-collected objects but aren't
// themselves garbaged allocated, have some extra macros available which
// allows their use to be restricted to cases where the garbage collector
// is able to discover their references. These macros will be useful for
// non-garbage-collected objects to avoid unintended allocations.
//
// STACK_ALLOCATED(): Use if the object is only stack allocated.
// Garbage-collected objects should be in Members but you do not need the
// trace method as they are on the stack.  (Down the line these might turn
// in to raw pointers, but for now Members indicate that we have thought
// about them and explicitly taken care of them.)
//
// DISALLOW_NEW(): Cannot be allocated with new operators but can be a
// part of object.  If it has Members you need a trace method and the containing
// object needs to call that trace method.
//
// DISALLOW_NEW_EXCEPT_PLACEMENT_NEW(): Allows only placement new operator.  This
// disallows general allocation of this object but allows to put the object as a
// value object in collections.  If these have Members you need to have a trace
// method. That trace method will be called automatically by the on-heap
// collections.
//
#define DISALLOW_NEW()                                          \
    private:                                                    \
        void* operator new(size_t) = delete;                    \
        void* operator new(size_t, NotNullTag, void*) = delete; \
        void* operator new(size_t, void*) = delete;             \
    public:

#define DISALLOW_NEW_EXCEPT_PLACEMENT_NEW()                                         \
    public:                                                                         \
        using IsAllowOnlyPlacementNew = int;                                        \
        void* operator new(size_t, NotNullTag, void* location) { return location; } \
        void* operator new(size_t, void* location) { return location; }             \
    private:                                                                        \
        void* operator new(size_t) = delete;                                        \
    public:

#define STATIC_ONLY(Type) \
    private:              \
        Type() = delete;  \
        Type(const Type&) = delete;                             \
        Type& operator=(const Type&) = delete;                  \
        void* operator new(size_t) = delete;                    \
        void* operator new(size_t, NotNullTag, void*) = delete; \
        void* operator new(size_t, void*) = delete;             \
    public:

#define IS_GARBAGE_COLLECTED_TYPE()                 \
    public:                                         \
        using IsGarbageCollectedTypeMarker = int;   \
    private:

#if COMPILER(CLANG)
#define STACK_ALLOCATED()                                       \
    private:                                                    \
        __attribute__((annotate("blink_stack_allocated")))      \
        void* operator new(size_t) = delete;                    \
        void* operator new(size_t, NotNullTag, void*) = delete; \
        void* operator new(size_t, void*) = delete;             \
    public:
#else
#define STACK_ALLOCATED() DISALLOW_NEW()
#endif

// Provides customizable overrides of fastMalloc/fastFree and operator new/delete
//
// Provided functionality:
//    Macro: USING_FAST_MALLOC
//
// Example usage:
//    class Widget {
//        USING_FAST_MALLOC(Widget)
//    ...
//    };
//
//    struct Data {
//        USING_FAST_MALLOC(Data)
//    public:
//    ...
//    };
//

#define USING_FAST_MALLOC_INTERNAL(type, typeName) \
public: \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new(size_t size) \
    { \
        return ::WTF::Partitions::fastMalloc(size, typeName); \
    } \
    \
    void operator delete(void* p) \
    { \
        ::WTF::Partitions::fastFree(p);         \
    } \
    \
    void* operator new[](size_t size) \
    { \
        return ::WTF::Partitions::fastMalloc(size, typeName); \
    } \
    \
    void operator delete[](void* p) \
    { \
        ::WTF::Partitions::fastFree(p);                   \
    } \
    void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
    static const char* classNameForAllocator() \
    { \
        return #type; \
    } \
private: \
typedef int __thisIsHereToForceASemicolonAfterThisMacro

// Both of these macros enable fast malloc and provide type info to the heap
// profiler. The regular macro does not provide type info in official builds,
// to avoid bloating the binary with type name strings. The |WITH_TYPE_NAME|
// variant provides type info unconditionally, so it should be used sparingly.
// Furthermore, the |WITH_TYPE_NAME| variant does not work if |type| is a
// template argument; |USING_FAST_MALLOC| does.
#define USING_FAST_MALLOC(type) USING_FAST_MALLOC_INTERNAL(type, WTF_HEAP_PROFILER_TYPE_NAME(type))
#define USING_FAST_MALLOC_WITH_TYPE_NAME(type) USING_FAST_MALLOC_INTERNAL(type, #type)

} // namespace WTF

#endif /* WTF_Allocator_h */
