/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef QGEOCODINGMANAGERENGINE_TEST_H
#define QGEOCODINGMANAGERENGINE_TEST_H

#include <qgeoserviceprovider.h>
#include <qgeocodingmanagerengine.h>
#include <QLocale>
#include <qgeoaddress.h>
#include <qgeolocation.h>
#include <qgeocodereply.h>
#include <QtPositioning/QGeoCoordinate>

QT_USE_NAMESPACE

class GeocodeReplyTest : public QGeoCodeReply
{
    Q_OBJECT
public:
    GeocodeReplyTest(QObject *parent = 0) : QGeoCodeReply(parent) {}

    void  callAddLocation ( const QGeoLocation & location ) {addLocation(location);}
    void  callSetError ( Error error, const QString & errorString ) {setError(error, errorString);}
    void  callSetFinished ( bool finished ) {setFinished(finished);}
    void  callSetLimit ( int limit ) {setLimit(limit);}
    void  callSetOffset ( int offset ) {setOffset(offset);}
    void  callSetLocations ( const QList<QGeoLocation> & locations ) {setLocations(locations);}
    void  callSetViewport ( const QGeoShape &viewport ) {setViewport(viewport);}

};

class QGeoCodingManagerEngineTest: public QGeoCodingManagerEngine

{
Q_OBJECT
public:
    QGeoCodingManagerEngineTest(const QVariantMap &parameters,
                                QGeoServiceProvider::Error *error, QString *errorString) :
                                QGeoCodingManagerEngine(parameters)
    {
        Q_UNUSED(error)
        Q_UNUSED(errorString)
        setLocale(QLocale(QLocale::German, QLocale::Germany));
    }

    QGeoCodeReply* geocode(const QString &searchString, int limit, int offset, const QGeoShape &bounds)
    {
        GeocodeReplyTest *geocodereply = new GeocodeReplyTest();
        geocodereply->callSetLimit(limit);
        geocodereply->callSetOffset(offset);
        geocodereply->callSetViewport(bounds);
        geocodereply->callSetError(QGeoCodeReply::NoError,searchString);
        geocodereply->callSetFinished(true);
        emit(this->finished(geocodereply));

        return static_cast<QGeoCodeReply*>(geocodereply);
    }

    QGeoCodeReply* geocode (const QGeoAddress &address, const QGeoShape &bounds)
    {
        GeocodeReplyTest *geocodereply = new GeocodeReplyTest();
        geocodereply->callSetViewport(bounds);
        geocodereply->callSetError(QGeoCodeReply::NoError,address.city());
        geocodereply->callSetFinished(true);
        emit(this->finished(geocodereply));

        return static_cast<QGeoCodeReply*>(geocodereply);
    }

    QGeoCodeReply* reverseGeocode(const QGeoCoordinate &coordinate, const QGeoShape &bounds)
    {
        GeocodeReplyTest *geocodereply = new GeocodeReplyTest();
        geocodereply->callSetViewport(bounds);
        geocodereply->callSetError(QGeoCodeReply::NoError,coordinate.toString());
        geocodereply->callSetFinished(true);
        emit(this->finished(geocodereply));
        return static_cast<QGeoCodeReply*>(geocodereply);
    }
};

#endif
