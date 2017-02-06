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

#include "qqmllistwrapper_p.h"
#include <private/qv8engine_p.h>
#include <private/qqmllist_p.h>
#include <private/qv4objectproto_p.h>
#include <qv4objectiterator_p.h>

#include <private/qv4functionobject_p.h>
#include <private/qv4qobjectwrapper_p.h>

QT_BEGIN_NAMESPACE

using namespace QV4;

DEFINE_OBJECT_VTABLE(QmlListWrapper);

void Heap::QmlListWrapper::init()
{
    Object::init();
    object.init();
    QV4::Scope scope(internalClass->engine);
    QV4::ScopedObject o(scope, this);
    o->setArrayType(Heap::ArrayData::Custom);
}

void Heap::QmlListWrapper::destroy()
{
    object.destroy();
    Object::destroy();
}

ReturnedValue QmlListWrapper::create(ExecutionEngine *engine, QObject *object, int propId, int propType)
{
    if (!object || propId == -1)
        return Encode::null();

    Scope scope(engine);

    Scoped<QmlListWrapper> r(scope, engine->memoryManager->allocObject<QmlListWrapper>());
    r->d()->object = object;
    r->d()->propertyType = propType;
    void *args[] = { &r->d()->property(), 0 };
    QMetaObject::metacall(object, QMetaObject::ReadProperty, propId, args);
    return r.asReturnedValue();
}

ReturnedValue QmlListWrapper::create(ExecutionEngine *engine, const QQmlListProperty<QObject> &prop, int propType)
{
    Scope scope(engine);

    Scoped<QmlListWrapper> r(scope, engine->memoryManager->allocObject<QmlListWrapper>());
    r->d()->object = prop.object;
    r->d()->property() = prop;
    r->d()->propertyType = propType;
    return r.asReturnedValue();
}

QVariant QmlListWrapper::toVariant() const
{
    if (!d()->object)
        return QVariant();

    return QVariant::fromValue(QQmlListReferencePrivate::init(d()->property(), d()->propertyType, engine()->qmlEngine()));
}


ReturnedValue QmlListWrapper::get(const Managed *m, String *name, bool *hasProperty)
{
    Q_ASSERT(m->as<QmlListWrapper>());
    const QmlListWrapper *w = static_cast<const QmlListWrapper *>(m);
    QV4::ExecutionEngine *v4 = w->engine();

    if (name->equals(v4->id_length()) && !w->d()->object.isNull()) {
        quint32 count = w->d()->property().count ? w->d()->property().count(&w->d()->property()) : 0;
        return Primitive::fromUInt32(count).asReturnedValue();
    }

    uint idx = name->asArrayIndex();
    if (idx != UINT_MAX)
        return getIndexed(m, idx, hasProperty);

    return Object::get(m, name, hasProperty);
}

ReturnedValue QmlListWrapper::getIndexed(const Managed *m, uint index, bool *hasProperty)
{
    Q_UNUSED(hasProperty);

    Q_ASSERT(m->as<QmlListWrapper>());
    const QmlListWrapper *w = static_cast<const QmlListWrapper *>(m);
    QV4::ExecutionEngine *v4 = w->engine();

    quint32 count = w->d()->property().count ? w->d()->property().count(&w->d()->property()) : 0;
    if (index < count && w->d()->property().at) {
        if (hasProperty)
            *hasProperty = true;
        return QV4::QObjectWrapper::wrap(v4, w->d()->property().at(&w->d()->property(), index));
    }

    if (hasProperty)
        *hasProperty = false;
    return Primitive::undefinedValue().asReturnedValue();
}

void QmlListWrapper::put(Managed *m, String *name, const Value &value)
{
    // doesn't do anything. Should we throw?
    Q_UNUSED(m);
    Q_UNUSED(name);
    Q_UNUSED(value);
}

void QmlListWrapper::advanceIterator(Managed *m, ObjectIterator *it, Value *name, uint *index, Property *p, PropertyAttributes *attrs)
{
    name->setM(0);
    *index = UINT_MAX;
    Q_ASSERT(m->as<QmlListWrapper>());
    QmlListWrapper *w = static_cast<QmlListWrapper *>(m);
    quint32 count = w->d()->property().count ? w->d()->property().count(&w->d()->property()) : 0;
    if (it->arrayIndex < count) {
        *index = it->arrayIndex;
        ++it->arrayIndex;
        *attrs = QV4::Attr_Data;
        p->value = QV4::QObjectWrapper::wrap(w->engine(), w->d()->property().at(&w->d()->property(), *index));
        return;
    }
    return QV4::Object::advanceIterator(m, it, name, index, p, attrs);
}

QT_END_NAMESPACE
