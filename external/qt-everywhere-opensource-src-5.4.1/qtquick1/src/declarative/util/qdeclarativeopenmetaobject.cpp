/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "private/qdeclarativeopenmetaobject_p.h"
#include "private/qdeclarativepropertycache_p.h"
#include "private/qdeclarativedata_p.h"
#include <private/qmetaobjectbuilder_p.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE


class QDeclarativeOpenMetaObjectTypePrivate
{
public:
    QDeclarativeOpenMetaObjectTypePrivate() : mem(0), cache(0), engine(0) {}

    void init(const QMetaObject *metaObj);

    int propertyOffset;
    int signalOffset;
    QHash<QByteArray, int> names;
    QMetaObjectBuilder mob;
    QMetaObject *mem;
    QDeclarativePropertyCache *cache;
    QDeclarativeEngine *engine;
    QSet<QDeclarativeOpenMetaObject*> referers;
};

QDeclarativeOpenMetaObjectType::QDeclarativeOpenMetaObjectType(const QMetaObject *base, QDeclarativeEngine *engine)
    : d(new QDeclarativeOpenMetaObjectTypePrivate)
{
    d->engine = engine;
    d->init(base);
}

QDeclarativeOpenMetaObjectType::~QDeclarativeOpenMetaObjectType()
{
    if (d->mem)
        free(d->mem);
    if (d->cache)
        d->cache->release();
    delete d;
}


int QDeclarativeOpenMetaObjectType::propertyOffset() const
{
    return d->propertyOffset;
}

int QDeclarativeOpenMetaObjectType::signalOffset() const
{
    return d->signalOffset;
}

int QDeclarativeOpenMetaObjectType::createProperty(const QByteArray &name)
{
    int id = d->mob.propertyCount();
    d->mob.addSignal("__" + QByteArray::number(id) + "()");
    QMetaPropertyBuilder build = d->mob.addProperty(name, "QVariant", id);
    propertyCreated(id, build);
    free(d->mem);
    d->mem = d->mob.toMetaObject();
    d->names.insert(name, id);
    QSet<QDeclarativeOpenMetaObject*>::iterator it = d->referers.begin();
    while (it != d->referers.end()) {
        QDeclarativeOpenMetaObject *omo = *it;
        *static_cast<QMetaObject *>(omo) = *d->mem;
        if (d->cache)
            d->cache->update(d->engine, omo);
        ++it;
    }

    return d->propertyOffset + id;
}

void QDeclarativeOpenMetaObjectType::propertyCreated(int id, QMetaPropertyBuilder &builder)
{
    if (d->referers.count())
        (*d->referers.begin())->propertyCreated(id, builder);
}

void QDeclarativeOpenMetaObjectTypePrivate::init(const QMetaObject *metaObj)
{
    if (!mem) {
        mob.setSuperClass(metaObj);
        mob.setClassName(metaObj->className());
        mob.setFlags(QMetaObjectBuilder::DynamicMetaObject);

        mem = mob.toMetaObject();

        propertyOffset = mem->propertyOffset();
        signalOffset = mem->methodOffset();
    }
}

//----------------------------------------------------------------------------

class QDeclarativeOpenMetaObjectPrivate
{
public:
    QDeclarativeOpenMetaObjectPrivate(QDeclarativeOpenMetaObject *_q)
        : q(_q), parent(0), type(0), cacheProperties(false) {}

    inline QVariant &getData(int idx) {
        while (data.count() <= idx)
            data << QPair<QVariant, bool>(QVariant(), false);
        QPair<QVariant, bool> &prop = data[idx];
        if (!prop.second) {
            prop.first = q->initialValue(idx);
            prop.second = true;
        }
        return prop.first;
    }

    inline void writeData(int idx, const QVariant &value) {
        while (data.count() <= idx)
            data << QPair<QVariant, bool>(QVariant(), false);
        QPair<QVariant, bool> &prop = data[idx];
        prop.first = value;
        prop.second = true;
    }

    inline bool hasData(int idx) const {
        if (idx >= data.count())
            return false;
        return data[idx].second;
    }

    bool autoCreate;
    QDeclarativeOpenMetaObject *q;
    QAbstractDynamicMetaObject *parent;
    QList<QPair<QVariant, bool> > data;
    QObject *object;
    QDeclarativeOpenMetaObjectType *type;
    bool cacheProperties;
};

QDeclarativeOpenMetaObject::QDeclarativeOpenMetaObject(QObject *obj, bool automatic)
: d(new QDeclarativeOpenMetaObjectPrivate(this))
{
    d->autoCreate = automatic;
    d->object = obj;

    d->type = new QDeclarativeOpenMetaObjectType(obj->metaObject(), 0);
    d->type->d->referers.insert(this);

    QObjectPrivate *op = QObjectPrivate::get(obj);
    d->parent = static_cast<QAbstractDynamicMetaObject *>(op->metaObject);
    *static_cast<QMetaObject *>(this) = *d->type->d->mem;
    op->metaObject = this;
}

QDeclarativeOpenMetaObject::QDeclarativeOpenMetaObject(QObject *obj, QDeclarativeOpenMetaObjectType *type, bool automatic)
: d(new QDeclarativeOpenMetaObjectPrivate(this))
{
    d->autoCreate = automatic;
    d->object = obj;

    d->type = type;
    d->type->addref();
    d->type->d->referers.insert(this);

    QObjectPrivate *op = QObjectPrivate::get(obj);
    d->parent = static_cast<QAbstractDynamicMetaObject *>(op->metaObject);
    *static_cast<QMetaObject *>(this) = *d->type->d->mem;
    op->metaObject = this;
}

QDeclarativeOpenMetaObject::~QDeclarativeOpenMetaObject()
{
    if (d->parent)
        delete d->parent;
    d->type->d->referers.remove(this);
    d->type->release();
    delete d;
}

QDeclarativeOpenMetaObjectType *QDeclarativeOpenMetaObject::type() const
{
    return d->type;
}

int QDeclarativeOpenMetaObject::metaCall(QMetaObject::Call c, int id, void **a)
{
    if (( c == QMetaObject::ReadProperty || c == QMetaObject::WriteProperty)
            && id >= d->type->d->propertyOffset) {
        int propId = id - d->type->d->propertyOffset;
        if (c == QMetaObject::ReadProperty) {
            propertyRead(propId);
            *reinterpret_cast<QVariant *>(a[0]) = d->getData(propId);
        } else if (c == QMetaObject::WriteProperty) {
            if (propId <= d->data.count() || d->data[propId].first != *reinterpret_cast<QVariant *>(a[0]))  {
                propertyWrite(propId);
                d->writeData(propId, *reinterpret_cast<QVariant *>(a[0]));
                propertyWritten(propId);
                activate(d->object, d->type->d->signalOffset + propId, 0);
            }
        }
        return -1;
    } else {
        if (d->parent)
            return d->parent->metaCall(c, id, a);
        else
            return d->object->qt_metacall(c, id, a);
    }
}

QAbstractDynamicMetaObject *QDeclarativeOpenMetaObject::parent() const
{
    return d->parent;
}

QVariant QDeclarativeOpenMetaObject::value(int id) const
{
    return d->getData(id);
}

void QDeclarativeOpenMetaObject::setValue(int id, const QVariant &value)
{
    d->writeData(id, value);
    activate(d->object, id + d->type->d->signalOffset, 0);
}

QVariant QDeclarativeOpenMetaObject::value(const QByteArray &name) const
{
    QHash<QByteArray, int>::ConstIterator iter = d->type->d->names.find(name);
    if (iter == d->type->d->names.end())
        return QVariant();

    return d->getData(*iter);
}

QVariant &QDeclarativeOpenMetaObject::operator[](const QByteArray &name)
{
    QHash<QByteArray, int>::ConstIterator iter = d->type->d->names.find(name);
    Q_ASSERT(iter != d->type->d->names.end());

    return d->getData(*iter);
}

QVariant &QDeclarativeOpenMetaObject::operator[](int id)
{
    return d->getData(id);
}

void QDeclarativeOpenMetaObject::setValue(const QByteArray &name, const QVariant &val)
{
    QHash<QByteArray, int>::ConstIterator iter = d->type->d->names.find(name);

    int id = -1;
    if (iter == d->type->d->names.end()) {
        id = createProperty(name.constData(), "") - d->type->d->propertyOffset;
    } else {
        id = *iter;
    }

    if (id >= 0) {
        QVariant &dataVal = d->getData(id);
        if (dataVal == val)
            return;

        dataVal = val;
        activate(d->object, id + d->type->d->signalOffset, 0);
    }
}

// returns true if this value has been initialized by a call to either value() or setValue()
bool QDeclarativeOpenMetaObject::hasValue(int id) const
{
    return d->hasData(id);
}

void QDeclarativeOpenMetaObject::setCached(bool c)
{
    if (c == d->cacheProperties || !d->type->d->engine)
        return;

    d->cacheProperties = c;

    QDeclarativeData *qmldata = QDeclarativeData::get(d->object, true);
    if (d->cacheProperties) {
        if (!d->type->d->cache)
            d->type->d->cache = new QDeclarativePropertyCache(d->type->d->engine, this);
        qmldata->propertyCache = d->type->d->cache;
        d->type->d->cache->addref();
    } else {
        if (d->type->d->cache)
            d->type->d->cache->release();
        qmldata->propertyCache = 0;
    }
}


int QDeclarativeOpenMetaObject::createProperty(const char *name, const char *)
{
    if (d->autoCreate)
        return d->type->createProperty(name);
    else
        return -1;
}

void QDeclarativeOpenMetaObject::propertyRead(int)
{
}

void QDeclarativeOpenMetaObject::propertyWrite(int)
{
}

void QDeclarativeOpenMetaObject::propertyWritten(int)
{
}

void QDeclarativeOpenMetaObject::propertyCreated(int, QMetaPropertyBuilder &)
{
}

QVariant QDeclarativeOpenMetaObject::initialValue(int)
{
    return QVariant();
}

int QDeclarativeOpenMetaObject::count() const
{
    return d->type->d->names.count();
}

QByteArray QDeclarativeOpenMetaObject::name(int idx) const
{
    Q_ASSERT(idx >= 0 && idx < d->type->d->names.count());

    return d->type->d->mob.property(idx).name();
}

QObject *QDeclarativeOpenMetaObject::object() const
{
    return d->object;
}

QT_END_NAMESPACE
