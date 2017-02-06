/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qqmldesignermetaobject_p.h"

#include <QSharedPointer>
#include <QMetaProperty>
#include <QtCore/private/qnumeric_p.h>
#include <QDebug>

#include <private/qqmlengine_p.h>
#include <private/qqmlpropertycache_p.h>

QT_BEGIN_NAMESPACE

static QHash<QDynamicMetaObjectData *, bool> nodeInstanceMetaObjectList;
static void (*notifyPropertyChangeCallBack)(QObject*, const QQuickDesignerSupport::PropertyName &propertyName) = 0;

struct MetaPropertyData {
    inline QPair<QVariant, bool> &getDataRef(int idx) {
        while (m_data.count() <= idx)
            m_data << QPair<QVariant, bool>(QVariant(), false);
        return m_data[idx];
    }

    inline QVariant &getData(int idx) {
        QPair<QVariant, bool> &prop = getDataRef(idx);
        if (!prop.second) {
            prop.first = QVariant();
            prop.second = true;
        }
        return prop.first;
    }

    inline bool hasData(int idx) const {
        if (idx >= m_data.count())
            return false;
        return m_data[idx].second;
    }

    inline int count() { return m_data.count(); }

    QVector<QPair<QVariant, bool> > m_data;
};

static QQmlPropertyCache *cacheForObject(QObject *object, QQmlEngine *engine)
{
    QQmlVMEMetaObject *metaObject = QQmlVMEMetaObject::get(object);
    if (metaObject)
        return metaObject->cache;

    return QQmlEnginePrivate::get(engine)->cache(object);
}

QQmlDesignerMetaObject* QQmlDesignerMetaObject::getNodeInstanceMetaObject(QObject *object, QQmlEngine *engine)
{
    //Avoid setting up multiple MetaObjects on the same QObject
    QObjectPrivate *op = QObjectPrivate::get(object);
    QDynamicMetaObjectData *parent = op->metaObject;
    if (nodeInstanceMetaObjectList.contains(parent))
        return static_cast<QQmlDesignerMetaObject *>(parent);

    // we just create one and the ownership goes automatically to the object in nodeinstance see init method

    QQmlData *ddata = QQmlData::get(object, false);

    const bool hadVMEMetaObject = ddata ? ddata->hasVMEMetaObject : false;
    QQmlDesignerMetaObject *mo = new QQmlDesignerMetaObject(object, engine);
    //If our parent is not a VMEMetaObject we just set the flag to false again
    if (ddata)
        ddata->hasVMEMetaObject = hadVMEMetaObject;
    return mo;
}

void QQmlDesignerMetaObject::init(QObject *object, QQmlEngine *engine)
{
    //Creating QQmlOpenMetaObjectType
    m_type = new QQmlOpenMetaObjectType(metaObjectParent(), engine);
    m_type->addref();
    //Assigning type to this
    copyTypeMetaObject();

    //Assign this to object
    QObjectPrivate *op = QObjectPrivate::get(object);
    op->metaObject = this;

    cache = QQmlEnginePrivate::get(engine)->cache(this);

    nodeInstanceMetaObjectList.insert(this, true);
    hasAssignedMetaObjectData = true;
}

QQmlDesignerMetaObject::QQmlDesignerMetaObject(QObject *object, QQmlEngine *engine)
    : QQmlVMEMetaObject(object, cacheForObject(object, engine), /*qml compilation unit*/nullptr, /*qmlObjectId*/-1),
      m_context(engine->contextForObject(object)),
      m_data(new MetaPropertyData)
{
    init(object, engine);

    QQmlData *ddata = QQmlData::get(object, false);
    //Assign cache to object
    if (ddata && ddata->propertyCache) {
        cache->setParent(ddata->propertyCache);
        cache->invalidate(engine, this);
        ddata->propertyCache->release();
        ddata->propertyCache = cache;
        ddata->propertyCache->addref();
    }

}

QQmlDesignerMetaObject::~QQmlDesignerMetaObject()
{
    m_type->release();

    nodeInstanceMetaObjectList.remove(this);
}

void QQmlDesignerMetaObject::createNewDynamicProperty(const QString &name)
{
    int id = m_type->createProperty(name.toUtf8());
    copyTypeMetaObject();
    setValue(id, QVariant());
    Q_ASSERT(id >= 0);
    Q_UNUSED(id);

    //Updating cache
    QQmlPropertyCache *oldParent = cache->parent();
    QQmlEnginePrivate::get(m_context->engine())->cache(this)->invalidate(m_context->engine(), this);
    cache->setParent(oldParent);

    QQmlProperty property(myObject(), name, m_context);
    Q_ASSERT(property.isValid());
}

void QQmlDesignerMetaObject::setValue(int id, const QVariant &value)
{
    QPair<QVariant, bool> &prop = m_data->getDataRef(id);
    prop.first = propertyWriteValue(id, value);
    prop.second = true;
    QMetaObject::activate(myObject(), id + m_type->signalOffset(), 0);
}

QVariant QQmlDesignerMetaObject::propertyWriteValue(int, const QVariant &value)
{
    return value;
}

const QAbstractDynamicMetaObject *QQmlDesignerMetaObject::dynamicMetaObjectParent() const
{
    if (QQmlVMEMetaObject::parent.isT1())
        return QQmlVMEMetaObject::parent.asT1()->toDynamicMetaObject(QQmlVMEMetaObject::object);
    else
        return 0;
}

const QMetaObject *QQmlDesignerMetaObject::metaObjectParent() const
{
    if (QQmlVMEMetaObject::parent.isT1())
        return QQmlVMEMetaObject::parent.asT1()->toDynamicMetaObject(QQmlVMEMetaObject::object);

    return QQmlVMEMetaObject::parent.asT2();
}

int QQmlDesignerMetaObject::propertyOffset() const
{
    return cache->propertyOffset();
}

int QQmlDesignerMetaObject::openMetaCall(QObject *o, QMetaObject::Call call, int id, void **a)
{
    if ((call == QMetaObject::ReadProperty || call == QMetaObject::WriteProperty)
            && id >= m_type->propertyOffset()) {
        int propId = id - m_type->propertyOffset();
        if (call == QMetaObject::ReadProperty) {
            //propertyRead(propId);
            *reinterpret_cast<QVariant *>(a[0]) = m_data->getData(propId);
        } else if (call == QMetaObject::WriteProperty) {
            if (propId <= m_data->count() || m_data->m_data[propId].first != *reinterpret_cast<QVariant *>(a[0]))  {
                //propertyWrite(propId);
                QPair<QVariant, bool> &prop = m_data->getDataRef(propId);
                prop.first = propertyWriteValue(propId, *reinterpret_cast<QVariant *>(a[0]));
                prop.second = true;
                //propertyWritten(propId);
                activate(myObject(), m_type->signalOffset() + propId, 0);
            }
        }
        return -1;
    } else {
        QAbstractDynamicMetaObject *directParent = parent();
        if (directParent)
            return directParent->metaCall(o, call, id, a);
        else
            return myObject()->qt_metacall(call, id, a);
    }
}

int QQmlDesignerMetaObject::metaCall(QObject *o, QMetaObject::Call call, int id, void **a)
{
    Q_ASSERT(myObject() == o);

    int metaCallReturnValue = -1;

    const QMetaProperty propertyById = QQmlVMEMetaObject::property(id);

    if (call == QMetaObject::WriteProperty
            && propertyById.userType() == QMetaType::QVariant
            && reinterpret_cast<QVariant *>(a[0])->type() == QVariant::Double
            && qt_is_nan(reinterpret_cast<QVariant *>(a[0])->toDouble())) {
        return -1;
    }

    if (call == QMetaObject::WriteProperty
            && propertyById.userType() == QMetaType::Double
            && qt_is_nan(*reinterpret_cast<double*>(a[0]))) {
        return -1;
    }

    if (call == QMetaObject::WriteProperty
            && propertyById.userType() == QMetaType::Float
            && qt_is_nan(*reinterpret_cast<float*>(a[0]))) {
        return -1;
    }

    QVariant oldValue;

    if (call == QMetaObject::WriteProperty && !propertyById.hasNotifySignal())
    {
        oldValue = propertyById.read(myObject());
    }

    QAbstractDynamicMetaObject *directParent = parent();
    if (directParent && id < directParent->propertyOffset()) {
        metaCallReturnValue = directParent->metaCall(o, call, id, a);
    } else {
        openMetaCall(o, call, id, a);
    }


    if (call == QMetaObject::WriteProperty
            && !propertyById.hasNotifySignal()
            && oldValue != propertyById.read(myObject()))
        notifyPropertyChange(id);

    return metaCallReturnValue;
}

void QQmlDesignerMetaObject::notifyPropertyChange(int id)
{
    const QMetaProperty propertyById = property(id);

    if (id < propertyOffset()) {
        if (notifyPropertyChangeCallBack)
            notifyPropertyChangeCallBack(myObject(), propertyById.name());
    } else {
        if (notifyPropertyChangeCallBack)
            notifyPropertyChangeCallBack(myObject(), name(id - propertyOffset()));
    }
}

int QQmlDesignerMetaObject::count() const
{
    return m_type->propertyCount();
}

QByteArray QQmlDesignerMetaObject::name(int idx) const
{
    return m_type->propertyName(idx);
}

void QQmlDesignerMetaObject::copyTypeMetaObject()
{
    *static_cast<QMetaObject *>(this) = *m_type->metaObject();
}

void QQmlDesignerMetaObject::registerNotifyPropertyChangeCallBack(void (*callback)(QObject *, const QQuickDesignerSupport::PropertyName &))
{
    notifyPropertyChangeCallBack = callback;
}

QT_END_NAMESPACE
