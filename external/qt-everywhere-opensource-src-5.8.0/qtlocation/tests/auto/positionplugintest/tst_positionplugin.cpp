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

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QtPositioning/qgeopositioninfosource.h>
#include <QtPositioning/qgeosatelliteinfosource.h>
#include <QtPositioning/qgeoareamonitorsource.h>
#include <QtPositioning/qgeocoordinate.h>

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
#ifdef Q_OS_WIN
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() +
                                     QStringLiteral("/../../../../plugins"));
#else
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath()
                                     + QStringLiteral("/../../../plugins"));
#endif
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
