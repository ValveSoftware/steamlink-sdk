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

#include <QtCore/QMetaType>
#include <QString>
#include <QtTest/QtTest>

#include <qgeoserviceprovider.h>
#include <qplacemanager.h>


#ifndef WAIT_UNTIL
#define WAIT_UNTIL(__expr) \
        do { \
        const int __step = 50; \
        const int __timeout = 5000; \
        if (!(__expr)) { \
            QTest::qWait(0); \
        } \
        for (int __i = 0; __i < __timeout && !(__expr); __i+=__step) { \
            QTest::qWait(__step); \
        } \
    } while (0)
#endif

QT_USE_NAMESPACE

class tst_QPlaceManager : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void compatiblePlace();
    void testMetadata();
    void testLocales();
    void testMatchUnsupported();

private:
    bool checkSignals(QPlaceReply *reply, QPlaceReply::Error expectedError);

    QGeoServiceProvider *provider;
    QPlaceManager *placeManager;
};

void tst_QPlaceManager::initTestCase()
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
    provider = 0;

    QStringList providers = QGeoServiceProvider::availableServiceProviders();
    QVERIFY(providers.contains("qmlgeo.test.plugin"));

    provider = new QGeoServiceProvider("qmlgeo.test.plugin");
    provider->setAllowExperimental(true);
    QCOMPARE(provider->placesFeatures() & QGeoServiceProvider::OfflinePlacesFeature,
             QGeoServiceProvider::OfflinePlacesFeature);
    placeManager = provider->placeManager();
    QVERIFY(placeManager);
}

void tst_QPlaceManager::testMetadata()
{
    QCOMPARE(placeManager->managerName(), QStringLiteral("qmlgeo.test.plugin"));
    QCOMPARE(placeManager->managerVersion(), 100);
}

void tst_QPlaceManager::testLocales()
{
    QCOMPARE(placeManager->locales().count(), 1);
    QCOMPARE(placeManager->locales().at(0), QLocale());

    QLocale locale(QLocale::Norwegian, QLocale::Norway);
    placeManager->setLocale(locale);

    QCOMPARE(placeManager->locales().at(0), locale);

    QList<QLocale> locales;
    QLocale en_AU = QLocale(QLocale::English, QLocale::Australia);
    QLocale en_UK = QLocale(QLocale::English, QLocale::UnitedKingdom);
    locales << en_AU << en_UK;
    placeManager->setLocales(locales);
    QCOMPARE(placeManager->locales().count(), 2);
    QCOMPARE(placeManager->locales().at(0), en_AU);
    QCOMPARE(placeManager->locales().at(1), en_UK);
}

void tst_QPlaceManager::testMatchUnsupported()
{
    QPlaceMatchRequest request;
    QPlaceMatchReply *reply = placeManager->matchingPlaces(request);
    QVERIFY(checkSignals(reply, QPlaceReply::UnsupportedError));
}

void tst_QPlaceManager::compatiblePlace()
{
    QPlace place;
    place.setPlaceId(QStringLiteral("4-8-15-16-23-42"));
    place.setName(QStringLiteral("Island"));
    place.setVisibility(QLocation::PublicVisibility);

    QPlace compatPlace = placeManager->compatiblePlace(place);
    QVERIFY(compatPlace.placeId().isEmpty());
    QCOMPARE(compatPlace.name(), QStringLiteral("Island"));
    QCOMPARE(compatPlace.visibility(), QLocation::UnspecifiedVisibility);
}

void tst_QPlaceManager::cleanupTestCase()
{
    delete provider;
}

bool tst_QPlaceManager::checkSignals(QPlaceReply *reply, QPlaceReply::Error expectedError)
{
    QSignalSpy finishedSpy(reply, SIGNAL(finished()));
    QSignalSpy errorSpy(reply, SIGNAL(error(QPlaceReply::Error,QString)));
    QSignalSpy managerFinishedSpy(placeManager, SIGNAL(finished(QPlaceReply*)));
    QSignalSpy managerErrorSpy(placeManager,SIGNAL(error(QPlaceReply*,QPlaceReply::Error,QString)));

    if (expectedError != QPlaceReply::NoError) {
        //check that we get an error signal from the reply
        WAIT_UNTIL(errorSpy.count() == 1);
        if (errorSpy.count() != 1) {
            qWarning() << "Error signal for search operation not received";
            return false;
        }

        //check that we get the correct error from the reply's signal
        QPlaceReply::Error actualError = qvariant_cast<QPlaceReply::Error>(errorSpy.at(0).at(0));
        if (actualError != expectedError) {
            qWarning() << "Actual error code in reply signal does not match expected error code";
            qWarning() << "Actual error code = " << actualError;
            qWarning() << "Expected error coe =" << expectedError;
            return false;
        }

        //check that we get an error  signal from the manager
        WAIT_UNTIL(managerErrorSpy.count() == 1);
        if (managerErrorSpy.count() !=1) {
           qWarning() << "Error signal from manager for search operation not received";
           return false;
        }

        //check that we get the correct reply instance in the error signal from the manager
        if (qvariant_cast<QPlaceReply*>(managerErrorSpy.at(0).at(0)) != reply)  {
            qWarning() << "Reply instance in error signal from manager is incorrect";
            return false;
        }

        //check that we get the correct error from the signal of the manager
        actualError = qvariant_cast<QPlaceReply::Error>(managerErrorSpy.at(0).at(1));
        if (actualError != expectedError) {
            qWarning() << "Actual error code from manager signal does not match expected error code";
            qWarning() << "Actual error code =" << actualError;
            qWarning() << "Expected error code = " << expectedError;
            return false;
        }
    }

    //check that we get a finished signal
    WAIT_UNTIL(finishedSpy.count() == 1);
    if (finishedSpy.count() !=1) {
        qWarning() << "Finished signal from reply not received";
        return false;
    }

    if (reply->error() != expectedError) {
        qWarning() << "Actual error code does not match expected error code";
        qWarning() << "Actual error code: " << reply->error();
        qWarning() << "Expected error code" << expectedError;
        return false;
    }

    if (expectedError == QPlaceReply::NoError && !reply->errorString().isEmpty()) {
        qWarning() << "Expected error was no error but error string was not empty";
        qWarning() << "Error string=" << reply->errorString();
        return false;
    }

    //check that we get the finished signal from the manager
    WAIT_UNTIL(managerFinishedSpy.count() == 1);
    if (managerFinishedSpy.count() != 1) {
        qWarning() << "Finished signal from manager not received";
        return false;
    }

    //check that the reply instance in the finished signal from the manager is correct
    if (qvariant_cast<QPlaceReply *>(managerFinishedSpy.at(0).at(0)) != reply) {
        qWarning() << "Reply instance in finished signal from manager is incorrect";
        return false;
    }

    return true;
}

QTEST_GUILESS_MAIN(tst_QPlaceManager)

#include "tst_qplacemanager.moc"
