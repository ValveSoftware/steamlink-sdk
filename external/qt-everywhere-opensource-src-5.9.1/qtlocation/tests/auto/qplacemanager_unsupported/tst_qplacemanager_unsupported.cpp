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

#include <QtCore/QtPlugin>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtLocation/QGeoServiceProviderFactory>
#include <QtLocation/QGeoServiceProvider>
#include <QtLocation/QPlaceManager>
#include <QtLocation/QPlaceManagerEngine>
#include <QtLocation/QPlaceSearchReply>
#include <QtTest/QtTest>

QT_USE_NAMESPACE

class tst_QPlaceManagerUnsupported : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testMetadata();
    void testLocales();

    void testGetPlaceDetails();
    void testGetPlaceContent();
    void testSearch();
    void testSearchSuggestions();

    void testSavePlace();
    void testRemovePlace();
    void testSaveCategory();
    void testRemoveCategory();

    void testCategories();

    void compatiblePlace();

    void testMatchUnsupported();

private:
    void checkSignals(QPlaceReply *reply, QPlaceReply::Error expectedError, bool *failed);
    bool checkSignals(QPlaceReply *reply, QPlaceReply::Error expectedError);

    QGeoServiceProvider *m_provider;
    QPlaceManager *m_manager;
};

void tst_QPlaceManagerUnsupported::initTestCase()
{
#if QT_CONFIG(library)
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
#endif

    m_provider = 0;
    m_manager = 0;

    QStringList providers = QGeoServiceProvider::availableServiceProviders();
    QVERIFY(providers.contains("test.places.unsupported"));

    m_provider = new QGeoServiceProvider("test.places.unsupported");
    QVERIFY(m_provider);
    QCOMPARE(m_provider->error(), QGeoServiceProvider::NotSupportedError);
    m_provider->setAllowExperimental(true);
    QCOMPARE(m_provider->error(), QGeoServiceProvider::NoError);

    m_manager = m_provider->placeManager();
    QVERIFY(m_manager);
}

void tst_QPlaceManagerUnsupported::cleanupTestCase()
{
    delete m_provider;
}

void tst_QPlaceManagerUnsupported::testMetadata()
{
    QCOMPARE(m_manager->managerName(), QStringLiteral("test.places.unsupported"));
    QCOMPARE(m_manager->managerVersion(), 1);
    QCOMPARE(m_provider->placesFeatures(), QGeoServiceProvider::NoPlacesFeatures);
}

void tst_QPlaceManagerUnsupported::testLocales()
{
    QVERIFY(m_manager->locales().isEmpty());

    QLocale locale(QLocale::Norwegian, QLocale::Norway);
    m_manager->setLocale(locale);

    QVERIFY(m_manager->locales().isEmpty());

    QList<QLocale> locales;
    QLocale en_AU = QLocale(QLocale::English, QLocale::Australia);
    QLocale en_UK = QLocale(QLocale::English, QLocale::UnitedKingdom);
    locales << en_AU << en_UK;
    m_manager->setLocales(locales);

    QVERIFY(m_manager->locales().isEmpty());
}

void tst_QPlaceManagerUnsupported::testGetPlaceDetails()
{
    QPlaceDetailsReply *reply = m_manager->getPlaceDetails(QString());
    if (!checkSignals(reply, QPlaceReply::UnsupportedError))
        return;
}

void tst_QPlaceManagerUnsupported::testGetPlaceContent()
{
    QPlaceContentReply *reply = m_manager->getPlaceContent(QPlaceContentRequest());
    if (!checkSignals(reply, QPlaceReply::UnsupportedError))
        return;
}

void tst_QPlaceManagerUnsupported::testSearch()
{
    QPlaceSearchReply *reply = m_manager->search(QPlaceSearchRequest());
    if (!checkSignals(reply, QPlaceReply::UnsupportedError))
        return;
}

void tst_QPlaceManagerUnsupported::testSearchSuggestions()
{
    QPlaceSearchSuggestionReply *reply = m_manager->searchSuggestions(QPlaceSearchRequest());
    if (!checkSignals(reply, QPlaceReply::UnsupportedError))
        return;
}

void tst_QPlaceManagerUnsupported::testSavePlace()
{
    QPlaceIdReply *reply = m_manager->savePlace(QPlace());
    if (!checkSignals(reply, QPlaceReply::UnsupportedError))
        return;
}

void tst_QPlaceManagerUnsupported::testRemovePlace()
{
    QPlaceIdReply *reply = m_manager->removePlace(QString());
    if (!checkSignals(reply, QPlaceReply::UnsupportedError))
        return;
}

void tst_QPlaceManagerUnsupported::testSaveCategory()
{
    QPlaceIdReply *reply = m_manager->saveCategory(QPlaceCategory());
    if (!checkSignals(reply, QPlaceReply::UnsupportedError))
        return;
}

void tst_QPlaceManagerUnsupported::testRemoveCategory()
{
    QPlaceIdReply *reply = m_manager->removeCategory(QString());
    if (!checkSignals(reply, QPlaceReply::UnsupportedError))
        return;
}

void tst_QPlaceManagerUnsupported::testCategories()
{
    QPlaceReply *reply = m_manager->initializeCategories();
    if (!checkSignals(reply, QPlaceReply::UnsupportedError))
        return;

    QVERIFY(m_manager->childCategoryIds().isEmpty());
    QVERIFY(m_manager->parentCategoryId(QString()).isEmpty());
    QCOMPARE(m_manager->category(QString()), QPlaceCategory());
}

void tst_QPlaceManagerUnsupported::compatiblePlace()
{
    QPlace place;
    place.setPlaceId(QStringLiteral("4-8-15-16-23-42"));
    place.setName(QStringLiteral("Island"));
    place.setVisibility(QLocation::PublicVisibility);

    QPlace compatPlace = m_manager->compatiblePlace(place);
    QCOMPARE(compatPlace, QPlace());
}

void tst_QPlaceManagerUnsupported::testMatchUnsupported()
{
    QPlaceMatchReply *reply = m_manager->matchingPlaces(QPlaceMatchRequest());
    if (!checkSignals(reply, QPlaceReply::UnsupportedError))
        return;
}

void tst_QPlaceManagerUnsupported::checkSignals(QPlaceReply *reply,
                                                QPlaceReply::Error expectedError, bool *failed)
{
    *failed = true;

    QSignalSpy finishedSpy(reply, SIGNAL(finished()));
    QSignalSpy errorSpy(reply, SIGNAL(error(QPlaceReply::Error,QString)));
    QSignalSpy managerFinishedSpy(m_manager, SIGNAL(finished(QPlaceReply*)));
    QSignalSpy managerErrorSpy(m_manager,SIGNAL(error(QPlaceReply*,QPlaceReply::Error,QString)));

    if (expectedError != QPlaceReply::NoError) {
        //check that we get an error signal from the reply
        QTRY_VERIFY(errorSpy.count() == 1);

        //check that we get the correct error from the reply's signal
        QPlaceReply::Error actualError = qvariant_cast<QPlaceReply::Error>(errorSpy.at(0).at(0));
        QCOMPARE(actualError, expectedError);

        //check that we get an error  signal from the manager
        QTRY_VERIFY(managerErrorSpy.count() == 1);

        //check that we get the correct reply instance in the error signal from the manager
        QPlaceReply *managerReply = qvariant_cast<QPlaceReply*>(managerErrorSpy.at(0).at(0));
        QCOMPARE(managerReply, reply);

        //check that we get the correct error from the signal of the manager
        actualError = qvariant_cast<QPlaceReply::Error>(managerErrorSpy.at(0).at(1));
        QCOMPARE(actualError, expectedError);
    }

    //check that we get a finished signal
    QTRY_VERIFY(finishedSpy.count() == 1);

    QCOMPARE(reply->error(), expectedError);

    QCOMPARE(reply->errorString().isEmpty(), expectedError == QPlaceReply::NoError);

    //check that we get the finished signal from the manager
    QTRY_VERIFY(managerFinishedSpy.count() == 1);

    //check that the reply instance in the finished signal from the manager is correct
    QPlaceReply *managerReply = qvariant_cast<QPlaceReply *>(managerFinishedSpy.at(0).at(0));
    QCOMPARE(managerReply, reply);

    *failed = false;
}

bool tst_QPlaceManagerUnsupported::checkSignals(QPlaceReply *reply,
                                                QPlaceReply::Error expectedError)
{
    bool failed;
    checkSignals(reply, expectedError, &failed);
    return failed;
}

QTEST_GUILESS_MAIN(tst_QPlaceManagerUnsupported)

#include "tst_qplacemanager_unsupported.moc"
