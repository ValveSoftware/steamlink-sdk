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

#ifndef TST_QGEOCODINGMANAGER_H
#define TST_QGEOCODINGMANAGER_H

#include <QLocale>
#include <QtTest/QtTest>
#include <QSignalSpy>

#include <qgeoserviceprovider.h>
#include <qgeocodingmanager.h>
#include <qgeocodereply.h>
#include <QtPositioning/QGeoRectangle>
#include <qgeoaddress.h>
#include <qgeocoordinate.h>


QT_USE_NAMESPACE

class tst_QGeoCodingManager: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void locale();
    void name();
    void version();
    void search();
    void geocode();
    void reverseGeocode();

private:
    QGeoServiceProvider *qgeoserviceprovider;
    QGeoCodingManager *qgeocodingmanager;
    QSignalSpy *signalerror;
    QSignalSpy *signalfinished;
    void loadGeocodingManager();

};
Q_DECLARE_METATYPE(QGeoCodeReply*);
Q_DECLARE_METATYPE(QGeoCodeReply::Error);

#endif

