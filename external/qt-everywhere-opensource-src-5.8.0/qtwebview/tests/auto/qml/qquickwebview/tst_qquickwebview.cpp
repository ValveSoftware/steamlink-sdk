/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "testwindow.h"
#include "util.h"

#include <QScopedPointer>
#include <QtQml/QQmlEngine>
#include <QtTest/QtTest>
#include <QtWebView/private/qquickwebview_p.h>
#include <QtCore/qfile.h>
#include <QtCore/qstandardpaths.h>
#include <QtWebView/qtwebviewfunctions.h>

QString getTestFilePath(const QString &testFile)
{
    const QString tempTestFile = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + testFile;
    const bool exists = QFile::exists(tempTestFile);
    if (exists)
        return tempTestFile;

    QFile tf(QString(":/") + testFile);
    const bool copied = tf.copy(tempTestFile);

    return copied ? tempTestFile : testFile;
}

class tst_QQuickWebView : public QObject {
    Q_OBJECT
public:
    tst_QQuickWebView();

private Q_SLOTS:
    void init();
    void cleanup();

    void navigationStatusAtStartup();
    void stopEnabledAfterLoadStarted();
    void baseUrl();
    void loadEmptyUrl();
    void loadEmptyPageViewVisible();
    void loadEmptyPageViewHidden();
    void loadNonexistentFileUrl();
    void backAndForward();
    void reload();
    void stop();
    void loadProgress();

    void show();
    void showWebView();
    void removeFromCanvas();
    void multipleWebViewWindows();
    void multipleWebViews();
    void titleUpdate();

private:
    inline QQuickWebView *newWebView();
    inline QQuickWebView *webView() const;
    void runJavaScript(const QString &script);
    QScopedPointer<TestWindow> m_window;
    QScopedPointer<QQmlComponent> m_component;
};

tst_QQuickWebView::tst_QQuickWebView()
{
    static QQmlEngine *engine = new QQmlEngine(this);
    m_component.reset(new QQmlComponent(engine, this));
    m_component->setData(QByteArrayLiteral("import QtQuick 2.0\n"
                                           "import QtWebView 1.1\n"
                                           "WebView {}")
                         , QUrl());
}

QQuickWebView *tst_QQuickWebView::newWebView()
{
    QObject *viewInstance = m_component->create();
    QQuickWebView *webView = qobject_cast<QQuickWebView*>(viewInstance);
    return webView;
}

void tst_QQuickWebView::init()
{
    QtWebView::initialize();
    m_window.reset(new TestWindow(newWebView()));
}

void tst_QQuickWebView::cleanup()
{
    m_window.reset();
}

inline QQuickWebView *tst_QQuickWebView::webView() const
{
    return static_cast<QQuickWebView*>(m_window->webView.data());
}

void tst_QQuickWebView::runJavaScript(const QString &script)
{
    webView()->runJavaScript(script);
}

void tst_QQuickWebView::navigationStatusAtStartup()
{
    QCOMPARE(webView()->canGoBack(), false);

    QCOMPARE(webView()->canGoForward(), false);

    QCOMPARE(webView()->isLoading(), false);
}

void tst_QQuickWebView::stopEnabledAfterLoadStarted()
{
    QCOMPARE(webView()->isLoading(), false);

    LoadStartedCatcher catcher(webView());
    webView()->setUrl(QUrl(getTestFilePath("basic_page.html")));
    waitForSignal(&catcher, SIGNAL(finished()));

    QCOMPARE(webView()->isLoading(), true);

    QVERIFY(waitForLoadSucceeded(webView()));
}

void tst_QQuickWebView::baseUrl()
{
    // Test the url is in a well defined state when instanciating the view, but before loading anything.
    QVERIFY(webView()->url().isEmpty());
}

void tst_QQuickWebView::loadEmptyUrl()
{
    webView()->setUrl(QUrl());
    webView()->setUrl(QUrl(QLatin1String("")));
}

void tst_QQuickWebView::loadEmptyPageViewVisible()
{
    m_window->show();
    loadEmptyPageViewHidden();
}

void tst_QQuickWebView::loadEmptyPageViewHidden()
{
    QSignalSpy loadSpy(webView(), SIGNAL(loadingChanged(QQuickWebViewLoadRequest*)));

    webView()->setUrl(QUrl(getTestFilePath("basic_page.html")));
    QVERIFY(waitForLoadSucceeded(webView()));

    QCOMPARE(loadSpy.size(), 2);
}

void tst_QQuickWebView::loadNonexistentFileUrl()
{
    QSignalSpy loadSpy(webView(), SIGNAL(loadingChanged(QQuickWebViewLoadRequest*)));

    webView()->setUrl(QUrl(getTestFilePath("file_that_does_not_exist.html")));
    QVERIFY(waitForLoadFailed(webView()));

    QCOMPARE(loadSpy.size(), 2);
}

void tst_QQuickWebView::backAndForward()
{
    const QString basicPage = getTestFilePath("basic_page.html");
    const QString basicPage2 = getTestFilePath("basic_page2.html");
    webView()->setUrl(QUrl(basicPage));
    QVERIFY(waitForLoadSucceeded(webView()));

    QCOMPARE(webView()->url().path(), basicPage);

    webView()->setUrl(QUrl(basicPage2));
    QVERIFY(waitForLoadSucceeded(webView()));

    QCOMPARE(webView()->url().path(), basicPage2);

    webView()->goBack();
    QVERIFY(waitForLoadSucceeded(webView()));

    QCOMPARE(webView()->url().path(), basicPage);

    webView()->goForward();
    QVERIFY(waitForLoadSucceeded(webView()));

    QCOMPARE(webView()->url().path(), basicPage2);
}

void tst_QQuickWebView::reload()
{
    webView()->setUrl(QUrl(getTestFilePath("basic_page.html")));
    QVERIFY(waitForLoadSucceeded(webView()));

    QCOMPARE(webView()->url().path(), getTestFilePath("basic_page.html"));

    webView()->reload();
    QVERIFY(waitForLoadSucceeded(webView()));

    QCOMPARE(webView()->url().path(), getTestFilePath("basic_page.html"));
}

void tst_QQuickWebView::stop()
{
    webView()->setUrl(QUrl(getTestFilePath("basic_page.html")));
    QVERIFY(waitForLoadSucceeded(webView()));

    QCOMPARE(webView()->url().path(), getTestFilePath("basic_page.html"));

    webView()->stop();
}

void tst_QQuickWebView::loadProgress()
{
    QCOMPARE(webView()->loadProgress(), 0);

    webView()->setUrl(QUrl(getTestFilePath("basic_page.html")));
    QSignalSpy loadProgressChangedSpy(webView(), SIGNAL(loadProgressChanged()));
    QVERIFY(waitForLoadSucceeded(webView()));

    QVERIFY(loadProgressChangedSpy.count() >= 1);

    QCOMPARE(webView()->loadProgress(), 100);
}

void tst_QQuickWebView::show()
{
    // This should not crash.
    m_window->show();
    QTest::qWait(200);
    m_window->hide();
}

void tst_QQuickWebView::showWebView()
{
    webView()->setUrl(QUrl(getTestFilePath("direct-image-compositing.html")));
    QVERIFY(waitForLoadSucceeded(webView()));
    m_window->show();
    // This should not crash.
    webView()->setVisible(true);
    QTest::qWait(200);
    webView()->setVisible(false);
    QTest::qWait(200);
}

void tst_QQuickWebView::removeFromCanvas()
{
    showWebView();

    // This should not crash.
    QQuickItem *parent = webView()->parentItem();
    QQuickItem noCanvasItem;
    webView()->setParentItem(&noCanvasItem);
    QTest::qWait(200);
    webView()->setParentItem(parent);
    webView()->setVisible(true);
    QTest::qWait(200);
}

void tst_QQuickWebView::multipleWebViewWindows()
{
    showWebView();

    // This should not crash.
    QQuickWebView *webView1 = newWebView();
    QScopedPointer<TestWindow> window1(new TestWindow(webView1));
    QQuickWebView *webView2 = newWebView();
    QScopedPointer<TestWindow> window2(new TestWindow(webView2));

    webView1->setUrl(QUrl(getTestFilePath("scroll.html")));
    QVERIFY(waitForLoadSucceeded(webView1));
    window1->show();
    webView1->setVisible(true);

    webView2->setUrl(QUrl(getTestFilePath("basic_page.html")));
    QVERIFY(waitForLoadSucceeded(webView2));
    window2->show();
    webView2->setVisible(true);
    QTest::qWait(200);
}

void tst_QQuickWebView::multipleWebViews()
{
    showWebView();

    // This should not crash.
    QScopedPointer<QQuickWebView> webView1(newWebView());
    webView1->setParentItem(m_window->contentItem());
    QScopedPointer<QQuickWebView> webView2(newWebView());
    webView2->setParentItem(m_window->contentItem());

    webView1->setSize(QSizeF(300, 400));
    webView1->setUrl(QUrl(getTestFilePath("scroll.html")));
    QVERIFY(waitForLoadSucceeded(webView1.data()));
    webView1->setVisible(true);

    webView2->setSize(QSizeF(300, 400));
    webView2->setUrl(QUrl(getTestFilePath("basic_page.html")));
    QVERIFY(waitForLoadSucceeded(webView2.data()));
    webView2->setVisible(true);
    QTest::qWait(200);
}

void tst_QQuickWebView::titleUpdate()
{
    QSignalSpy titleSpy(webView(), SIGNAL(titleChanged()));

    // Load page with no title
    webView()->setUrl(QUrl(getTestFilePath("basic_page2.html")));
    QVERIFY(waitForLoadSucceeded(webView()));
    QCOMPARE(titleSpy.size(), 1);

    titleSpy.clear();

    // No titleChanged signal for failed load
    webView()->setUrl(QUrl(getTestFilePath("file_that_does_not_exist.html")));
    QVERIFY(waitForLoadFailed(webView()));
    QCOMPARE(titleSpy.size(), 0);

}

QTEST_MAIN(tst_QQuickWebView)
#include "tst_qquickwebview.moc"
