/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QV4GC_H
#define QV4GC_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qv4global_p.h>
#include <private/qv4value_p.h>
#include <private/qv4scopedvalue_p.h>
#include <private/qv4object_p.h>
#include <QVector>

//#define DETAILED_MM_STATS

#define QV4_MM_MAXBLOCK_SHIFT "QV4_MM_MAXBLOCK_SHIFT"
#define QV4_MM_MAX_CHUNK_SIZE "QV4_MM_MAX_CHUNK_SIZE"
#define QV4_MM_STATS "QV4_MM_STATS"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct GCDeletable;

class Q_QML_EXPORT MemoryManager
{
    Q_DISABLE_COPY(MemoryManager);

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
    MemoryManager(ExecutionEngine *engine);
    ~MemoryManager();

    // TODO: this is only for 64bit (and x86 with SSE/AVX), so exend it for other architectures to be slightly more efficient (meaning, align on 8-byte boundaries).
    // Note: all occurrences of "16" in alloc/dealloc are also due to the alignment.
    static inline std::size_t align(std::size_t size)
    { return (size + 15) & ~0xf; }

    template<typename ManagedType>
    inline typename ManagedType::Data *allocManaged(std::size_t size, std::size_t unmanagedSize = 0)
    {
        V4_ASSERT_IS_TRIVIAL(typename ManagedType::Data)
        size = align(size);
        Heap::Base *o = allocData(size, unmanagedSize);
        memset(o, 0, size);
        o->setVtable(ManagedType::staticVTable());
        return static_cast<typename ManagedType::Data *>(o);
    }

    template <typename ObjectType>
    typename ObjectType::Data *allocateObject(InternalClass *ic)
    {
        const int size = (sizeof(typename ObjectType::Data) + (sizeof(Value) - 1)) & ~(sizeof(Value) - 1);
        typename ObjectType::Data *o = allocManaged<ObjectType>(size + ic->size*sizeof(Value));
        o->internalClass = ic;
        o->inlineMemberSize = ic->size;
        o->inlineMemberOffset = size/sizeof(Value);
        return o;
    }

    template <typename ObjectType>
    typename ObjectType::Data *allocateObject()
    {
        InternalClass *ic = ObjectType::defaultInternalClass(engine);
        const int size = (sizeof(typename ObjectType::Data) + (sizeof(Value) - 1)) & ~(sizeof(Value) - 1);
        typename ObjectType::Data *o = allocManaged<ObjectType>(size + ic->size*sizeof(Value));
        Object *prototype = ObjectType::defaultPrototype(engine);
        o->internalClass = ic;
        o->prototype = prototype->d();
        o->inlineMemberSize = ic->size;
        o->inlineMemberOffset = size/sizeof(Value);
        return o;
    }

    template <typename ManagedType, typename Arg1>
    typename ManagedType::Data *allocWithStringData(std::size_t unmanagedSize, Arg1 arg1)
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data), unmanagedSize));
        t->d_unchecked()->init(this, arg1);
        return t->d();
    }

    template <typename ObjectType>
    typename ObjectType::Data *allocObject(InternalClass *ic)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        t->d_unchecked()->init();
        return t->d();
    }

    template <typename ObjectType>
    typename ObjectType::Data *allocObject(InternalClass *ic, Object *prototype)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        t->d_unchecked()->prototype = prototype->d();
        t->d_unchecked()->init();
        return t->d();
    }

    template <typename ObjectType, typename Arg1>
    typename ObjectType::Data *allocObject(InternalClass *ic, Object *prototype, Arg1 arg1)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        t->d_unchecked()->prototype = prototype->d();
        t->d_unchecked()->init(arg1);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2>
    typename ObjectType::Data *allocObject(InternalClass *ic, Object *prototype, Arg1 arg1, Arg2 arg2)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        t->d_unchecked()->prototype = prototype->d();
        t->d_unchecked()->init(arg1, arg2);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2, typename Arg3>
    typename ObjectType::Data *allocObject(InternalClass *ic, Object *prototype, Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        t->d_unchecked()->prototype = prototype->d();
        t->d_unchecked()->init(arg1, arg2, arg3);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    typename ObjectType::Data *allocObject(InternalClass *ic, Object *prototype, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        t->d_unchecked()->prototype = prototype->d();
        t->d_unchecked()->init(arg1, arg2, arg3, arg4);
        return t->d();
    }

    template <typename ObjectType>
    typename ObjectType::Data *allocObject()
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>());
        t->d_unchecked()->init();
        return t->d();
    }

    template <typename ObjectType, typename Arg1>
    typename ObjectType::Data *allocObject(Arg1 arg1)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>());
        t->d_unchecked()->init(arg1);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2>
    typename ObjectType::Data *allocObject(Arg1 arg1, Arg2 arg2)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>());
        t->d_unchecked()->init(arg1, arg2);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2, typename Arg3>
    typename ObjectType::Data *allocObject(Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>());
        t->d_unchecked()->init(arg1, arg2, arg3);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    typename ObjectType::Data *allocObject(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>());
        t->d_unchecked()->init(arg1, arg2, arg3, arg4);
        return t->d();
    }


    template <typename ManagedType>
    typename ManagedType::Data *alloc()
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init();
        return t->d();
    }

    template <typename ManagedType, typename Arg1>
    typename ManagedType::Data *alloc(Arg1 arg1)
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init(arg1);
        return t->d();
    }

    template <typename ManagedType, typename Arg1, typename Arg2>
    typename ManagedType::Data *alloc(Arg1 arg1, Arg2 arg2)
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init(arg1, arg2);
        return t->d();
    }

    template <typename ManagedType, typename Arg1, typename Arg2, typename Arg3>
    typename ManagedType::Data *alloc(Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init(arg1, arg2, arg3);
        return t->d();
    }

    template <typename ManagedType, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    typename ManagedType::Data *alloc(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init(arg1, arg2, arg3, arg4);
        return t->d();
    }

    template <typename ManagedType, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
    typename ManagedType::Data *alloc(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init(arg1, arg2, arg3, arg4, arg5);
        return t->d();
    }

    bool isGCBlocked() const;
    void setGCBlocked(bool blockGC);
    void runGC();

    void dumpStats() const;

    size_t getUsedMem() const;
    size_t getAllocatedMem() const;
    size_t getLargeItemsMem() const;

    void growUnmanagedHeapSizeUsage(size_t delta); // called when a JS object grows itself. Specifically: Heap::String::append

protected:
    /// expects size to be aligned
    // TODO: try to inline
    Heap::Base *allocData(std::size_t size, std::size_t unmanagedSize);

#ifdef DETAILED_MM_STATS
    void willAllocate(std::size_t size);
#endif // DETAILED_MM_STATS

private:
    void collectFromJSStack() const;
    void mark();
    void sweep(bool lastSweep = false);

public:
    QV4::ExecutionEngine *engine;
    QScopedPointer<Data> m_d;
    PersistentValueStorage *m_persistentValues;
    PersistentValueStorage *m_weakValues;
    QVector<Value *> m_pendingFreedObjectWrapperValue;
};

}

QT_END_NAMESPACE

#endif // QV4GC_H
