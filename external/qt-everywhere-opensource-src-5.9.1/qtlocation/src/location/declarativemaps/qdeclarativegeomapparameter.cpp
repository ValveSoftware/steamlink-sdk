/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativegeomapparameter_p.h"

#include <QByteArray>
#include <QMetaObject>
#include <QMetaProperty>
#include <QSignalMapper>

QT_BEGIN_NAMESPACE

/*!
    \qmltype MapParameter
    \instantiates QDeclarativeGeoMapParameter
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.9

    \brief The MapParameter type represents a parameter for a Map element.
    This type provides a mean to specify plugin-dependent optional parameters
    for a map.

    MapParameters by default contain only the \l type property, and
    are highly plugin-dependent.
    For this reason, additional properties have to be defined inside a
    MapParameter at declaration time, using the QML syntax "property var foo".

    What properties have to be put inside a particular MapParameter type for
    a particular plugin can be found in the documentation of the plugin.
    Note that MapProperties are \b optional.
    By not specifying any of them, the Map will have the default behavior.

    The release of this API with Qt 5.9 is a Technology Preview.
*/

/*!
    \qmlproperty string QtLocation::MapParameter::type

    Set-once property which holds a string defining the type of the MapParameter
*/

QDeclarativeGeoMapParameter::QDeclarativeGeoMapParameter(QObject *parent)
:   QGeoMapParameter(parent), m_initialPropertyCount(metaObject()->propertyCount()), m_complete(false)
{

}

QDeclarativeGeoMapParameter::~QDeclarativeGeoMapParameter()
{
}

bool QDeclarativeGeoMapParameter::isComponentComplete() const
{
    return m_complete;
}

int QDeclarativeGeoMapParameter::initialPropertyCount() const
{
    return m_initialPropertyCount;
}

void QDeclarativeGeoMapParameter::classBegin()
{
}

void QDeclarativeGeoMapParameter::componentComplete()
{
    for (int i = m_initialPropertyCount; i < metaObject()->propertyCount(); ++i) {
        QMetaProperty property = metaObject()->property(i);

        if (!property.hasNotifySignal()) {
            return;
        }

        QSignalMapper *mapper = new QSignalMapper(this);
        mapper->setMapping(this, i);

        const QByteArray signalName = '2' + property.notifySignal().methodSignature(); // TODO: explain why '2'
        QObject::connect(this, signalName, mapper, SLOT(map()));
        QObject::connect(mapper, SIGNAL(mapped(int)), this, SLOT(onPropertyUpdated(int)));
    }
    m_complete = true;
    emit completed(this);
}

void QDeclarativeGeoMapParameter::onPropertyUpdated(int index)
{
    emit propertyUpdated(this, metaObject()->property(index).name());
}

QT_END_NAMESPACE
