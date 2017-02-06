/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011, Benjamin Poulain <ikipou@gmail.com>
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

#ifndef WTF_ListHashSet_h
#define WTF_ListHashSet_h

#include "wtf/HashSet.h"
#include "wtf/allocator/PartitionAllocator.h"
#include <memory>

namespace WTF {

// ListHashSet: Just like HashSet, this class provides a Set interface - a
// collection of unique objects with O(1) insertion, removal and test for
// containership. However, it also has an order - iterating it will always give
// back values in the order in which they are added.

// Unlike iteration of most WTF Hash data structures, iteration is guaranteed
// safe against mutation of the ListHashSet, except for removal of the item
// currently pointed to by a given iterator.

template <typename Value, size_t inlineCapacity, typename HashFunctions, typename Allocator> class ListHashSet;

template <typename Set> class ListHashSetIterator;
template <typename Set> class ListHashSetConstIterator;
template <typename Set> class ListHashSetReverseIterator;
template <typename Set> class ListHashSetConstReverseIterator;

template <typename ValueArg> class ListHashSetNodeBase;
template <typename ValueArg, typename Allocator> class ListHashSetNode;
template <typename ValueArg, size_t inlineCapacity> struct ListHashSetAllocator;

template <typename HashArg> struct ListHashSetNodeHashFunctions;
template <typename HashArg> struct ListHashSetTranslator;

// Note that for a ListHashSet you cannot specify the HashTraits as a template
// argument. It uses the default hash traits for the ValueArg type.
template <typename ValueArg, size_t inlineCapacity = 256, typename HashArg = typename DefaultHash<ValueArg>::Hash, typename AllocatorArg = ListHashSetAllocator<ValueArg, inlineCapacity>>
class ListHashSet : public ConditionalDestructor<ListHashSet<ValueArg, inlineCapacity, HashArg, AllocatorArg>, AllocatorArg::isGarbageCollected> {
    typedef AllocatorArg Allocator;
    WTF_USE_ALLOCATOR(ListHashSet, Allocator);

    typedef ListHashSetNode<ValueArg, Allocator> Node;
    typedef HashTraits<Node*> NodeTraits;
    typedef ListHashSetNodeHashFunctions<HashArg> NodeHash;
    typedef ListHashSetTranslator<HashArg> BaseTranslator;

    typedef HashTable<Node*, Node*, IdentityExtractor, NodeHash, NodeTraits, NodeTraits, typename Allocator::TableAllocator> ImplType;
    typedef HashTableIterator<Node*, Node*, IdentityExtractor, NodeHash, NodeTraits, NodeTraits, typename Allocator::TableAllocator> ImplTypeIterator;
    typedef HashTableConstIterator<Node*, Node*, IdentityExtractor, NodeHash, NodeTraits, NodeTraits, typename Allocator::TableAllocator> ImplTypeConstIterator;

    typedef HashArg HashFunctions;

public:
    typedef ValueArg ValueType;
    typedef HashTraits<ValueType> ValueTraits;
    typedef typename ValueTraits::PeekInType ValuePeekInType;

    typedef ListHashSetIterator<ListHashSet> iterator;
    typedef ListHashSetConstIterator<ListHashSet> const_iterator;
    friend class ListHashSetIterator<ListHashSet>;
    friend class ListHashSetConstIterator<ListHashSet>;

    typedef ListHashSetReverseIterator<ListHashSet> reverse_iterator;
    typedef ListHashSetConstReverseIterator<ListHashSet> const_reverse_iterator;
    friend class ListHashSetReverseIterator<ListHashSet>;
    friend class ListHashSetConstReverseIterator<ListHashSet>;

    struct AddResult final {
        STACK_ALLOCATED();
        friend class ListHashSet<ValueArg, inlineCapacity, HashArg, AllocatorArg>;
        AddResult(Node* node, bool isNewEntry)
            : storedValue(&node->m_value)
            , isNewEntry(isNewEntry)
            , m_node(node) { }
        ValueType* storedValue;
        bool isNewEntry;
    private:
        Node* m_node;
    };

    ListHashSet();
    ListHashSet(const ListHashSet&);
    ListHashSet(ListHashSet&&);
    ListHashSet& operator=(const ListHashSet&);
    ListHashSet& operator=(ListHashSet&&);
    void finalize();

    void swap(ListHashSet&);

    unsigned size() const { return m_impl.size(); }
    unsigned capacity() const { return m_impl.capacity(); }
    bool isEmpty() const { return m_impl.isEmpty(); }

    iterator begin() { return makeIterator(m_head); }
    iterator end() { return makeIterator(0); }
    const_iterator begin() const { return makeConstIterator(m_head); }
    const_iterator end() const { return makeConstIterator(0); }

    reverse_iterator rbegin() { return makeReverseIterator(m_tail); }
    reverse_iterator rend() { return makeReverseIterator(0); }
    const_reverse_iterator rbegin() const { return makeConstReverseIterator(m_tail); }
    const_reverse_iterator rend() const { return makeConstReverseIterator(0); }

    ValueType& first();
    const ValueType& first() const;
    void removeFirst();

    ValueType& last();
    const ValueType& last() const;
    void removeLast();

    iterator find(ValuePeekInType);
    const_iterator find(ValuePeekInType) const;
    bool contains(ValuePeekInType) const;

    // An alternate version of find() that finds the object by hashing and
    // comparing with some other type, to avoid the cost of type conversion.
    // The HashTranslator interface is defined in HashSet.
    template <typename HashTranslator, typename T> iterator find(const T&);
    template <typename HashTranslator, typename T> const_iterator find(const T&) const;
    template <typename HashTranslator, typename T> bool contains(const T&) const;

    // The return value of add is a pair of a pointer to the stored value, and a
    // bool that is true if an new entry was added.
    template <typename IncomingValueType>
    AddResult add(IncomingValueType&&);

    // Same as add() except that the return value is an iterator. Useful in
    // cases where it's needed to have the same return value as find() and where
    // it's not possible to use a pointer to the storedValue.
    template <typename IncomingValueType>
    iterator addReturnIterator(IncomingValueType&&);

    // Add the value to the end of the collection. If the value was already in
    // the list, it is moved to the end.
    template <typename IncomingValueType>
    AddResult appendOrMoveToLast(IncomingValueType&&);

    // Add the value to the beginning of the collection. If the value was
    // already in the list, it is moved to the beginning.
    template <typename IncomingValueType>
    AddResult prependOrMoveToFirst(IncomingValueType&&);

    template <typename IncomingValueType>
    AddResult insertBefore(ValuePeekInType beforeValue, IncomingValueType&& newValue);
    template <typename IncomingValueType>
    AddResult insertBefore(iterator, IncomingValueType&&);

    void remove(ValuePeekInType value) { return remove(find(value)); }
    void remove(iterator);
    void clear();
    template <typename Collection>
    void removeAll(const Collection& other) { WTF::removeAll(*this, other); }

    ValueType take(iterator);
    ValueType take(ValuePeekInType);
    ValueType takeFirst();

    template <typename VisitorDispatcher>
    void trace(VisitorDispatcher);

private:
    void unlink(Node*);
    void unlinkAndDelete(Node*);
    void appendNode(Node*);
    void prependNode(Node*);
    void insertNodeBefore(Node* beforeNode, Node* newNode);
    void deleteAllNodes();
    Allocator* getAllocator() const { return m_allocatorProvider.get(); }
    void createAllocatorIfNeeded() { m_allocatorProvider.createAllocatorIfNeeded(); }
    void deallocate(Node* node) const { m_allocatorProvider.deallocate(node); }

    iterator makeIterator(Node* position) { return iterator(this, position); }
    const_iterator makeConstIterator(Node* position) const { return const_iterator(this, position); }
    reverse_iterator makeReverseIterator(Node* position) { return reverse_iterator(this, position); }
    const_reverse_iterator makeConstReverseIterator(Node* position) const { return const_reverse_iterator(this, position); }

    ImplType m_impl;
    Node* m_head;
    Node* m_tail;
    typename Allocator::AllocatorProvider m_allocatorProvider;
};

// ListHashSetNode has this base class to hold the members because the MSVC
// compiler otherwise gets into circular template dependencies when trying to do
// sizeof on a node.
template <typename ValueArg> class ListHashSetNodeBase {
    DISALLOW_NEW();
protected:
    template <typename U>
    explicit ListHashSetNodeBase(U&& value)
        : m_value(std::forward<U>(value))
        , m_prev(nullptr)
        , m_next(nullptr)
#if ENABLE(ASSERT)
        , m_isAllocated(true)
#endif
    {
    }

public:
    ValueArg m_value;
    ListHashSetNodeBase* m_prev;
    ListHashSetNodeBase* m_next;
#if ENABLE(ASSERT)
    bool m_isAllocated;
#endif
};

// This allocator is only used for non-Heap ListHashSets.
template <typename ValueArg, size_t inlineCapacity>
struct ListHashSetAllocator : public PartitionAllocator {
    typedef PartitionAllocator TableAllocator;
    typedef ListHashSetNode<ValueArg, ListHashSetAllocator> Node;
    typedef ListHashSetNodeBase<ValueArg> NodeBase;

    class AllocatorProvider {
        DISALLOW_NEW();
    public:
        AllocatorProvider() : m_allocator(nullptr) {}
        void createAllocatorIfNeeded()
        {
            if (!m_allocator)
                m_allocator = new ListHashSetAllocator;
        }

        void releaseAllocator()
        {
            delete m_allocator;
            m_allocator = nullptr;
        }

        void swap(AllocatorProvider& other)
        {
            std::swap(m_allocator, other.m_allocator);
        }

        void deallocate(Node* node) const
        {
            ASSERT(m_allocator);
            m_allocator->deallocate(node);
        }

        ListHashSetAllocator* get() const
        {
            ASSERT(m_allocator);
            return m_allocator;
        }

    private:
        // Not using std::unique_ptr as this pointer should be deleted at
        // releaseAllocator() method rather than at destructor.
        ListHashSetAllocator* m_allocator;
    };

    ListHashSetAllocator()
        : m_freeList(pool())
        , m_isDoneWithInitialFreeList(false)
    {
        memset(m_pool.buffer, 0, sizeof(m_pool.buffer));
    }

    Node* allocateNode()
    {
        Node* result = m_freeList;

        if (!result)
            return static_cast<Node*>(WTF::Partitions::fastMalloc(sizeof(NodeBase), WTF_HEAP_PROFILER_TYPE_NAME(Node)));

        ASSERT(!result->m_isAllocated);

        Node* next = result->next();
        ASSERT(!next || !next->m_isAllocated);
        if (!next && !m_isDoneWithInitialFreeList) {
            next = result + 1;
            if (next == pastPool()) {
                m_isDoneWithInitialFreeList = true;
                next = nullptr;
            } else {
                ASSERT(inPool(next));
                ASSERT(!next->m_isAllocated);
            }
        }
        m_freeList = next;

        return result;
    }

    void deallocate(Node* node)
    {
        if (inPool(node)) {
#if ENABLE(ASSERT)
            node->m_isAllocated = false;
#endif
            node->m_next = m_freeList;
            m_freeList = node;
            return;
        }

        WTF::Partitions::fastFree(node);
    }

    bool inPool(Node* node)
    {
        return node >= pool() && node < pastPool();
    }

    static void traceValue(typename PartitionAllocator::Visitor* visitor, Node* node) {}

private:
    Node* pool() { return reinterpret_cast_ptr<Node*>(m_pool.buffer); }
    Node* pastPool() { return pool() + m_poolSize; }

    Node* m_freeList;
    bool m_isDoneWithInitialFreeList;
#if defined(MEMORY_SANITIZER_INITIAL_SIZE)
    // The allocation pool for nodes is one big chunk that ASAN has no insight
    // into, so it can cloak errors. Make it as small as possible to force nodes
    // to be allocated individually where ASAN can see them.
    static const size_t m_poolSize = 1;
#else
    static const size_t m_poolSize = inlineCapacity;
#endif
    AlignedBuffer<sizeof(NodeBase) * m_poolSize, WTF_ALIGN_OF(NodeBase)> m_pool;
};

template <typename ValueArg, typename AllocatorArg> class ListHashSetNode : public ListHashSetNodeBase<ValueArg> {
public:
    typedef AllocatorArg NodeAllocator;
    typedef ValueArg Value;

    template <typename U>
    ListHashSetNode(U&& value)
        : ListHashSetNodeBase<ValueArg>(std::forward<U>(value)) {}

    void* operator new(size_t, NodeAllocator* allocator)
    {
        static_assert(sizeof(ListHashSetNode) == sizeof(ListHashSetNodeBase<ValueArg>), "please add any fields to the base");
        return allocator->allocateNode();
    }

    void setWasAlreadyDestructed()
    {
        if (NodeAllocator::isGarbageCollected && !IsTriviallyDestructible<ValueArg>::value)
            this->m_prev = unlinkedNodePointer();
    }

    bool wasAlreadyDestructed() const
    {
        ASSERT(NodeAllocator::isGarbageCollected);
        return this->m_prev == unlinkedNodePointer();
    }

    static void finalize(void* pointer)
    {
        ASSERT(!IsTriviallyDestructible<ValueArg>::value); // No need to waste time calling finalize if it's not needed.
        ListHashSetNode* self = reinterpret_cast_ptr<ListHashSetNode*>(pointer);

        // Check whether this node was already destructed before being unlinked
        // from the collection.
        if (self->wasAlreadyDestructed())
            return;

        self->m_value.~ValueArg();
    }
    void finalizeGarbageCollectedObject()
    {
        finalize(this);
    }

    void destroy(NodeAllocator* allocator)
    {
        this->~ListHashSetNode();
        setWasAlreadyDestructed();
        allocator->deallocate(this);
    }

    // This is not called in normal tracing, but it is called if we find a
    // pointer to a node on the stack using conservative scanning. Since the
    // original ListHashSet may no longer exist we make sure to mark the
    // neighbours in the chain too.
    template <typename VisitorDispatcher>
    void trace(VisitorDispatcher visitor)
    {
        // The conservative stack scan can find nodes that have been removed
        // from the set and destructed. We don't need to trace these, and it
        // would be wrong to do so, because the class will not expect the trace
        // method to be called after the destructor.  It's an error to remove a
        // node from the ListHashSet while an iterator is positioned at that
        // node, so there should be no valid pointers from the stack to a
        // destructed node.
        if (wasAlreadyDestructed())
            return;
        NodeAllocator::traceValue(visitor, this);
        visitor->mark(next());
        visitor->mark(prev());
    }

    ListHashSetNode* next() const { return reinterpret_cast<ListHashSetNode*>(this->m_next); }
    ListHashSetNode* prev() const { return reinterpret_cast<ListHashSetNode*>(this->m_prev); }

    // Don't add fields here, the ListHashSetNodeBase and this should have the
    // same size.

    static ListHashSetNode* unlinkedNodePointer() { return reinterpret_cast<ListHashSetNode*>(-1); }

    template <typename HashArg>
    friend struct ListHashSetNodeHashFunctions;
};

template <typename HashArg> struct ListHashSetNodeHashFunctions {
    STATIC_ONLY(ListHashSetNodeHashFunctions);
    template <typename T> static unsigned hash(const T& key) { return HashArg::hash(key->m_value); }
    template <typename T> static bool equal(const T& a, const T& b) { return HashArg::equal(a->m_value, b->m_value); }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

template <typename Set> class ListHashSetIterator {
    DISALLOW_NEW();
private:
    typedef typename Set::const_iterator const_iterator;
    typedef typename Set::Node Node;
    typedef typename Set::ValueType ValueType;
    typedef ValueType& ReferenceType;
    typedef ValueType* PointerType;

    ListHashSetIterator(const Set* set, Node* position) : m_iterator(set, position) {}

public:
    ListHashSetIterator() {}

    // default copy, assignment and destructor are OK

    PointerType get() const { return const_cast<PointerType>(m_iterator.get()); }
    ReferenceType operator*() const { return *get(); }
    PointerType operator->() const { return get(); }

    ListHashSetIterator& operator++() { ++m_iterator; return *this; }
    ListHashSetIterator& operator--() { --m_iterator; return *this; }

    // Postfix ++ and -- intentionally omitted.

    // Comparison.
    bool operator==(const ListHashSetIterator& other) const { return m_iterator == other.m_iterator; }
    bool operator!=(const ListHashSetIterator& other) const { return m_iterator != other.m_iterator; }

    operator const_iterator() const { return m_iterator; }

private:
    Node* getNode() { return m_iterator.getNode(); }

    const_iterator m_iterator;

    template <typename T, size_t inlineCapacity, typename U, typename V>
    friend class ListHashSet;
};

template <typename Set>
class ListHashSetConstIterator {
    DISALLOW_NEW();
private:
    typedef typename Set::const_iterator const_iterator;
    typedef typename Set::Node Node;
    typedef typename Set::ValueType ValueType;
    typedef const ValueType& ReferenceType;
    typedef const ValueType* PointerType;

    friend class ListHashSetIterator<Set>;

    ListHashSetConstIterator(const Set* set, Node* position)
        : m_set(set)
        , m_position(position)
    {
    }

public:
    ListHashSetConstIterator()
    {
    }

    PointerType get() const
    {
        return &m_position->m_value;
    }
    ReferenceType operator*() const { return *get(); }
    PointerType operator->() const { return get(); }

    ListHashSetConstIterator& operator++()
    {
        ASSERT(m_position != 0);
        m_position = m_position->next();
        return *this;
    }

    ListHashSetConstIterator& operator--()
    {
        ASSERT(m_position != m_set->m_head);
        if (!m_position)
            m_position = m_set->m_tail;
        else
            m_position = m_position->prev();
        return *this;
    }

    // Postfix ++ and -- intentionally omitted.

    // Comparison.
    bool operator==(const ListHashSetConstIterator& other) const
    {
        return m_position == other.m_position;
    }
    bool operator!=(const ListHashSetConstIterator& other) const
    {
        return m_position != other.m_position;
    }

private:
    Node* getNode() { return m_position; }

    const Set* m_set;
    Node* m_position;

    template <typename T, size_t inlineCapacity, typename U, typename V>
    friend class ListHashSet;
};

template <typename Set>
class ListHashSetReverseIterator {
    DISALLOW_NEW();
private:
    typedef typename Set::const_reverse_iterator const_reverse_iterator;
    typedef typename Set::Node Node;
    typedef typename Set::ValueType ValueType;
    typedef ValueType& ReferenceType;
    typedef ValueType* PointerType;

    ListHashSetReverseIterator(const Set* set, Node* position) : m_iterator(set, position) {}

public:
    ListHashSetReverseIterator() {}

    // default copy, assignment and destructor are OK

    PointerType get() const { return const_cast<PointerType>(m_iterator.get()); }
    ReferenceType operator*() const { return *get(); }
    PointerType operator->() const { return get(); }

    ListHashSetReverseIterator& operator++() { ++m_iterator; return *this; }
    ListHashSetReverseIterator& operator--() { --m_iterator; return *this; }

    // Postfix ++ and -- intentionally omitted.

    // Comparison.
    bool operator==(const ListHashSetReverseIterator& other) const { return m_iterator == other.m_iterator; }
    bool operator!=(const ListHashSetReverseIterator& other) const { return m_iterator != other.m_iterator; }

    operator const_reverse_iterator() const { return m_iterator; }

private:
    Node* getNode() { return m_iterator.node(); }

    const_reverse_iterator m_iterator;

    template <typename T, size_t inlineCapacity, typename U, typename V>
    friend class ListHashSet;
};

template <typename Set> class ListHashSetConstReverseIterator {
    DISALLOW_NEW();
private:
    typedef typename Set::reverse_iterator reverse_iterator;
    typedef typename Set::Node Node;
    typedef typename Set::ValueType ValueType;
    typedef const ValueType& ReferenceType;
    typedef const ValueType* PointerType;

    friend class ListHashSetReverseIterator<Set>;

    ListHashSetConstReverseIterator(const Set* set, Node* position)
        : m_set(set)
        , m_position(position)
    {
    }

public:
    ListHashSetConstReverseIterator()
    {
    }

    PointerType get() const
    {
        return &m_position->m_value;
    }
    ReferenceType operator*() const { return *get(); }
    PointerType operator->() const { return get(); }

    ListHashSetConstReverseIterator& operator++()
    {
        ASSERT(m_position != 0);
        m_position = m_position->prev();
        return *this;
    }

    ListHashSetConstReverseIterator& operator--()
    {
        ASSERT(m_position != m_set->m_tail);
        if (!m_position)
            m_position = m_set->m_head;
        else
            m_position = m_position->next();
        return *this;
    }

    // Postfix ++ and -- intentionally omitted.

    // Comparison.
    bool operator==(const ListHashSetConstReverseIterator& other) const
    {
        return m_position == other.m_position;
    }
    bool operator!=(const ListHashSetConstReverseIterator& other) const
    {
        return m_position != other.m_position;
    }

private:
    Node* getNode() { return m_position; }

    const Set* m_set;
    Node* m_position;

    template <typename T, size_t inlineCapacity, typename U, typename V>
    friend class ListHashSet;
};

template <typename HashFunctions>
struct ListHashSetTranslator {
    STATIC_ONLY(ListHashSetTranslator);
    template <typename T> static unsigned hash(const T& key) { return HashFunctions::hash(key); }
    template <typename T, typename U> static bool equal(const T& a, const U& b) { return HashFunctions::equal(a->m_value, b); }
    template <typename T, typename U, typename V> static void translate(T*& location, U&& key, const V& allocator)
    {
        location = new (const_cast<V*>(&allocator)) T(std::forward<U>(key));
    }
};

template <typename T, size_t inlineCapacity, typename U, typename V>
inline ListHashSet<T, inlineCapacity, U, V>::ListHashSet()
    : m_head(nullptr)
    , m_tail(nullptr)
{
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline ListHashSet<T, inlineCapacity, U, V>::ListHashSet(const ListHashSet& other)
    : m_head(nullptr)
    , m_tail(nullptr)
{
    const_iterator end = other.end();
    for (const_iterator it = other.begin(); it != end; ++it)
        add(*it);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline ListHashSet<T, inlineCapacity, U, V>::ListHashSet(ListHashSet&& other)
    : m_head(nullptr)
    , m_tail(nullptr)
{
    swap(other);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline ListHashSet<T, inlineCapacity, U, V>& ListHashSet<T, inlineCapacity, U, V>::operator=(const ListHashSet& other)
{
    ListHashSet tmp(other);
    swap(tmp);
    return *this;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline ListHashSet<T, inlineCapacity, U, V>& ListHashSet<T, inlineCapacity, U, V>::operator=(ListHashSet&& other)
{
    swap(other);
    return *this;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::swap(ListHashSet& other)
{
    m_impl.swap(other.m_impl);
    std::swap(m_head, other.m_head);
    std::swap(m_tail, other.m_tail);
    m_allocatorProvider.swap(other.m_allocatorProvider);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::finalize()
{
    static_assert(!Allocator::isGarbageCollected, "heap allocated ListHashSet should never call finalize()");
    deleteAllNodes();
    m_allocatorProvider.releaseAllocator();
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline T& ListHashSet<T, inlineCapacity, U, V>::first()
{
    ASSERT(!isEmpty());
    return m_head->m_value;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::removeFirst()
{
    ASSERT(!isEmpty());
    m_impl.remove(m_head);
    unlinkAndDelete(m_head);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline const T& ListHashSet<T, inlineCapacity, U, V>::first() const
{
    ASSERT(!isEmpty());
    return m_head->m_value;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline T& ListHashSet<T, inlineCapacity, U, V>::last()
{
    ASSERT(!isEmpty());
    return m_tail->m_value;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline const T& ListHashSet<T, inlineCapacity, U, V>::last() const
{
    ASSERT(!isEmpty());
    return m_tail->m_value;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::removeLast()
{
    ASSERT(!isEmpty());
    m_impl.remove(m_tail);
    unlinkAndDelete(m_tail);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline typename ListHashSet<T, inlineCapacity, U, V>::iterator ListHashSet<T, inlineCapacity, U, V>::find(ValuePeekInType value)
{
    ImplTypeIterator it = m_impl.template find<BaseTranslator>(value);
    if (it == m_impl.end())
        return end();
    return makeIterator(*it);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline typename ListHashSet<T, inlineCapacity, U, V>::const_iterator ListHashSet<T, inlineCapacity, U, V>::find(ValuePeekInType value) const
{
    ImplTypeConstIterator it = m_impl.template find<BaseTranslator>(value);
    if (it == m_impl.end())
        return end();
    return makeConstIterator(*it);
}

template <typename Translator>
struct ListHashSetTranslatorAdapter {
    STATIC_ONLY(ListHashSetTranslatorAdapter);
    template <typename T> static unsigned hash(const T& key) { return Translator::hash(key); }
    template <typename T, typename U> static bool equal(const T& a, const U& b) { return Translator::equal(a->m_value, b); }
};

template <typename ValueType, size_t inlineCapacity, typename U, typename V>
template <typename HashTranslator, typename T>
inline typename ListHashSet<ValueType, inlineCapacity, U, V>::iterator ListHashSet<ValueType, inlineCapacity, U, V>::find(const T& value)
{
    ImplTypeConstIterator it = m_impl.template find<ListHashSetTranslatorAdapter<HashTranslator>>(value);
    if (it == m_impl.end())
        return end();
    return makeIterator(*it);
}

template <typename ValueType, size_t inlineCapacity, typename U, typename V>
template <typename HashTranslator, typename T>
inline typename ListHashSet<ValueType, inlineCapacity, U, V>::const_iterator ListHashSet<ValueType, inlineCapacity, U, V>::find(const T& value) const
{
    ImplTypeConstIterator it = m_impl.template find<ListHashSetTranslatorAdapter<HashTranslator>>(value);
    if (it == m_impl.end())
        return end();
    return makeConstIterator(*it);
}

template <typename ValueType, size_t inlineCapacity, typename U, typename V>
template <typename HashTranslator, typename T>
inline bool ListHashSet<ValueType, inlineCapacity, U, V>::contains(const T& value) const
{
    return m_impl.template contains<ListHashSetTranslatorAdapter<HashTranslator>>(value);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline bool ListHashSet<T, inlineCapacity, U, V>::contains(ValuePeekInType value) const
{
    return m_impl.template contains<BaseTranslator>(value);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename IncomingValueType>
typename ListHashSet<T, inlineCapacity, U, V>::AddResult ListHashSet<T, inlineCapacity, U, V>::add(IncomingValueType&& value)
{
    createAllocatorIfNeeded();
    // The second argument is a const ref. This is useful for the HashTable
    // because it lets it take lvalues by reference, but for our purposes it's
    // inconvenient, since it constrains us to be const, whereas the allocator
    // actually changes when it does allocations.
    auto result = m_impl.template add<BaseTranslator>(std::forward<IncomingValueType>(value), *this->getAllocator());
    if (result.isNewEntry)
        appendNode(*result.storedValue);
    return AddResult(*result.storedValue, result.isNewEntry);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename IncomingValueType>
typename ListHashSet<T, inlineCapacity, U, V>::iterator ListHashSet<T, inlineCapacity, U, V>::addReturnIterator(IncomingValueType&& value)
{
    return makeIterator(add(std::forward<IncomingValueType>(value)).m_node);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename IncomingValueType>
typename ListHashSet<T, inlineCapacity, U, V>::AddResult ListHashSet<T, inlineCapacity, U, V>::appendOrMoveToLast(IncomingValueType&& value)
{
    createAllocatorIfNeeded();
    auto result = m_impl.template add<BaseTranslator>(std::forward<IncomingValueType>(value), *this->getAllocator());
    Node* node = *result.storedValue;
    if (!result.isNewEntry)
        unlink(node);
    appendNode(node);
    return AddResult(*result.storedValue, result.isNewEntry);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename IncomingValueType>
typename ListHashSet<T, inlineCapacity, U, V>::AddResult ListHashSet<T, inlineCapacity, U, V>::prependOrMoveToFirst(IncomingValueType&& value)
{
    createAllocatorIfNeeded();
    auto result = m_impl.template add<BaseTranslator>(std::forward<IncomingValueType>(value), *this->getAllocator());
    Node* node = *result.storedValue;
    if (!result.isNewEntry)
        unlink(node);
    prependNode(node);
    return AddResult(*result.storedValue, result.isNewEntry);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename IncomingValueType>
typename ListHashSet<T, inlineCapacity, U, V>::AddResult ListHashSet<T, inlineCapacity, U, V>::insertBefore(iterator it, IncomingValueType&& newValue)
{
    createAllocatorIfNeeded();
    auto result = m_impl.template add<BaseTranslator>(std::forward<IncomingValueType>(newValue), *this->getAllocator());
    if (result.isNewEntry)
        insertNodeBefore(it.getNode(), *result.storedValue);
    return AddResult(*result.storedValue, result.isNewEntry);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename IncomingValueType>
typename ListHashSet<T, inlineCapacity, U, V>::AddResult ListHashSet<T, inlineCapacity, U, V>::insertBefore(ValuePeekInType beforeValue, IncomingValueType&& newValue)
{
    createAllocatorIfNeeded();
    return insertBefore(find(beforeValue), std::forward<IncomingValueType>(newValue));
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::remove(iterator it)
{
    if (it == end())
        return;
    m_impl.remove(it.getNode());
    unlinkAndDelete(it.getNode());
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::clear()
{
    deleteAllNodes();
    m_impl.clear();
    m_head = nullptr;
    m_tail = nullptr;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
auto ListHashSet<T, inlineCapacity, U, V>::take(iterator it) -> ValueType
{
    if (it == end())
        return ValueTraits::emptyValue();

    m_impl.remove(it.getNode());
    ValueType result = std::move(it.getNode()->m_value);
    unlinkAndDelete(it.getNode());

    return result;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
auto ListHashSet<T, inlineCapacity, U, V>::take(ValuePeekInType value) -> ValueType
{
    return take(find(value));
}

template <typename T, size_t inlineCapacity, typename U, typename V>
auto ListHashSet<T, inlineCapacity, U, V>::takeFirst() -> ValueType
{
    ASSERT(!isEmpty());
    m_impl.remove(m_head);
    ValueType result = std::move(m_head->m_value);
    unlinkAndDelete(m_head);

    return result;
}

template <typename T, size_t inlineCapacity, typename U, typename Allocator>
void ListHashSet<T, inlineCapacity, U, Allocator>::unlink(Node* node)
{
    if (!node->m_prev) {
        ASSERT(node == m_head);
        m_head = node->next();
    } else {
        ASSERT(node != m_head);
        node->m_prev->m_next = node->m_next;
    }

    if (!node->m_next) {
        ASSERT(node == m_tail);
        m_tail = node->prev();
    } else {
        ASSERT(node != m_tail);
        node->m_next->m_prev = node->m_prev;
    }
}

template <typename T, size_t inlineCapacity, typename U, typename V>
void ListHashSet<T, inlineCapacity, U, V>::unlinkAndDelete(Node* node)
{
    unlink(node);
    node->destroy(this->getAllocator());
}

template <typename T, size_t inlineCapacity, typename U, typename V>
void ListHashSet<T, inlineCapacity, U, V>::appendNode(Node* node)
{
    node->m_prev = m_tail;
    node->m_next = nullptr;

    if (m_tail) {
        ASSERT(m_head);
        m_tail->m_next = node;
    } else {
        ASSERT(!m_head);
        m_head = node;
    }

    m_tail = node;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
void ListHashSet<T, inlineCapacity, U, V>::prependNode(Node* node)
{
    node->m_prev = nullptr;
    node->m_next = m_head;

    if (m_head)
        m_head->m_prev = node;
    else
        m_tail = node;

    m_head = node;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
void ListHashSet<T, inlineCapacity, U, V>::insertNodeBefore(Node* beforeNode, Node* newNode)
{
    if (!beforeNode)
        return appendNode(newNode);

    newNode->m_next = beforeNode;
    newNode->m_prev = beforeNode->m_prev;
    if (beforeNode->m_prev)
        beforeNode->m_prev->m_next = newNode;
    beforeNode->m_prev = newNode;

    if (!newNode->m_prev)
        m_head = newNode;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
void ListHashSet<T, inlineCapacity, U, V>::deleteAllNodes()
{
    if (!m_head)
        return;

    for (Node* node = m_head, *next = m_head->next(); node; node = next, next = node ? node->next() : 0)
        node->destroy(this->getAllocator());
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename VisitorDispatcher>
void ListHashSet<T, inlineCapacity, U, V>::trace(VisitorDispatcher visitor)
{
    static_assert(HashTraits<T>::weakHandlingFlag == NoWeakHandlingInCollections, "HeapListHashSet does not support weakness, consider using HeapLinkedHashSet instead.");
    // This marks all the nodes and their contents live that can be accessed
    // through the HashTable. That includes m_head and m_tail so we do not have
    // to explicitly trace them here.
    m_impl.trace(visitor);
}

} // namespace WTF

using WTF::ListHashSet;

#endif // WTF_ListHashSet_h
