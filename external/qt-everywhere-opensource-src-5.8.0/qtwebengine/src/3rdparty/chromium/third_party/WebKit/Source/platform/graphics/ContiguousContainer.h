// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContiguousContainer_h
#define ContiguousContainer_h

#include "platform/PlatformExport.h"
#include "wtf/Alignment.h"
#include "wtf/Allocator.h"
#include "wtf/Compiler.h"
#include "wtf/Noncopyable.h"
#include "wtf/TypeTraits.h"
#include "wtf/Vector.h"
#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>

namespace blink {

// ContiguousContainer is a container which stores a list of heterogeneous
// objects (in particular, of varying sizes), packed next to one another in
// memory. Objects are never relocated, so it is safe to store pointers to them
// for the lifetime of the container (unless the object is removed).
//
// Memory is allocated in a series of buffers (with exponential growth). When an
// object is allocated, it is given only the space it requires (possibly with
// enough padding to preserve alignment), rather than the maximum possible size.
// This allows small and large objects to coexist without wasting much space.
//
// Since it stores pointers to all of the objects it allocates in a vector, it
// supports efficient iteration and indexing. However, for mutation the
// supported operations are limited to appending to, and removing from, the end
// of the list.
//
// Clients should instantiate ContiguousContainer; ContiguousContainerBase is an
// artifact of the implementation.

class PLATFORM_EXPORT ContiguousContainerBase {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(ContiguousContainerBase);
protected:
    explicit ContiguousContainerBase(size_t maxObjectSize);
    ContiguousContainerBase(ContiguousContainerBase&&);
    ~ContiguousContainerBase();

    ContiguousContainerBase& operator=(ContiguousContainerBase&&);

    size_t size() const { return m_elements.size(); }
    bool isEmpty() const { return !size(); }
    size_t capacityInBytes() const;
    size_t usedCapacityInBytes() const;
    size_t memoryUsageInBytes() const;

    // These do not invoke constructors or destructors.
    void reserveInitialCapacity(size_t, const char* typeName);
    void* allocate(size_t objectSize, const char* typeName);
    void removeLast();
    void clear();
    void swap(ContiguousContainerBase&);

    Vector<void*> m_elements;

private:
    class Buffer;

    Buffer* allocateNewBufferForNextAllocation(size_t, const char* typeName);

    Vector<std::unique_ptr<Buffer>> m_buffers;
    unsigned m_endIndex;
    size_t m_maxObjectSize;
};

// For most cases, no alignment stricter than pointer alignment is required. If
// one of the derived classes has stronger alignment requirements (and the
// static_assert fires), set alignment to the LCM of the derived class
// alignments. For small structs without pointers, it may be possible to reduce
// alignment for tighter packing.

template <class BaseElementType, unsigned alignment = sizeof(void*)>
class ContiguousContainer : public ContiguousContainerBase {
private:
    // Declares itself as a forward iterator, but also supports a few more
    // things. The whole random access iterator interface is a bit much.
    template <typename BaseIterator, typename ValueType>
    class IteratorWrapper : public std::iterator<std::forward_iterator_tag, ValueType> {
        DISALLOW_NEW();
    public:
        IteratorWrapper() {}
        bool operator==(const IteratorWrapper& other) const { return m_it == other.m_it; }
        bool operator!=(const IteratorWrapper& other) const { return m_it != other.m_it; }
        ValueType& operator*() const { return *static_cast<ValueType*>(*m_it); }
        ValueType* operator->() const { return &operator*(); }
        IteratorWrapper operator+(std::ptrdiff_t n) const { return IteratorWrapper(m_it + n); }
        IteratorWrapper operator++(int) { IteratorWrapper tmp = *this; ++m_it; return tmp; }
        std::ptrdiff_t operator-(const IteratorWrapper& other) const { return m_it - other.m_it; }
        IteratorWrapper& operator++() { ++m_it; return *this; }
    private:
        explicit IteratorWrapper(const BaseIterator& it) : m_it(it) {}
        BaseIterator m_it;
        friend class ContiguousContainer;
    };

public:
    using iterator = IteratorWrapper<Vector<void*>::iterator, BaseElementType>;
    using const_iterator = IteratorWrapper<Vector<void*>::const_iterator, const BaseElementType>;
    using reverse_iterator = IteratorWrapper<Vector<void*>::reverse_iterator, BaseElementType>;
    using const_reverse_iterator = IteratorWrapper<Vector<void*>::const_reverse_iterator, const BaseElementType>;

    explicit ContiguousContainer(size_t maxObjectSize) : ContiguousContainerBase(align(maxObjectSize)) {}

    ContiguousContainer(size_t maxObjectSize, size_t initialSizeBytes)
        : ContiguousContainer(maxObjectSize)
    {
        reserveInitialCapacity(std::max(maxObjectSize, initialSizeBytes), WTF_HEAP_PROFILER_TYPE_NAME(BaseElementType));
    }

    ContiguousContainer(ContiguousContainer&& source)
        : ContiguousContainerBase(std::move(source)) {}

    ~ContiguousContainer()
    {
        for (auto& element : *this) {
            (void)element; // MSVC incorrectly reports this variable as unused.
            element.~BaseElementType();
        }
    }

    ContiguousContainer& operator=(ContiguousContainer&& source)
    {
        // Must clear in the derived class to ensure that element destructors
        // care called.
        clear();

        ContiguousContainerBase::operator=(std::move(source));
        return *this;
    }

    using ContiguousContainerBase::size;
    using ContiguousContainerBase::isEmpty;
    using ContiguousContainerBase::capacityInBytes;
    using ContiguousContainerBase::usedCapacityInBytes;
    using ContiguousContainerBase::memoryUsageInBytes;

    iterator begin() { return iterator(m_elements.begin()); }
    iterator end() { return iterator(m_elements.end()); }
    const_iterator begin() const { return const_iterator(m_elements.begin()); }
    const_iterator end() const { return const_iterator(m_elements.end()); }
    reverse_iterator rbegin() { return reverse_iterator(m_elements.rbegin()); }
    reverse_iterator rend() { return reverse_iterator(m_elements.rend()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(m_elements.rbegin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(m_elements.rend()); }

    BaseElementType& first() { return *begin(); }
    const BaseElementType& first() const { return *begin(); }
    BaseElementType& last() { return *rbegin(); }
    const BaseElementType& last() const { return *rbegin(); }
    BaseElementType& operator[](size_t index) { return *(begin() + index); }
    const BaseElementType& operator[](size_t index) const { return *(begin() + index); }

    template <class DerivedElementType, typename... Args>
    DerivedElementType& allocateAndConstruct(Args&&... args)
    {
        static_assert(WTF::IsSubclass<DerivedElementType, BaseElementType>::value,
            "Must use subclass of BaseElementType.");
        static_assert(alignment % WTF_ALIGN_OF(DerivedElementType) == 0,
            "Derived type requires stronger alignment.");
        size_t allocSize = align(sizeof(DerivedElementType));
        return *new (allocate(allocSize)) DerivedElementType(std::forward<Args>(args)...);
    }

    void removeLast()
    {
        ASSERT(!isEmpty());
        last().~BaseElementType();
        ContiguousContainerBase::removeLast();
    }

    void clear()
    {
        for (auto& element : *this) {
            (void)element; // MSVC incorrectly reports this variable as unused.
            element.~BaseElementType();
        }
        ContiguousContainerBase::clear();
    }

    void swap(ContiguousContainer& other) { ContiguousContainerBase::swap(other); }

    // Appends a new element using memcpy, then default-constructs a base
    // element in its place. Use with care.
    BaseElementType& appendByMoving(BaseElementType& item, size_t size)
    {
        ASSERT(size >= sizeof(BaseElementType));
        void* newItem = allocate(size);
        memcpy(newItem, static_cast<void*>(&item), size);
        new (&item) BaseElementType;
        return *static_cast<BaseElementType*>(newItem);
    }

private:
    void* allocate(size_t objectSize)
    {
        return ContiguousContainerBase::allocate(objectSize, WTF_HEAP_PROFILER_TYPE_NAME(BaseElementType));
    }

    static size_t align(size_t size)
    {
        size_t alignedSize = alignment * ((size + alignment - 1) / alignment);
        ASSERT(alignedSize % alignment == 0);
        ASSERT(alignedSize >= size);
        ASSERT(alignedSize < size + alignment);
        return alignedSize;
    }
};

} // namespace blink

#endif // ContiguousContainer_h
