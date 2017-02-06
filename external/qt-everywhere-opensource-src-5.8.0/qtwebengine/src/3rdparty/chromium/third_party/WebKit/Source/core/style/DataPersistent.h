// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DataPersistent_h
#define DataPersistent_h

#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

// DataPersistent<T> provides the copy-on-modify behavior of DataRef<>,
// but for Persistent<> heap references.
//
// That is, the DataPersistent<T> copy assignment, |a = b;|, makes |a|
// share a reference to the T object that |b| holds until |a| is mutated
// and access() on it is called. Or, dually, if |b| is mutated after the
// assignment that mutation isn't observable to |a| but will be performed
// on a copy of the underlying T object.
//
// DataPersistent<T> does assume that no one keeps non-DataPersistent<> shared
// references to the underlying T object that it manages, and is mutating the
// object via those.
template <typename T>
class DataPersistent {
    USING_FAST_MALLOC(DataPersistent);
public:
    DataPersistent()
        : m_ownCopy(false)
    {
    }

    DataPersistent(const DataPersistent& other)
        : m_ownCopy(false)
    {
        if (other.m_data)
            m_data = wrapUnique(new Persistent<T>(other.m_data->get()));

        // Invalidated, subsequent mutations will happen on a new copy.
        //
        // (Clearing |m_ownCopy| will not be observable over T, hence
        // the const_cast<> is considered acceptable here.)
        const_cast<DataPersistent&>(other).m_ownCopy = false;
    }

    const T* get() const { return m_data ? m_data->get() : nullptr; }

    const T& operator*() const { return m_data ? *get() : nullptr; }
    const T* operator->() const { return get(); }

    T* access()
    {
        if (m_data && !m_ownCopy) {
            *m_data = (*m_data)->copy();
            m_ownCopy = true;
        }
        return m_data ? m_data->get() : nullptr;
    }

    void init()
    {
        ASSERT(!m_data);
        m_data = wrapUnique(new Persistent<T>(T::create()));
        m_ownCopy = true;
    }

    bool operator==(const DataPersistent<T>& o) const
    {
        ASSERT(m_data);
        ASSERT(o.m_data);
        return m_data->get() == o.m_data->get() || *m_data->get() == *o.m_data->get();
    }

    bool operator!=(const DataPersistent<T>& o) const
    {
        ASSERT(m_data);
        ASSERT(o.m_data);
        return m_data->get() != o.m_data->get() && *m_data->get() != *o.m_data->get();
    }

    void operator=(std::nullptr_t) { m_data.clear(); }
private:
    // Reduce size of DataPersistent<> by delaying creation of Persistent<>.
    std::unique_ptr<Persistent<T>> m_data;
    unsigned m_ownCopy:1;
};

} // namespace blink

#endif // DataPersistent_h
