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

#include "qquickdesignersupportproperties_p.h"

#include "qqmldesignermetaobject_p.h"
#include "qquickdesignercustomobjectdata_p.h"

QT_BEGIN_NAMESPACE

static void addToPropertyNameListIfNotBlackListed(QQuickDesignerSupport::PropertyNameList *propertyNameList,
                                                  const QQuickDesignerSupport::PropertyName &propertyName)
{
    if (!QQuickDesignerSupportProperties::isPropertyBlackListed(propertyName))
        propertyNameList->append(propertyName);
}

void QQuickDesignerSupportProperties::createNewDynamicProperty(QObject *object,  QQmlEngine *engine, const QString &name)
{
    QQmlDesignerMetaObject::getNodeInstanceMetaObject(object, engine)->createNewDynamicProperty(name);
}

void QQuickDesignerSupportProperties::registerNodeInstanceMetaObject(QObject *object, QQmlEngine *engine)
{
    // we just create one and the ownership goes automatically to the object in nodeinstance see init method
    QQmlDesignerMetaObject::getNodeInstanceMetaObject(object, engine);
}

bool QQuickDesignerSupportProperties::hasFullImplementedListInterface(const QQmlListReference &list)
{
    return list.isValid() && list.canCount() && list.canAt() && list.canAppend() && list.canClear();
}

void QQuickDesignerSupportProperties::registerCustomData(QObject *object)
{
    QQuickDesignerCustomObjectData::registerData(object);
}

QVariant QQuickDesignerSupportProperties::getResetValue(QObject *object, const QQuickDesignerSupport::PropertyName &propertyName)
{
    return QQuickDesignerCustomObjectData::getResetValue(object, propertyName);
}

void QQuickDesignerSupportProperties::doResetProperty(QObject *object, QQmlContext *context, const QQuickDesignerSupport::PropertyName &propertyName)
{
    QQuickDesignerCustomObjectData::doResetProperty(object, context, propertyName);
}

bool QQuickDesignerSupportProperties::hasValidResetBinding(QObject *object, const QQuickDesignerSupport::PropertyName &propertyName)
{
    return QQuickDesignerCustomObjectData::hasValidResetBinding(object, propertyName);
}

bool QQuickDesignerSupportProperties::hasBindingForProperty(QObject *object,
                                                      QQmlContext *context,
                                                      const QQuickDesignerSupport::PropertyName &propertyName,
                                                      bool *hasChanged)
{
    return QQuickDesignerCustomObjectData::hasBindingForProperty(object, context, propertyName, hasChanged);
}

void QQuickDesignerSupportProperties::setPropertyBinding(QObject *object,
                                                   QQmlContext *context,
                                                   const QQuickDesignerSupport::PropertyName &propertyName,
                                                   const QString &expression)
{
    QQuickDesignerCustomObjectData::setPropertyBinding(object, context, propertyName, expression);
}

void QQuickDesignerSupportProperties::keepBindingFromGettingDeleted(QObject *object,
                                                              QQmlContext *context,
                                                              const QQuickDesignerSupport::PropertyName &propertyName)
{
    QQuickDesignerCustomObjectData::keepBindingFromGettingDeleted(object, context, propertyName);
}

bool QQuickDesignerSupportProperties::isPropertyQObject(const QMetaProperty &metaProperty)
{
    return QQmlMetaType::isQObject(metaProperty.userType());
}


QObject *QQuickDesignerSupportProperties::readQObjectProperty(const QMetaProperty &metaProperty, QObject *object)
{
    return QQmlMetaType::toQObject(metaProperty.read(object));
}

void QQuickDesignerSupportProperties::getPropertyCache(QObject *object, QQmlEngine *engine)
{
    QQmlEnginePrivate::get(engine)->cache(object->metaObject());
}

QQuickDesignerSupport::PropertyNameList QQuickDesignerSupportProperties::propertyNameListForWritableProperties(QObject *object,
                                                       const QQuickDesignerSupport::PropertyName &baseName,
                                                       QObjectList *inspectedObjects)
{
    QQuickDesignerSupport::PropertyNameList propertyNameList;

    QObjectList localObjectList;

    if (inspectedObjects == 0)
        inspectedObjects = &localObjectList;


    if (inspectedObjects->contains(object))
        return propertyNameList;

    inspectedObjects->append(object);

    const QMetaObject *metaObject = object->metaObject();
    for (int index = 0; index < metaObject->propertyCount(); ++index) {
        QMetaProperty metaProperty = metaObject->property(index);
        QQmlProperty declarativeProperty(object, QString::fromUtf8(metaProperty.name()));
        if (declarativeProperty.isValid() && !declarativeProperty.isWritable() && declarativeProperty.propertyTypeCategory() == QQmlProperty::Object) {
            if (declarativeProperty.name() != QLatin1String("parent")) {
                QObject *childObject = QQmlMetaType::toQObject(declarativeProperty.read());
                if (childObject)
                    propertyNameList.append(propertyNameListForWritableProperties(childObject,
                                                                                  baseName +  QQuickDesignerSupport::PropertyName(metaProperty.name())
                                                                                  + '.', inspectedObjects));
            }
        } else if (QQmlValueTypeFactory::valueType(metaProperty.userType())) {
            QQmlValueType *valueType = QQmlValueTypeFactory::valueType(metaProperty.userType());
            valueType->setValue(metaProperty.read(object));
            propertyNameList.append(propertyNameListForWritableProperties(valueType,
                                                                          baseName +  QQuickDesignerSupport::PropertyName(metaProperty.name())
                                                                          + '.', inspectedObjects));
        }

        if (metaProperty.isReadable() && metaProperty.isWritable()) {
            addToPropertyNameListIfNotBlackListed(&propertyNameList,
                                                  baseName + QQuickDesignerSupport::PropertyName(metaProperty.name()));
        }
    }

    return propertyNameList;
}

bool QQuickDesignerSupportProperties::isPropertyBlackListed(const QQuickDesignerSupport::PropertyName &propertyName)
{
    if (propertyName.contains(".") && propertyName.contains("__"))
        return true;

    if (propertyName.count(".") > 1)
        return true;

    return false;
}

QQuickDesignerSupport::PropertyNameList QQuickDesignerSupportProperties::allPropertyNames(QObject *object,
                                  const QQuickDesignerSupport::PropertyName &baseName,
                                  QObjectList *inspectedObjects)
{
    QQuickDesignerSupport::PropertyNameList propertyNameList;

    QObjectList localObjectList;

    if (inspectedObjects == 0)
        inspectedObjects = &localObjectList;


    if (inspectedObjects->contains(object))
        return propertyNameList;

    inspectedObjects->append(object);


    const QMetaObject *metaObject = object->metaObject();
    for (int index = 0; index < metaObject->propertyCount(); ++index) {
        QMetaProperty metaProperty = metaObject->property(index);
        QQmlProperty declarativeProperty(object, QString::fromUtf8(metaProperty.name()));
        if (declarativeProperty.isValid() && declarativeProperty.propertyTypeCategory() == QQmlProperty::Object) {
            if (declarativeProperty.name() != QLatin1String("parent")) {
                QObject *childObject = QQmlMetaType::toQObject(declarativeProperty.read());
                if (childObject)
                    propertyNameList.append(allPropertyNames(childObject,
                                                             baseName
                                                             + QQuickDesignerSupport::PropertyName(metaProperty.name())
                                                             + '.', inspectedObjects));
            }
        } else if (QQmlValueTypeFactory::valueType(metaProperty.userType())) {
            QQmlValueType *valueType = QQmlValueTypeFactory::valueType(metaProperty.userType());
            valueType->setValue(metaProperty.read(object));
            propertyNameList.append(baseName + QQuickDesignerSupport::PropertyName(metaProperty.name()));
            propertyNameList.append(allPropertyNames(valueType,
                                                     baseName
                                                     + QQuickDesignerSupport::PropertyName(metaProperty.name())
                                                     + '.', inspectedObjects));
        } else  {
            addToPropertyNameListIfNotBlackListed(&propertyNameList,
                                                  baseName + QQuickDesignerSupport::PropertyName(metaProperty.name()));
        }
    }

    return propertyNameList;
}


QT_END_NAMESPACE




