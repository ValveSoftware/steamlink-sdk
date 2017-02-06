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

//TESTED_COMPONENT=src/location

#ifndef TST_QGEOROUTINGMANAGER_H
#define TST_QGEOROUTINGMANAGER_H

#include <QLocale>
#include <QtTest/QtTest>
#include <qgeoserviceprovider.h>
#include <qgeoroutingmanager.h>
#include <qgeorouterequest.h>
#include <qgeoroutereply.h>
#include <qgeocoordinate.h>
#include <qgeoroute.h>

QT_USE_NAMESPACE

class tst_QGeoRoutingManager: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void supports();
    void locale();
    void name();
    void version();
    void calculate();
    void update();

private:
    QGeoServiceProvider *qgeoserviceprovider;
    QGeoRoutingManager *qgeoroutingmanager;
    QGeoRouteRequest *request;
    QGeoRouteReply *reply;
    QGeoCoordinate *origin;
    QGeoCoordinate *destination;
    QGeoRoute *route;
    QGeoCoordinate *position;
    void loadRoutingManager();

};

#endif
