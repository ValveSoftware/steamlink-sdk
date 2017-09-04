/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
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
