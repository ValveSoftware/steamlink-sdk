/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#ifndef WTF_HashSet_h
#define WTF_HashSet_h

#include "wtf/HashTable.h"
#include "wtf/allocator/PartitionAllocator.h"
#include <initializer_list>

namespace WTF {

struct IdentityExtractor;

// Note: empty or deleted values are not allowed, using them may lead to
// undefined behavior.  For pointer valuess this means that null pointers are
// not allowed unless you supply custom traits.
template <
    typename ValueArg,
    typename HashArg = typename DefaultHash<ValueArg>::Hash,
    typename TraitsArg = HashTraits<ValueArg>,
    typename Allocator = PartitionAllocator>
class HashSet {
    WTF_USE_ALLOCATOR(HashSet, Allocator);
private:
    typedef HashArg HashFunctions;
    typedef TraitsArg ValueTraits;
    typedef typename ValueTraits::PeekInType ValuePeekInType;

public:
    typedef typename ValueTraits::TraitType ValueType;

private:
    typedef HashTable<ValueType, ValueType, IdentityExtractor,
        HashFunctions, ValueTraits, ValueTraits, Allocator> HashTableType;

public:
    typedef HashTableConstIteratorAdapter<HashTableType, ValueTraits> iterator;
    typedef HashTableConstIteratorAdapter<HashTableType, ValueTraits> const_iterator;
    typedef typename HashTableType::AddResult AddResult;

    HashSet() = default;
    HashSet(const HashSet&) = default;
    HashSet& operator=(const HashSet&) = default;
    HashSet(HashSet&&) = default;
    HashSet& operator=(HashSet&&) = default;

    HashSet(std::initializer_list<ValueType> elements);
    HashSet& operator=(std::initializer_list<ValueType> elements);

    void swap(HashSet& ref)
    {
        m_impl.swap(ref.m_impl);
    }

    unsigned size() const;
    unsigned capacity() const;
    bool isEmpty() const;

    void reserveCapacityForSize(unsigned size)
    {
        m_impl.reserveCapacityForSize(size);
    }

    iterator begin() const;
    iterator end() const;

    iterator find(ValuePeekInType) const;
    bool contains(ValuePeekInType) const;

    // An alternate version of find() that finds the object by hashing and
    // comparing with some other type, to avoid the cost of type
    // conversion. HashTranslator must have the following function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    template <typename HashTranslator, typename T> iterator find(const T&) const;
    template <typename HashTranslator, typename T> bool contains(const T&) const;

    // The return value is a pair of an iterator to the new value's location,
    // and a bool that is true if an new entry was added.
    template <typename IncomingValueType>
    AddResult add(IncomingValueType&&);

    // An alternate version of add() that finds the object by hashing and
    // comparing with some other type, to avoid the cost of type conversion if
    // the object is already in the table. HashTranslator must have the
    // following function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    //   static translate(ValueType&, T&&, unsigned hashCode);
    template <typename HashTranslator, typename T> AddResult addWithTranslator(T&&);

    void remove(ValuePeekInType);
    void remove(iterator);
    void clear();
    template <typename Collection>
    void removeAll(const Collection& toBeRemoved) { WTF::removeAll(*this, toBeRemoved); }

    ValueType take(iterator);
    ValueType take(ValuePeekInType);
    ValueType takeAny();

    template <typename VisitorDispatcher>
    void trace(VisitorDispatcher visitor) { m_impl.trace(visitor); }

private:
    HashTableType m_impl;
};

struct IdentityExtractor {
    STATIC_ONLY(IdentityExtractor);
    template <typename T>
    static const T& extract(const T& t) { return t; }
};

template <typename Translator>
struct HashSetTranslatorAdapter {
    STATIC_ONLY(HashSetTranslatorAdapter);
    template <typename T> static unsigned hash(const T& key) { return Translator::hash(key); }
    template <typename T, typename U> static bool equal(const T& a, const U& b) { return Translator::equal(a, b); }
    template <typename T, typename U, typename V> static void translate(T& location, U&& key, const V&, unsigned hashCode)
    {
        Translator::translate(location, std::forward<U>(key), hashCode);
    }
};

template <typename Value, typename HashFunctions, typename Traits, typename Allocator>
HashSet<Value, HashFunctions, Traits, Allocator>::HashSet(std::initializer_list<ValueType> elements)
{
    if (elements.size())
        m_impl.reserveCapacityForSize(elements.size());
    for (const ValueType& element : elements)
        add(element);
}

template <typename Value, typename HashFunctions, typename Traits, typename Allocator>
auto HashSet<Value, HashFunctions, Traits, Allocator>::operator=(std::initializer_list<ValueType> elements) -> HashSet&
{
    *this = HashSet(std::move(elements));
    return *this;
}

template <typename T, typename U, typename V, typename W>
inline unsigned HashSet<T, U, V, W>::size() const
{
    return m_impl.size();
}

template <typename T, typename U, typename V, typename W>
inline unsigned HashSet<T, U, V, W>::capacity() const
{
    return m_impl.capacity();
}

template <typename T, typename U, typename V, typename W>
inline bool HashSet<T, U, V, W>::isEmpty() const
{
    return m_impl.isEmpty();
}

template <typename T, typename U, typename V, typename W>
inline typename HashSet<T, U, V, W>::iterator HashSet<T, U, V, W>::begin() const
{
    return m_impl.begin();
}

template <typename T, typename U, typename V, typename W>
inline typename HashSet<T, U, V, W>::iterator HashSet<T, U, V, W>::end() const
{
    return m_impl.end();
}

template <typename T, typename U, typename V, typename W>
inline typename HashSet<T, U, V, W>::iterator HashSet<T, U, V, W>::find(ValuePeekInType value) const
{
    return m_impl.find(value);
}

template <typename Value, typename HashFunctions, typename Traits, typename Allocator>
inline bool HashSet<Value, HashFunctions, Traits, Allocator>::contains(ValuePeekInType value) const
{
    return m_impl.contains(value);
}

template <typename Value, typename HashFunctions, typename Traits, typename Allocator>
template <typename HashTranslator, typename T>
typename HashSet<Value, HashFunctions, Traits, Allocator>::iterator
inline HashSet<Value, HashFunctions, Traits, Allocator>::find(const T& value) const
{
    return m_impl.template find<HashSetTranslatorAdapter<HashTranslator>>(value);
}

template <typename Value, typename HashFunctions, typename Traits, typename Allocator>
template <typename HashTranslator, typename T>
inline bool HashSet<Value, HashFunctions, Traits, Allocator>::contains(const T& value) const
{
    return m_impl.template contains<HashSetTranslatorAdapter<HashTranslator>>(value);
}

template <typename T, typename U, typename V, typename W>
template <typename IncomingValueType>
inline typename HashSet<T, U, V, W>::AddResult HashSet<T, U, V, W>::add(IncomingValueType&& value)
{
    return m_impl.add(std::forward<IncomingValueType>(value));
}

template <typename Value, typename HashFunctions, typename Traits, typename Allocator>
template <typename HashTranslator, typename T>
inline typename HashSet<Value, HashFunctions, Traits, Allocator>::AddResult
HashSet<Value, HashFunctions, Traits, Allocator>::addWithTranslator(T&& value)
{
    // Forward only the first argument, because the second argument isn't actually used in HashSetTranslatorAdapter.
    return m_impl.template addPassingHashCode<HashSetTranslatorAdapter<HashTranslator>>(std::forward<T>(value), value);
}

template <typename T, typename U, typename V, typename W>
inline void HashSet<T, U, V, W>::remove(iterator it)
{
    m_impl.remove(it.m_impl);
}

template <typename T, typename U, typename V, typename W>
inline void HashSet<T, U, V, W>::remove(ValuePeekInType value)
{
    remove(find(value));
}

template <typename T, typename U, typename V, typename W>
inline void HashSet<T, U, V, W>::clear()
{
    m_impl.clear();
}

template <typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::take(iterator it) -> ValueType
{
    if (it == end())
        return ValueTraits::emptyValue();

    ValueType result = std::move(const_cast<ValueType&>(*it));
    remove(it);

    return result;
}

template <typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::take(ValuePeekInType value) -> ValueType
{
    return take(find(value));
}

template <typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::takeAny() -> ValueType
{
    return take(begin());
}

template <typename C, typename W>
inline void copyToVector(const C& collection, W& vector)
{
    typedef typename C::const_iterator iterator;

    {
        // Disallow GC across resize allocation, see crbug.com/568173
        typename W::GCForbiddenScope scope;
        vector.resize(collection.size());
    }

    iterator it = collection.begin();
    iterator end = collection.end();
    for (unsigned i = 0; it != end; ++it, ++i)
        vector[i] = *it;
}

} // namespace WTF

using WTF::HashSet;

#endif // WTF_HashSet_h
