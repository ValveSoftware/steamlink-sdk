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
#include <QtWebEngineCore/qwebengineurlrequestinterceptor.h>
#include <QtWebEngineWidgets/qwebenginepage.h>
#include <QtWebEngineWidgets/qwebengineprofile.h>
#include <QtWebEngineWidgets/qwebenginesettings.h>
#include <QtWebEngineWidgets/qwebengineview.h>

class tst_QWebEngineUrlRequestInterceptor : public QObject
{
    Q_OBJECT

public:
    tst_QWebEngineUrlRequestInterceptor();
    ~tst_QWebEngineUrlRequestInterceptor();

public Q_SLOTS:
    void init();
    void cleanup();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void interceptRequest();
    void ipv6HostEncoding();
    void requestedUrl();
    void setUrlSameUrl();
    void firstPartyUrl();
};

tst_QWebEngineUrlRequestInterceptor::tst_QWebEngineUrlRequestInterceptor()
{
}

tst_QWebEngineUrlRequestInterceptor::~tst_QWebEngineUrlRequestInterceptor()
{
}

void tst_QWebEngineUrlRequestInterceptor::init()
{
}

void tst_QWebEngineUrlRequestInterceptor::cleanup()
{
}

void tst_QWebEngineUrlRequestInterceptor::initTestCase()
{
}

void tst_QWebEngineUrlRequestInterceptor::cleanupTestCase()
{
}

class TestRequestInterceptor : public QWebEngineUrlRequestInterceptor
{
public:
    QList<QUrl> observedUrls;
    QList<QUrl> firstPartyUrls;
    bool shouldIntercept;

    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        info.block(info.requestMethod() != QByteArrayLiteral("GET"));
        if (shouldIntercept && info.requestUrl().toString().endsWith(QLatin1String("__placeholder__")))
            info.redirect(QUrl("qrc:///resources/content.html"));

        observedUrls.append(info.requestUrl());
        firstPartyUrls.append(info.firstPartyUrl());
    }
    TestRequestInterceptor(bool intercept)
        : shouldIntercept(intercept)
    {
    }
};

void tst_QWebEngineUrlRequestInterceptor::interceptRequest()
{
    QWebEngineView view;
    TestRequestInterceptor interceptor(/* intercept */ true);

    QSignalSpy loadSpy(&view, SIGNAL(loadFinished(bool)));
    view.page()->profile()->setRequestInterceptor(&interceptor);
    view.load(QUrl("qrc:///resources/index.html"));
    QTRY_COMPARE(loadSpy.count(), 1);
    QVariant success = loadSpy.takeFirst().takeFirst();
    QVERIFY(success.toBool());
    loadSpy.clear();
    QVariant ok;

    view.page()->runJavaScript("post();", [&ok](const QVariant result){ ok = result; });
    QTRY_VERIFY(ok.toBool());
    QTRY_COMPARE(loadSpy.count(), 1);
    success = loadSpy.takeFirst().takeFirst();
    // We block non-GET requests, so this should not succeed.
    QVERIFY(!success.toBool());
    loadSpy.clear();

    view.load(QUrl("qrc:///resources/__placeholder__"));
    QTRY_COMPARE(loadSpy.count(), 1);
    success = loadSpy.takeFirst().takeFirst();
    // The redirection for __placeholder__ should succeed.
    QVERIFY(success.toBool());
    loadSpy.clear();
    QCOMPARE(interceptor.observedUrls.count(), 4);

    // Make sure that registering an observer does not modify the request.
    TestRequestInterceptor observer(/* intercept */ false);
    view.page()->profile()->setRequestInterceptor(&observer);
    view.load(QUrl("qrc:///resources/__placeholder__"));
    QTRY_COMPARE(loadSpy.count(), 1);
    success = loadSpy.takeFirst().takeFirst();
    // Since we do not intercept, loading an invalid path should not succeed.
    QVERIFY(!success.toBool());
    QCOMPARE(observer.observedUrls.count(), 1);
}

class LocalhostContentProvider : public QWebEngineUrlRequestInterceptor
{
public:
    LocalhostContentProvider() { }

    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeFavicon)
            return;

        requestedUrls.append(info.requestUrl());
        info.redirect(QUrl("data:text/html,<p>hello"));
    }

    QList<QUrl> requestedUrls;
};

void tst_QWebEngineUrlRequestInterceptor::ipv6HostEncoding()
{
    QWebEngineView view;
    QWebEnginePage *page = view.page();
    LocalhostContentProvider contentProvider;
    QSignalSpy spyLoadFinished(page, SIGNAL(loadFinished(bool)));

    page->profile()->setRequestInterceptor(&contentProvider);

    page->setHtml("<p>Hi", QUrl::fromEncoded("http://[::1]/index.html"));
    QTRY_COMPARE(spyLoadFinished.count(), 1);
    QCOMPARE(contentProvider.requestedUrls.count(), 0);

    evaluateJavaScriptSync(page, "var r = new XMLHttpRequest();"
            "r.open('GET', 'http://[::1]/test.xml', false);"
            "r.send(null);"
            );

    QCOMPARE(contentProvider.requestedUrls.count(), 1);
    QCOMPARE(contentProvider.requestedUrls.at(0), QUrl::fromEncoded("http://[::1]/test.xml"));
}

void tst_QWebEngineUrlRequestInterceptor::requestedUrl()
{
    QWebEnginePage page;
    page.settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);

    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));
    TestRequestInterceptor interceptor(/* intercept */ true);
    page.profile()->setRequestInterceptor(&interceptor);

    page.setUrl(QUrl("qrc:///resources/__placeholder__"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(interceptor.observedUrls.at(0), QUrl("qrc:///resources/content.html"));
    QCOMPARE(page.requestedUrl(), QUrl("qrc:///resources/__placeholder__"));
    QCOMPARE(page.url(), QUrl("qrc:///resources/content.html"));

    page.setUrl(QUrl("qrc:/non-existent.html"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(interceptor.observedUrls.at(2), QUrl("qrc:/non-existent.html"));
    QCOMPARE(page.requestedUrl(), QUrl("qrc:///resources/__placeholder__"));
    QCOMPARE(page.url(), QUrl("qrc:///resources/content.html"));

    page.setUrl(QUrl("http://abcdef.abcdef"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(spy.count(), 3);
    QCOMPARE(interceptor.observedUrls.at(3), QUrl("http://abcdef.abcdef/"));
    QCOMPARE(page.requestedUrl(), QUrl("qrc:///resources/__placeholder__"));
    QCOMPARE(page.url(), QUrl("qrc:///resources/content.html"));
}

void tst_QWebEngineUrlRequestInterceptor::setUrlSameUrl()
{
    QWebEnginePage page;
    TestRequestInterceptor interceptor(/* intercept */ true);
    page.profile()->setRequestInterceptor(&interceptor);

    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));

    page.setUrl(QUrl("qrc:///resources/__placeholder__"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(page.url(), QUrl("qrc:///resources/content.html"));
    QCOMPARE(spy.count(), 1);

    page.setUrl(QUrl("qrc:///resources/__placeholder__"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(page.url(), QUrl("qrc:///resources/content.html"));
    QCOMPARE(spy.count(), 2);

    // Now a case without redirect.
    page.setUrl(QUrl("qrc:///resources/content.html"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(page.url(), QUrl("qrc:///resources/content.html"));
    QCOMPARE(spy.count(), 3);

    page.setUrl(QUrl("qrc:///resources/__placeholder__"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(page.url(), QUrl("qrc:///resources/content.html"));
    QCOMPARE(spy.count(), 4);
}

void tst_QWebEngineUrlRequestInterceptor::firstPartyUrl()
{
    QWebEnginePage page;
    TestRequestInterceptor interceptor(/* intercept */ false);
    page.profile()->setRequestInterceptor(&interceptor);

    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));

    page.setUrl(QUrl("qrc:///resources/firstparty.html"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(interceptor.observedUrls.at(0), QUrl("qrc:///resources/firstparty.html"));
    QCOMPARE(interceptor.observedUrls.at(1), QUrl("qrc:///resources/content.html"));
    QCOMPARE(interceptor.firstPartyUrls.at(0), QUrl("qrc:///resources/firstparty.html"));
    QCOMPARE(interceptor.firstPartyUrls.at(1), QUrl("qrc:///resources/firstparty.html"));
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(tst_QWebEngineUrlRequestInterceptor)
#include "tst_qwebengineurlrequestinterceptor.moc"
