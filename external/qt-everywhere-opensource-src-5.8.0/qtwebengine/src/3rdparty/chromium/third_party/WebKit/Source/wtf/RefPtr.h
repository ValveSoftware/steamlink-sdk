/*
 *  Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
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

// RefPtr and PassRefPtr are documented at http://webkit.org/coding/RefPtr.html

#ifndef WTF_RefPtr_h
#define WTF_RefPtr_h

#include "wtf/Allocator.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/PassRefPtr.h"
#include <algorithm>
#include <utility>

namespace WTF {

template <typename T> class PassRefPtr;
template <typename T> class RefPtrValuePeeker;

template <typename T> class RefPtr {
    USING_FAST_MALLOC(RefPtr);
public:
    ALWAYS_INLINE RefPtr() : m_ptr(nullptr) {}
    ALWAYS_INLINE RefPtr(std::nullptr_t) : m_ptr(nullptr) {}
    ALWAYS_INLINE RefPtr(T* ptr) : m_ptr(ptr) { refIfNotNull(ptr); }
    ALWAYS_INLINE explicit RefPtr(T& ref) : m_ptr(&ref) { m_ptr->ref(); }
    ALWAYS_INLINE RefPtr(const RefPtr& o) : m_ptr(o.m_ptr) { refIfNotNull(m_ptr); }
    template <typename U> RefPtr(const RefPtr<U>& o, EnsurePtrConvertibleArgDecl(U, T)) : m_ptr(o.get()) { refIfNotNull(m_ptr); }
    RefPtr(RefPtr&& o) : m_ptr(o.m_ptr) { o.m_ptr = nullptr; }

    // See comments in PassRefPtr.h for an explanation of why this takes a const
    // reference.
    template <typename U> RefPtr(const PassRefPtr<U>&, EnsurePtrConvertibleArgDecl(U, T));

    // Hash table deleted values, which are only constructed and never copied or
    // destroyed.
    RefPtr(HashTableDeletedValueType) : m_ptr(hashTableDeletedValue()) {}
    bool isHashTableDeletedValue() const { return m_ptr == hashTableDeletedValue(); }

    ALWAYS_INLINE ~RefPtr() { derefIfNotNull(m_ptr); }

    ALWAYS_INLINE T* get() const { return m_ptr; }

    void clear();
    PassRefPtr<T> release()
    {
        PassRefPtr<T> tmp = adoptRef(m_ptr);
        m_ptr = nullptr;
        return tmp;
    }

    T& operator*() const { return *m_ptr; }
    ALWAYS_INLINE T* operator->() const { return m_ptr; }

    bool operator!() const { return !m_ptr; }
    explicit operator bool() const { return m_ptr != nullptr; }

    RefPtr& operator=(RefPtr o) { swap(o); return *this; }
    RefPtr& operator=(std::nullptr_t) { clear(); return *this; }
    // This is required by HashMap<RefPtr>>.
    template <typename U> RefPtr& operator=(RefPtrValuePeeker<U>);

    void swap(RefPtr&);

    static T* hashTableDeletedValue() { return reinterpret_cast<T*>(-1); }

private:
    T* m_ptr;
};

template <typename T>
template <typename U> inline RefPtr<T>::RefPtr(const PassRefPtr<U>& o, EnsurePtrConvertibleArgDefn(U, T))
    : m_ptr(o.leakRef())
{
}

template <typename T> inline void RefPtr<T>::clear()
{
    T* ptr = m_ptr;
    m_ptr = nullptr;
    derefIfNotNull(ptr);
}

template <typename T>
template <typename U> inline RefPtr<T>& RefPtr<T>::operator=(RefPtrValuePeeker<U> optr)
{
    RefPtr ptr = static_cast<U*>(optr);
    swap(ptr);
    return *this;
}

template <class T> inline void RefPtr<T>::swap(RefPtr& o)
{
    std::swap(m_ptr, o.m_ptr);
}

template <class T> inline void swap(RefPtr<T>& a, RefPtr<T>& b)
{
    a.swap(b);
}

template <typename T, typename U> inline bool operator==(const RefPtr<T>& a, const RefPtr<U>& b)
{
    return a.get() == b.get();
}

template <typename T, typename U> inline bool operator==(const RefPtr<T>& a, U* b)
{
    return a.get() == b;
}

template <typename T, typename U> inline bool operator==(T* a, const RefPtr<U>& b)
{
    return a == b.get();
}

template <typename T> inline bool operator==(const RefPtr<T>& a, std::nullptr_t)
{
    return !a.get();
}

template <typename T> inline bool operator==(std::nullptr_t, const RefPtr<T>& b)
{
    return !b.get();
}

template <typename T, typename U> inline bool operator!=(const RefPtr<T>& a, const RefPtr<U>& b)
{
    return a.get() != b.get();
}

template <typename T, typename U> inline bool operator!=(const RefPtr<T>& a, U* b)
{
    return a.get() != b;
}

template <typename T, typename U> inline bool operator!=(T* a, const RefPtr<U>& b)
{
    return a != b.get();
}

template <typename T> inline bool operator!=(const RefPtr<T>& a, std::nullptr_t)
{
    return a.get();
}

template <typename T> inline bool operator!=(std::nullptr_t, const RefPtr<T>& b)
{
    return b.get();
}

template <typename T, typename U> inline RefPtr<T> static_pointer_cast(const RefPtr<U>& p)
{
    return RefPtr<T>(static_cast<T*>(p.get()));
}

template <typename T> inline T* getPtr(const RefPtr<T>& p)
{
    return p.get();
}

template <typename T> class RefPtrValuePeeker {
    DISALLOW_NEW();
public:
    ALWAYS_INLINE RefPtrValuePeeker(T* p): m_ptr(p) {}
    ALWAYS_INLINE RefPtrValuePeeker(std::nullptr_t): m_ptr(nullptr) {}
    template <typename U> RefPtrValuePeeker(const RefPtr<U>& p): m_ptr(p.get()) {}
    template <typename U> RefPtrValuePeeker(const PassRefPtr<U>& p): m_ptr(p.get()) {}

    ALWAYS_INLINE operator T*() const { return m_ptr; }
private:
    T* m_ptr;
};

} // namespace WTF

using WTF::RefPtr;
using WTF::static_pointer_cast;

#endif // WTF_RefPtr_h
