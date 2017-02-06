/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QT3DCORE_QABSTRACTRESOURCESMANAGER_H
#define QT3DCORE_QABSTRACTRESOURCESMANAGER_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGlobal>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QMutex>
#include <QHash>
#include <Qt3DCore/qt3dcore_global.h>
#include "qhandle_p.h"
#include "qhandlemanager_p.h"

// Silence complaints about unreferenced local variables in
// ArrayAllocatingPolicy::deallocateBuckets() when the compiler
// inlines the call to the dtor and it is empty. Default warning
// setting re-enabled at bottom of this file
#if defined(Q_CC_MSVC)
#pragma warning(disable : 4189)
#endif

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

template <class Host>
struct NonLockingPolicy
{
    struct ReadLocker
    {
        ReadLocker(const NonLockingPolicy*) {}
        void unlock() {}
        void relock() {}
    };

    struct WriteLocker
    {
        WriteLocker(const NonLockingPolicy*) {}
        void unlock() {}
        void relock() {}
    };

    struct Locker
    {
        Locker(const NonLockingPolicy*) {}
        void unlock() {}
        void relock() {}
    };
};

template <class Host>
class ObjectLevelLockingPolicy
{
public :
    ObjectLevelLockingPolicy()
    {}

    class ReadLocker
    {
    public:
        ReadLocker(const ObjectLevelLockingPolicy *host)
            : m_locker(&host->m_readWritelock)
        { }

        void unlock()
        {
            m_locker.unlock();
        }

        void relock()
        {
            m_locker.relock();
        }

    private:
        QReadLocker m_locker;
    };

    class WriteLocker
    {
    public:
        WriteLocker(const ObjectLevelLockingPolicy *host)
            : m_locker(&host->m_readWritelock)
        { }

        void unlock()
        {
            m_locker.unlock();
        }

        void relock()
        {
            m_locker.relock();
        }

    private:
        QWriteLocker m_locker;
    };

    class Locker
    {
    public:
        Locker(const ObjectLevelLockingPolicy *host)
            : m_locker(&host->m_lock)
        { }

        void unlock()
        {
            m_locker.unlock();
        }

        void relock()
        {
            m_locker.relock();
        }

    private:
        QMutexLocker m_locker;
    };

private:
    friend class Locker;
    friend class ReadLocker;
    friend class WriteLocker;
    mutable QReadWriteLock m_readWritelock;
    mutable QMutex m_lock;
};

template <typename T>
struct QResourceInfo
{
    enum
    {
        needsCleanup = false
    };
};

template <>
struct QResourceInfo<void>
{
    enum
    {
        needsCleanup = false
    };
};

enum
{
    Q_REQUIRES_CLEANUP = 0
};

#define Q_DECLARE_RESOURCE_INFO(TYPE, FLAGS) \
    namespace Qt3DCore { \
    template<> \
    struct QResourceInfo<TYPE > \
{ \
    enum \
{ \
    needsCleanup = ((FLAGS & Q_REQUIRES_CLEANUP) == 0) \
}; \
}; \
} // namespace Qt3DCore

template <int v>
struct Int2Type
{
    enum
    {
        value = v
    };
};

template <typename T, uint INDEXBITS>
class ArrayAllocatingPolicy
{
public:
    ArrayAllocatingPolicy()
        : m_numBuckets(0)
        , m_numConstructed(0)
    {
        reset();
    }

    ~ArrayAllocatingPolicy()
    {
        deallocateBuckets();
    }

    T* allocateResource()
    {
        Q_ASSERT(!m_freeList.isEmpty());
        int idx = m_freeList.takeLast();
        int bucketIdx = idx / BucketSize;
        int localIdx = idx % BucketSize;
        Q_ASSERT(bucketIdx <= m_numBuckets);
        if (bucketIdx == m_numBuckets) {
            m_bucketDataPtrs[bucketIdx] = static_cast<T*>(malloc(sizeof(T) * BucketSize));
            // ### memset is only needed as long as we also use this for primitive types (see FrameGraphManager)
            // ### remove once this is fixed, add a static_assert on T instead
            memset((void*) m_bucketDataPtrs[bucketIdx], 0, sizeof(T) * BucketSize);
            ++m_numBuckets;
        }

        Q_ASSERT(idx <= m_numConstructed);
        if (idx == m_numConstructed) {
            new (m_bucketDataPtrs[bucketIdx] + localIdx) T;
            ++m_numConstructed;
        }

        return m_bucketDataPtrs[bucketIdx] + localIdx;
    }

    void releaseResource(T *r)
    {
        // search linearly over buckets to find the index of the resource
        // and put it into the free list
        for (int bucketIdx = 0; bucketIdx < m_numBuckets; ++bucketIdx) {
            const T* firstItem = m_bucketDataPtrs[bucketIdx];
            if (firstItem > r || r >= firstItem + BucketSize) {
                // resource is not in this bucket when its pointer address
                // is outside the address range spanned by the addresses of
                // the first and last items in a bucket
                continue;
            }

            // now we found the bucket we can reconstruct the global index
            // and put it back into the free list
            const int localIdx = static_cast<int>(r - firstItem);
            const int idx = bucketIdx * BucketSize + localIdx;
            m_freeList.append(idx);
            performCleanup(r, Int2Type<QResourceInfo<T>::needsCleanup>());
            break;
        }
    }

    void reset()
    {
        deallocateBuckets();
        m_freeList.resize(MaxSize);
        for (int i = 0; i < MaxSize; i++)
            m_freeList[i] = MaxSize - (i + 1);
    }

private:
    Q_DISABLE_COPY(ArrayAllocatingPolicy)

    enum {
        MaxSize = (1 << INDEXBITS),
        // use at most 1024 items per bucket, or put all items into a single
        // bucket if MaxSize is small enough
        BucketSize = (1 << (INDEXBITS < 10 ? INDEXBITS : 10))
    };

    void deallocateBuckets()
    {
        while (m_numConstructed > 0) {
            --m_numConstructed;
            int bucketIdx = m_numConstructed / BucketSize;
            int localIdx = m_numConstructed % BucketSize;
            (m_bucketDataPtrs[bucketIdx] + localIdx)->~T();
        }

        while (m_numBuckets > 0) {
            --m_numBuckets;
            free(m_bucketDataPtrs[m_numBuckets]);
        }
    }

    T* m_bucketDataPtrs[MaxSize / BucketSize];
    QVector<int> m_freeList;
    int m_numBuckets;
    int m_numConstructed;

    void performCleanup(T *r, Int2Type<true>)
    {
        r->cleanup();
    }

    void performCleanup(T *, Int2Type<false>)
    {}

};

template <typename T, uint INDEXBITS>
class ArrayPreallocationPolicy
{
public:
    ArrayPreallocationPolicy()
    {
        reset();
    }

    T* allocateResource()
    {
        Q_ASSERT(!m_freeList.isEmpty());
        int idx = m_freeList.last();
        m_freeList.pop_back();
        return m_bucket.data() + idx;
    }

    void releaseResource(T *r)
    {
        Q_ASSERT(m_bucket.data() <= r && r < m_bucket.data() + MaxSize);
        int idx = r - m_bucket.data();
        m_freeList.append(idx);
        performCleanup(r, Int2Type<QResourceInfo<T>::needsCleanup>());
        *r = T();
    }

    void reset()
    {
        m_bucket.clear();
        m_bucket.resize(MaxSize);
        m_freeList.resize(MaxSize);
        for (int i = 0; i < MaxSize; i++)
            m_freeList[i] = MaxSize - (i + 1);
    }

private:
    enum {
        MaxSize = 1 << INDEXBITS
    };

    QVector<T> m_bucket;
    QVector<int> m_freeList;

    void performCleanup(T *r, Int2Type<true>)
    {
        r->cleanup();
    }

    void performCleanup(T *, Int2Type<false>)
    {}

};

#ifndef QT_NO_DEBUG_STREAM
template <typename ValueType, typename KeyType, uint INDEXBITS,
          template <typename, uint> class AllocatingPolicy,
          template <class> class LockingPolicy
          >
class QResourceManager;

template <typename ValueType, typename KeyType, uint INDEXBITS = 16,
          template <typename, uint> class AllocatingPolicy = ArrayAllocatingPolicy,
          template <class> class LockingPolicy = NonLockingPolicy
          >
QDebug operator<<(QDebug dbg, const QResourceManager<ValueType, KeyType, INDEXBITS, AllocatingPolicy, LockingPolicy> &manager);
#endif

template <typename ValueType, typename KeyType, uint INDEXBITS = 16,
          template <typename, uint> class AllocatingPolicy = ArrayAllocatingPolicy,
          template <class> class LockingPolicy = NonLockingPolicy
          >
class QResourceManager
        : public AllocatingPolicy<ValueType, INDEXBITS>
        , public LockingPolicy< QResourceManager<ValueType, KeyType, INDEXBITS, AllocatingPolicy, LockingPolicy> >
{
public:
    QResourceManager() :
        AllocatingPolicy<ValueType, INDEXBITS>(),
        m_maxSize((1 << INDEXBITS) - 1)
    {
    }

    ~QResourceManager()
    {}

    QHandle<ValueType, INDEXBITS> acquire()
    {
        typename LockingPolicy<QResourceManager>::WriteLocker lock(this);
        QHandle<ValueType, INDEXBITS> handle = m_handleManager.acquire(AllocatingPolicy<ValueType, INDEXBITS>::allocateResource());
        m_activeHandles.push_back(handle);
        return handle;
    }

    ValueType* data(const QHandle<ValueType, INDEXBITS> &handle)
    {
        typename LockingPolicy<QResourceManager>::ReadLocker lock(this);
        ValueType* d = m_handleManager.data(handle);
        return d;
    }

    void release(const QHandle<ValueType, INDEXBITS> &handle)
    {
        typename LockingPolicy<QResourceManager>::WriteLocker lock(this);
        releaseLocked(handle);
    }

    void reset()
    {
        typename LockingPolicy<QResourceManager>::WriteLocker lock(this);
        m_handleManager.reset();
        m_activeHandles.clear();
        AllocatingPolicy<ValueType, INDEXBITS>::reset();
    }

    bool contains(const KeyType &id) const
    {
        typename LockingPolicy<QResourceManager>::ReadLocker lock(this);
        return m_keyToHandleMap.contains(id);
    }

    QHandle<ValueType, INDEXBITS> getOrAcquireHandle(const KeyType &id)
    {
        typename LockingPolicy<QResourceManager>::ReadLocker lock(this);
        QHandle<ValueType, INDEXBITS> handle = m_keyToHandleMap.value(id);
        if (handle.isNull()) {
            lock.unlock();
            typename LockingPolicy<QResourceManager>::WriteLocker writeLock(this);
            // Test that the handle hasn't been set (in the meantime between the read unlock and the write lock)
            QHandle<ValueType, INDEXBITS> &handleToSet = m_keyToHandleMap[id];
            if (handleToSet.isNull()) {
                handleToSet = m_handleManager.acquire(AllocatingPolicy<ValueType, INDEXBITS>::allocateResource());
                m_activeHandles.push_back(handleToSet);
            }
            return handleToSet;
        }
        return handle;
    }

    QHandle<ValueType, INDEXBITS> lookupHandle(const KeyType &id)
    {
        typename LockingPolicy<QResourceManager>::ReadLocker lock(this);
        return m_keyToHandleMap.value(id);
    }

    ValueType *lookupResource(const KeyType &id)
    {
        ValueType* ret = nullptr;
        {
            typename LockingPolicy<QResourceManager>::ReadLocker lock(this);
            QHandle<ValueType, INDEXBITS> handle = m_keyToHandleMap.value(id);
            if (!handle.isNull())
                ret = m_handleManager.data(handle);
        }
        return ret;
    }

    ValueType *getOrCreateResource(const KeyType &id)
    {
        const QHandle<ValueType, INDEXBITS> handle = getOrAcquireHandle(id);
        typename LockingPolicy<QResourceManager>::ReadLocker lock(this);
        return m_handleManager.data(handle);
    }

    void releaseResource(const KeyType &id)
    {
        typename LockingPolicy<QResourceManager>::WriteLocker lock(this);
        QHandle<ValueType, INDEXBITS> handle = m_keyToHandleMap.take(id);
        if (!handle.isNull())
            releaseLocked(handle);
    }

    int maximumSize() const { return m_maxSize; }

    int count() const Q_DECL_NOEXCEPT { return m_handleManager.activeEntries(); }

    inline QVector<QHandle<ValueType, INDEXBITS> > activeHandles() const Q_DECL_NOEXCEPT { return m_activeHandles; }

protected:
    QHandleManager<ValueType, INDEXBITS> m_handleManager;
    QHash<KeyType, QHandle<ValueType, INDEXBITS> > m_keyToHandleMap;
    QVector<QHandle<ValueType, INDEXBITS> > m_activeHandles;
    const int m_maxSize;

private:
    void releaseLocked(const QHandle<ValueType, INDEXBITS> &handle)
    {
        ValueType *val = m_handleManager.data(handle);
        m_handleManager.release(handle);
        m_activeHandles.removeOne(handle);
        AllocatingPolicy<ValueType, INDEXBITS>::releaseResource(val);
    }

    friend QDebug operator<< <>(QDebug dbg, const QResourceManager<ValueType, KeyType, INDEXBITS, AllocatingPolicy, LockingPolicy> &manager);
};

#ifndef QT_NO_DEBUG_STREAM
template <typename ValueType, typename KeyType, uint INDEXBITS,
          template <typename, uint> class AllocatingPolicy,
          template <class> class LockingPolicy
          >
QDebug operator<<(QDebug dbg, const QResourceManager<ValueType, KeyType, INDEXBITS, AllocatingPolicy, LockingPolicy> &manager)
{
    QDebugStateSaver saver(dbg);
    dbg << "Contains" << manager.count() << "items" << "of a maximum" << manager.maximumSize() << endl;

    dbg << "Key to Handle Map:" << endl;
    const auto end = manager.m_keyToHandleMap.cend();
    for (auto it = manager.m_keyToHandleMap.cbegin(); it != end; ++it)
        dbg << "QNodeId =" << it.key() << "Handle =" << it.value() << endl;

    dbg << "Resources:" << endl;
    dbg << manager.m_handleManager;
    return dbg;
}
#endif

}// Qt3D

QT_END_NAMESPACE

#if defined(Q_CC_MSVC)
#pragma warning(default : 4189)
#endif

#endif // QT3DCORE_QABSTRACTRESOURCESMANAGER_H
