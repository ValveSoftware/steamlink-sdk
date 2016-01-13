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
#ifndef QV4OBJECTITERATOR_H
#define QV4OBJECTITERATOR_H

#include "qv4global_p.h"
#include "qv4property_p.h"
#include "qv4scopedvalue_p.h"
#include "qv4object_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct SparseArrayNode;
struct Object;
struct ArrayObject;
struct PropertyAttributes;
struct ExecutionContext;
struct Property;
struct String;
struct InternalClass;
struct ForEachIteratorObject;

struct Q_QML_EXPORT ObjectIterator
{
    enum Flags {
        NoFlags = 0,
        EnumerableOnly = 0x1,
        WithProtoChain = 0x2,
    };

    Value *object;
    Value *current;
    SparseArrayNode *arrayNode;
    uint arrayIndex;
    uint memberIndex;
    uint flags;

    ObjectIterator(Value *scratch1, Value *scratch2, Object *o, uint flags);
    ObjectIterator(Scope &scope, Object *o, uint flags);
    void init(Object *o);
    void next(String *&name, uint *index, Property *pd, PropertyAttributes *attributes = 0);
    ReturnedValue nextPropertyName(ValueRef value);
    ReturnedValue nextPropertyNameAsString(ValueRef value);
    ReturnedValue nextPropertyNameAsString();
private:
    friend struct ForEachIteratorObject;
    ObjectIterator(Value *scratch1, Value *scratch2, uint flags); // Constructor that requires calling init()
};

struct ForEachIteratorObject: Object {
    struct Data : Object::Data {
        Data(ExecutionContext *ctx, Object *o)
            : Object::Data(ctx->engine())
            , it(workArea, workArea + 1, o, ObjectIterator::EnumerableOnly|ObjectIterator::WithProtoChain) {
            setVTable(staticVTable());
        }
        ObjectIterator it;
        Value workArea[2];
    };
    V4_OBJECT(Object)
    Q_MANAGED_TYPE(ForeachIteratorObject)

    ReturnedValue nextPropertyName() { return d()->it.nextPropertyNameAsString(); }

protected:
    static void markObjects(Managed *that, ExecutionEngine *e);
};


}

QT_END_NAMESPACE

#endif
