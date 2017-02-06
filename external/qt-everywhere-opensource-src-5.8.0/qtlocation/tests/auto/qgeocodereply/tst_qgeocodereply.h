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

#ifndef TST_QGEOCODEREPLY_H
#define TST_QGEOCODEREPLY_H

#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QtCore/QCoreApplication>
#include <QMetaType>
#include <QSignalSpy>

#include "../utils/qlocationtestutils_p.h"

#include <qgeocodereply.h>
#include <qgeolocation.h>
#include <qgeoaddress.h>
#include <qgeocoordinate.h>
#include <QtPositioning/QGeoRectangle>

QT_USE_NAMESPACE
class SubGeocodeReply : public QGeoCodeReply
{
    Q_OBJECT
public:
    SubGeocodeReply() : QGeoCodeReply() {}

    void  callAddLocation ( const QGeoLocation & location ) {addLocation(location);}
    void  callSetError ( Error error, const QString & errorString ) {setError(error, errorString);}
    void  callSetFinished ( bool finished ) {setFinished(finished);}
    void  callSetLimit ( int limit ) {setLimit(limit);}
    void  callSetOffset ( int offset ) {setOffset(offset);}
    void  callSetLocations ( const QList<QGeoLocation> & locations ) {setLocations(locations);}
    void  callSetViewport ( const QGeoShape &viewport ) {setViewport(viewport);}

};

class tst_QGeoCodeReply :public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    //Start Unit Test for QGeoCodeReply
private slots:
    void constructor();
    void constructor_error();
    void constructor_error_data();
    void destructor();
    void destructor_data();
    void abort();
    void error();
    void error_data();
    void finished();
    void limit();
    void offset();
    void locations();
    void viewport();

    //End Unit Test for QGeoCodeReply



private:
    QSignalSpy *signalerror;
    QSignalSpy *signalfinished;
    SubGeocodeReply* reply;
    QGeoLocation *qgeolocation;
    QGeoRectangle *qgeoboundingbox;
};

Q_DECLARE_METATYPE(QList<double>);
Q_DECLARE_METATYPE(QGeoCodeReply::Error);

#endif // TST_QGEOCODEREPLY_H

