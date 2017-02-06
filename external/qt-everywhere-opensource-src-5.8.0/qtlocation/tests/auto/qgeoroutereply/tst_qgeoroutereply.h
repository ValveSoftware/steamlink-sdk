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

#ifndef TST_QGEOROUTEREPLY_H
#define TST_QGEOROUTEREPLY_H

#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QMetaType>
#include <QSignalSpy>

#include <qgeocoordinate.h>
#include <qgeorouterequest.h>
#include <qgeoroutereply.h>
#include <qgeoroute.h>

QT_USE_NAMESPACE
class SubRouteReply :public QGeoRouteReply
{
    Q_OBJECT
public:
    SubRouteReply(QGeoRouteRequest request):QGeoRouteReply(request) {}
    void callSetError(QGeoRouteReply::Error error, QString msg) {setError(error,msg);}
    void callSetFinished(bool finished) {setFinished(finished);}
    void callSetRoutes(const QList<QGeoRoute> & routes ) {setRoutes(routes);}
};

class tst_QGeoRouteReply :public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    //Start Unit Test for QGeoRouteReply
private slots:
    void constructor();
    void constructor_error();
    void constructor_error_data();
    void destructor();
    void destructor_data();
    void routes();
    void finished();
    void abort();
    void error();
    void error_data();
    void request();
    //End Unit Test for QGeoRouteReply


private:
    QGeoCoordinate *qgeocoordinate1;
    QGeoCoordinate *qgeocoordinate2;
    QGeoCoordinate *qgeocoordinate3;
    QGeoRouteRequest *qgeorouterequest;
    QSignalSpy *signalerror;
    QSignalSpy *signalfinished;
    QList<QGeoCoordinate> waypoints;
    SubRouteReply* reply;
};

Q_DECLARE_METATYPE( QList<QGeoRoute>);
Q_DECLARE_METATYPE( QList<QGeoCoordinate>);
Q_DECLARE_METATYPE( QList<double>);
Q_DECLARE_METATYPE( QGeoRouteReply::Error);

#endif // TST_QGEOROUTEREPLY_H

