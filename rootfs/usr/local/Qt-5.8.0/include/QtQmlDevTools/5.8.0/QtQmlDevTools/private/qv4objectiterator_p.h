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
#ifndef QV4OBJECTITERATOR_H
#define QV4OBJECTITERATOR_H

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

#include "qv4global_p.h"
#include "qv4object_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct Q_QML_EXPORT ObjectIteratorData
{
    enum Flags {
        NoFlags = 0,
        EnumerableOnly = 0x1,
        WithProtoChain = 0x2,
    };

    ExecutionEngine *engine;
    Value *object;
    Value *current;
    SparseArrayNode *arrayNode;
    uint arrayIndex;
    uint memberIndex;
    uint flags;
};
V4_ASSERT_IS_TRIVIAL(ObjectIteratorData)

struct Q_QML_EXPORT ObjectIterator: ObjectIteratorData
{
    ObjectIterator(ExecutionEngine *e, Value *scratch1, Value *scratch2, Object *o, uint flags)
    {
        engine = e;
        object = scratch1;
        current = scratch2;
        arrayNode = nullptr;
        arrayIndex = 0;
        memberIndex = 0;
        this->flags = flags;
        init(o);
    }

    ObjectIterator(Scope &scope, const Object *o, uint flags)
    {
        engine = scope.engine;
        object = scope.alloc(1);
        current = scope.alloc(1);
        arrayNode = nullptr;
        arrayIndex = 0;
        memberIndex = 0;
        this->flags = flags;
        init(o);
    }

    void next(Value *name, uint *index, Property *pd, PropertyAttributes *attributes = 0);
    ReturnedValue nextPropertyName(Value *value);
    ReturnedValue nextPropertyNameAsString(Value *value);
    ReturnedValue nextPropertyNameAsString();

private:
    void init(const Object *o);
};

namespace Heap {
struct ForEachIteratorObject : Object {
    void init(QV4::Object *o);
    ObjectIterator &it() { return *reinterpret_cast<ObjectIterator*>(&itData); }
    Value workArea[2];

private:
    ObjectIteratorData itData;
};

}

struct ForEachIteratorObject: Object {
    V4_OBJECT2(ForEachIteratorObject, Object)
    Q_MANAGED_TYPE(ForeachIteratorObject)

    ReturnedValue nextPropertyName() { return d()->it().nextPropertyNameAsString(); }

protected:
    static void markObjects(Heap::Base *that, ExecutionEngine *e);
};

inline
void Heap::ForEachIteratorObject::init(QV4::Object *o)
{
    Object::init();
    it() = ObjectIterator(internalClass->engine, workArea, workArea + 1, o,
                          ObjectIterator::EnumerableOnly | ObjectIterator::WithProtoChain);
}


}

QT_END_NAMESPACE

#endif
