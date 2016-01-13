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

#include "qqmlaccessors_p.h"

#include "qqmldata_p.h"
#include "qqmlnotifier_p.h"

QT_BEGIN_NAMESPACE

struct AccessorProperties {
    AccessorProperties();

    QReadWriteLock lock;
    QHash<const QMetaObject *, QQmlAccessorProperties::Properties> properties;
};

Q_GLOBAL_STATIC(AccessorProperties, accessorProperties)

static void buildNameMask(QQmlAccessorProperties::Properties &properties)
{
    quint32 mask = 0;

    for (int ii = 0; ii < properties.count; ++ii) {
        Q_ASSERT(strlen(properties.properties[ii].name) == properties.properties[ii].nameLength);
        Q_ASSERT(properties.properties[ii].nameLength > 0);

        mask |= (1 << qMin(31U, properties.properties[ii].nameLength - 1));
    }

    properties.nameMask = mask;
}

AccessorProperties::AccessorProperties()
{
}

QQmlAccessorProperties::Properties::Properties(Property *properties, int count)
: count(count), properties(properties)
{
    buildNameMask(*this);
}

QQmlAccessorProperties::Properties
QQmlAccessorProperties::properties(const QMetaObject *mo)
{
    AccessorProperties *This = accessorProperties();

    QReadLocker lock(&This->lock);
    return This->properties.value(mo);
}

void QQmlAccessorProperties::registerProperties(const QMetaObject *mo, int count,
                                                        Property *props)
{
    Q_ASSERT(count > 0);

    Properties properties(props, count);

    AccessorProperties *This = accessorProperties();

    QWriteLocker lock(&This->lock);

    Q_ASSERT(!This->properties.contains(mo) || This->properties.value(mo) == properties);

    This->properties.insert(mo, properties);
}

QT_END_NAMESPACE
