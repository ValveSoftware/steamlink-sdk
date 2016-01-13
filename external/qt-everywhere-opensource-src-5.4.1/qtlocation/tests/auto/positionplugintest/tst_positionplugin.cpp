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

//TESTED_COMPONENT=src/location

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <qgeopositioninfosource.h>
#include <qgeosatelliteinfosource.h>
#include <qgeoareamonitorsource.h>
#include <qgeocoordinate.h>

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QGeoPositionInfo)

class tst_PositionPlugin : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void availableSources();
    void create();
    void getUpdates();
};

void tst_PositionPlugin::initTestCase()
{
    /*
     * Set custom path since CI doesn't install test plugins
     */
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath()
                                     + QStringLiteral("/../../../plugins"));
    qRegisterMetaType<QGeoPositionInfo>();
}

void tst_PositionPlugin::availableSources()
{
    QVERIFY(QGeoPositionInfoSource::availableSources().contains("test.source"));
    QVERIFY(!QGeoSatelliteInfoSource::availableSources().contains("test.source"));
    QVERIFY(!QGeoAreaMonitorSource::availableSources().contains("test.source"));
}

void tst_PositionPlugin::create()
{
    QGeoPositionInfoSource *src = 0;
    src = QGeoPositionInfoSource::createSource("test.source", 0);
    QVERIFY(src != 0);

    QVERIFY(src->minimumUpdateInterval() == 1000);

    src = QGeoPositionInfoSource::createSource("invalid source that will never exist", 0);
    QVERIFY(src == 0);

    QGeoSatelliteInfoSource *ssrc = 0;
    ssrc = QGeoSatelliteInfoSource::createSource("test.source", 0);
    QVERIFY(ssrc == 0);
}

void tst_PositionPlugin::getUpdates()
{
    QGeoPositionInfoSource *src = QGeoPositionInfoSource::createSource("test.source", 0);
    src->setUpdateInterval(1000);

    QSignalSpy spy(src, SIGNAL(positionUpdated(QGeoPositionInfo)));
    src->startUpdates();
    QTest::qWait(1500);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy[0].size(), 1);

    QGeoPositionInfo info = qvariant_cast<QGeoPositionInfo>(spy[0][0]);
    QCOMPARE(info.coordinate().latitude(), 0.1);
    QCOMPARE(info.coordinate().longitude(), 0.1);
}



QTEST_GUILESS_MAIN(tst_PositionPlugin)
#include "tst_positionplugin.moc"
