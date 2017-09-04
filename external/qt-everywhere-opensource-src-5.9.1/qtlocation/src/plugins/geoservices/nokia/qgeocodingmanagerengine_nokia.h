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

#ifndef QGEOCODINGMANAGER_NOKIA_H
#define QGEOCODINGMANAGER_NOKIA_H

#include "qgeoserviceproviderplugin_nokia.h"

#include <qgeoserviceprovider.h>
#include <qgeocodingmanagerengine.h>

#include <QLocale>

QT_BEGIN_NAMESPACE

class QGeoNetworkAccessManager;
class QGeoUriProvider;

class QGeoCodingManagerEngineNokia : public QGeoCodingManagerEngine
{
    Q_OBJECT
public:
    QGeoCodingManagerEngineNokia(QGeoNetworkAccessManager *networkManager,
                                 const QVariantMap &parameters,
                                 QGeoServiceProvider::Error *error,
                                 QString *errorString);
    ~QGeoCodingManagerEngineNokia();

    QGeoCodeReply *geocode(const QGeoAddress &address,
                             const QGeoShape &bounds);
    QGeoCodeReply *reverseGeocode(const QGeoCoordinate &coordinate,
                                    const QGeoShape &bounds);

    QGeoCodeReply *geocode(const QString &searchString,
                            int limit,
                            int offset,
                            const QGeoShape &bounds);

private Q_SLOTS:
    void placesFinished();
    void placesError(QGeoCodeReply::Error error, const QString &errorString);

private:
    static QString trimDouble(double degree, int decimalDigits = 10);
    QGeoCodeReply *geocode(QString requestString, const QGeoShape &bounds, bool manualBoundsRequired = true, int limit = -1, int offset = 0);
    QString languageToMarc(QLocale::Language language);
    QString getAuthenticationString() const;

    QGeoNetworkAccessManager *m_networkManager;
    QGeoUriProvider *m_uriProvider;
    QGeoUriProvider *m_reverseGeocodingUriProvider;
    QString m_token;
    QString m_applicationId;
};

QT_END_NAMESPACE

#endif
