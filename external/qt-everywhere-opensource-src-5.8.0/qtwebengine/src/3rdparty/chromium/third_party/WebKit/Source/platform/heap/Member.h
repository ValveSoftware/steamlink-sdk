// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Member_h
#define Member_h

#include "wtf/Allocator.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashTraits.h"

namespace blink {

template<typename T> class Persistent;

// Members are used in classes to contain strong pointers to other oilpan heap
// allocated objects.
// All Member fields of a class must be traced in the class' trace method.
// During the mark phase of the GC all live objects are marked as live and
// all Member fields of a live object will be traced marked as live as well.
template<typename T>
class Member {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    Member() : m_raw(nullptr)
    {
    }

    Member(std::nullptr_t) : m_raw(nullptr)
    {
    }

    Member(T* raw) : m_raw(raw)
    {
        checkPointer();
    }

    explicit Member(T& raw) : m_raw(&raw)
    {
        checkPointer();
    }

    Member(WTF::HashTableDeletedValueType) : m_raw(reinterpret_cast<T*>(-1))
    {
    }

    bool isHashTableDeletedValue() const { return m_raw == reinterpret_cast<T*>(-1); }

    Member(const Member& other) : m_raw(other)
    {
        checkPointer();
    }

    template<typename U>
    Member(const Persistent<U>& other)
    {
        m_raw = other;
        checkPointer();
    }

    template<typename U>
    Member(const Member<U>& other) : m_raw(other)
    {
        checkPointer();
    }

    T* release()
    {
        T* result = m_raw;
        m_raw = nullptr;
        return result;
    }

    explicit operator bool() const { return m_raw; }

    operator T*() const { return m_raw; }

    T* operator->() const { return m_raw; }
    T& operator*() const { return *m_raw; }

    template<typename U>
    Member& operator=(const Persistent<U>& other)
    {
        m_raw = other;
        checkPointer();
        return *this;
    }

    template<typename U>
    Member& operator=(const Member<U>& other)
    {
        m_raw = other;
        checkPointer();
        return *this;
    }

    template<typename U>
    Member& operator=(U* other)
    {
        m_raw = other;
        checkPointer();
        return *this;
    }

    Member& operator=(std::nullptr_t)
    {
        m_raw = nullptr;
        return *this;
    }

    void swap(Member<T>& other)
    {
        std::swap(m_raw, other.m_raw);
        checkPointer();
    }

    T* get() const { return m_raw; }

    void clear() { m_raw = nullptr; }


protected:
    void checkPointer()
    {
#if ENABLE(ASSERT) && defined(ADDRESS_SANITIZER)
        if (!m_raw)
            return;
        // HashTable can store a special value (which is not aligned to the
        // allocation granularity) to Member<> to represent a deleted entry.
        // Thus we treat a pointer that is not aligned to the granularity
        // as a valid pointer.
        if (reinterpret_cast<intptr_t>(m_raw) % allocationGranularity)
            return;

        // TODO(haraken): What we really want to check here is that the pointer
        // is a traceable object. In other words, the pointer is either of:
        //
        //   (a) a pointer to the head of an on-heap object.
        //   (b) a pointer to the head of an on-heap mixin object.
        //
        // We can check it by calling ThreadHeap::isHeapObjectAlive(m_raw),
        // but we cannot call it here because it requires to include T.h.
        // So we currently only try to implement the check for (a), but do
        // not insist that T's definition is in scope.
        if (IsFullyDefined<T>::value && !IsGarbageCollectedMixin<T>::value)
            ASSERT(HeapObjectHeader::fromPayload(m_raw)->checkHeader());
#endif
    }

    T* m_raw;

    template<bool x, WTF::WeakHandlingFlag y, WTF::ShouldWeakPointersBeMarkedStrongly z, typename U, typename V> friend struct CollectionBackingTraceTrait;
    friend class Visitor;

};

// WeakMember is similar to Member in that it is used to point to other oilpan
// heap allocated objects.
// However instead of creating a strong pointer to the object, the WeakMember creates
// a weak pointer, which does not keep the pointee alive. Hence if all pointers to
// to a heap allocated object are weak the object will be garbage collected. At the
// time of GC the weak pointers will automatically be set to null.
template<typename T>
class WeakMember : public Member<T> {
public:
    WeakMember() : Member<T>() { }

    WeakMember(std::nullptr_t) : Member<T>(nullptr) { }

    WeakMember(T* raw) : Member<T>(raw) { }

    WeakMember(WTF::HashTableDeletedValueType x) : Member<T>(x) { }

    template<typename U>
    WeakMember(const Persistent<U>& other) : Member<T>(other) { }

    template<typename U>
    WeakMember(const Member<U>& other) : Member<T>(other) { }

    template<typename U>
    WeakMember& operator=(const Persistent<U>& other)
    {
        this->m_raw = other;
        this->checkPointer();
        return *this;
    }

    template<typename U>
    WeakMember& operator=(const Member<U>& other)
    {
        this->m_raw = other;
        this->checkPointer();
        return *this;
    }

    template<typename U>
    WeakMember& operator=(U* other)
    {
        this->m_raw = other;
        this->checkPointer();
        return *this;
    }

    WeakMember& operator=(std::nullptr_t)
    {
        this->m_raw = nullptr;
        return *this;
    }

private:
    T** cell() const { return const_cast<T**>(&this->m_raw); }

    template<typename Derived> friend class VisitorHelper;
};

// UntracedMember is a pointer to an on-heap object that is not traced for some
// reason. Please don't use this unless you understand what you're doing.
// Basically, all pointers to on-heap objects must be stored in either of
// Persistent, Member or WeakMember. It is not allowed to leave raw pointers to
// on-heap objects. However, there can be scenarios where you have to use raw
// pointers for some reason, and in that case you can use UntracedMember. Of
// course, it must be guaranteed that the pointing on-heap object is kept alive
// while the raw pointer is pointing to the object.
template<typename T>
class UntracedMember final : public Member<T> {
public:
    UntracedMember() : Member<T>() { }

    UntracedMember(std::nullptr_t) : Member<T>(nullptr) { }

    UntracedMember(T* raw) : Member<T>(raw) { }

    template<typename U>
    UntracedMember(const Persistent<U>& other) : Member<T>(other) { }

    template<typename U>
    UntracedMember(const Member<U>& other) : Member<T>(other) { }

    UntracedMember(WTF::HashTableDeletedValueType x) : Member<T>(x) { }

    template<typename U>
    UntracedMember& operator=(const Persistent<U>& other)
    {
        this->m_raw = other;
        this->checkPointer();
        return *this;
    }

    template<typename U>
    UntracedMember& operator=(const Member<U>& other)
    {
        this->m_raw = other;
        this->checkPointer();
        return *this;
    }

    template<typename U>
    UntracedMember& operator=(U* other)
    {
        this->m_raw = other;
        this->checkPointer();
        return *this;
    }

    UntracedMember& operator=(std::nullptr_t)
    {
        this->m_raw = nullptr;
        return *this;
    }
};

} // namespace blink

namespace WTF {

// PtrHash is the default hash for hash tables with Member<>-derived elements.
template <typename T>
struct MemberHash : PtrHash<T> {
    STATIC_ONLY(MemberHash);
    template <typename U>
    static unsigned hash(const U& key) { return PtrHash<T>::hash(key); }
    template <typename U, typename V>
    static bool equal(const U& a, const V& b) { return a == b; }
};

template <typename T>
struct DefaultHash<blink::Member<T>> {
    STATIC_ONLY(DefaultHash);
    using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::WeakMember<T>> {
    STATIC_ONLY(DefaultHash);
    using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::UntracedMember<T>> {
    STATIC_ONLY(DefaultHash);
    using Hash = MemberHash<T>;
};

template<typename T>
struct IsTraceable<blink::Member<T>> {
    STATIC_ONLY(IsTraceable);
    static const bool value = true;
};

template<typename T>
struct IsWeak<blink::WeakMember<T>> {
    STATIC_ONLY(IsWeak);
    static const bool value = true;
};

template<typename T>
struct IsTraceable<blink::WeakMember<T>> {
    STATIC_ONLY(IsTraceable);
    static const bool value = true;
};

} // namespace WTF

#endif // Member_h
