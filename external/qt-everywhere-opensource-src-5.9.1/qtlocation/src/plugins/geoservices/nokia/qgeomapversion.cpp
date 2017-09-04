/****************************************************************************
**
** Copyright (C) 2014 Appello Systems AB.
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

#include "qgeomapversion.h"

#include <QJsonDocument>

QT_BEGIN_NAMESPACE

QGeoMapVersion::QGeoMapVersion()
    : m_version(-1) {}

bool QGeoMapVersion::isNewVersion(const QJsonObject &newVersionData)
{
    return m_versionData != newVersionData;
}

int QGeoMapVersion::version() const
{
    return m_version;
}

void QGeoMapVersion::setVersion(int version)
{
    m_version = version;
}

void QGeoMapVersion::setVersionData(const QJsonObject &versionData)
{
    m_versionData = versionData;
}


QByteArray QGeoMapVersion::toJson() const
{

    QJsonObject object;
    object[QLatin1String("version")] = m_version;
    object[QLatin1String("data")] = m_versionData;

    QJsonDocument document(object);

    return document.toJson();
}

QT_END_NAMESPACE
