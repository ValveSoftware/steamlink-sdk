/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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
        QString appId, token;

        appId = parameters.value(QStringLiteral("here.app_id")).toString();
        token = parameters.value(QStringLiteral("here.token")).toString();

        if (isValidParameter(appId) && isValidParameter(token))
             return;
        else if (!isValidParameter(appId))
            qWarning() << "Invalid here.app_id";
        else
            qWarning() << "Invalid here.token";

        if (parameters.contains(QStringLiteral("app_id")) || parameters.contains(QStringLiteral("token")))
            qWarning() << QStringLiteral("Please prefix 'app_id' and 'token' with prefix 'here' (e.g.: 'here.app_id')");

        *error = QGeoServiceProvider::MissingRequiredParameterError;
        *errorString = QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, MISSED_CREDENTIALS);
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
