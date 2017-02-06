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

#include "../../widgets/util.h"
#include <QtTest/QtTest>
#include <QtWebEngineCore/qwebenginecallback.h>
#include <QtWebEngineCore/qwebenginecookiestore.h>
#include <QtWebEngineWidgets/qwebenginepage.h>
#include <QtWebEngineWidgets/qwebengineprofile.h>
#include <QtWebEngineWidgets/qwebengineview.h>

class tst_QWebEngineCookieStore : public QObject
{
    Q_OBJECT

public:
    tst_QWebEngineCookieStore();
    ~tst_QWebEngineCookieStore();

public Q_SLOTS:
    void init();
    void cleanup();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void cookieSignals();
    void setAndDeleteCookie();
    void batchCookieTasks();
};

tst_QWebEngineCookieStore::tst_QWebEngineCookieStore()
{
}

tst_QWebEngineCookieStore::~tst_QWebEngineCookieStore()
{
}

void tst_QWebEngineCookieStore::init()
{
}

void tst_QWebEngineCookieStore::cleanup()
{
}

void tst_QWebEngineCookieStore::initTestCase()
{
}

void tst_QWebEngineCookieStore::cleanupTestCase()
{
}

void tst_QWebEngineCookieStore::cookieSignals()
{
    QWebEngineView view;
    QWebEngineCookieStore *client = view.page()->profile()->cookieStore();
    client->deleteAllCookies();

    QSignalSpy loadSpy(&view, SIGNAL(loadFinished(bool)));
    QSignalSpy cookieAddedSpy(client, SIGNAL(cookieAdded(const QNetworkCookie &)));
    QSignalSpy cookieRemovedSpy(client, SIGNAL(cookieRemoved(const QNetworkCookie &)));

    view.load(QUrl("qrc:///resources/index.html"));

    QTRY_COMPARE(loadSpy.count(), 1);
    QVariant success = loadSpy.takeFirst().takeFirst();
    QVERIFY(success.toBool());
    QTRY_COMPARE(cookieAddedSpy.count(), 2);

    // try whether updating a cookie to be expired results in that cookie being removed.
    QNetworkCookie expiredCookie(QNetworkCookie::parseCookies(QByteArrayLiteral("SessionCookie=delete; expires=Thu, 01-Jan-1970 00:00:00 GMT; path=///resources")).first());
    client->setCookie(expiredCookie, QUrl("qrc:///resources/index.html"));

    QTRY_COMPARE(cookieRemovedSpy.count(), 1);
    cookieRemovedSpy.clear();

    // try removing the other cookie.
    QNetworkCookie nonSessionCookie(QNetworkCookie::parseCookies(QByteArrayLiteral("CookieWithExpiresField=QtWebEngineCookieTest; path=///resources")).first());
    client->deleteCookie(nonSessionCookie, QUrl("qrc:///resources/index.html"));
    QTRY_COMPARE(cookieRemovedSpy.count(), 1);
}

void tst_QWebEngineCookieStore::setAndDeleteCookie()
{
    QWebEngineView view;
    QWebEngineCookieStore *client = view.page()->profile()->cookieStore();
    client->deleteAllCookies();

    QSignalSpy loadSpy(&view, SIGNAL(loadFinished(bool)));
    QSignalSpy cookieAddedSpy(client, SIGNAL(cookieAdded(const QNetworkCookie &)));
    QSignalSpy cookieRemovedSpy(client, SIGNAL(cookieRemoved(const QNetworkCookie &)));

    QNetworkCookie cookie1(QNetworkCookie::parseCookies(QByteArrayLiteral("khaos=I9GX8CWI; Domain=.example.com; Path=/docs")).first());
    QNetworkCookie cookie2(QNetworkCookie::parseCookies(QByteArrayLiteral("Test%20Cookie=foobar; domain=example.com; Path=/")).first());
    QNetworkCookie cookie3(QNetworkCookie::parseCookies(QByteArrayLiteral("SessionCookie=QtWebEngineCookieTest; Path=///resources")).first());
    QNetworkCookie expiredCookie3(QNetworkCookie::parseCookies(QByteArrayLiteral("SessionCookie=delete; expires=Thu, 01-Jan-1970 00:00:00 GMT; path=///resources")).first());

    // check if pending cookies are set and removed
    client->setCookie(cookie1);
    QTRY_COMPARE(cookieAddedSpy.count(),1);
    client->setCookie(cookie2);
    QTRY_COMPARE(cookieAddedSpy.count(),2);
    client->deleteCookie(cookie1);

    view.load(QUrl("qrc:///resources/content.html"));

    QTRY_COMPARE(loadSpy.count(), 1);
    QVariant success = loadSpy.takeFirst().takeFirst();
    QVERIFY(success.toBool());
    QTRY_COMPARE(cookieAddedSpy.count(), 2);
    QTRY_COMPARE(cookieRemovedSpy.count(), 1);
    cookieAddedSpy.clear();
    cookieRemovedSpy.clear();

    client->setCookie(cookie3);
    QTRY_COMPARE(cookieAddedSpy.count(), 1);
    // updating a cookie with an expired 'expires' field should remove the cookie with the same name
    client->setCookie(expiredCookie3);
    client->deleteCookie(cookie2);
    QTRY_COMPARE(cookieAddedSpy.count(), 1);
    QTRY_COMPARE(cookieRemovedSpy.count(), 2);
}

void tst_QWebEngineCookieStore::batchCookieTasks()
{
    QWebEngineView view;
    QWebEngineCookieStore *client = view.page()->profile()->cookieStore();
    client->deleteAllCookies();

    QSignalSpy loadSpy(&view, SIGNAL(loadFinished(bool)));
    QSignalSpy cookieAddedSpy(client, SIGNAL(cookieAdded(const QNetworkCookie &)));
    QSignalSpy cookieRemovedSpy(client, SIGNAL(cookieRemoved(const QNetworkCookie &)));

    QNetworkCookie cookie1(QNetworkCookie::parseCookies(QByteArrayLiteral("khaos=I9GX8CWI; Domain=.example.com; Path=/docs")).first());
    QNetworkCookie cookie2(QNetworkCookie::parseCookies(QByteArrayLiteral("Test%20Cookie=foobar; domain=example.com; Path=/")).first());

    client->setCookie(cookie1);
    QTRY_COMPARE(cookieAddedSpy.count(), 1);
    client->setCookie(cookie2);
    QTRY_COMPARE(cookieAddedSpy.count(), 2);

    view.load(QUrl("qrc:///resources/index.html"));

    QTRY_COMPARE(loadSpy.count(), 1);
    QVariant success = loadSpy.takeFirst().takeFirst();
    QVERIFY(success.toBool());
    QTRY_COMPARE(cookieAddedSpy.count(), 4);
    QTRY_COMPARE(cookieRemovedSpy.count(), 0);

    cookieAddedSpy.clear();
    cookieRemovedSpy.clear();

    client->deleteSessionCookies();
    QTRY_COMPARE(cookieRemovedSpy.count(), 3);

    client->deleteAllCookies();
    QTRY_COMPARE(cookieRemovedSpy.count(), 4);
}

QTEST_MAIN(tst_QWebEngineCookieStore)
#include "tst_qwebenginecookiestore.moc"
