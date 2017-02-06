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

#include "tst_qgeocodingmanager.h"

QT_USE_NAMESPACE


void tst_QGeoCodingManager::initTestCase()
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
    tst_QGeoCodingManager::loadGeocodingManager();
}

void tst_QGeoCodingManager::cleanupTestCase()
{
    delete qgeoserviceprovider;
}

void tst_QGeoCodingManager::init()
{
    qRegisterMetaType<QGeoCodeReply::Error>();
    qRegisterMetaType<QGeoCodeReply*>();

    signalerror = new QSignalSpy(qgeocodingmanager, SIGNAL(error(QGeoCodeReply*,QGeoCodeReply::Error,QString)));
    signalfinished = new QSignalSpy(qgeocodingmanager, SIGNAL(finished(QGeoCodeReply*)));
    QVERIFY( signalerror->isValid() );
    QVERIFY( signalfinished->isValid() );
}

void tst_QGeoCodingManager::cleanup()
{
    delete signalerror;
    delete signalfinished;
}

void tst_QGeoCodingManager::loadGeocodingManager()
{
    QStringList providers = QGeoServiceProvider::availableServiceProviders();
    QVERIFY(providers.contains("geocode.test.plugin"));

    qgeoserviceprovider = new QGeoServiceProvider("geocode.test.plugin");
    QVERIFY(qgeoserviceprovider);
    QCOMPARE(qgeoserviceprovider->error(), QGeoServiceProvider::NotSupportedError);

    qgeoserviceprovider->setAllowExperimental(true);
    QCOMPARE(qgeoserviceprovider->error(), QGeoServiceProvider::NoError);
    QCOMPARE(qgeoserviceprovider->geocodingFeatures(),
             QGeoServiceProvider::OfflineGeocodingFeature
             | QGeoServiceProvider::ReverseGeocodingFeature);

    qgeocodingmanager = qgeoserviceprovider->geocodingManager();
    QVERIFY(qgeocodingmanager);
}

void tst_QGeoCodingManager::locale()
{
    QLocale *german = new QLocale (QLocale::German, QLocale::Germany);
    QLocale *english = new QLocale (QLocale::C, QLocale::AnyCountry);

    //Default Locale from the Search Engine
    QCOMPARE(qgeocodingmanager->locale(),*german);

    qgeocodingmanager->setLocale(*english);

    QCOMPARE(qgeocodingmanager->locale(),*english);

    QVERIFY(qgeocodingmanager->locale() != *german);

    delete german;
    delete english;
}

void tst_QGeoCodingManager::name()
{
    QString name = "geocode.test.plugin";
    QCOMPARE(qgeocodingmanager->managerName(),name);
}

void tst_QGeoCodingManager::version()
{
    int version=100;
    QCOMPARE(qgeocodingmanager->managerVersion(),version);

}

void tst_QGeoCodingManager::search()
{
    QCOMPARE(signalerror->count(),0);
    QCOMPARE(signalfinished->count(),0);

    QString search = "Berlin. Invaliendenstrasse";
    int limit = 10;
    int offset = 2;

    QGeoCodeReply * reply = qgeocodingmanager->geocode(search, limit,offset);

    QCOMPARE(reply->errorString(),search);
    QCOMPARE(signalfinished->count(),1);
    QCOMPARE(signalerror->count(),0);

    delete reply;
}

void tst_QGeoCodingManager::geocode()
{
    QCOMPARE(signalerror->count(),0);
    QCOMPARE(signalfinished->count(),0);

    QGeoAddress *address = new QGeoAddress ();
    QString city = "Berlin";
    address->setCity(city);

    QGeoCodeReply *reply = qgeocodingmanager->geocode(*address);

    QCOMPARE(reply->errorString(),city);
    QCOMPARE(signalfinished->count(),1);
    QCOMPARE(signalerror->count(),0);

    delete address;
    delete reply;
}

void tst_QGeoCodingManager::reverseGeocode()
{
    QCOMPARE(signalerror->count(), 0);
    QCOMPARE(signalfinished->count(), 0);

    QGeoCoordinate *coordinate = new QGeoCoordinate(34.34, 56.65);

    QGeoCodeReply *reply = qgeocodingmanager->reverseGeocode(*coordinate);

    QCOMPARE(reply->errorString(), coordinate->toString());
    QCOMPARE(signalfinished->count(), 1);
    QCOMPARE(signalerror->count(), 0);

    delete coordinate;
    delete reply;


}


QTEST_GUILESS_MAIN(tst_QGeoCodingManager)

