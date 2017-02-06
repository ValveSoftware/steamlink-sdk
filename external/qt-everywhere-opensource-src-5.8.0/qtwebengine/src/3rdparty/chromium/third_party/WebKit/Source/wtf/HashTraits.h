/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef WTF_HashTraits_h
#define WTF_HashTraits_h

#include "wtf/Forward.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/StdLibExtras.h"
#include "wtf/TypeTraits.h"
#include <limits>
#include <memory>
#include <string.h> // For memset.
#include <type_traits>
#include <utility>

namespace WTF {

template <bool isInteger, typename T> struct GenericHashTraitsBase;
template <typename T> struct HashTraits;

enum ShouldWeakPointersBeMarkedStrongly {
    WeakPointersActStrong,
    WeakPointersActWeak
};

template <typename T> struct GenericHashTraitsBase<false, T> {
    // The emptyValueIsZero flag is used to optimize allocation of empty hash
    // tables with zeroed memory.
    static const bool emptyValueIsZero = false;

    // The hasIsEmptyValueFunction flag allows the hash table to automatically
    // generate code to check for the empty value when it can be done with the
    // equality operator, but allows custom functions for cases like String that
    // need them.
    static const bool hasIsEmptyValueFunction = false;

    // The starting table size. Can be overridden when we know beforehand that a
    // hash table will have at least N entries.
#if defined(MEMORY_SANITIZER_INITIAL_SIZE)
    static const unsigned minimumTableSize = 1;
#else
    static const unsigned minimumTableSize = 8;
#endif

    // When a hash table backing store is traced, its elements will be
    // traced if their class type has a trace method. However, weak-referenced
    // elements should not be traced then, but handled by the weak processing
    // phase that follows.
    template <typename U = void>
    struct IsTraceableInCollection {
        static const bool value = IsTraceable<T>::value && !IsWeak<T>::value;
    };

    // The NeedsToForbidGCOnMove flag is used to make the hash table move
    // operations safe when GC is enabled: if a move constructor invokes
    // an allocation triggering the GC then it should be invoked within GC
    // forbidden scope.
    template <typename U = void>
    struct NeedsToForbidGCOnMove {
        // TODO(yutak): Consider using of std:::is_trivially_move_constructible
        // when it is accessible.
        static const bool value = !std::is_pod<T>::value;
    };

    static const WeakHandlingFlag weakHandlingFlag = IsWeak<T>::value ? WeakHandlingInCollections : NoWeakHandlingInCollections;
};

// Default integer traits disallow both 0 and -1 as keys (max value instead of
// -1 for unsigned).
template <typename T> struct GenericHashTraitsBase<true, T> : GenericHashTraitsBase<false, T> {
    static const bool emptyValueIsZero = true;
    static void constructDeletedValue(T& slot, bool) { slot = static_cast<T>(-1); }
    static bool isDeletedValue(T value) { return value == static_cast<T>(-1); }
};

template <typename T> struct GenericHashTraits : GenericHashTraitsBase<std::is_integral<T>::value, T> {
    typedef T TraitType;
    typedef T EmptyValueType;

    static T emptyValue() { return T(); }

    // Type for functions that do not take ownership, such as contains.
    typedef const T& PeekInType;
    typedef T* IteratorGetType;
    typedef const T* IteratorConstGetType;
    typedef T& IteratorReferenceType;
    typedef const T& IteratorConstReferenceType;
    static IteratorReferenceType getToReferenceConversion(IteratorGetType x) { return *x; }
    static IteratorConstReferenceType getToReferenceConstConversion(IteratorConstGetType x) { return *x; }

    template <typename IncomingValueType>
    static void store(IncomingValueType&& value, T& storage) { storage = std::forward<IncomingValueType>(value); }

    // Type for return value of functions that do not transfer ownership, such
    // as get.
    // FIXME: We could change this type to const T& for better performance if we
    // figured out a way to handle the return value from emptyValue, which is a
    // temporary.
    typedef T PeekOutType;
    static const T& peek(const T& value) { return value; }
};

template <typename T> struct HashTraits : GenericHashTraits<T> { };

template <typename T> struct FloatHashTraits : GenericHashTraits<T> {
    static T emptyValue() { return std::numeric_limits<T>::infinity(); }
    static void constructDeletedValue(T& slot, bool) { slot = -std::numeric_limits<T>::infinity(); }
    static bool isDeletedValue(T value) { return value == -std::numeric_limits<T>::infinity(); }
};

template <> struct HashTraits<float> : FloatHashTraits<float> { };
template <> struct HashTraits<double> : FloatHashTraits<double> { };

// Default unsigned traits disallow both 0 and max as keys -- use these traits
// to allow zero and disallow max - 1.
template <typename T> struct UnsignedWithZeroKeyHashTraits : GenericHashTraits<T> {
    static const bool emptyValueIsZero = false;
    static T emptyValue() { return std::numeric_limits<T>::max(); }
    static void constructDeletedValue(T& slot, bool) { slot = std::numeric_limits<T>::max() - 1; }
    static bool isDeletedValue(T value) { return value == std::numeric_limits<T>::max() - 1; }
};

template <typename P> struct HashTraits<P*> : GenericHashTraits<P*> {
    static const bool emptyValueIsZero = true;
    static void constructDeletedValue(P*& slot, bool) { slot = reinterpret_cast<P*>(-1); }
    static bool isDeletedValue(P* value) { return value == reinterpret_cast<P*>(-1); }
};

template <typename T> struct SimpleClassHashTraits : GenericHashTraits<T> {
    static const bool emptyValueIsZero = true;
    template <typename U = void>
    struct NeedsToForbidGCOnMove {
        static const bool value = false;
    };
    static void constructDeletedValue(T& slot, bool) { new (NotNull, &slot) T(HashTableDeletedValue); }
    static bool isDeletedValue(const T& value) { return value.isHashTableDeletedValue(); }
};

template <typename P> struct HashTraits<RefPtr<P>> : SimpleClassHashTraits<RefPtr<P>> {
    typedef std::nullptr_t EmptyValueType;
    static EmptyValueType emptyValue() { return nullptr; }

    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const RefPtr<P>& value) { return !value; }

    typedef RefPtrValuePeeker<P> PeekInType;
    typedef RefPtr<P>* IteratorGetType;
    typedef const RefPtr<P>* IteratorConstGetType;
    typedef RefPtr<P>& IteratorReferenceType;
    typedef const RefPtr<P>& IteratorConstReferenceType;
    static IteratorReferenceType getToReferenceConversion(IteratorGetType x) { return *x; }
    static IteratorConstReferenceType getToReferenceConstConversion(IteratorConstGetType x) { return *x; }

    static void store(PassRefPtr<P> value, RefPtr<P>& storage) { storage = value; }

    typedef P* PeekOutType;
    static PeekOutType peek(const RefPtr<P>& value) { return value.get(); }
    static PeekOutType peek(std::nullptr_t) { return 0; }
};

template <typename T>
struct HashTraits<std::unique_ptr<T>> : SimpleClassHashTraits<std::unique_ptr<T>> {
    using EmptyValueType = std::nullptr_t;
    static EmptyValueType emptyValue() { return nullptr; }

    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const std::unique_ptr<T>& value) { return !value; }

    using PeekInType = T*;

    static void store(std::unique_ptr<T>&& value, std::unique_ptr<T>& storage) { storage = std::move(value); }

    using PeekOutType = T*;
    static PeekOutType peek(const std::unique_ptr<T>& value) { return value.get(); }
    static PeekOutType peek(std::nullptr_t) { return nullptr; }

    static void constructDeletedValue(std::unique_ptr<T>& slot, bool)
    {
        // Dirty trick: implant an invalid pointer to unique_ptr. Destructor isn't called for deleted buckets,
        // so this is okay.
        new (NotNull, &slot) std::unique_ptr<T>(reinterpret_cast<T*>(1u));
    }
    static bool isDeletedValue(const std::unique_ptr<T>& value) { return value.get() == reinterpret_cast<T*>(1u); }
};

template <> struct HashTraits<String> : SimpleClassHashTraits<String> {
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const String&);
};

// This struct template is an implementation detail of the
// isHashTraitsEmptyValue function, which selects either the emptyValue function
// or the isEmptyValue function to check for empty values.
template <typename Traits, bool hasEmptyValueFunction> struct HashTraitsEmptyValueChecker;
template <typename Traits> struct HashTraitsEmptyValueChecker<Traits, true> {
    template <typename T> static bool isEmptyValue(const T& value) { return Traits::isEmptyValue(value); }
};
template <typename Traits> struct HashTraitsEmptyValueChecker<Traits, false> {
    template <typename T> static bool isEmptyValue(const T& value) { return value == Traits::emptyValue(); }
};
template <typename Traits, typename T> inline bool isHashTraitsEmptyValue(const T& value)
{
    return HashTraitsEmptyValueChecker<Traits, Traits::hasIsEmptyValueFunction>::isEmptyValue(value);
}

template <typename FirstTraitsArg, typename SecondTraitsArg>
struct PairHashTraits : GenericHashTraits<std::pair<typename FirstTraitsArg::TraitType, typename SecondTraitsArg::TraitType>> {
    typedef FirstTraitsArg FirstTraits;
    typedef SecondTraitsArg SecondTraits;
    typedef std::pair<typename FirstTraits::TraitType, typename SecondTraits::TraitType> TraitType;
    typedef std::pair<typename FirstTraits::EmptyValueType, typename SecondTraits::EmptyValueType> EmptyValueType;

    static const bool emptyValueIsZero = FirstTraits::emptyValueIsZero && SecondTraits::emptyValueIsZero;
    static EmptyValueType emptyValue() { return std::make_pair(FirstTraits::emptyValue(), SecondTraits::emptyValue()); }

    static const bool hasIsEmptyValueFunction = FirstTraits::hasIsEmptyValueFunction || SecondTraits::hasIsEmptyValueFunction;
    static bool isEmptyValue(const TraitType& value) { return isHashTraitsEmptyValue<FirstTraits>(value.first) && isHashTraitsEmptyValue<SecondTraits>(value.second); }

    static const unsigned minimumTableSize = FirstTraits::minimumTableSize;

    static void constructDeletedValue(TraitType& slot, bool zeroValue)
    {
        FirstTraits::constructDeletedValue(slot.first, zeroValue);
        // For GC collections the memory for the backing is zeroed when it is
        // allocated, and the constructors may take advantage of that,
        // especially if a GC occurs during insertion of an entry into the
        // table. This slot is being marked deleted, but If the slot is reused
        // at a later point, the same assumptions around memory zeroing must
        // hold as they did at the initial allocation.  Therefore we zero the
        // value part of the slot here for GC collections.
        if (zeroValue)
            memset(reinterpret_cast<void*>(&slot.second), 0, sizeof(slot.second));
    }
    static bool isDeletedValue(const TraitType& value) { return FirstTraits::isDeletedValue(value.first); }
};

template <typename First, typename Second>
struct HashTraits<std::pair<First, Second>> : public PairHashTraits<HashTraits<First>, HashTraits<Second>> { };

template <typename KeyTypeArg, typename ValueTypeArg>
struct KeyValuePair {
    typedef KeyTypeArg KeyType;

    template <typename IncomingKeyType, typename IncomingValueType>
    KeyValuePair(IncomingKeyType&& key, IncomingValueType&& value)
        : key(std::forward<IncomingKeyType>(key))
        , value(std::forward<IncomingValueType>(value))
    {
    }

    template <typename OtherKeyType, typename OtherValueType>
    KeyValuePair(KeyValuePair<OtherKeyType, OtherValueType>&& other)
        : key(std::move(other.key))
        , value(std::move(other.value))
    {
    }

    KeyTypeArg key;
    ValueTypeArg value;
};

template <typename KeyTraitsArg, typename ValueTraitsArg>
struct KeyValuePairHashTraits : GenericHashTraits<KeyValuePair<typename KeyTraitsArg::TraitType, typename ValueTraitsArg::TraitType>> {
    typedef KeyTraitsArg KeyTraits;
    typedef ValueTraitsArg ValueTraits;
    typedef KeyValuePair<typename KeyTraits::TraitType, typename ValueTraits::TraitType> TraitType;
    typedef KeyValuePair<typename KeyTraits::EmptyValueType, typename ValueTraits::EmptyValueType> EmptyValueType;

    static const bool emptyValueIsZero = KeyTraits::emptyValueIsZero && ValueTraits::emptyValueIsZero;
    static EmptyValueType emptyValue() { return KeyValuePair<typename KeyTraits::EmptyValueType, typename ValueTraits::EmptyValueType>(KeyTraits::emptyValue(), ValueTraits::emptyValue()); }

    template <typename U = void>
    struct IsTraceableInCollection {
        static const bool value = IsTraceableInCollectionTrait<KeyTraits>::value || IsTraceableInCollectionTrait<ValueTraits>::value;
    };

    template <typename U = void>
    struct NeedsToForbidGCOnMove {
        static const bool value = KeyTraits::template NeedsToForbidGCOnMove<>::value || ValueTraits::template NeedsToForbidGCOnMove<>::value;
    };

    static const WeakHandlingFlag weakHandlingFlag = (KeyTraits::weakHandlingFlag == WeakHandlingInCollections || ValueTraits::weakHandlingFlag == WeakHandlingInCollections) ? WeakHandlingInCollections : NoWeakHandlingInCollections;

    static const unsigned minimumTableSize = KeyTraits::minimumTableSize;

    static void constructDeletedValue(TraitType& slot, bool zeroValue)
    {
        KeyTraits::constructDeletedValue(slot.key, zeroValue);
        // See similar code in this file for why we need to do this.
        if (zeroValue)
            memset(reinterpret_cast<void*>(&slot.value), 0, sizeof(slot.value));
    }
    static bool isDeletedValue(const TraitType& value) { return KeyTraits::isDeletedValue(value.key); }
};

template <typename Key, typename Value>
struct HashTraits<KeyValuePair<Key, Value>> : public KeyValuePairHashTraits<HashTraits<Key>, HashTraits<Value>> { };

template <typename T>
struct NullableHashTraits : public HashTraits<T> {
    static const bool emptyValueIsZero = false;
    static T emptyValue() { return reinterpret_cast<T>(1); }
};

// This is for tracing inside collections that have special support for weak
// pointers. The trait has a trace method which returns true if there are weak
// pointers to things that have not (yet) been marked live. Returning true
// indicates that the entry in the collection may yet be removed by weak
// handling. Default implementation for non-weak types is to use the regular
// non-weak TraceTrait. Default implementation for types with weakness is to
// call traceInCollection on the type's trait.
template <WeakHandlingFlag weakHandlingFlag, ShouldWeakPointersBeMarkedStrongly strongify, typename T, typename Traits>
struct TraceInCollectionTrait;

} // namespace WTF

using WTF::HashTraits;
using WTF::PairHashTraits;
using WTF::NullableHashTraits;
using WTF::SimpleClassHashTraits;

#endif // WTF_HashTraits_h
