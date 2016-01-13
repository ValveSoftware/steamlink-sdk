/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "qgeoserviceproviderplugin_nokia.h"

#include "qgeocodingmanagerengine_nokia.h"
#include "qgeoroutingmanagerengine_nokia.h"
#include "qgeotiledmappingmanagerengine_nokia.h"
#include "qplacemanagerengine_nokiav2.h"
#include "qgeointrinsicnetworkaccessmanager.h"
#include "qgeoerror_messages.h"

#include <QtPlugin>
#include <QNetworkProxy>
#include <QCoreApplication>

QT_BEGIN_NAMESPACE

namespace
{
    bool isValidParameter(const QString &param)
    {
        if (param.isEmpty())
            return false;

        if (param.length() > 512)
            return false;

        foreach (QChar c, param) {
            if (!c.isLetterOrNumber() && c.toLatin1() != '%' && c.toLatin1() != '-' &&
                c.toLatin1() != '+' && c.toLatin1() != '_') {
                return false;
            }
        }
        return true;
    }

    QGeoNetworkAccessManager *tryGetNetworkAccessManager(const QVariantMap &parameters)
    {
        return static_cast<QGeoNetworkAccessManager *>(qvariant_cast<void *>(parameters.value(QStringLiteral("nam"))));
    }

    void checkUsageTerms(const QVariantMap &parameters, QGeoServiceProvider::Error *error, QString *errorString)
    {
        const QString appId = parameters.value(QStringLiteral("app_id")).toString();
        const QString token = parameters.value(QStringLiteral("token")).toString();

        if (!isValidParameter(appId) || !isValidParameter(token)) {

            *error = QGeoServiceProvider::MissingRequiredParameterError;
            *errorString = QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, MISSED_CREDENTIALS);
        }
    }

    template<class TInstance>
    TInstance * CreateInstanceOf(const QVariantMap &parameters, QGeoServiceProvider::Error *error, QString *errorString)
    {
        checkUsageTerms(parameters, error, errorString);

        if (*error != QGeoServiceProvider::NoError)
            return 0;

        QGeoNetworkAccessManager *networkManager = tryGetNetworkAccessManager(parameters);
        if (!networkManager)
            networkManager = new QGeoIntrinsicNetworkAccessManager(parameters);

        return new TInstance(networkManager, parameters, error, errorString);
    }
}

QGeoCodingManagerEngine *QGeoServiceProviderFactoryNokia::createGeocodingManagerEngine(
        const QVariantMap &parameters,
        QGeoServiceProvider::Error *error,
        QString *errorString) const
{
    return CreateInstanceOf<QGeoCodingManagerEngineNokia>(parameters, error, errorString);
}

QGeoMappingManagerEngine *QGeoServiceProviderFactoryNokia::createMappingManagerEngine(
        const QVariantMap &parameters,
        QGeoServiceProvider::Error *error,
        QString *errorString) const
{
    return CreateInstanceOf<QGeoTiledMappingManagerEngineNokia>(parameters, error, errorString);
}

QGeoRoutingManagerEngine *QGeoServiceProviderFactoryNokia::createRoutingManagerEngine(
        const QVariantMap &parameters,
        QGeoServiceProvider::Error *error,
        QString *errorString) const
{
    return CreateInstanceOf<QGeoRoutingManagerEngineNokia>(parameters, error, errorString);
}

QPlaceManagerEngine *QGeoServiceProviderFactoryNokia::createPlaceManagerEngine(
        const QVariantMap &parameters,
        QGeoServiceProvider::Error *error,
        QString *errorString) const
{
    return CreateInstanceOf<QPlaceManagerEngineNokiaV2>(parameters, error, errorString);
}

QT_END_NAMESPACE
