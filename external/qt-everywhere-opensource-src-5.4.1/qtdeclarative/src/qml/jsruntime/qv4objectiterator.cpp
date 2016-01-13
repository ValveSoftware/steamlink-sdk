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
#include "qv4objectiterator_p.h"
#include "qv4object_p.h"
#include "qv4stringobject_p.h"
#include "qv4identifier_p.h"
#include "qv4argumentsobject_p.h"

using namespace QV4;

ObjectIterator::ObjectIterator(Value *scratch1, Value *scratch2, Object *o, uint flags)
    : object(scratch1)
    , current(scratch2)
    , arrayNode(0)
    , arrayIndex(0)
    , memberIndex(0)
    , flags(flags)
{
    init(o);
}

ObjectIterator::ObjectIterator(Scope &scope, Object *o, uint flags)
    : object(scope.alloc(1))
    , current(scope.alloc(1))
    , arrayNode(0)
    , arrayIndex(0)
    , memberIndex(0)
    , flags(flags)
{
    init(o);
}

ObjectIterator::ObjectIterator(Value *scratch1, Value *scratch2, uint flags)
    : object(scratch1)
    , current(scratch2)
    , arrayNode(0)
    , arrayIndex(0)
    , memberIndex(0)
    , flags(flags)
{
    object->o = (Object*)0;
    current->o = (Object*)0;
    // Caller needs to call init!
}

void ObjectIterator::init(Object *o)
{
    object->o = o;
    current->o = o;

#if QT_POINTER_SIZE == 4
    object->tag = QV4::Value::Managed_Type;
    current->tag = QV4::Value::Managed_Type;
#endif

    if (object->as<ArgumentsObject>()) {
        Scope scope(object->engine());
        Scoped<ArgumentsObject> (scope, object->asReturnedValue())->fullyCreate();
    }
}

void ObjectIterator::next(String *&name, uint *index, Property *pd, PropertyAttributes *attrs)
{
    name = (String *)0;
    *index = UINT_MAX;

    if (!object->asObject()) {
        *attrs = PropertyAttributes();
        return;
    }

    while (1) {
        if (!current->asObject())
            break;

        while (1) {
            current->asObject()->advanceIterator(this, name, index, pd, attrs);
            if (attrs->isEmpty())
                break;
            // check the property is not already defined earlier in the proto chain
            if (current->asObject() != object->asObject()) {
                Object *o = object->asObject();
                bool shadowed = false;
                while (o != current->asObject()) {
                    if ((!!name && o->hasOwnProperty(name)) ||
                        (*index != UINT_MAX && o->hasOwnProperty(*index))) {
                        shadowed = true;
                        break;
                    }
                    o = o->prototype();
                }
                if (shadowed)
                    continue;
            }
            return;
        }

        if (flags & WithProtoChain)
            current->o = current->objectValue()->prototype();
        else
            current->o = (Object *)0;

        arrayIndex = 0;
        memberIndex = 0;
    }
    *attrs = PropertyAttributes();
}

ReturnedValue ObjectIterator::nextPropertyName(ValueRef value)
{
    if (!object->asObject())
        return Encode::null();

    PropertyAttributes attrs;
    Property p;
    uint index;
    Scope scope(object->engine());
    ScopedString name(scope);
    String *n;
    next(n, &index, &p, &attrs);
    name = n;
    if (attrs.isEmpty())
        return Encode::null();

    value = object->objectValue()->getValue(&p, attrs);

    if (!!name)
        return name->asReturnedValue();
    assert(index < UINT_MAX);
    return Encode(index);
}

ReturnedValue ObjectIterator::nextPropertyNameAsString(ValueRef value)
{
    if (!object->asObject())
        return Encode::null();

    PropertyAttributes attrs;
    Property p;
    uint index;
    Scope scope(object->engine());
    ScopedString name(scope);
    String *n;
    next(n, &index, &p, &attrs);
    name = n;
    if (attrs.isEmpty())
        return Encode::null();

    value = object->objectValue()->getValue(&p, attrs);

    if (!!name)
        return name->asReturnedValue();
    assert(index < UINT_MAX);
    return Encode(object->engine()->newString(QString::number(index)));
}

ReturnedValue ObjectIterator::nextPropertyNameAsString()
{
    if (!object->asObject())
        return Encode::null();

    PropertyAttributes attrs;
    Property p;
    uint index;
    Scope scope(object->engine());
    ScopedString name(scope);
    String *n;
    next(n, &index, &p, &attrs);
    name = n;
    if (attrs.isEmpty())
        return Encode::null();

    if (!!name)
        return name->asReturnedValue();
    assert(index < UINT_MAX);
    return Encode(object->engine()->newString(QString::number(index)));
}


DEFINE_OBJECT_VTABLE(ForEachIteratorObject);

void ForEachIteratorObject::markObjects(Managed *that, ExecutionEngine *e)
{
    ForEachIteratorObject *o = static_cast<ForEachIteratorObject *>(that);
    o->d()->workArea[0].mark(e);
    o->d()->workArea[1].mark(e);
    Object::markObjects(that, e);
}
