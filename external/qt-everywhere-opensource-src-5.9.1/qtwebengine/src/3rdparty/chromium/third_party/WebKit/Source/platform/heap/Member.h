// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Member_h
#define Member_h

#include "wtf/Allocator.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashTraits.h"

namespace blink {

template <typename T>
class Persistent;
template <typename T>
class TraceWrapperMember;

enum class TracenessMemberConfiguration {
  Traced,
  Untraced,
};

template <typename T,
          TracenessMemberConfiguration tracenessConfiguration =
              TracenessMemberConfiguration::Traced>
class MemberBase {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  MemberBase() : m_raw(nullptr) { saveCreationThreadState(); }

  MemberBase(std::nullptr_t) : m_raw(nullptr) { saveCreationThreadState(); }

  MemberBase(T* raw) : m_raw(raw) {
    saveCreationThreadState();
    checkPointer();
  }

  explicit MemberBase(T& raw) : m_raw(&raw) {
    saveCreationThreadState();
    checkPointer();
  }

  MemberBase(WTF::HashTableDeletedValueType) : m_raw(reinterpret_cast<T*>(-1)) {
    saveCreationThreadState();
  }

  bool isHashTableDeletedValue() const {
    return m_raw == reinterpret_cast<T*>(-1);
  }

  MemberBase(const MemberBase& other) : m_raw(other) {
    saveCreationThreadState();
    checkPointer();
  }

  template <typename U>
  MemberBase(const Persistent<U>& other) {
    saveCreationThreadState();
    m_raw = other;
    checkPointer();
  }

  template <typename U>
  MemberBase(const MemberBase<U>& other) : m_raw(other) {
    saveCreationThreadState();
    checkPointer();
  }

  T* release() {
    T* result = m_raw;
    m_raw = nullptr;
    return result;
  }

  explicit operator bool() const { return m_raw; }

  operator T*() const { return m_raw; }

  T* operator->() const { return m_raw; }
  T& operator*() const { return *m_raw; }

  template <typename U>
  MemberBase& operator=(const Persistent<U>& other) {
    m_raw = other;
    checkPointer();
    return *this;
  }

  template <typename U>
  MemberBase& operator=(const MemberBase<U>& other) {
    m_raw = other;
    checkPointer();
    return *this;
  }

  template <typename U>
  MemberBase& operator=(U* other) {
    m_raw = other;
    checkPointer();
    return *this;
  }

  MemberBase& operator=(std::nullptr_t) {
    m_raw = nullptr;
    return *this;
  }

  void swap(MemberBase<T>& other) {
    std::swap(m_raw, other.m_raw);
    checkPointer();
  }

  T* get() const { return m_raw; }

  void clear() { m_raw = nullptr; }

 protected:
  void checkPointer() {
#if DCHECK_IS_ON()
    if (!m_raw)
      return;
    // HashTable can store a special value (which is not aligned to the
    // allocation granularity) to Member<> to represent a deleted entry.
    // Thus we treat a pointer that is not aligned to the granularity
    // as a valid pointer.
    if (reinterpret_cast<intptr_t>(m_raw) % allocationGranularity)
      return;

    if (tracenessConfiguration != TracenessMemberConfiguration::Untraced) {
      ThreadState* current = ThreadState::current();
      DCHECK(current);
      // m_creationThreadState may be null when this is used in a heap
      // collection which initialized the Member with memset and the
      // constructor wasn't called.
      if (m_creationThreadState) {
        // Member should point to objects that belong in the same ThreadHeap.
        DCHECK_EQ(&ThreadState::fromObject(m_raw)->heap(),
                  &m_creationThreadState->heap());
        // Member should point to objects that belong in the same ThreadHeap.
        DCHECK_EQ(&current->heap(), &m_creationThreadState->heap());
      } else {
        DCHECK_EQ(&ThreadState::fromObject(m_raw)->heap(), &current->heap());
      }
    }

#if defined(ADDRESS_SANITIZER)
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
#endif
  }

  void saveCreationThreadState() {
#if DCHECK_IS_ON()
    if (tracenessConfiguration == TracenessMemberConfiguration::Untraced) {
      m_creationThreadState = nullptr;
    } else {
      m_creationThreadState = ThreadState::current();
      // Members should be created in an attached thread. But an empty
      // value Member may be created on an unattached thread by a heap
      // collection iterator.
      DCHECK(m_creationThreadState || !m_raw);
    }
#endif
  }

  T* m_raw;
#if DCHECK_IS_ON()
  const ThreadState* m_creationThreadState;
#endif

  template <bool x,
            WTF::WeakHandlingFlag y,
            WTF::ShouldWeakPointersBeMarkedStrongly z,
            typename U,
            typename V>
  friend struct CollectionBackingTraceTrait;
  friend class Visitor;
};

// Members are used in classes to contain strong pointers to other oilpan heap
// allocated objects.
// All Member fields of a class must be traced in the class' trace method.
// During the mark phase of the GC all live objects are marked as live and
// all Member fields of a live object will be traced marked as live as well.
template <typename T>
class Member : public MemberBase<T, TracenessMemberConfiguration::Traced> {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  typedef MemberBase<T, TracenessMemberConfiguration::Traced> Parent;

 public:
  Member() : Parent() {}
  Member(std::nullptr_t) : Parent(nullptr) {}
  Member(T* raw) : Parent(raw) {}
  Member(T& raw) : Parent(raw) {}
  Member(WTF::HashTableDeletedValueType x) : Parent(x) {}

  Member(const Member& other) : Parent(other) {}
  template <typename U>
  Member(const Member<U>& other) : Parent(other) {}
  template <typename U>
  Member(const Persistent<U>& other) : Parent(other) {}

  template <typename U>
  Member& operator=(const Persistent<U>& other) {
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  Member& operator=(const Member<U>& other) {
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  Member& operator=(const WeakMember<U>& other) {
    Parent::operator=(other);
    return *this;
  }

  template <typename U>
  Member& operator=(U* other) {
    Parent::operator=(other);
    return *this;
  }

  Member& operator=(std::nullptr_t) {
    Parent::operator=(nullptr);
    return *this;
  }

 protected:
  template <bool x,
            WTF::WeakHandlingFlag y,
            WTF::ShouldWeakPointersBeMarkedStrongly z,
            typename U,
            typename V>
  friend struct CollectionBackingTraceTrait;
  friend class Visitor;
};

// WeakMember is similar to Member in that it is used to point to other oilpan
// heap allocated objects.
// However instead of creating a strong pointer to the object, the WeakMember
// creates a weak pointer, which does not keep the pointee alive. Hence if all
// pointers to to a heap allocated object are weak the object will be garbage
// collected. At the time of GC the weak pointers will automatically be set to
// null.
template <typename T>
class WeakMember : public MemberBase<T, TracenessMemberConfiguration::Traced> {
  typedef MemberBase<T, TracenessMemberConfiguration::Traced> Parent;

 public:
  WeakMember() : Parent() {}

  WeakMember(std::nullptr_t) : Parent(nullptr) {}

  WeakMember(T* raw) : Parent(raw) {}

  WeakMember(WTF::HashTableDeletedValueType x) : Parent(x) {}

  template <typename U>
  WeakMember(const Persistent<U>& other) : Parent(other) {}

  template <typename U>
  WeakMember(const Member<U>& other) : Parent(other) {}

  template <typename U>
  WeakMember& operator=(const Persistent<U>& other) {
    this->m_raw = other;
    this->checkPointer();
    return *this;
  }

  template <typename U>
  WeakMember& operator=(const Member<U>& other) {
    this->m_raw = other;
    this->checkPointer();
    return *this;
  }

  template <typename U>
  WeakMember& operator=(U* other) {
    this->m_raw = other;
    this->checkPointer();
    return *this;
  }

  WeakMember& operator=(std::nullptr_t) {
    this->m_raw = nullptr;
    return *this;
  }

 private:
  T** cell() const { return const_cast<T**>(&this->m_raw); }

  template <typename Derived>
  friend class VisitorHelper;
};

// UntracedMember is a pointer to an on-heap object that is not traced for some
// reason. Please don't use this unless you understand what you're doing.
// Basically, all pointers to on-heap objects must be stored in either of
// Persistent, Member or WeakMember. It is not allowed to leave raw pointers to
// on-heap objects. However, there can be scenarios where you have to use raw
// pointers for some reason, and in that case you can use UntracedMember. Of
// course, it must be guaranteed that the pointing on-heap object is kept alive
// while the raw pointer is pointing to the object.
template <typename T>
class UntracedMember final
    : public MemberBase<T, TracenessMemberConfiguration::Untraced> {
  typedef MemberBase<T, TracenessMemberConfiguration::Untraced> Parent;

 public:
  UntracedMember() : Parent() {}

  UntracedMember(std::nullptr_t) : Parent(nullptr) {}

  UntracedMember(T* raw) : Parent(raw) {}

  template <typename U>
  UntracedMember(const Persistent<U>& other) : Parent(other) {}

  template <typename U>
  UntracedMember(const Member<U>& other) : Parent(other) {}

  UntracedMember(WTF::HashTableDeletedValueType x) : Parent(x) {}

  template <typename U>
  UntracedMember& operator=(const Persistent<U>& other) {
    this->m_raw = other;
    this->checkPointer();
    return *this;
  }

  template <typename U>
  UntracedMember& operator=(const Member<U>& other) {
    this->m_raw = other;
    this->checkPointer();
    return *this;
  }

  template <typename U>
  UntracedMember& operator=(U* other) {
    this->m_raw = other;
    this->checkPointer();
    return *this;
  }

  UntracedMember& operator=(std::nullptr_t) {
    this->m_raw = nullptr;
    return *this;
  }
};

}  // namespace blink

namespace WTF {

// PtrHash is the default hash for hash tables with Member<>-derived elements.
template <typename T>
struct MemberHash : PtrHash<T> {
  STATIC_ONLY(MemberHash);
  template <typename U>
  static unsigned hash(const U& key) {
    return PtrHash<T>::hash(key);
  }
  template <typename U, typename V>
  static bool equal(const U& a, const V& b) {
    return a == b;
  }
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

template <typename T>
struct DefaultHash<blink::TraceWrapperMember<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct IsTraceable<blink::Member<T>> {
  STATIC_ONLY(IsTraceable);
  static const bool value = true;
};

template <typename T>
struct IsWeak<blink::WeakMember<T>> {
  STATIC_ONLY(IsWeak);
  static const bool value = true;
};

template <typename T>
struct IsTraceable<blink::WeakMember<T>> {
  STATIC_ONLY(IsTraceable);
  static const bool value = true;
};

template <typename T>
struct IsTraceable<blink::TraceWrapperMember<T>> {
  STATIC_ONLY(IsTraceable);
  static const bool value = true;
};

}  // namespace WTF

#endif  // Member_h
