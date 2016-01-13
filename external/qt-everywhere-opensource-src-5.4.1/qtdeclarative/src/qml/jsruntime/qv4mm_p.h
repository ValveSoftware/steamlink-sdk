/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QV4GC_H
#define QV4GC_H

#include "qv4global_p.h"
#include "qv4context_p.h"
#include "qv4value_inl_p.h"

#include <QScopedPointer>

//#define DETAILED_MM_STATS

QT_BEGIN_NAMESPACE

namespace QV4 {

struct ExecutionEngine;
struct ExecutionContext;
struct Managed;
struct GCDeletable;

class Q_QML_EXPORT MemoryManager
{
    MemoryManager(const MemoryManager &);
    MemoryManager &operator=(const MemoryManager&);

public:
    struct Data;

    class GCBlocker
    {
    public:
        GCBlocker(MemoryManager *mm)
            : mm(mm)
            , wasBlocked(mm->isGCBlocked())
        {
            mm->setGCBlocked(true);
        }

        ~GCBlocker()
        {
            mm->setGCBlocked(wasBlocked);
        }

    private:
        MemoryManager *mm;
        bool wasBlocked;
    };

public:
    MemoryManager();
    ~MemoryManager();

    // TODO: this is only for 64bit (and x86 with SSE/AVX), so exend it for other architectures to be slightly more efficient (meaning, align on 8-byte boundaries).
    // Note: all occurrences of "16" in alloc/dealloc are also due to the alignment.
    static inline std::size_t align(std::size_t size)
    { return (size + 15) & ~0xf; }

    inline Managed *allocManaged(std::size_t size)
    {
        size = align(size);
        Managed *o = allocData(size);
        return o;
    }

    template <typename ManagedType>
    ManagedType *alloc()
    {
        ManagedType *t = static_cast<ManagedType*>(allocManaged(sizeof(typename ManagedType::Data)));
        (void)new (t->d()) typename ManagedType::Data();
        return t;
    }

    template <typename ManagedType, typename Arg1>
    ManagedType *alloc(Arg1 arg1)
    {
        ManagedType *t = static_cast<ManagedType*>(allocManaged(sizeof(typename ManagedType::Data)));
        (void)new (t->d()) typename ManagedType::Data(arg1);
        return t;
    }

    template <typename ManagedType, typename Arg1, typename Arg2>
    ManagedType *alloc(Arg1 arg1, Arg2 arg2)
    {
        ManagedType *t = static_cast<ManagedType*>(allocManaged(sizeof(typename ManagedType::Data)));
        (void)new (t->d()) typename ManagedType::Data(arg1, arg2);
        return t;
    }

    template <typename ManagedType, typename Arg1, typename Arg2, typename Arg3>
    ManagedType *alloc(Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        ManagedType *t = static_cast<ManagedType*>(allocManaged(sizeof(typename ManagedType::Data)));
        (void)new (t->d()) typename ManagedType::Data(arg1, arg2, arg3);
        return t;
    }

    template <typename ManagedType, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    ManagedType *alloc(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        ManagedType *t = static_cast<ManagedType*>(allocManaged(sizeof(typename ManagedType::Data)));
        (void)new (t->d()) typename ManagedType::Data(arg1, arg2, arg3, arg4);
        return t;
    }

    template <typename ManagedType, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
    ManagedType *alloc(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
    {
        ManagedType *t = static_cast<ManagedType*>(allocManaged(sizeof(typename ManagedType::Data)));
        (void)new (t->d()) typename ManagedType::Data(arg1, arg2, arg3, arg4, arg5);
        return t;
    }

    bool isGCBlocked() const;
    void setGCBlocked(bool blockGC);
    void runGC();

    void setExecutionEngine(ExecutionEngine *engine);

    void dumpStats() const;

    void registerDeletable(GCDeletable *d);

    size_t getUsedMem() const;
    size_t getAllocatedMem() const;
    size_t getLargeItemsMem() const;

protected:
    /// expects size to be aligned
    // TODO: try to inline
    Managed *allocData(std::size_t size);

#ifdef DETAILED_MM_STATS
    void willAllocate(std::size_t size);
#endif // DETAILED_MM_STATS

private:
    void collectFromJSStack() const;
    void mark();
    void sweep(bool lastSweep = false);
    void sweep(char *chunkStart, std::size_t chunkSize, size_t size);

protected:
    QScopedPointer<Data> m_d;
public:
    PersistentValuePrivate *m_persistentValues;
    PersistentValuePrivate *m_weakValues;
};

}

QT_END_NAMESPACE

#endif // QV4GC_H
