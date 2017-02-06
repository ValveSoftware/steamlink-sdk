// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CallbackStack_h
#define CallbackStack_h

#include "platform/heap/BlinkGC.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"

namespace blink {

// The CallbackStack contains all the visitor callbacks used to trace and mark
// objects. A specific CallbackStack instance contains at most bufferSize elements.
// If more space is needed a new CallbackStack instance is created and chained
// together with the former instance. I.e. a logical CallbackStack can be made of
// multiple chained CallbackStack object instances.
class CallbackStack final {
    USING_FAST_MALLOC(CallbackStack);
public:
    class Item {
        DISALLOW_NEW();
    public:
        Item() { }
        Item(void* object, VisitorCallback callback)
            : m_object(object)
            , m_callback(callback)
        {
        }
        void* object() { return m_object; }
        VisitorCallback callback() { return m_callback; }
        void call(Visitor* visitor) { m_callback(visitor, m_object); }

    private:
        void* m_object;
        VisitorCallback m_callback;
    };

    explicit CallbackStack(size_t blockSize = kDefaultBlockSize);
    ~CallbackStack();

    void clear();
    void decommit();

    Item* allocateEntry();
    Item* pop();

    bool isEmpty() const;

    void invokeEphemeronCallbacks(Visitor*);

#if ENABLE(ASSERT)
    bool hasCallbackForObject(const void*);
#endif

    static const size_t kMinimalBlockSize;
    static const size_t kDefaultBlockSize = (1 << 13);

private:
    class Block {
        USING_FAST_MALLOC(Block);
    public:
        Block(Block* next, size_t blockSize);
        ~Block();

#if ENABLE(ASSERT)
        void clear();
#endif
        void reset();
        void decommit();

        Block* next() const { return m_next; }
        void setNext(Block* next) { m_next = next; }

        bool isEmptyBlock() const
        {
            return m_current == &(m_buffer[0]);
        }

        size_t blockSize() const { return m_blockSize; }

        Item* allocateEntry()
        {
            if (LIKELY(m_current < m_limit))
                return m_current++;
            return nullptr;
        }

        Item* pop()
        {
            if (UNLIKELY(isEmptyBlock()))
                return nullptr;
            return --m_current;
        }

        void invokeEphemeronCallbacks(Visitor*);

#if ENABLE(ASSERT)
        bool hasCallbackForObject(const void*);
#endif

    private:
        size_t m_blockSize;

        Item* m_buffer;
        Item* m_limit;
        Item* m_current;
        Block* m_next;
    };

    Item* popSlow();
    Item* allocateEntrySlow();
    void invokeOldestCallbacks(Block*, Block*, Visitor*);
    bool hasJustOneBlock() const;

    Block* m_first;
    Block* m_last;
};

ALWAYS_INLINE CallbackStack::Item* CallbackStack::allocateEntry()
{
    Item* item = m_first->allocateEntry();
    if (LIKELY(!!item))
        return item;

    return allocateEntrySlow();
}

ALWAYS_INLINE CallbackStack::Item* CallbackStack::pop()
{
    Item* item = m_first->pop();
    if (LIKELY(!!item))
        return item;

    return popSlow();
}

} // namespace blink

#endif // CallbackStack_h
