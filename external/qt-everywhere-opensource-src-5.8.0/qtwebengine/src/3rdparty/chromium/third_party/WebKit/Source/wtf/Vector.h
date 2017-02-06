/*
 *  Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_Vector_h
#define WTF_Vector_h

#include "wtf/Alignment.h"
#include "wtf/ConditionalDestructor.h"
#include "wtf/ContainerAnnotations.h"
#include "wtf/Noncopyable.h"
#include "wtf/NotFound.h"
#include "wtf/StdLibExtras.h"
#include "wtf/VectorTraits.h"
#include "wtf/allocator/PartitionAllocator.h"
#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <string.h>
#include <utility>

// For ASAN builds, disable inline buffers completely as they cause various issues.
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
#define INLINE_CAPACITY 0
#else
#define INLINE_CAPACITY inlineCapacity
#endif

namespace WTF {

#if defined(MEMORY_SANITIZER_INITIAL_SIZE)
static const size_t kInitialVectorSize = 1;
#else
#ifndef WTF_VECTOR_INITIAL_SIZE
#define WTF_VECTOR_INITIAL_SIZE 4
#endif
static const size_t kInitialVectorSize = WTF_VECTOR_INITIAL_SIZE;
#endif

template <typename T, size_t inlineBuffer, typename Allocator>
class Deque;

template <bool needsDestruction, typename T>
struct VectorDestructor;

template <typename T>
struct VectorDestructor<false, T> {
    STATIC_ONLY(VectorDestructor);
    static void destruct(T*, T*) {}
};

template <typename T>
struct VectorDestructor<true, T> {
    STATIC_ONLY(VectorDestructor);
    static void destruct(T* begin, T* end)
    {
        for (T* cur = begin; cur != end; ++cur)
            cur->~T();
    }
};

template <bool unusedSlotsMustBeZeroed, typename T>
struct VectorUnusedSlotClearer;

template <typename T>
struct VectorUnusedSlotClearer<false, T> {
    STATIC_ONLY(VectorUnusedSlotClearer);
    static void clear(T*, T*) {}
#if ENABLE(ASSERT)
    static void checkCleared(const T*, const T*) {}
#endif
};

template <typename T>
struct VectorUnusedSlotClearer<true, T> {
    STATIC_ONLY(VectorUnusedSlotClearer);
    static void clear(T* begin, T* end)
    {
        memset(reinterpret_cast<void*>(begin), 0, sizeof(T) * (end - begin));
    }

#if ENABLE(ASSERT)
    static void checkCleared(const T* begin, const T* end)
    {
        const unsigned char* unusedArea = reinterpret_cast<const unsigned char*>(begin);
        const unsigned char* endAddress = reinterpret_cast<const unsigned char*>(end);
        ASSERT(endAddress >= unusedArea);
        for (int i = 0; i < endAddress - unusedArea; ++i)
            ASSERT(!unusedArea[i]);
    }
#endif
};

template <bool canInitializeWithMemset, typename T>
struct VectorInitializer;

template <typename T>
struct VectorInitializer<false, T> {
    STATIC_ONLY(VectorInitializer);
    static void initialize(T* begin, T* end)
    {
        for (T* cur = begin; cur != end; ++cur)
            new (NotNull, cur) T;
    }
};

template <typename T>
struct VectorInitializer<true, T> {
    STATIC_ONLY(VectorInitializer);
    static void initialize(T* begin, T* end)
    {
        memset(begin, 0, reinterpret_cast<char*>(end) - reinterpret_cast<char*>(begin));
    }
};

template <bool canMoveWithMemcpy, typename T>
struct VectorMover;

template <typename T>
struct VectorMover<false, T> {
    STATIC_ONLY(VectorMover);
    static void move(T* src, T* srcEnd, T* dst)
    {
        while (src != srcEnd) {
            new (NotNull, dst) T(std::move(*src));
            src->~T();
            ++dst;
            ++src;
        }
    }
    static void moveOverlapping(T* src, T* srcEnd, T* dst)
    {
        if (src > dst) {
            move(src, srcEnd, dst);
        } else {
            T* dstEnd = dst + (srcEnd - src);
            while (src != srcEnd) {
                --srcEnd;
                --dstEnd;
                new (NotNull, dstEnd) T(std::move(*srcEnd));
                srcEnd->~T();
            }
        }
    }
    static void swap(T* src, T* srcEnd, T* dst)
    {
        std::swap_ranges(src, srcEnd, dst);
    }
};

template <typename T>
struct VectorMover<true, T> {
    STATIC_ONLY(VectorMover);
    static void move(const T* src, const T* srcEnd, T* dst)
    {
        if (LIKELY(dst && src))
            memcpy(dst, src, reinterpret_cast<const char*>(srcEnd) - reinterpret_cast<const char*>(src));
    }
    static void moveOverlapping(const T* src, const T* srcEnd, T* dst)
    {
        if (LIKELY(dst && src))
            memmove(dst, src, reinterpret_cast<const char*>(srcEnd) - reinterpret_cast<const char*>(src));
    }
    static void swap(T* src, T* srcEnd, T* dst)
    {
        std::swap_ranges(reinterpret_cast<char*>(src), reinterpret_cast<char*>(srcEnd), reinterpret_cast<char*>(dst));
    }
};

template <bool canCopyWithMemcpy, typename T>
struct VectorCopier;

template <typename T>
struct VectorCopier<false, T> {
    STATIC_ONLY(VectorCopier);
    template <typename U>
    static void uninitializedCopy(const U* src, const U* srcEnd, T* dst)
    {
        while (src != srcEnd) {
            new (NotNull, dst) T(*src);
            ++dst;
            ++src;
        }
    }
};

template <typename T>
struct VectorCopier<true, T> {
    STATIC_ONLY(VectorCopier);
    static void uninitializedCopy(const T* src, const T* srcEnd, T* dst)
    {
        if (LIKELY(dst && src))
            memcpy(dst, src, reinterpret_cast<const char*>(srcEnd) - reinterpret_cast<const char*>(src));
    }
    template <typename U>
    static void uninitializedCopy(const U* src, const U* srcEnd, T* dst)
    {
        VectorCopier<false, T>::uninitializedCopy(src, srcEnd, dst);
    }
};

template <bool canFillWithMemset, typename T>
struct VectorFiller;

template <typename T>
struct VectorFiller<false, T> {
    STATIC_ONLY(VectorFiller);
    static void uninitializedFill(T* dst, T* dstEnd, const T& val)
    {
        while (dst != dstEnd) {
            new (NotNull, dst) T(val);
            ++dst;
        }
    }
};

template <typename T>
struct VectorFiller<true, T> {
    STATIC_ONLY(VectorFiller);
    static void uninitializedFill(T* dst, T* dstEnd, const T& val)
    {
        static_assert(sizeof(T) == sizeof(char), "size of type should be one");
#if COMPILER(GCC) && defined(_FORTIFY_SOURCE)
        if (!__builtin_constant_p(dstEnd - dst) || (!(dstEnd - dst)))
            memset(dst, val, dstEnd - dst);
#else
        memset(dst, val, dstEnd - dst);
#endif
    }
};

template <bool canCompareWithMemcmp, typename T>
struct VectorComparer;

template <typename T>
struct VectorComparer<false, T> {
    STATIC_ONLY(VectorComparer);
    static bool compare(const T* a, const T* b, size_t size)
    {
        ASSERT(a);
        ASSERT(b);
        return std::equal(a, a + size, b);
    }
};

template <typename T>
struct VectorComparer<true, T> {
    STATIC_ONLY(VectorComparer);
    static bool compare(const T* a, const T* b, size_t size)
    {
        ASSERT(a);
        ASSERT(b);
        return memcmp(a, b, sizeof(T) * size) == 0;
    }
};

template <typename T>
struct VectorElementComparer {
    STATIC_ONLY(VectorElementComparer);
    template <typename U>
    static bool compareElement(const T& left, const U& right)
    {
        return left == right;
    }
};

template <typename T>
struct VectorElementComparer<std::unique_ptr<T>> {
    STATIC_ONLY(VectorElementComparer);
    template <typename U>
    static bool compareElement(const std::unique_ptr<T>& left, const U& right)
    {
        return left.get() == right;
    }
};

template <typename T>
struct VectorTypeOperations {
    STATIC_ONLY(VectorTypeOperations);
    static void destruct(T* begin, T* end)
    {
        VectorDestructor<VectorTraits<T>::needsDestruction, T>::destruct(begin, end);
    }

    static void initialize(T* begin, T* end)
    {
        VectorInitializer<VectorTraits<T>::canInitializeWithMemset, T>::initialize(begin, end);
    }

    static void move(T* src, T* srcEnd, T* dst)
    {
        VectorMover<VectorTraits<T>::canMoveWithMemcpy, T>::move(src, srcEnd, dst);
    }

    static void moveOverlapping(T* src, T* srcEnd, T* dst)
    {
        VectorMover<VectorTraits<T>::canMoveWithMemcpy, T>::moveOverlapping(src, srcEnd, dst);
    }

    static void swap(T* src, T* srcEnd, T* dst)
    {
        VectorMover<VectorTraits<T>::canMoveWithMemcpy, T>::swap(src, srcEnd, dst);
    }

    static void uninitializedCopy(const T* src, const T* srcEnd, T* dst)
    {
        VectorCopier<VectorTraits<T>::canCopyWithMemcpy, T>::uninitializedCopy(src, srcEnd, dst);
    }

    static void uninitializedFill(T* dst, T* dstEnd, const T& val)
    {
        VectorFiller<VectorTraits<T>::canFillWithMemset, T>::uninitializedFill(dst, dstEnd, val);
    }

    static bool compare(const T* a, const T* b, size_t size)
    {
        return VectorComparer<VectorTraits<T>::canCompareWithMemcmp, T>::compare(a, b, size);
    }

    template <typename U>
    static bool compareElement(const T& left, U&& right)
    {
        return VectorElementComparer<T>::compareElement(left, std::forward<U>(right));
    }
};

template <typename T, bool hasInlineCapacity, typename Allocator>
class VectorBufferBase {
    WTF_MAKE_NONCOPYABLE(VectorBufferBase);
    DISALLOW_NEW();
public:
    void allocateBuffer(size_t newCapacity)
    {
        ASSERT(newCapacity);
        size_t sizeToAllocate = allocationSize(newCapacity);
        if (hasInlineCapacity)
            m_buffer = Allocator::template allocateInlineVectorBacking<T>(sizeToAllocate);
        else
            m_buffer = Allocator::template allocateVectorBacking<T>(sizeToAllocate);
        m_capacity = sizeToAllocate / sizeof(T);
    }

    void allocateExpandedBuffer(size_t newCapacity)
    {
        ASSERT(newCapacity);
        size_t sizeToAllocate = allocationSize(newCapacity);
        if (hasInlineCapacity)
            m_buffer = Allocator::template allocateInlineVectorBacking<T>(sizeToAllocate);
        else
            m_buffer = Allocator::template allocateExpandedVectorBacking<T>(sizeToAllocate);
        m_capacity = sizeToAllocate / sizeof(T);
    }

    size_t allocationSize(size_t capacity) const
    {
        return Allocator::template quantizedSize<T>(capacity);
    }

    T* buffer() { return m_buffer; }
    const T* buffer() const { return m_buffer; }
    size_t capacity() const { return m_capacity; }

    void clearUnusedSlots(T* from, T* to)
    {
        // If the vector backing is garbage-collected and needs tracing or
        // finalizing, we clear out the unused slots so that the visitor or the
        // finalizer does not cause a problem when visiting the unused slots.
        VectorUnusedSlotClearer<Allocator::isGarbageCollected && (VectorTraits<T>::needsDestruction || IsTraceableInCollectionTrait<VectorTraits<T>>::value), T>::clear(from, to);
    }

    void checkUnusedSlots(const T* from, const T* to)
    {
#if ENABLE(ASSERT) && !defined(ANNOTATE_CONTIGUOUS_CONTAINER)
        VectorUnusedSlotClearer<Allocator::isGarbageCollected && (VectorTraits<T>::needsDestruction || IsTraceableInCollectionTrait<VectorTraits<T>>::value), T>::checkCleared(from, to);
#endif
    }

    // |end| is exclusive, a la STL.
    struct OffsetRange final {
        OffsetRange() : begin(0), end(0) { }
        explicit OffsetRange(size_t begin, size_t end) : begin(begin), end(end) { ASSERT(begin <= end); }
        bool empty() const { return begin == end; }
        size_t begin;
        size_t end;
    };

protected:
    VectorBufferBase()
        : m_buffer(nullptr)
        , m_capacity(0)
    {
    }

    VectorBufferBase(T* buffer, size_t capacity)
        : m_buffer(buffer)
        , m_capacity(capacity)
    {
    }

    T* m_buffer;
    unsigned m_capacity;
    unsigned m_size;
};

template <typename T, size_t inlineCapacity, typename Allocator = PartitionAllocator>
class VectorBuffer;

template <typename T, typename Allocator>
class VectorBuffer<T, 0, Allocator> : protected VectorBufferBase<T, false, Allocator> {
private:
    using Base = VectorBufferBase<T, false, Allocator>;

public:
    using OffsetRange = typename Base::OffsetRange;

    VectorBuffer()
    {
    }

    explicit VectorBuffer(size_t capacity)
    {
        // Calling malloc(0) might take a lock and may actually do an allocation
        // on some systems.
        if (capacity)
            allocateBuffer(capacity);
    }

    void destruct()
    {
        deallocateBuffer(m_buffer);
        m_buffer = nullptr;
    }

    void deallocateBuffer(T* bufferToDeallocate)
    {
        Allocator::freeVectorBacking(bufferToDeallocate);
    }

    bool expandBuffer(size_t newCapacity)
    {
        size_t sizeToAllocate = allocationSize(newCapacity);
        if (Allocator::expandVectorBacking(m_buffer, sizeToAllocate)) {
            m_capacity = sizeToAllocate / sizeof(T);
            return true;
        }
        return false;
    }

    inline bool shrinkBuffer(size_t newCapacity)
    {
        ASSERT(newCapacity < capacity());
        size_t sizeToAllocate = allocationSize(newCapacity);
        if (Allocator::shrinkVectorBacking(m_buffer, allocationSize(capacity()), sizeToAllocate)) {
            m_capacity = sizeToAllocate / sizeof(T);
            return true;
        }
        return false;
    }

    void resetBufferPointer()
    {
        m_buffer = nullptr;
        m_capacity = 0;
    }

    // See the other specialization for the meaning of |thisHole| and |otherHole|. They are irrelevant in this case.
    void swapVectorBuffer(VectorBuffer<T, 0, Allocator>& other, OffsetRange thisHole, OffsetRange otherHole)
    {
        std::swap(m_buffer, other.m_buffer);
        std::swap(m_capacity, other.m_capacity);
        std::swap(m_size, other.m_size);
    }

    using Base::allocateBuffer;
    using Base::allocationSize;

    using Base::buffer;
    using Base::capacity;

    using Base::clearUnusedSlots;
    using Base::checkUnusedSlots;

    bool hasOutOfLineBuffer() const
    {
        // When inlineCapacity is 0 we have an out of line buffer if we have a
        // buffer.
        return buffer();
    }

protected:
    using Base::m_size;

private:
    using Base::m_buffer;
    using Base::m_capacity;
};

template <typename T, size_t inlineCapacity, typename Allocator>
class VectorBuffer : protected VectorBufferBase<T, true, Allocator> {
    WTF_MAKE_NONCOPYABLE(VectorBuffer);
private:
    using Base = VectorBufferBase<T, true, Allocator>;

public:
    using OffsetRange = typename Base::OffsetRange;

    VectorBuffer()
        : Base(inlineBuffer(), inlineCapacity)
    {
    }

    explicit VectorBuffer(size_t capacity)
        : Base(inlineBuffer(), inlineCapacity)
    {
        if (capacity > inlineCapacity)
            Base::allocateBuffer(capacity);
    }

    void destruct()
    {
        deallocateBuffer(m_buffer);
        m_buffer = nullptr;
    }

    NEVER_INLINE void reallyDeallocateBuffer(T* bufferToDeallocate)
    {
        Allocator::freeInlineVectorBacking(bufferToDeallocate);
    }

    void deallocateBuffer(T* bufferToDeallocate)
    {
        if (UNLIKELY(bufferToDeallocate != inlineBuffer()))
            reallyDeallocateBuffer(bufferToDeallocate);
    }

    bool expandBuffer(size_t newCapacity)
    {
        ASSERT(newCapacity > inlineCapacity);
        if (m_buffer == inlineBuffer())
            return false;

        size_t sizeToAllocate = allocationSize(newCapacity);
        if (Allocator::expandInlineVectorBacking(m_buffer, sizeToAllocate)) {
            m_capacity = sizeToAllocate / sizeof(T);
            return true;
        }
        return false;
    }

    inline bool shrinkBuffer(size_t newCapacity)
    {
        ASSERT(newCapacity < capacity());
        if (newCapacity <= inlineCapacity) {
            // We need to switch to inlineBuffer.  Vector::shrinkCapacity will
            // handle it.
            return false;
        }
        ASSERT(m_buffer != inlineBuffer());
        size_t newSize = allocationSize(newCapacity);
        if (!Allocator::shrinkInlineVectorBacking(m_buffer, allocationSize(capacity()), newSize))
            return false;
        m_capacity = newSize / sizeof(T);
        return true;
    }

    void resetBufferPointer()
    {
        m_buffer = inlineBuffer();
        m_capacity = inlineCapacity;
    }

    void allocateBuffer(size_t newCapacity)
    {
        // FIXME: This should ASSERT(!m_buffer) to catch misuse/leaks.
        if (newCapacity > inlineCapacity)
            Base::allocateBuffer(newCapacity);
        else
            resetBufferPointer();
    }

    void allocateExpandedBuffer(size_t newCapacity)
    {
        if (newCapacity > inlineCapacity)
            Base::allocateExpandedBuffer(newCapacity);
        else
            resetBufferPointer();
    }

    size_t allocationSize(size_t capacity) const
    {
        if (capacity <= inlineCapacity)
            return m_inlineBufferSize;
        return Base::allocationSize(capacity);
    }

    // Swap two vector buffers, both of which have the same non-zero inline capacity.
    //
    // If the data is in an out-of-line buffer, we can just pass the pointers across the two buffers.
    // If the data is in an inline buffer, we need to either swap or move each element, depending on whether each
    // slot is occupied or not.
    //
    // Further complication comes from the fact that VectorBuffer is also used as the backing store of a Deque.
    // Deque allocates the objects like a ring buffer, so there may be a "hole" (unallocated region) in the middle
    // of the buffer. This function assumes elements in a range [m_buffer, m_buffer + m_size) are all allocated
    // except for elements within |thisHole|. The same applies for |other.m_buffer| and |otherHole|.
    void swapVectorBuffer(VectorBuffer<T, inlineCapacity, Allocator>& other, OffsetRange thisHole, OffsetRange otherHole)
    {
        using TypeOperations = VectorTypeOperations<T>;

        if (buffer() != inlineBuffer() && other.buffer() != other.inlineBuffer()) {
            // The easiest case: both buffers are non-inline. We just need to swap the pointers.
            std::swap(m_buffer, other.m_buffer);
            std::swap(m_capacity, other.m_capacity);
            std::swap(m_size, other.m_size);
            return;
        }

        Allocator::enterGCForbiddenScope();

        // Otherwise, we at least need to move some elements from one inline buffer to another.
        //
        // Terminology: "source" is a place from which elements are copied, and "destination" is a place to which
        // elements are copied. thisSource or otherSource can be empty (represented by nullptr) when this range or
        // other range is in an out-of-line buffer.
        //
        // We first record which range needs to get moved and where elements in such a range will go. Elements in
        // an inline buffer will go to the other buffer's inline buffer. Elements in an out-of-line buffer won't move,
        // because we can just swap pointers of out-of-line buffers.
        T* thisSourceBegin = nullptr;
        size_t thisSourceSize = 0;
        T* thisDestinationBegin = nullptr;
        if (buffer() == inlineBuffer()) {
            thisSourceBegin = buffer();
            thisSourceSize = m_size;
            thisDestinationBegin = other.inlineBuffer();
            if (!thisHole.empty()) { // Sanity check.
                ASSERT(thisHole.begin < thisHole.end);
                ASSERT(thisHole.end <= thisSourceSize);
            }
        } else {
            // We don't need the hole information for an out-of-line buffer.
            thisHole.begin = thisHole.end = 0;
        }
        T* otherSourceBegin = nullptr;
        size_t otherSourceSize = 0;
        T* otherDestinationBegin = nullptr;
        if (other.buffer() == other.inlineBuffer()) {
            otherSourceBegin = other.buffer();
            otherSourceSize = other.m_size;
            otherDestinationBegin = inlineBuffer();
            if (!otherHole.empty()) {
                ASSERT(otherHole.begin < otherHole.end);
                ASSERT(otherHole.end <= otherSourceSize);
            }
        } else {
            otherHole.begin = otherHole.end = 0;
        }

        // Next, we mutate members and do other bookkeeping. We do pointer swapping (for out-of-line buffers) here if
        // we can. From now on, don't assume buffer() or capacity() maintains their original values.
        std::swap(m_capacity, other.m_capacity);
        if (thisSourceBegin && !otherSourceBegin) { // Our buffer is inline, theirs is not.
            ASSERT(buffer() == inlineBuffer());
            ASSERT(other.buffer() != other.inlineBuffer());
            ANNOTATE_DELETE_BUFFER(m_buffer, inlineCapacity, m_size);
            m_buffer = other.buffer();
            other.m_buffer = other.inlineBuffer();
            std::swap(m_size, other.m_size);
            ANNOTATE_NEW_BUFFER(other.m_buffer, inlineCapacity, other.m_size);
        } else if (!thisSourceBegin && otherSourceBegin) { // Their buffer is inline, ours is not.
            ASSERT(buffer() != inlineBuffer());
            ASSERT(other.buffer() == other.inlineBuffer());
            ANNOTATE_DELETE_BUFFER(other.m_buffer, inlineCapacity, other.m_size);
            other.m_buffer = buffer();
            m_buffer = inlineBuffer();
            std::swap(m_size, other.m_size);
            ANNOTATE_NEW_BUFFER(m_buffer, inlineCapacity, m_size);
        } else { // Both buffers are inline.
            ASSERT(thisSourceBegin && otherSourceBegin);
            ASSERT(buffer() == inlineBuffer());
            ASSERT(other.buffer() == other.inlineBuffer());
            ANNOTATE_CHANGE_SIZE(m_buffer, inlineCapacity, m_size, other.m_size);
            ANNOTATE_CHANGE_SIZE(other.m_buffer, inlineCapacity, other.m_size, m_size);
            std::swap(m_size, other.m_size);
        }

        // We are ready to move elements. We determine an action for each "section", which is a contiguous range such
        // that all elements in the range are treated similarly.
        size_t sectionBegin = 0;
        while (sectionBegin < inlineCapacity) {
            // To determine the end of this section, we list up all the boundaries where the "occupiedness" may change.
            size_t sectionEnd = inlineCapacity;
            if (thisSourceBegin && sectionBegin < thisSourceSize)
                sectionEnd = std::min(sectionEnd, thisSourceSize);
            if (!thisHole.empty() && sectionBegin < thisHole.begin)
                sectionEnd = std::min(sectionEnd, thisHole.begin);
            if (!thisHole.empty() && sectionBegin < thisHole.end)
                sectionEnd = std::min(sectionEnd, thisHole.end);
            if (otherSourceBegin && sectionBegin < otherSourceSize)
                sectionEnd = std::min(sectionEnd, otherSourceSize);
            if (!otherHole.empty() && sectionBegin < otherHole.begin)
                sectionEnd = std::min(sectionEnd, otherHole.begin);
            if (!otherHole.empty() && sectionBegin < otherHole.end)
                sectionEnd = std::min(sectionEnd, otherHole.end);

            ASSERT(sectionBegin < sectionEnd);

            // Is the |sectionBegin|-th element of |thisSource| occupied?
            bool thisOccupied = false;
            if (thisSourceBegin && sectionBegin < thisSourceSize) {
                // Yes, it's occupied, unless the position is in a hole.
                if (thisHole.empty() || sectionBegin < thisHole.begin || sectionBegin >= thisHole.end)
                    thisOccupied = true;
            }
            bool otherOccupied = false;
            if (otherSourceBegin && sectionBegin < otherSourceSize) {
                if (otherHole.empty() || sectionBegin < otherHole.begin || sectionBegin >= otherHole.end)
                    otherOccupied = true;
            }

            if (thisOccupied && otherOccupied) {
                // Both occupied; swap them. In this case, one's destination must be the other's source (i.e. both
                // ranges are in inline buffers).
                ASSERT(thisDestinationBegin == otherSourceBegin);
                ASSERT(otherDestinationBegin == thisSourceBegin);
                TypeOperations::swap(thisSourceBegin + sectionBegin, thisSourceBegin + sectionEnd,
                    otherSourceBegin + sectionBegin);
            } else if (thisOccupied) {
                // Move from ours to theirs.
                TypeOperations::move(thisSourceBegin + sectionBegin, thisSourceBegin + sectionEnd,
                    thisDestinationBegin + sectionBegin);
                Base::clearUnusedSlots(thisSourceBegin + sectionBegin, thisSourceBegin + sectionEnd);
            } else if (otherOccupied) {
                // Move from theirs to ours.
                TypeOperations::move(otherSourceBegin + sectionBegin, otherSourceBegin + sectionEnd,
                    otherDestinationBegin + sectionBegin);
                Base::clearUnusedSlots(otherSourceBegin + sectionBegin, otherSourceBegin + sectionEnd);
            } else {
                // Both empty; nothing to do.
            }

            sectionBegin = sectionEnd;
        }

        Allocator::leaveGCForbiddenScope();
    }

    using Base::buffer;
    using Base::capacity;

    bool hasOutOfLineBuffer() const
    {
        return buffer() && buffer() != inlineBuffer();
    }

protected:
    using Base::m_size;

private:
    using Base::m_buffer;
    using Base::m_capacity;

    static const size_t m_inlineBufferSize = inlineCapacity * sizeof(T);
    T* inlineBuffer() { return reinterpret_cast_ptr<T*>(m_inlineBuffer.buffer); }
    const T* inlineBuffer() const { return reinterpret_cast_ptr<const T*>(m_inlineBuffer.buffer); }

    AlignedBuffer<m_inlineBufferSize, WTF_ALIGN_OF(T)> m_inlineBuffer;
    template <typename U, size_t inlineBuffer, typename V>
    friend class Deque;
};

template <typename T, size_t inlineCapacity = 0, typename Allocator = PartitionAllocator> // Heap-allocated vectors with no inlineCapacity never need a destructor.
class Vector : private VectorBuffer<T, INLINE_CAPACITY, Allocator>, public ConditionalDestructor<Vector<T, INLINE_CAPACITY, Allocator>, (INLINE_CAPACITY == 0) && Allocator::isGarbageCollected> {
    WTF_USE_ALLOCATOR(Vector, Allocator);
    using Base = VectorBuffer<T, INLINE_CAPACITY, Allocator>;
    using TypeOperations = VectorTypeOperations<T>;
    using OffsetRange = typename Base::OffsetRange;

public:
    typedef T ValueType;
    typedef T value_type;

    typedef T* iterator;
    typedef const T* const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    Vector()
    {
        static_assert(!std::is_polymorphic<T>::value || !VectorTraits<T>::canInitializeWithMemset, "Cannot initialize with memset if there is a vtable");
        static_assert(Allocator::isGarbageCollected || !AllowsOnlyPlacementNew<T>::value || !IsTraceable<T>::value, "Cannot put DISALLOW_NEW_EXCEPT_PLACEMENT_NEW objects that have trace methods into an off-heap Vector");
        static_assert(Allocator::isGarbageCollected || !IsPointerToGarbageCollectedType<T>::value, "Cannot put raw pointers to garbage-collected classes into an off-heap Vector.  Use HeapVector<Member<T>> instead.");

        ANNOTATE_NEW_BUFFER(begin(), capacity(), 0);
        m_size = 0;
    }

    explicit Vector(size_t size)
        : Base(size)
    {
        static_assert(!std::is_polymorphic<T>::value || !VectorTraits<T>::canInitializeWithMemset, "Cannot initialize with memset if there is a vtable");
        static_assert(Allocator::isGarbageCollected || !AllowsOnlyPlacementNew<T>::value || !IsTraceable<T>::value, "Cannot put DISALLOW_NEW_EXCEPT_PLACEMENT_NEW objects that have trace methods into an off-heap Vector");
        static_assert(Allocator::isGarbageCollected || !IsPointerToGarbageCollectedType<T>::value, "Cannot put raw pointers to garbage-collected classes into an off-heap Vector.  Use HeapVector<Member<T>> instead.");

        ANNOTATE_NEW_BUFFER(begin(), capacity(), size);
        m_size = size;
        TypeOperations::initialize(begin(), end());
    }

    // Off-GC-heap vectors: Destructor should be called.
    // On-GC-heap vectors: Destructor should be called for inline buffers (if
    // any) but destructor shouldn't be called for vector backing since it is
    // managed by the traced GC heap.
    void finalize()
    {
        if (!INLINE_CAPACITY) {
            if (LIKELY(!Base::buffer()))
                return;
        }
        ANNOTATE_DELETE_BUFFER(begin(), capacity(), m_size);
        if (LIKELY(m_size) && !(Allocator::isGarbageCollected && this->hasOutOfLineBuffer())) {
            TypeOperations::destruct(begin(), end());
            m_size = 0; // Partial protection against use-after-free.
        }

        Base::destruct();
    }

    void finalizeGarbageCollectedObject()
    {
        finalize();
    }

    Vector(const Vector&);
    template <size_t otherCapacity>
    explicit Vector(const Vector<T, otherCapacity, Allocator>&);

    Vector& operator=(const Vector&);
    template <size_t otherCapacity>
    Vector& operator=(const Vector<T, otherCapacity, Allocator>&);

    Vector(Vector&&);
    Vector& operator=(Vector&&);

    Vector(std::initializer_list<T> elements);
    Vector& operator=(std::initializer_list<T> elements);

    size_t size() const { return m_size; }
    size_t capacity() const { return Base::capacity(); }
    bool isEmpty() const { return !size(); }

    T& at(size_t i)
    {
        RELEASE_ASSERT(i < size());
        return Base::buffer()[i];
    }
    const T& at(size_t i) const
    {
        RELEASE_ASSERT(i < size());
        return Base::buffer()[i];
    }

    T& operator[](size_t i) { return at(i); }
    const T& operator[](size_t i) const { return at(i); }

    T* data() { return Base::buffer(); }
    const T* data() const { return Base::buffer(); }

    iterator begin() { return data(); }
    iterator end() { return begin() + m_size; }
    const_iterator begin() const { return data(); }
    const_iterator end() const { return begin() + m_size; }

    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

    T& first() { return at(0); }
    const T& first() const { return at(0); }
    T& last() { return at(size() - 1); }
    const T& last() const { return at(size() - 1); }

    template <typename U> bool contains(const U&) const;
    template <typename U> size_t find(const U&) const;
    template <typename U> size_t reverseFind(const U&) const;

    void shrink(size_t);
    void grow(size_t);
    void resize(size_t);
    void reserveCapacity(size_t newCapacity);
    void reserveInitialCapacity(size_t initialCapacity);
    void shrinkToFit() { shrinkCapacity(size()); }
    void shrinkToReasonableCapacity()
    {
        if (size() * 2 < capacity())
            shrinkCapacity(size() + size() / 4 + 1);
    }

    void clear() { shrinkCapacity(0); }

    template <typename U> void append(const U*, size_t);
    template <typename U> void append(U&&);
    template <typename U> void uncheckedAppend(U&& val);
    template <typename U, size_t otherCapacity, typename V> void appendVector(const Vector<U, otherCapacity, V>&);

    template <typename U> void insert(size_t position, const U*, size_t);
    template <typename U> void insert(size_t position, U&&);
    template <typename U, size_t c, typename V> void insert(size_t position, const Vector<U, c, V>&);

    template <typename U> void prepend(const U*, size_t);
    template <typename U> void prepend(U&&);
    template <typename U, size_t c, typename V> void prependVector(const Vector<U, c, V>&);

    void remove(size_t position);
    void remove(size_t position, size_t length);

    void removeLast()
    {
        ASSERT(!isEmpty());
        shrink(size() - 1);
    }

    Vector(size_t size, const T& val)
        : Base(size)
    {
        ANNOTATE_NEW_BUFFER(begin(), capacity(), size);
        m_size = size;
        TypeOperations::uninitializedFill(begin(), end(), val);
    }

    void fill(const T&, size_t);
    void fill(const T& val) { fill(val, size()); }

    template <typename Iterator> void appendRange(Iterator start, Iterator end);

    void swap(Vector& other)
    {
        Base::swapVectorBuffer(other, OffsetRange(), OffsetRange());
    }

    void reverse();

    template <typename VisitorDispatcher> void trace(VisitorDispatcher);

    class GCForbiddenScope {
        STACK_ALLOCATED();
    public:
        GCForbiddenScope()
        {
            Allocator::enterGCForbiddenScope();
        }
        ~GCForbiddenScope()
        {
            Allocator::leaveGCForbiddenScope();
        }
    };

protected:
    using Base::checkUnusedSlots;
    using Base::clearUnusedSlots;

private:
    void expandCapacity(size_t newMinCapacity);
    T* expandCapacity(size_t newMinCapacity, T*);
    T* expandCapacity(size_t newMinCapacity, const T* data)
    {
        return expandCapacity(newMinCapacity, const_cast<T*>(data));
    }

    template <typename U> U* expandCapacity(size_t newMinCapacity, U*);
    void shrinkCapacity(size_t newCapacity);
    template <typename U> void appendSlowCase(U&&);

    using Base::m_size;
    using Base::buffer;
    using Base::swapVectorBuffer;
    using Base::allocateBuffer;
    using Base::allocationSize;
};

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>::Vector(const Vector& other)
    : Base(other.capacity())
{
    ANNOTATE_NEW_BUFFER(begin(), capacity(), other.size());
    m_size = other.size();
    TypeOperations::uninitializedCopy(other.begin(), other.end(), begin());
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <size_t otherCapacity>
Vector<T, inlineCapacity, Allocator>::Vector(const Vector<T, otherCapacity, Allocator>& other)
    : Base(other.capacity())
{
    ANNOTATE_NEW_BUFFER(begin(), capacity(), other.size());
    m_size = other.size();
    TypeOperations::uninitializedCopy(other.begin(), other.end(), begin());
}

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::operator=(const Vector<T, inlineCapacity, Allocator>& other)
{
    if (UNLIKELY(&other == this))
        return *this;

    if (size() > other.size()) {
        shrink(other.size());
    } else if (other.size() > capacity()) {
        clear();
        reserveCapacity(other.size());
        ASSERT(begin());
    }

    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, other.size());
    std::copy(other.begin(), other.begin() + size(), begin());
    TypeOperations::uninitializedCopy(other.begin() + size(), other.end(), end());
    m_size = other.size();

    return *this;
}

inline bool typelessPointersAreEqual(const void* a, const void* b) { return a == b; }

template <typename T, size_t inlineCapacity, typename Allocator>
template <size_t otherCapacity>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::operator=(const Vector<T, otherCapacity, Allocator>& other)
{
    // If the inline capacities match, we should call the more specific
    // template.  If the inline capacities don't match, the two objects
    // shouldn't be allocated the same address.
    ASSERT(!typelessPointersAreEqual(&other, this));

    if (size() > other.size()) {
        shrink(other.size());
    } else if (other.size() > capacity()) {
        clear();
        reserveCapacity(other.size());
        ASSERT(begin());
    }

    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, other.size());
    std::copy(other.begin(), other.begin() + size(), begin());
    TypeOperations::uninitializedCopy(other.begin() + size(), other.end(), end());
    m_size = other.size();

    return *this;
}

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>::Vector(Vector<T, inlineCapacity, Allocator>&& other)
{
    m_size = 0;
    // It's a little weird to implement a move constructor using swap but this
    // way we don't have to add a move constructor to VectorBuffer.
    swap(other);
}

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::operator=(Vector<T, inlineCapacity, Allocator>&& other)
{
    swap(other);
    return *this;
}

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>::Vector(std::initializer_list<T> elements)
    : Base(elements.size())
{
    ANNOTATE_NEW_BUFFER(begin(), capacity(), elements.size());
    m_size = elements.size();
    TypeOperations::uninitializedCopy(elements.begin(), elements.end(), begin());
}

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::operator=(std::initializer_list<T> elements)
{
    if (size() > elements.size()) {
        shrink(elements.size());
    } else if (elements.size() > capacity()) {
        clear();
        reserveCapacity(elements.size());
        DCHECK(begin());
    }

    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, elements.size());
    std::copy(elements.begin(), elements.begin() + m_size, begin());
    TypeOperations::uninitializedCopy(elements.begin() + m_size, elements.end(), end());
    m_size = elements.size();

    return *this;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
bool Vector<T, inlineCapacity, Allocator>::contains(const U& value) const
{
    return find(value) != kNotFound;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
size_t Vector<T, inlineCapacity, Allocator>::find(const U& value) const
{
    const T* b = begin();
    const T* e = end();
    for (const T* iter = b; iter < e; ++iter) {
        if (TypeOperations::compareElement(*iter, value))
            return iter - b;
    }
    return kNotFound;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
size_t Vector<T, inlineCapacity, Allocator>::reverseFind(const U& value) const
{
    const T* b = begin();
    const T* iter = end();
    while (iter > b) {
        --iter;
        if (TypeOperations::compareElement(*iter, value))
            return iter - b;
    }
    return kNotFound;
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::fill(const T& val, size_t newSize)
{
    if (size() > newSize) {
        shrink(newSize);
    } else if (newSize > capacity()) {
        clear();
        reserveCapacity(newSize);
        ASSERT(begin());
    }

    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, newSize);
    std::fill(begin(), end(), val);
    TypeOperations::uninitializedFill(end(), begin() + newSize, val);
    m_size = newSize;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename Iterator>
void Vector<T, inlineCapacity, Allocator>::appendRange(Iterator start, Iterator end)
{
    for (Iterator it = start; it != end; ++it)
        append(*it);
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::expandCapacity(size_t newMinCapacity)
{
    size_t oldCapacity = capacity();
    size_t expandedCapacity = oldCapacity;
    // We use a more aggressive expansion strategy for Vectors with inline
    // storage.  This is because they are more likely to be on the stack, so the
    // risk of heap bloat is minimized.  Furthermore, exceeding the inline
    // capacity limit is not supposed to happen in the common case and may
    // indicate a pathological condition or microbenchmark.
    if (INLINE_CAPACITY) {
        expandedCapacity *= 2;
        // Check for integer overflow, which could happen in the 32-bit build.
        RELEASE_ASSERT(expandedCapacity > oldCapacity);
    } else {
        // This cannot integer overflow.
        // On 64-bit, the "expanded" integer is 32-bit, and any encroachment
        // above 2^32 will fail allocation in allocateBuffer().  On 32-bit,
        // there's not enough address space to hold the old and new buffers.  In
        // addition, our underlying allocator is supposed to always fail on >
        // (2^31 - 1) allocations.
        expandedCapacity += (expandedCapacity / 4) + 1;
    }
    reserveCapacity(std::max(newMinCapacity, std::max(static_cast<size_t>(kInitialVectorSize), expandedCapacity)));
}

template <typename T, size_t inlineCapacity, typename Allocator>
T* Vector<T, inlineCapacity, Allocator>::expandCapacity(size_t newMinCapacity, T* ptr)
{
    if (ptr < begin() || ptr >= end()) {
        expandCapacity(newMinCapacity);
        return ptr;
    }
    size_t index = ptr - begin();
    expandCapacity(newMinCapacity);
    return begin() + index;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
inline U* Vector<T, inlineCapacity, Allocator>::expandCapacity(size_t newMinCapacity, U* ptr)
{
    expandCapacity(newMinCapacity);
    return ptr;
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::resize(size_t size)
{
    if (size <= m_size) {
        TypeOperations::destruct(begin() + size, end());
        clearUnusedSlots(begin() + size, end());
        ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, size);
    } else {
        if (size > capacity())
            expandCapacity(size);
        ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, size);
        TypeOperations::initialize(end(), begin() + size);
    }

    m_size = size;
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::shrink(size_t size)
{
    ASSERT(size <= m_size);
    TypeOperations::destruct(begin() + size, end());
    clearUnusedSlots(begin() + size, end());
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, size);
    m_size = size;
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::grow(size_t size)
{
    ASSERT(size >= m_size);
    if (size > capacity())
        expandCapacity(size);
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, size);
    TypeOperations::initialize(end(), begin() + size);
    m_size = size;
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::reserveCapacity(size_t newCapacity)
{
    if (UNLIKELY(newCapacity <= capacity()))
        return;
    T* oldBuffer = begin();
    if (!oldBuffer) {
        Base::allocateBuffer(newCapacity);
        return;
    }
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    size_t oldCapacity = capacity();
#endif
    // The Allocator::isGarbageCollected check is not needed.  The check is just
    // a static hint for a compiler to indicate that Base::expandBuffer returns
    // false if Allocator is a PartitionAllocator.
    if (Allocator::isGarbageCollected && Base::expandBuffer(newCapacity)) {
        ANNOTATE_CHANGE_CAPACITY(begin(), oldCapacity, m_size, capacity());
        return;
    }
    T* oldEnd = end();
    Base::allocateExpandedBuffer(newCapacity);
    ANNOTATE_NEW_BUFFER(begin(), capacity(), m_size);
    TypeOperations::move(oldBuffer, oldEnd, begin());
    clearUnusedSlots(oldBuffer, oldEnd);
    ANNOTATE_DELETE_BUFFER(oldBuffer, oldCapacity, m_size);
    Base::deallocateBuffer(oldBuffer);
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::reserveInitialCapacity(size_t initialCapacity)
{
    ASSERT(!m_size);
    ASSERT(capacity() == INLINE_CAPACITY);
    if (initialCapacity > INLINE_CAPACITY) {
        ANNOTATE_DELETE_BUFFER(begin(), capacity(), m_size);
        Base::allocateBuffer(initialCapacity);
        ANNOTATE_NEW_BUFFER(begin(), capacity(), m_size);
    }
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::shrinkCapacity(size_t newCapacity)
{
    if (newCapacity >= capacity())
        return;

    if (newCapacity < size())
        shrink(newCapacity);

    T* oldBuffer = begin();
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    size_t oldCapacity = capacity();
#endif
    if (newCapacity > 0) {
        if (Base::shrinkBuffer(newCapacity)) {
            ANNOTATE_CHANGE_CAPACITY(begin(), oldCapacity, m_size, capacity());
            return;
        }

        T* oldEnd = end();
        Base::allocateBuffer(newCapacity);
        if (begin() != oldBuffer) {
            ANNOTATE_NEW_BUFFER(begin(), capacity(), m_size);
            TypeOperations::move(oldBuffer, oldEnd, begin());
            clearUnusedSlots(oldBuffer, oldEnd);
            ANNOTATE_DELETE_BUFFER(oldBuffer, oldCapacity, m_size);
        }
    } else {
        Base::resetBufferPointer();
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
        if (oldBuffer != begin()) {
            ANNOTATE_NEW_BUFFER(begin(), capacity(), m_size);
            ANNOTATE_DELETE_BUFFER(oldBuffer, oldCapacity, m_size);
        }
#endif
    }

    Base::deallocateBuffer(oldBuffer);
}

// Templatizing these is better than just letting the conversion happen
// implicitly, because for instance it allows a PassRefPtr to be appended to a
// RefPtr vector without refcount thrash.

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
void Vector<T, inlineCapacity, Allocator>::append(const U* data, size_t dataSize)
{
    ASSERT(Allocator::isAllocationAllowed());
    size_t newSize = m_size + dataSize;
    if (newSize > capacity()) {
        data = expandCapacity(newSize, data);
        ASSERT(begin());
    }
    RELEASE_ASSERT(newSize >= m_size);
    T* dest = end();
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, newSize);
    VectorCopier<VectorTraits<T>::canCopyWithMemcpy, T>::uninitializedCopy(data, &data[dataSize], dest);
    m_size = newSize;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
ALWAYS_INLINE void Vector<T, inlineCapacity, Allocator>::append(U&& val)
{
    ASSERT(Allocator::isAllocationAllowed());
    if (LIKELY(size() != capacity())) {
        ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size + 1);
        new (NotNull, end()) T(std::forward<U>(val));
        ++m_size;
        return;
    }

    appendSlowCase(std::forward<U>(val));
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
NEVER_INLINE void Vector<T, inlineCapacity, Allocator>::appendSlowCase(U&& val)
{
    ASSERT(size() == capacity());

    typename std::remove_reference<U>::type* ptr = &val;
    ptr = expandCapacity(size() + 1, ptr);
    ASSERT(begin());

    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size + 1);
    new (NotNull, end()) T(std::forward<U>(*ptr));
    ++m_size;
}

// This version of append saves a branch in the case where you know that the
// vector's capacity is large enough for the append to succeed.

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
ALWAYS_INLINE void Vector<T, inlineCapacity, Allocator>::uncheckedAppend(U&& val)
{
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    // Vectors in ASAN builds don't have inlineCapacity.
    append(std::forward<U>(val));
#else
    ASSERT(size() < capacity());
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size + 1);
    new (NotNull, end()) T(std::forward<U>(val));
    ++m_size;
#endif
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U, size_t otherCapacity, typename OtherAllocator>
inline void Vector<T, inlineCapacity, Allocator>::appendVector(const Vector<U, otherCapacity, OtherAllocator>& val)
{
    append(val.begin(), val.size());
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
void Vector<T, inlineCapacity, Allocator>::insert(size_t position, const U* data, size_t dataSize)
{
    ASSERT(Allocator::isAllocationAllowed());
    RELEASE_ASSERT(position <= size());
    size_t newSize = m_size + dataSize;
    if (newSize > capacity()) {
        data = expandCapacity(newSize, data);
        ASSERT(begin());
    }
    RELEASE_ASSERT(newSize >= m_size);
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, newSize);
    T* spot = begin() + position;
    TypeOperations::moveOverlapping(spot, end(), spot + dataSize);
    VectorCopier<VectorTraits<T>::canCopyWithMemcpy, T>::uninitializedCopy(data, &data[dataSize], spot);
    m_size = newSize;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
inline void Vector<T, inlineCapacity, Allocator>::insert(size_t position, U&& val)
{
    ASSERT(Allocator::isAllocationAllowed());
    RELEASE_ASSERT(position <= size());
    typename std::remove_reference<U>::type* data = &val;
    if (size() == capacity()) {
        data = expandCapacity(size() + 1, data);
        ASSERT(begin());
    }
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size + 1);
    T* spot = begin() + position;
    TypeOperations::moveOverlapping(spot, end(), spot + 1);
    new (NotNull, spot) T(std::forward<U>(*data));
    ++m_size;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U, size_t c, typename OtherAllocator>
inline void Vector<T, inlineCapacity, Allocator>::insert(size_t position, const Vector<U, c, OtherAllocator>& val)
{
    insert(position, val.begin(), val.size());
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
void Vector<T, inlineCapacity, Allocator>::prepend(const U* data, size_t dataSize)
{
    insert(0, data, dataSize);
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
inline void Vector<T, inlineCapacity, Allocator>::prepend(U&& val)
{
    insert(0, std::forward<U>(val));
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U, size_t c, typename V>
inline void Vector<T, inlineCapacity, Allocator>::prependVector(const Vector<U, c, V>& val)
{
    insert(0, val.begin(), val.size());
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::remove(size_t position)
{
    RELEASE_ASSERT(position < size());
    T* spot = begin() + position;
    spot->~T();
    TypeOperations::moveOverlapping(spot + 1, end(), spot);
    clearUnusedSlots(end() - 1, end());
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size - 1);
    --m_size;
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::remove(size_t position, size_t length)
{
    ASSERT_WITH_SECURITY_IMPLICATION(position <= size());
    if (!length)
        return;
    RELEASE_ASSERT(position + length <= size());
    T* beginSpot = begin() + position;
    T* endSpot = beginSpot + length;
    TypeOperations::destruct(beginSpot, endSpot);
    TypeOperations::moveOverlapping(endSpot, end(), beginSpot);
    clearUnusedSlots(end() - length, end());
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size - length);
    m_size -= length;
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::reverse()
{
    for (size_t i = 0; i < m_size / 2; ++i)
        std::swap(at(i), at(m_size - 1 - i));
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void swap(Vector<T, inlineCapacity, Allocator>& a, Vector<T, inlineCapacity, Allocator>& b)
{
    a.swap(b);
}

template <typename T, size_t inlineCapacityA, size_t inlineCapacityB, typename Allocator>
bool operator==(const Vector<T, inlineCapacityA, Allocator>& a, const Vector<T, inlineCapacityB, Allocator>& b)
{
    if (a.size() != b.size())
        return false;
    if (a.isEmpty())
        return true;
    return VectorTypeOperations<T>::compare(a.data(), b.data(), a.size());
}

template <typename T, size_t inlineCapacityA, size_t inlineCapacityB, typename Allocator>
inline bool operator!=(const Vector<T, inlineCapacityA, Allocator>& a, const Vector<T, inlineCapacityB, Allocator>& b)
{
    return !(a == b);
}

// This is only called if the allocator is a HeapAllocator. It is used when
// visiting during a tracing GC.
template <typename T, size_t inlineCapacity, typename Allocator>
template <typename VisitorDispatcher>
void Vector<T, inlineCapacity, Allocator>::trace(VisitorDispatcher visitor)
{
    ASSERT(Allocator::isGarbageCollected); // Garbage collector must be enabled.
    if (!buffer())
        return;
    if (this->hasOutOfLineBuffer()) {
        // This is a performance optimization for a case where the buffer has
        // been already traced by somewhere. This can happen if the conservative
        // scanning traced an on-stack (false-positive or real) pointer to the
        // HeapVector, and then visitor->trace() traces the HeapVector.
        if (Allocator::isHeapObjectAlive(buffer()))
            return;
        Allocator::markNoTracing(visitor, buffer());
    }
    const T* bufferBegin = buffer();
    const T* bufferEnd = buffer() + size();
    if (IsTraceableInCollectionTrait<VectorTraits<T>>::value) {
        for (const T* bufferEntry = bufferBegin; bufferEntry != bufferEnd; bufferEntry++)
            Allocator::template trace<VisitorDispatcher, T, VectorTraits<T>>(visitor, *const_cast<T*>(bufferEntry));
        checkUnusedSlots(buffer() + size(), buffer() + capacity());
    }
}

} // namespace WTF

using WTF::Vector;

#endif // WTF_Vector_h
