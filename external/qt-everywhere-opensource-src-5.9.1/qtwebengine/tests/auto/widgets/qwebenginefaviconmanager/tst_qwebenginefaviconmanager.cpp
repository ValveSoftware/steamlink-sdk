/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include <QtTest/QtTest>
#include "../util.h"

#include <qwebenginepage.h>
#include <qwebenginesettings.h>
#include <qwebengineview.h>


class tst_QWebEngineFaviconManager : public QObject {
    Q_OBJECT

public Q_SLOTS:
    void init();
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

private Q_SLOTS:
    void faviconLoad();
    void faviconLoadFromResources();
    void faviconLoadEncodedUrl();
    void noFavicon();
    void aboutBlank();
    void unavailableFavicon();
    void errorPageEnabled();
    void errorPageDisabled();
    void bestFavicon();
    void touchIcon();
    void multiIcon();
    void candidateIcon();
    void downloadIconsDisabled_data();
    void downloadIconsDisabled();
    void downloadTouchIconsEnabled_data();
    void downloadTouchIconsEnabled();

private:
    QWebEngineView *m_view;
    QWebEnginePage *m_page;
};


void tst_QWebEngineFaviconManager::init()
{
    m_view = new QWebEngineView();
    m_page = m_view->page();
}


void tst_QWebEngineFaviconManager::initTestCase()
{
}

void tst_QWebEngineFaviconManager::cleanupTestCase()
{
}


void tst_QWebEngineFaviconManager::cleanup()
{
    delete m_view;
}

void tst_QWebEngineFaviconManager::faviconLoad()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QUrl url = QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/favicon-single.html"));
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QTRY_COMPARE(iconUrlChangedSpy.count(), 1);
    QTRY_COMPARE(iconChangedSpy.count(), 1);

    QUrl iconUrl = iconUrlChangedSpy.at(0).at(0).toString();
    QCOMPARE(iconUrl, m_page->iconUrl());
    QCOMPARE(iconUrl, QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/icons/qt32.ico")));

    const QIcon &icon = m_page->icon();
    QVERIFY(!icon.isNull());

    QCOMPARE(icon.availableSizes().count(), 1);
    QSize iconSize = icon.availableSizes().first();
    QCOMPARE(iconSize, QSize(32, 32));
}

void tst_QWebEngineFaviconManager::faviconLoadFromResources()
{
    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QUrl url("qrc:/resources/favicon-single.html");
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QTRY_COMPARE(iconUrlChangedSpy.count(), 1);
    QTRY_COMPARE(iconChangedSpy.count(), 1);

    QUrl iconUrl = iconUrlChangedSpy.at(0).at(0).toString();
    QCOMPARE(iconUrl, m_page->iconUrl());
    QCOMPARE(iconUrl, QUrl("qrc:/resources/icons/qt32.ico"));

    const QIcon &icon = m_page->icon();
    QVERIFY(!icon.isNull());

    QCOMPARE(icon.availableSizes().count(), 1);
    QSize iconSize = icon.availableSizes().first();
    QCOMPARE(iconSize, QSize(32, 32));
}

void tst_QWebEngineFaviconManager::faviconLoadEncodedUrl()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QString urlString = QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/favicon-single.html")).toString();
    QUrl url(urlString + QLatin1String("?favicon=load should work with#whitespace!"));
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QTRY_COMPARE(iconUrlChangedSpy.count(), 1);
    QTRY_COMPARE(iconChangedSpy.count(), 1);

    QUrl iconUrl = iconUrlChangedSpy.at(0).at(0).toString();
    QCOMPARE(m_page->iconUrl(), iconUrl);
    QCOMPARE(iconUrl, QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/icons/qt32.ico")));

    const QIcon &icon = m_page->icon();
    QVERIFY(!icon.isNull());

    QCOMPARE(icon.availableSizes().count(), 1);
    QSize iconSize = icon.availableSizes().first();
    QCOMPARE(iconSize, QSize(32, 32));
}

void tst_QWebEngineFaviconManager::noFavicon()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QUrl url = QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/test1.html"));
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QCOMPARE(iconUrlChangedSpy.count(), 0);
    QCOMPARE(iconChangedSpy.count(), 0);

    QVERIFY(m_page->iconUrl().isEmpty());
    QVERIFY(m_page->icon().isNull());
}

void tst_QWebEngineFaviconManager::aboutBlank()
{
    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QUrl url("about:blank");
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QCOMPARE(iconUrlChangedSpy.count(), 0);
    QCOMPARE(iconChangedSpy.count(), 0);

    QVERIFY(m_page->iconUrl().isEmpty());
    QVERIFY(m_page->icon().isNull());
}

void tst_QWebEngineFaviconManager::unavailableFavicon()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QUrl url = QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/favicon-unavailable.html"));
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QCOMPARE(iconUrlChangedSpy.count(), 0);
    QCOMPARE(iconChangedSpy.count(), 0);

    QVERIFY(m_page->iconUrl().isEmpty());
    QVERIFY(m_page->icon().isNull());
}

void tst_QWebEngineFaviconManager::errorPageEnabled()
{
    m_page->settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, true);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QUrl url("invalid://url");
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QCOMPARE(iconUrlChangedSpy.count(), 0);
    QCOMPARE(iconChangedSpy.count(), 0);

    QVERIFY(m_page->iconUrl().isEmpty());
    QVERIFY(m_page->icon().isNull());
}

void tst_QWebEngineFaviconManager::errorPageDisabled()
{
    m_page->settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QUrl url("invalid://url");
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QCOMPARE(iconUrlChangedSpy.count(), 0);
    QCOMPARE(iconChangedSpy.count(), 0);

    QVERIFY(m_page->iconUrl().isEmpty());
    QVERIFY(m_page->icon().isNull());
}

void tst_QWebEngineFaviconManager::bestFavicon()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QUrl url, iconUrl;
    QIcon icon;
    QSize iconSize;

    url = QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/favicon-misc.html"));
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QTRY_COMPARE(iconUrlChangedSpy.count(), 1);
    QTRY_COMPARE(iconChangedSpy.count(), 1);

    iconUrl = iconUrlChangedSpy.at(0).at(0).toString();
    QCOMPARE(iconUrl, m_page->iconUrl());
    // Touch icon is ignored
    QCOMPARE(iconUrl, QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/icons/qt32.ico")));

    icon = m_page->icon();
    QVERIFY(!icon.isNull());

    QCOMPARE(icon.availableSizes().count(), 1);
    iconSize = icon.availableSizes().first();
    QCOMPARE(iconSize, QSize(32, 32));

    loadFinishedSpy.clear();
    iconUrlChangedSpy.clear();
    iconChangedSpy.clear();

    url = QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/favicon-shortcut.html"));
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QTRY_VERIFY(iconUrlChangedSpy.count() >= 1);
    QTRY_VERIFY(iconChangedSpy.count() >= 1);

    iconUrl = iconUrlChangedSpy.last().at(0).toString();

    // If the icon URL is empty we have to wait for
    // the second iconChanged signal that propagates the expected URL
    if (iconUrl.isEmpty()) {
        QTRY_COMPARE(iconUrlChangedSpy.count(), 2);
        QTRY_COMPARE(iconChangedSpy.count(), 2);
        iconUrl = iconUrlChangedSpy.last().at(0).toString();
    }

    QCOMPARE(iconUrl, m_page->iconUrl());
    QCOMPARE(iconUrl, QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/icons/qt144.png")));

    icon = m_page->icon();
    QVERIFY(!icon.isNull());

    QVERIFY(icon.availableSizes().count() >= 1);
    QVERIFY(icon.availableSizes().contains(QSize(144, 144)));
}

void tst_QWebEngineFaviconManager::touchIcon()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QUrl url = QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/favicon-touch.html"));
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QCOMPARE(iconUrlChangedSpy.count(), 0);
    QCOMPARE(iconChangedSpy.count(), 0);

    QVERIFY(m_page->iconUrl().isEmpty());
    QVERIFY(m_page->icon().isNull());
}

void tst_QWebEngineFaviconManager::multiIcon()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QUrl url = QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/favicon-multi.html"));
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QTRY_COMPARE(iconUrlChangedSpy.count(), 1);
    QTRY_COMPARE(iconChangedSpy.count(), 1);

    QUrl iconUrl = iconUrlChangedSpy.at(0).at(0).toString();
    QCOMPARE(m_page->iconUrl(), iconUrl);
    QCOMPARE(iconUrl, QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/icons/qtmulti.ico")));

    const QIcon &icon = m_page->icon();
    QVERIFY(!icon.isNull());
    QCOMPARE(icon.availableSizes().count(), 3);
    QVERIFY(icon.availableSizes().contains(QSize(16, 16)));
    QVERIFY(icon.availableSizes().contains(QSize(32, 32)));
    QVERIFY(icon.availableSizes().contains(QSize(64, 64)));
}

void tst_QWebEngineFaviconManager::candidateIcon()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    QUrl url = QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/favicon-shortcut.html"));
    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QTRY_COMPARE(iconUrlChangedSpy.count(), 1);
    QTRY_COMPARE(iconChangedSpy.count(), 1);

    QUrl iconUrl = iconUrlChangedSpy.at(0).at(0).toString();
    QCOMPARE(m_page->iconUrl(), iconUrl);
    QCOMPARE(iconUrl, QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginefaviconmanager/resources/icons/qt144.png")));

    const QIcon &icon = m_page->icon();
    QVERIFY(!icon.isNull());
    QCOMPARE(icon.availableSizes().count(), 2);
    QVERIFY(icon.availableSizes().contains(QSize(32, 32)));
    QVERIFY(icon.availableSizes().contains(QSize(144, 144)));
}

void tst_QWebEngineFaviconManager::downloadIconsDisabled_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::newRow("misc") << QUrl("qrc:/resources/favicon-misc.html");
    QTest::newRow("shortcut") << QUrl("qrc:/resources/favicon-shortcut.html");
    QTest::newRow("single") << QUrl("qrc:/resources/favicon-single.html");
    QTest::newRow("touch") << QUrl("qrc:/resources/favicon-touch.html");
    QTest::newRow("unavailable") << QUrl("qrc:/resources/favicon-unavailable.html");
}

void tst_QWebEngineFaviconManager::downloadIconsDisabled()
{
    QFETCH(QUrl, url);

    m_page->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage, false);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QCOMPARE(iconUrlChangedSpy.count(), 0);
    QCOMPARE(iconChangedSpy.count(), 0);

    QVERIFY(m_page->iconUrl().isEmpty());
    QVERIFY(m_page->icon().isNull());
}

void tst_QWebEngineFaviconManager::downloadTouchIconsEnabled_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QUrl>("expectedIconUrl");
    QTest::addColumn<QSize>("expectedIconSize");
    QTest::newRow("misc") << QUrl("qrc:/resources/favicon-misc.html") << QUrl("qrc:/resources/icons/qt144.png") << QSize(144, 144);
    QTest::newRow("shortcut") << QUrl("qrc:/resources/favicon-shortcut.html") << QUrl("qrc:/resources/icons/qt144.png") << QSize(144, 144);
    QTest::newRow("single") << QUrl("qrc:/resources/favicon-single.html") << QUrl("qrc:/resources/icons/qt32.ico") << QSize(32, 32);
    QTest::newRow("touch") << QUrl("qrc:/resources/favicon-touch.html") << QUrl("qrc:/resources/icons/qt144.png") << QSize(144, 144);
}

void tst_QWebEngineFaviconManager::downloadTouchIconsEnabled()
{
    QFETCH(QUrl, url);
    QFETCH(QUrl, expectedIconUrl);
    QFETCH(QSize, expectedIconSize);

    m_page->settings()->setAttribute(QWebEngineSettings::TouchIconsEnabled, true);

    QSignalSpy loadFinishedSpy(m_page, SIGNAL(loadFinished(bool)));
    QSignalSpy iconUrlChangedSpy(m_page, SIGNAL(iconUrlChanged(QUrl)));
    QSignalSpy iconChangedSpy(m_page, SIGNAL(iconChanged(QIcon)));

    m_page->load(url);

    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QTRY_COMPARE(iconUrlChangedSpy.count(), 1);
    QTRY_COMPARE(iconChangedSpy.count(), 1);

    const QUrl &iconUrl = iconUrlChangedSpy.at(0).at(0).toString();
    QCOMPARE(m_page->iconUrl(), iconUrl);
    QCOMPARE(iconUrl, expectedIconUrl);

    const QIcon &icon = m_page->icon();
    QVERIFY(!icon.isNull());

    QVERIFY(icon.availableSizes().count() >= 1);
    QVERIFY(icon.availableSizes().contains(expectedIconSize));
}

QTEST_MAIN(tst_QWebEngineFaviconManager)

#include "tst_qwebenginefaviconmanager.moc"
