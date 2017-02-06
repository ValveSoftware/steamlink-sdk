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

#ifndef QT3DCORE_QHANDLEMANAGER_P_H
#define QT3DCORE_QHANDLEMANAGER_P_H

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
#include <Qt3DCore/qt3dcore_global.h>
#include "qhandle_p.h"

#include <QtCore/QVector>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

#ifndef QT_NO_DEBUG_STREAM
template <typename T, uint INDEXBITS>
class QHandleManager;

template <typename T, uint INDEXBITS = 16>
QDebug operator<<(QDebug dbg, const QHandleManager<T, INDEXBITS> &manager);
#endif

template <typename T, uint INDEXBITS = 16>
class QHandleManager
{
public:
    QHandleManager()
        : m_firstFreeEntry(0)
        , m_activeEntryCount(0)
        , m_entries(1 << INDEXBITS)
    {
        reset();
    }

    quint32 activeEntries() const { return m_activeEntryCount; }

    void reset();
    QHandle<T, INDEXBITS> acquire(T *d);
    void release(const QHandle<T, INDEXBITS> &handle);
    void update(const QHandle<T, INDEXBITS> &, T *d);
    T *data(const QHandle<T, INDEXBITS> &handle, bool *ok = 0);
    const T *constData(const QHandle<T, INDEXBITS> &handle, bool *ok = 0) const;
    QVector<T *> entries() const;

private:
    Q_DISABLE_COPY(QHandleManager)

    friend QDebug operator<< <>(QDebug dbg, const QHandleManager<T, INDEXBITS> &manager);

    template <typename U>
    struct HandleEntry
    {
        HandleEntry()
            : m_data(nullptr)
            , m_nextFreeIndex(0)
            , m_counter(0)
            , m_active(false)
            , m_endOfFreeList(false)
        {}

        explicit HandleEntry(quint32 nextFreeIndex)
            : m_data(nullptr)
            , m_nextFreeIndex(nextFreeIndex)
            , m_counter(0)
            , m_active(false)
            , m_endOfFreeList(false)
        {}

        U *m_data;
        unsigned int m_nextFreeIndex :  QHandle<U, INDEXBITS>::IndexBits;
        unsigned int m_counter : QHandle<U, INDEXBITS>::CounterBits;
        unsigned int m_active : 1;
        unsigned int m_endOfFreeList : 1;
    };

    quint32 m_firstFreeEntry;
    int m_activeEntryCount;
    QVector<HandleEntry<T> > m_entries;
};

template <typename T, uint INDEXBITS>
void QHandleManager<T, INDEXBITS>::reset()
{
    m_activeEntryCount = 0;
    m_firstFreeEntry = 0;

    for (int i = 0; i < QHandle<T, INDEXBITS >::MaxIndex; ++i)
        m_entries[i] = HandleEntry<T>(i + 1);
    m_entries[QHandle<T, INDEXBITS>::MaxIndex] = HandleEntry<T>();
    m_entries[QHandle<T, INDEXBITS>::MaxIndex].m_endOfFreeList = true;
}

template <typename T, uint INDEXBITS>
QHandle<T, INDEXBITS> QHandleManager<T, INDEXBITS>::acquire(T *d)
{
    typedef QHandle<T, INDEXBITS> qHandle;
    Q_ASSERT(m_activeEntryCount < qHandle::MaxIndex);

    const quint32 newIndex = m_firstFreeEntry;
    Q_ASSERT(newIndex < qHandle::MaxIndex);
    Q_ASSERT(m_entries[newIndex].m_active == false);
    Q_ASSERT(!m_entries[newIndex].m_endOfFreeList);

    m_firstFreeEntry = m_entries[newIndex].m_nextFreeIndex;
    m_entries[newIndex].m_nextFreeIndex = 0;
    ++m_entries[newIndex].m_counter;
    // Check if the counter is about to overflow and reset if necessary
    if (m_entries[newIndex].m_counter == qHandle::MaxCounter)
        m_entries[newIndex].m_counter = 0;
    if (m_entries[newIndex].m_counter == 0)
        m_entries[newIndex].m_counter = 1;
    m_entries[newIndex].m_active = true;
    m_entries[newIndex].m_data = d;

    ++m_activeEntryCount;

    return qHandle(newIndex, m_entries[newIndex].m_counter);
}

template <typename T, uint INDEXBITS>
void QHandleManager<T, INDEXBITS>::release(const QHandle<T, INDEXBITS> &handle)
{
    const quint32 index = handle.index();
    Q_ASSERT(m_entries[index].m_counter == handle.counter());
    Q_ASSERT(m_entries[index].m_active == true);

    m_entries[index].m_nextFreeIndex = m_firstFreeEntry;
    m_entries[index].m_active = false;
    m_firstFreeEntry = index;

    --m_activeEntryCount;
}

// Needed in case the QResourceManager has reordered
// memory so that the handle still points to valid data
template <typename T, uint INDEXBITS>
void QHandleManager<T, INDEXBITS>::update(const QHandle<T, INDEXBITS> &handle, T *d)
{
    const quint32 index = handle.index();
    Q_ASSERT(m_entries[index].m_counter == handle.counter());
    Q_ASSERT(m_entries[index].m_active == true);
    m_entries[index].m_data = d;
}

template <typename T, uint INDEXBITS>
T *QHandleManager<T, INDEXBITS>::data(const QHandle<T, INDEXBITS> &handle, bool *ok)
{
    const quint32 index = handle.index();
    if (m_entries[index].m_counter != handle.counter() ||
        m_entries[index].m_active == false) {
        if (ok)
            *ok = false;
        return nullptr;
    }

    T *d = m_entries[index].m_data;
    if (ok)
        *ok = true;
    return d;
}

template <typename T, uint INDEXBITS>
const T *QHandleManager<T, INDEXBITS>::constData(const QHandle<T, INDEXBITS> &handle, bool *ok) const
{
    const quint32 index = handle.index();
    if (m_entries[index].m_counter != handle.counter() ||
        m_entries[index].m_active == false) {
        if (ok)
            *ok = false;
        return nullptr;
    }

    const T *d = m_entries[index].m_data;
    if (ok)
        *ok = true;
    return d;
}

#ifndef QT_NO_DEBUG_STREAM
template <typename T, uint INDEXBITS>
QDebug operator<<(QDebug dbg, const QHandleManager<T, INDEXBITS> &manager)
{
    QDebugStateSaver saver(dbg);
    dbg << "First free entry =" << manager.m_firstFreeEntry << endl;

    const auto end = manager.m_entries.cend();
    const auto max = manager.m_activeEntryCount;
    auto i = 0;
    for (auto it = manager.m_entries.cbegin(); it != end && i < max; ++it) {
        const auto isActive = it->m_active;
        if (isActive) {
            dbg << *(it->m_data);
            ++i;
        }
    }

    return dbg;
}
#endif

template <typename T, uint INDEXBITS>
QVector<T *> QHandleManager<T, INDEXBITS>::entries() const
{
    QVector<T *> entries;
    for (auto handle : qAsConst(m_entries))
        entries.append(handle.m_data);
    return entries;
}

} // Qt3D

QT_END_NAMESPACE

#endif // QT3DCORE_QHANDLEMANAGER_H
