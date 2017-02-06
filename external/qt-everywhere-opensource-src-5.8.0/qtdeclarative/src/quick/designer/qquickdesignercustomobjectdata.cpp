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

#include "qquickdesignersupportmetainfo_p.h"
#include "qquickdesignersupportproperties_p.h"

#include "qquickdesignercustomobjectdata_p.h"

#include <QGlobalStatic>
#include <QQmlContext>
#include <QQmlEngine>

#include <private/qqmlbinding_p.h>

QT_BEGIN_NAMESPACE

typedef QHash<QObject*, QQuickDesignerCustomObjectData*> CustomObjectDataHash;
Q_GLOBAL_STATIC(CustomObjectDataHash, s_designerObjectToDataHash)

struct HandleDestroyedFunctor {
  QQuickDesignerCustomObjectData *data;
  void operator()() { data->handleDestroyed(); }
};

QQuickDesignerCustomObjectData::QQuickDesignerCustomObjectData(QObject *object)
    : m_object(object)
{
    if (object) {
        populateResetHashes();
        s_designerObjectToDataHash()->insert(object, this);

        HandleDestroyedFunctor functor;
        functor.data = this;
        QObject::connect(object, &QObject::destroyed, functor);
    }
}

void QQuickDesignerCustomObjectData::registerData(QObject *object)
{
    new QQuickDesignerCustomObjectData(object);
}

QQuickDesignerCustomObjectData *QQuickDesignerCustomObjectData::get(QObject *object)
{
    return s_designerObjectToDataHash()->value(object);
}

QVariant QQuickDesignerCustomObjectData::getResetValue(QObject *object, const QQuickDesignerSupport::PropertyName &propertyName)
{
    QQuickDesignerCustomObjectData* data = get(object);

    if (data)
        return data->getResetValue(propertyName);

    return QVariant();
}

void QQuickDesignerCustomObjectData::doResetProperty(QObject *object, QQmlContext *context, const QQuickDesignerSupport::PropertyName &propertyName)
{
    QQuickDesignerCustomObjectData* data = get(object);

    if (data)
        data->doResetProperty(context, propertyName);
}

bool QQuickDesignerCustomObjectData::hasValidResetBinding(QObject *object, const QQuickDesignerSupport::PropertyName &propertyName)
{
    QQuickDesignerCustomObjectData* data = get(object);

    if (data)
        return data->hasValidResetBinding(propertyName);

    return false;
}

bool QQuickDesignerCustomObjectData::hasBindingForProperty(QObject *object,
                                                     QQmlContext *context,
                                                     const QQuickDesignerSupport::PropertyName &propertyName,
                                                     bool *hasChanged)
{
    QQuickDesignerCustomObjectData* data = get(object);

    if (data)
        return data->hasBindingForProperty(context, propertyName, hasChanged);

    return false;
}

void QQuickDesignerCustomObjectData::setPropertyBinding(QObject *object,
                                                  QQmlContext *context,
                                                  const QQuickDesignerSupport::PropertyName &propertyName,
                                                  const QString &expression)
{
    QQuickDesignerCustomObjectData* data = get(object);

    if (data)
        data->setPropertyBinding(context, propertyName, expression);
}

void QQuickDesignerCustomObjectData::keepBindingFromGettingDeleted(QObject *object,
                                                             QQmlContext *context,
                                                             const QQuickDesignerSupport::PropertyName &propertyName)
{
    QQuickDesignerCustomObjectData* data = get(object);

    if (data)
        data->keepBindingFromGettingDeleted(context, propertyName);
}

void QQuickDesignerCustomObjectData::populateResetHashes()
{
    const QQuickDesignerSupport::PropertyNameList propertyNameList =
            QQuickDesignerSupportProperties::propertyNameListForWritableProperties(object());

    for (const QQuickDesignerSupport::PropertyName &propertyName : propertyNameList) {
        QQmlProperty property(object(), QString::fromUtf8(propertyName), QQmlEngine::contextForObject(object()));

        QQmlAbstractBinding::Ptr binding = QQmlAbstractBinding::Ptr(QQmlPropertyPrivate::binding(property));

        if (binding) {
            m_resetBindingHash.insert(propertyName, binding);
        } else if (property.isWritable()) {
            m_resetValueHash.insert(propertyName, property.read());
        }
    }
}

QObject *QQuickDesignerCustomObjectData::object() const
{
    return m_object;
}

QVariant QQuickDesignerCustomObjectData::getResetValue(const QQuickDesignerSupport::PropertyName &propertyName) const
{
    return m_resetValueHash.value(propertyName);
}

void QQuickDesignerCustomObjectData::doResetProperty(QQmlContext *context, const QQuickDesignerSupport::PropertyName &propertyName)
{
    QQmlProperty property(object(), QString::fromUtf8(propertyName), context);

    if (!property.isValid())
        return;

    QQmlAbstractBinding *binding = QQmlPropertyPrivate::binding(property);
    if (binding && !(hasValidResetBinding(propertyName) && getResetBinding(propertyName) == binding)) {
        binding->setEnabled(false, 0);
    }


    if (hasValidResetBinding(propertyName)) {
        QQmlAbstractBinding *binding = getResetBinding(propertyName);

#if defined(QT_NO_DYNAMIC_CAST)
        QQmlBinding *qmlBinding = static_cast<QQmlBinding*>(binding);
#else
        QQmlBinding *qmlBinding = dynamic_cast<QQmlBinding*>(binding);
#endif
        if (qmlBinding)
            qmlBinding->setTarget(property);
        QQmlPropertyPrivate::setBinding(binding, QQmlPropertyPrivate::None, QQmlPropertyData::DontRemoveBinding);
        if (qmlBinding)
            qmlBinding->update();

    } else if (property.isResettable()) {
        property.reset();
    } else if (property.propertyTypeCategory() == QQmlProperty::List) {
        QQmlListReference list = qvariant_cast<QQmlListReference>(property.read());

        if (!QQuickDesignerSupportProperties::hasFullImplementedListInterface(list)) {
            qWarning() << "Property list interface not fully implemented for Class " << property.property().typeName() << " in property " << property.name() << "!";
            return;
        }

        list.clear();
    } else if (property.isWritable()) {
        if (property.read() == getResetValue(propertyName))
            return;

        property.write(getResetValue(propertyName));
    }
}

bool QQuickDesignerCustomObjectData::hasValidResetBinding(const QQuickDesignerSupport::PropertyName &propertyName) const
{
    return m_resetBindingHash.contains(propertyName) &&  m_resetBindingHash.value(propertyName).data();
}

QQmlAbstractBinding *QQuickDesignerCustomObjectData::getResetBinding(const QQuickDesignerSupport::PropertyName &propertyName) const
{
    return m_resetBindingHash.value(propertyName).data();
}

bool QQuickDesignerCustomObjectData::hasBindingForProperty(QQmlContext *context,
                                                     const QQuickDesignerSupport::PropertyName &propertyName,
                                                     bool *hasChanged) const
{
    if (QQuickDesignerSupportProperties::isPropertyBlackListed(propertyName))
        return false;

    QQmlProperty property(object(), QString::fromUtf8(propertyName), context);

    bool hasBinding = QQmlPropertyPrivate::binding(property);

    if (hasChanged) {
        *hasChanged = hasBinding != m_hasBindingHash.value(propertyName, false);
        if (*hasChanged)
            m_hasBindingHash.insert(propertyName, hasBinding);
    }

    return QQmlPropertyPrivate::binding(property);
}

void QQuickDesignerCustomObjectData::setPropertyBinding(QQmlContext *context,
                                                  const QQuickDesignerSupport::PropertyName &propertyName,
                                                  const QString &expression)
{
    QQmlProperty property(object(), QString::fromUtf8(propertyName), context);

    if (!property.isValid())
        return;

    if (property.isProperty()) {
        QQmlBinding *binding = QQmlBinding::create(&QQmlPropertyPrivate::get(property)->core,
                                                   expression, object(), context);
        binding->setTarget(property);
        binding->setNotifyOnValueChanged(true);

        QQmlPropertyPrivate::setBinding(binding, QQmlPropertyPrivate::None, QQmlPropertyData::DontRemoveBinding);
        //Refcounting is taking take care of deletion
        binding->update();
        if (binding->hasError()) {
            if (property.property().userType() == QVariant::String)
                property.write(QVariant(QLatin1Char('#') + expression + QLatin1Char('#')));
        }

    } else {
        qWarning() << Q_FUNC_INFO << ": Cannot set binding for property" << propertyName << ": property is unknown for type";
    }
}

void QQuickDesignerCustomObjectData::keepBindingFromGettingDeleted(QQmlContext *context,
                                                             const QQuickDesignerSupport::PropertyName &propertyName)
{
    //Refcounting is taking care
    Q_UNUSED(context)
    Q_UNUSED(propertyName)
}

void QQuickDesignerCustomObjectData::handleDestroyed()
{
    s_designerObjectToDataHash()->remove(m_object);
    delete this;
}

QT_END_NAMESPACE

