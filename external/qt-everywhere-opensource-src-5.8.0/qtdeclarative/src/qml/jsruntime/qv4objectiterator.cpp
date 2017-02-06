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
#include "qv4objectiterator_p.h"
#include "qv4object_p.h"
#include "qv4stringobject_p.h"
#include "qv4identifier_p.h"
#include "qv4argumentsobject_p.h"
#include "qv4string_p.h"

using namespace QV4;

void ObjectIterator::init(const Object *o)
{
    object->setM(o ? o->m() : 0);
    current->setM(o ? o->m() : 0);

    if (object->as<ArgumentsObject>()) {
        Scope scope(engine);
        Scoped<ArgumentsObject> (scope, object->asReturnedValue())->fullyCreate();
    }
}

void ObjectIterator::next(Value *name, uint *index, Property *pd, PropertyAttributes *attrs)
{
    name->setM(0);
    *index = UINT_MAX;

    if (!object->as<Object>()) {
        *attrs = PropertyAttributes();
        return;
    }
    Scope scope(engine);
    ScopedObject o(scope);
    ScopedString n(scope);

    while (1) {
        if (!current->as<Object>())
            break;

        while (1) {
            current->as<Object>()->advanceIterator(this, name, index, pd, attrs);
            if (attrs->isEmpty())
                break;
            // check the property is not already defined earlier in the proto chain
            if (current->heapObject() != object->heapObject()) {
                o = object->as<Object>();
                n = *name;
                bool shadowed = false;
                while (o->d() != current->heapObject()) {
                    if ((!!n && o->hasOwnProperty(n)) ||
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
            current->setM(current->objectValue()->prototype());
        else
            current->setM(0);

        arrayIndex = 0;
        memberIndex = 0;
    }
    *attrs = PropertyAttributes();
}

ReturnedValue ObjectIterator::nextPropertyName(Value *value)
{
    if (!object->as<Object>())
        return Encode::null();

    PropertyAttributes attrs;
    uint index;
    Scope scope(engine);
    ScopedProperty p(scope);
    ScopedString name(scope);
    next(name.getRef(), &index, p, &attrs);
    if (attrs.isEmpty())
        return Encode::null();

    *value = object->objectValue()->getValue(p->value, attrs);

    if (!!name)
        return name->asReturnedValue();
    Q_ASSERT(index < UINT_MAX);
    return Encode(index);
}

ReturnedValue ObjectIterator::nextPropertyNameAsString(Value *value)
{
    if (!object->as<Object>())
        return Encode::null();

    PropertyAttributes attrs;
    uint index;
    Scope scope(engine);
    ScopedProperty p(scope);
    ScopedString name(scope);
    next(name.getRef(), &index, p, &attrs);
    if (attrs.isEmpty())
        return Encode::null();

    *value = object->objectValue()->getValue(p->value, attrs);

    if (!!name)
        return name->asReturnedValue();
    Q_ASSERT(index < UINT_MAX);
    return Encode(engine->newString(QString::number(index)));
}

ReturnedValue ObjectIterator::nextPropertyNameAsString()
{
    if (!object->as<Object>())
        return Encode::null();

    PropertyAttributes attrs;
    uint index;
    Scope scope(engine);
    ScopedProperty p(scope);
    ScopedString name(scope);
    next(name.getRef(), &index, p, &attrs);
    if (attrs.isEmpty())
        return Encode::null();

    if (!!name)
        return name->asReturnedValue();
    Q_ASSERT(index < UINT_MAX);
    return Encode(engine->newString(QString::number(index)));
}


DEFINE_OBJECT_VTABLE(ForEachIteratorObject);

void ForEachIteratorObject::markObjects(Heap::Base *that, ExecutionEngine *e)
{
    ForEachIteratorObject::Data *o = static_cast<ForEachIteratorObject::Data *>(that);
    o->workArea[0].mark(e);
    o->workArea[1].mark(e);
    Object::markObjects(that, e);
}
