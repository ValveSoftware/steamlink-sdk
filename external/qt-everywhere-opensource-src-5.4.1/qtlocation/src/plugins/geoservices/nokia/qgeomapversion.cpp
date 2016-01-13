/****************************************************************************
**
** Copyright (C) 2014 Appello Systems AB.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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
** This file is part of the Nokia services plugin for the Maps and
** Navigation API.  The use of these services, whether by use of the
** plugin or by other means, is governed by the terms and conditions
** described by the file NOKIA_TERMS_AND_CONDITIONS.txt in
** this package, located in the directory containing the Nokia services
** plugin source code.
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
