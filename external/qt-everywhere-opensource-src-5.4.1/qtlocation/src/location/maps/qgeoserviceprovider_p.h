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
****************************************************************************/

#ifndef QGEOSERVICEPROVIDER_P_H
#define QGEOSERVICEPROVIDER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qgeoserviceprovider.h"

#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QLocale>

QT_BEGIN_NAMESPACE

class QGeoCodingManager;
class QGeoRoutingManager;
class QGeoMappingManager;

class QGeoServiceProviderFactory;

class QGeoServiceProviderPrivate
{
public:
    QGeoServiceProviderPrivate();
    ~QGeoServiceProviderPrivate();

    void loadMeta();
    void loadPlugin(const QVariantMap &parameters);
    void unload();

    /* helper templates for generating the feature and manager accessors */
    template <class Manager, class Engine>
    Manager *manager(QGeoServiceProvider::Error *error,
                     QString *errorString, Manager **manager);
    template <class Flags>
    Flags features(const char *enumName);

    QGeoServiceProviderFactory *factory;
    QJsonObject metaData;

    QVariantMap parameterMap;

    bool experimental;

    QGeoCodingManager *geocodingManager;
    QGeoRoutingManager *routingManager;
    QGeoMappingManager *mappingManager;
    QPlaceManager *placeManager;

    QGeoServiceProvider::Error geocodeError;
    QGeoServiceProvider::Error routingError;
    QGeoServiceProvider::Error mappingError;
    QGeoServiceProvider::Error placeError;

    QString geocodeErrorString;
    QString routingErrorString;
    QString mappingErrorString;
    QString placeErrorString;

    QGeoServiceProvider::Error error;
    QString errorString;

    QString providerName;

    QLocale locale;
    bool localeSet;

    static QHash<QString, QJsonObject> plugins(bool reload = false);
    static void loadPluginMetadata(QHash<QString, QJsonObject> &list);
};

QT_END_NAMESPACE

#endif
