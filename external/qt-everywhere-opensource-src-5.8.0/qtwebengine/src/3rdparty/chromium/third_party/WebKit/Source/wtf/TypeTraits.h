 /*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef TypeTraits_h
#define TypeTraits_h

#include <cstddef>
#include <type_traits>
#include <utility>

#include "wtf/Compiler.h"

namespace WTF {

// Returns a string that contains the type name of |T| as a substring.
template<typename T>
inline const char* getStringWithTypeName()
{
    return WTF_PRETTY_FUNCTION;
}

template <typename T> struct IsWeak {
    static const bool value = false;
};

enum WeakHandlingFlag {
    NoWeakHandlingInCollections,
    WeakHandlingInCollections
};

// Compilers behave differently on __has_trivial_assign(T) if T has a user-deleted copy assignment operator:
//
//     * MSVC returns false; but
//     * The others return true.
//
// To workaround that, here we have IsAssignable<T, From> class template, but unfortunately, MSVC 2013 cannot compile
// it due to the lack of expression SFINAE.
//
// Thus, IsAssignable is only defined on non-MSVC compilers.
#if !COMPILER(MSVC) || COMPILER(CLANG)
template <typename T, typename From>
class IsAssignable {
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename T2, typename From2, typename = decltype(std::declval<T2&>() = std::declval<From2>())>
    static YesType checkAssignability(int);
    template <typename T2, typename From2>
    static NoType checkAssignability(...);

public:
    static const bool value = sizeof(checkAssignability<T, From>(0)) == sizeof(YesType);
};

template <typename T>
struct IsCopyAssignable {
    static_assert(!std::is_reference<T>::value, "T must not be a reference.");
    static const bool value = IsAssignable<T, const T&>::value;
};

template <typename T>
struct IsMoveAssignable {
    static_assert(!std::is_reference<T>::value, "T must not be a reference.");
    static const bool value = IsAssignable<T, T&&>::value;
};
#endif // !COMPILER(MSVC) || COMPILER(CLANG)

template <typename T> struct IsTriviallyCopyAssignable {
#if COMPILER(MSVC) && !COMPILER(CLANG)
    static const bool value = __has_trivial_assign(T);
#else
    static const bool value = __has_trivial_assign(T) && IsCopyAssignable<T>::value;
#endif
};

template <typename T> struct IsTriviallyMoveAssignable {
    // TODO(yutak): This isn't really correct, because __has_trivial_assign appears to look only at copy assignment.
    // However, std::is_trivially_move_assignable isn't available at this moment, and there isn't a good way to
    // write that ourselves.
    //
    // Here we use IsTriviallyCopyAssignable as a conservative approximation: if T is trivially copy assignable,
    // T is trivially move assignable, too. This definition misses a case where T is trivially move-only assignable,
    // but such cases should be rare.
    static const bool value = IsTriviallyCopyAssignable<T>::value;
};

// Same as above, but for __has_trivial_constructor and __has_trivial_destructor. For IsTriviallyDefaultConstructible,
// we don't have to write IsDefaultConstructible ourselves since we can use std::is_constructible<T>. For
// IsTriviallyDestructible, though, we can't rely on std::is_destructible<T> right now.
#if !COMPILER(MSVC) || COMPILER(CLANG)
template <typename T>
class IsDestructible {
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename T2, typename = decltype(std::declval<T2>().~T2())>
    static YesType checkDestructibility(int);
    template <typename T2>
    static NoType checkDestructibility(...);

public:
    static const bool value = sizeof(checkDestructibility<T>(0)) == sizeof(YesType);
};
#endif

template <typename T> struct IsTriviallyDefaultConstructible {
#if COMPILER(MSVC) && !COMPILER(CLANG)
    static const bool value = __has_trivial_constructor(T);
#else
    static const bool value = __has_trivial_constructor(T) && std::is_constructible<T>::value;
#endif
};

template <typename T> struct IsTriviallyDestructible {
#if COMPILER(MSVC) && !COMPILER(CLANG)
    static const bool value = __has_trivial_destructor(T);
#else
    static const bool value = __has_trivial_destructor(T) && IsDestructible<T>::value;
#endif
};

template <typename T, typename U> struct IsSubclass {
private:
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    static YesType subclassCheck(U*);
    static NoType subclassCheck(...);
    static T* t;
public:
    static const bool value = sizeof(subclassCheck(t)) == sizeof(YesType);
};

template <typename T, template <typename... V> class U> struct IsSubclassOfTemplate {
private:
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename... W> static YesType subclassCheck(U<W...>*);
    static NoType subclassCheck(...);
    static T* t;
public:
    static const bool value = sizeof(subclassCheck(t)) == sizeof(YesType);
};

template <typename T, template <typename V, size_t W> class U>
struct IsSubclassOfTemplateTypenameSize {
private:
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename X, size_t Y> static YesType subclassCheck(U<X, Y>*);
    static NoType subclassCheck(...);
    static T* t;
public:
    static const bool value = sizeof(subclassCheck(t)) == sizeof(YesType);
};

template <typename T, template <typename V, size_t W, typename X> class U>
struct IsSubclassOfTemplateTypenameSizeTypename {
private:
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename Y, size_t Z, typename A> static YesType subclassCheck(U<Y, Z, A>*);
    static NoType subclassCheck(...);
    static T* t;
public:
    static const bool value = sizeof(subclassCheck(t)) == sizeof(YesType);
};

template <typename T, template <class V> class OuterTemplate>
struct RemoveTemplate {
    typedef T Type;
};

template <typename T, template <class V> class OuterTemplate>
struct RemoveTemplate<OuterTemplate<T>, OuterTemplate> {
    typedef T Type;
};

#if (COMPILER(MSVC) || !GCC_VERSION_AT_LEAST(4, 9, 0)) && !COMPILER(CLANG)
// FIXME: MSVC bug workaround. Remove once MSVC STL is fixed.
// FIXME: GCC before 4.9.0 seems to have the same issue.
// C++ 2011 Spec (ISO/IEC 14882:2011(E)) 20.9.6.2 Table 51 states that
// the template parameters shall be a complete type if they are different types.
// However, MSVC checks for type completeness even if they are the same type.
// Here, we use a template specialization for same type case to allow incomplete
// types.

template <typename T, typename U> struct IsBaseOf {
    static const bool value = std::is_base_of<T, U>::value;
};

template <typename T> struct IsBaseOf<T, T> {
    static const bool value = true;
};

#define EnsurePtrConvertibleArgDecl(From, To) \
    typename std::enable_if<WTF::IsBaseOf<To, From>::value>::type* = nullptr
#define EnsurePtrConvertibleArgDefn(From, To) \
    typename std::enable_if<WTF::IsBaseOf<To, From>::value>::type*
#else
#define EnsurePtrConvertibleArgDecl(From, To) \
    typename std::enable_if<std::is_base_of<To, From>::value>::type* = nullptr
#define EnsurePtrConvertibleArgDefn(From, To) \
    typename std::enable_if<std::is_base_of<To, From>::value>::type*
#endif

} // namespace WTF

namespace blink {

class Visitor;

} // namespace blink

namespace WTF {

template <typename T>
class IsTraceable {
    typedef char YesType;
    typedef struct NoType {
        char padding[8];
    } NoType;

    // Note that this also checks if a superclass of V has a trace method.
    template <typename V> static YesType checkHasTraceMethod(V* v, blink::Visitor* p = nullptr, typename std::enable_if<std::is_same<decltype(v->trace(p)), void>::value>::type* g = nullptr);
    template <typename V> static NoType checkHasTraceMethod(...);
public:
    // We add sizeof(T) to both sides here, because we want it to fail for
    // incomplete types. Otherwise it just assumes that incomplete types do not
    // have a trace method, which may not be true.
    static const bool value = sizeof(YesType) + sizeof(T) == sizeof(checkHasTraceMethod<T>(nullptr)) + sizeof(T);
};

// Convenience template wrapping the IsTraceableInCollection template in
// Collection Traits. It helps make the code more readable.
template <typename Traits>
class IsTraceableInCollectionTrait {
public:
    static const bool value = Traits::template IsTraceableInCollection<>::value;
};

template <typename T, typename U>
struct IsTraceable<std::pair<T, U>> {
    static const bool value = IsTraceable<T>::value || IsTraceable<U>::value;
};

// This is used to check that DISALLOW_NEW_EXCEPT_PLACEMENT_NEW objects are not
// stored in off-heap Vectors, HashTables etc.
template <typename T>
struct AllowsOnlyPlacementNew {
private:
    using YesType = char;
    struct NoType {
        char padding[8];
    };

    template <typename U> static YesType checkMarker(typename U::IsAllowOnlyPlacementNew*);
    template <typename U> static NoType checkMarker(...);
public:
    static const bool value = sizeof(checkMarker<T>(nullptr)) == sizeof(YesType);
};

template<typename T>
class IsGarbageCollectedType {
    typedef char YesType;
    typedef struct NoType {
        char padding[8];
    } NoType;

    static_assert(sizeof(T), "T must be fully defined");

    using NonConstType = typename std::remove_const<T>::type;
    template <typename U> static YesType checkGarbageCollectedType(typename U::IsGarbageCollectedTypeMarker*);
    template <typename U> static NoType checkGarbageCollectedType(...);

    // Separately check for GarbageCollectedMixin, which declares a different
    // marker typedef, to avoid resolution ambiguity for cases like
    // IsGarbageCollectedType<B> over:
    //
    //    class A : public GarbageCollected<A>, public GarbageCollectedMixin {
    //        USING_GARBAGE_COLLECTED_MIXIN(A);
    //        ...
    //    };
    //    class B : public A, public GarbageCollectedMixin { ... };
    //
    template <typename U> static YesType checkGarbageCollectedMixinType(typename U::IsGarbageCollectedMixinMarker*);
    template <typename U> static NoType checkGarbageCollectedMixinType(...);
public:
    static const bool value = (sizeof(YesType) == sizeof(checkGarbageCollectedType<NonConstType>(nullptr)))
        || (sizeof(YesType) == sizeof(checkGarbageCollectedMixinType<NonConstType>(nullptr)));
};

template<>
class IsGarbageCollectedType<void> {
public:
    static const bool value = false;
};

template<typename T>
class IsPersistentReferenceType {
    typedef char YesType;
    typedef struct NoType {
        char padding[8];
    } NoType;

    template <typename U> static YesType checkPersistentReferenceType(typename U::IsPersistentReferenceTypeMarker*);
    template <typename U> static NoType checkPersistentReferenceType(...);
public:
    static const bool value = (sizeof(YesType) == sizeof(checkPersistentReferenceType<T>(nullptr)));
};

template<typename T, bool = std::is_function<typename std::remove_const<typename std::remove_pointer<T>::type>::type>::value || std::is_void<typename std::remove_const<typename std::remove_pointer<T>::type>::type>::value>
class IsPointerToGarbageCollectedType {
public:
    static const bool value = false;
};

template<typename T>
class IsPointerToGarbageCollectedType<T*, false> {
public:
    static const bool value = IsGarbageCollectedType<T>::value;
};

} // namespace WTF

using WTF::IsGarbageCollectedType;

#endif // TypeTraits_h
