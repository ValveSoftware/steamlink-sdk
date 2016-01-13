/*
    Copyright (C) 2008,2009 Nokia Corporation and/or its subsidiary(-ies)

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


#include <QtTest/QtTest>

#include <qwebenginepage.h>
#include <qwebengineview.h>
#include <qwebenginehistory.h>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QPaintEngine>
#include <QPicture>
#include <QRegExp>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTextCodec>
#include <QWebEngineSettings>
#ifndef QT_NO_OPENSSL
#include <qsslerror.h>
#endif
#include "../util.h"

class tst_QWebEngineFrame : public QObject
{
    Q_OBJECT

public:
    bool eventFilter(QObject* watched, QEvent* event);

public Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void horizontalScrollAfterBack();
    void symmetricUrl();
    void progressSignal();
    void urlChange();
    void requestedUrl();
    void requestedUrlAfterSetAndLoadFailures();
    void javaScriptWindowObjectCleared_data();
    void javaScriptWindowObjectCleared();
    void javaScriptWindowObjectClearedOnEvaluate();
    void asyncAndDelete();
    void earlyToHtml();
    void setHtml();
    void setHtmlWithImageResource();
    void setHtmlWithStylesheetResource();
    void setHtmlWithBaseURL();
    void setHtmlWithJSAlert();
    void ipv6HostEncoding();
    void metaData();
#if !defined(QT_NO_COMBOBOX)
    void popupFocus();
#endif
    void inputFieldFocus();
    void hitTestContent();
    void baseUrl_data();
    void baseUrl();
    void hasSetFocus();
    void renderGeometry();
    void renderHints();
    void scrollPosition();
    void scrollToAnchor();
    void scrollbarsOff();
    void evaluateWillCauseRepaint();
    void setContent_data();
    void setContent();
    void setCacheLoadControlAttribute();
    void setUrlWithPendingLoads();
    void setUrlWithFragment_data();
    void setUrlWithFragment();
    void setUrlToEmpty();
    void setUrlToInvalid();
    void setUrlHistory();
    void setUrlUsingStateObject();
    void setUrlSameUrl();
    void setUrlThenLoads_data();
    void setUrlThenLoads();
    void loadFinishedAfterNotFoundError();
    void loadInSignalHandlers_data();
    void loadInSignalHandlers();

private:
    QWebEngineView* m_view;
    QWebEnginePage* m_page;
    QWebEngineView* m_inputFieldsTestView;
    int m_inputFieldTestPaintCount;
};

bool tst_QWebEngineFrame::eventFilter(QObject* watched, QEvent* event)
{
    // used on the inputFieldFocus test
    if (watched == m_inputFieldsTestView) {
        if (event->type() == QEvent::Paint)
            m_inputFieldTestPaintCount++;
    }
    return QObject::eventFilter(watched, event);
}

void tst_QWebEngineFrame::initTestCase()
{
}

void tst_QWebEngineFrame::init()
{
    m_view = new QWebEngineView();
    m_page = m_view->page();
    m_page->settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);
}

void tst_QWebEngineFrame::cleanup()
{
    delete m_view;
}

void tst_QWebEngineFrame::symmetricUrl()
{
    QWebEngineView view;
    QSignalSpy loadFinishedSpy(view.page(), SIGNAL(loadFinished(bool)));

    QVERIFY(view.url().isEmpty());

    QCOMPARE(view.history()->count(), 0);

    QUrl dataUrl("data:text/html,<h1>Test");

    view.setUrl(dataUrl);
    QCOMPARE(view.url(), dataUrl);
    QCOMPARE(view.history()->count(), 0);

    // loading is _not_ immediate, so the text isn't set just yet.
    QVERIFY(toPlainTextSync(view.page()).isEmpty());

    QTRY_COMPARE(loadFinishedSpy.count(), 1);

    QCOMPARE(view.history()->count(), 1);
    QCOMPARE(toPlainTextSync(view.page()), QString("Test"));

    QUrl dataUrl2("data:text/html,<h1>Test2");
    QUrl dataUrl3("data:text/html,<h1>Test3");

    view.setUrl(dataUrl2);
    view.setUrl(dataUrl3);

    QCOMPARE(view.url(), dataUrl3);

    QTRY_VERIFY(loadFinishedSpy.count() >= 2);
    QTRY_COMPARE(loadFinishedSpy.count(), 3);

    QCOMPARE(view.history()->count(), 2);

    QCOMPARE(toPlainTextSync(view.page()), QString("Test3"));
}

void tst_QWebEngineFrame::progressSignal()
{
    QSignalSpy progressSpy(m_view, SIGNAL(loadProgress(int)));

    QUrl dataUrl("data:text/html,<h1>Test");
    m_view->setUrl(dataUrl);

    ::waitForSignal(m_view, SIGNAL(loadFinished(bool)));

    QVERIFY(progressSpy.size() >= 2);
    int previousValue = -1;
    for (QSignalSpy::ConstIterator it = progressSpy.begin(); it < progressSpy.end(); ++it) {
        int current = (*it).first().toInt();
        QVERIFY(current > previousValue);
        previousValue = current;
    }

    // But we always end at 100%
    QCOMPARE(progressSpy.last().first().toInt(), 100);
}

void tst_QWebEngineFrame::urlChange()
{
    QSignalSpy urlSpy(m_page, SIGNAL(urlChanged(QUrl)));

    QUrl dataUrl("data:text/html,<h1>Test");
    m_view->setUrl(dataUrl);

    ::waitForSignal(m_page, SIGNAL(urlChanged(QUrl)));

    QCOMPARE(urlSpy.size(), 1);

    QUrl dataUrl2("data:text/html,<html><head><title>title</title></head><body><h1>Test</body></html>");
    m_view->setUrl(dataUrl2);

    ::waitForSignal(m_page, SIGNAL(urlChanged(QUrl)));

    QCOMPARE(urlSpy.size(), 2);
}

class FakeReply : public QNetworkReply {
    Q_OBJECT

public:
    static const QUrl urlFor404ErrorWithoutContents;

    FakeReply(const QNetworkRequest& request, QObject* parent = 0)
        : QNetworkReply(parent)
    {
        setOperation(QNetworkAccessManager::GetOperation);
        setRequest(request);
        setUrl(request.url());
        if (request.url() == QUrl("qrc:/test1.html")) {
            setHeader(QNetworkRequest::LocationHeader, QString("qrc:/test2.html"));
            setAttribute(QNetworkRequest::RedirectionTargetAttribute, QUrl("qrc:/test2.html"));
            QTimer::singleShot(0, this, SLOT(continueRedirect()));
        }
#ifndef QT_NO_OPENSSL
        else if (request.url() == QUrl("qrc:/fake-ssl-error.html")) {
            setError(QNetworkReply::SslHandshakeFailedError, tr("Fake error!"));
            QTimer::singleShot(0, this, SLOT(continueError()));
        }
#endif
        else if (request.url().host() == QLatin1String("abcdef.abcdef")) {
            setError(QNetworkReply::HostNotFoundError, tr("Invalid URL"));
            QTimer::singleShot(0, this, SLOT(continueError()));
        } else if (request.url() == FakeReply::urlFor404ErrorWithoutContents) {
            setError(QNetworkReply::ContentNotFoundError, "Not found");
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 404);
            QTimer::singleShot(0, this, SLOT(continueError()));
        }

        open(QIODevice::ReadOnly);
    }
    ~FakeReply()
    {
        close();
    }
    virtual void abort() {}
    virtual void close() {}

protected:
    qint64 readData(char*, qint64)
    {
        return 0;
    }

private Q_SLOTS:
    void continueRedirect()
    {
        emit metaDataChanged();
        emit finished();
    }

    void continueError()
    {
        emit error(this->error());
        emit finished();
    }
};

const QUrl FakeReply::urlFor404ErrorWithoutContents = QUrl("http://this.will/return-http-404-error-without-contents.html");

class FakeNetworkManager : public QNetworkAccessManager {
    Q_OBJECT

public:
    FakeNetworkManager(QObject* parent) : QNetworkAccessManager(parent) { }

protected:
    virtual QNetworkReply* createRequest(Operation op, const QNetworkRequest& request, QIODevice* outgoingData)
    {
        QString url = request.url().toString();
        if (op == QNetworkAccessManager::GetOperation) {
#ifndef QT_NO_OPENSSL
            if (url == "qrc:/fake-ssl-error.html") {
                FakeReply* reply = new FakeReply(request, this);
                QList<QSslError> errors;
                emit sslErrors(reply, errors << QSslError(QSslError::UnspecifiedError));
                return reply;
            }
#endif
            if (url == "qrc:/test1.html" || url == "http://abcdef.abcdef/" || request.url() == FakeReply::urlFor404ErrorWithoutContents)
                return new FakeReply(request, this);
        }

        return QNetworkAccessManager::createRequest(op, request, outgoingData);
    }
};

void tst_QWebEngineFrame::requestedUrl()
{
#if !defined(QWEBENGINEPAGE_SETNETWORKACCESSMANAGER)
    QSKIP("QWEBENGINEPAGE_SETNETWORKACCESSMANAGER");
#else
    QWebEnginePage page;

    // in few seconds, the image should be completely loaded
    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));
    FakeNetworkManager* networkManager = new FakeNetworkManager(&page);
    page.setNetworkAccessManager(networkManager);

    page.setUrl(QUrl("qrc:/test1.html"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(page.requestedUrl(), QUrl("qrc:/test1.html"));
    QCOMPARE(page.url(), QUrl("qrc:/test2.html"));

    page.setUrl(QUrl("qrc:/non-existent.html"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(page.requestedUrl(), QUrl("qrc:/non-existent.html"));
    QCOMPARE(page.url(), QUrl("qrc:/non-existent.html"));

    page.setUrl(QUrl("http://abcdef.abcdef"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(page.requestedUrl(), QUrl("http://abcdef.abcdef/"));
    QCOMPARE(page.url(), QUrl("http://abcdef.abcdef/"));

#ifndef QT_NO_OPENSSL
    qRegisterMetaType<QList<QSslError> >("QList<QSslError>");
    qRegisterMetaType<QNetworkReply* >("QNetworkReply*");

    QSignalSpy spy2(page.networkAccessManager(), SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)));
    page.setUrl(QUrl("qrc:/fake-ssl-error.html"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(page.requestedUrl(), QUrl("qrc:/fake-ssl-error.html"));
    QCOMPARE(page.url(), QUrl("qrc:/fake-ssl-error.html"));
#endif
#endif
}

void tst_QWebEngineFrame::requestedUrlAfterSetAndLoadFailures()
{
    QWebEnginePage page;
    page.settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);
    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));

    const QUrl first("http://abcdef.abcdef/");
    page.setUrl(first);
    ::waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(page.url(), first);
    QCOMPARE(page.requestedUrl(), first);
    QVERIFY(!spy.at(0).first().toBool());

    const QUrl second("http://abcdef.abcdef/another_page.html");
    QVERIFY(first != second);

    page.load(second);
    ::waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(page.url(), first);
    QCOMPARE(page.requestedUrl(), second);
    QVERIFY(!spy.at(1).first().toBool());
}

void tst_QWebEngineFrame::javaScriptWindowObjectCleared_data()
{
    QTest::addColumn<QString>("html");
    QTest::addColumn<int>("signalCount");
    QTest::newRow("with <script>") << "<html><body><script>i=0</script><p>hello world</p></body></html>" << 1;
    // NOTE: Empty scripts no longer cause this signal to be emitted.
    QTest::newRow("with empty <script>") << "<html><body><script></script><p>hello world</p></body></html>" << 0;
    QTest::newRow("without <script>") << "<html><body><p>hello world</p></body></html>" << 0;
}

void tst_QWebEngineFrame::javaScriptWindowObjectCleared()
{
#if !defined(QWEBENGINEPAGE_JAVASCRIPTWINDOWOBJECTCLEARED)
    QSKIP("QWEBENGINEPAGE_JAVASCRIPTWINDOWOBJECTCLEARED");
#else
    QWebEnginePage page;
    QSignalSpy spy(&page, SIGNAL(javaScriptWindowObjectCleared()));
    QFETCH(QString, html);
    page.setHtml(html);

    QFETCH(int, signalCount);
    QCOMPARE(spy.count(), signalCount);
#endif
}

void tst_QWebEngineFrame::javaScriptWindowObjectClearedOnEvaluate()
{
#if !defined(QWEBENGINEPAGE_EVALUATEJAVASCRIPT)
    QSKIP("QWEBENGINEPAGE_EVALUATEJAVASCRIPT");
#else
    QWebEnginePage page;
    QSignalSpy spy(&page, SIGNAL(javaScriptWindowObjectCleared()));
    page.setHtml("<html></html>");
    QCOMPARE(spy.count(), 0);
    page.evaluateJavaScript("var a = 'a';");
    QCOMPARE(spy.count(), 1);
    // no new clear for a new script:
    page.evaluateJavaScript("var a = 1;");
    QCOMPARE(spy.count(), 1);
#endif
}

void tst_QWebEngineFrame::asyncAndDelete()
{
    QWebEnginePage *page = new QWebEnginePage;
    CallbackSpy<QString> plainTextSpy;
    CallbackSpy<QString> htmlSpy;
    page->toPlainText(plainTextSpy.ref());
    page->toHtml(htmlSpy.ref());

    delete page;
    // Pending callbacks should be called with an empty value in the page's destructor.
    QCOMPARE(plainTextSpy.waitForResult(), QString());
    QVERIFY(plainTextSpy.wasCalled());
    QCOMPARE(htmlSpy.waitForResult(), QString());
    QVERIFY(htmlSpy.wasCalled());
}

void tst_QWebEngineFrame::earlyToHtml()
{
    QString html("<html><head></head><body></body></html>");
    QCOMPARE(toHtmlSync(m_view->page()), html);
}

void tst_QWebEngineFrame::setHtml()
{
    QString html("<html><head></head><body><p>hello world</p></body></html>");
    QSignalSpy spy(m_view->page(), SIGNAL(loadFinished(bool)));
    m_view->page()->setHtml(html);
    QVERIFY(spy.wait());
    QCOMPARE(toHtmlSync(m_view->page()), html);
}

void tst_QWebEngineFrame::setHtmlWithImageResource()
{
    // By default, only security origins of local files can load local resources.
    // So we should specify baseUrl to be a local file in order to get a proper origin and load the local image.

    QLatin1String html("<html><body><p>hello world</p><img src='qrc:/image.png'/></body></html>");
    QWebEnginePage page;

    page.setHtml(html, QUrl(QLatin1String("file:///path/to/file")));
    waitForSignal(&page, SIGNAL(loadFinished(bool)), 200);

    QCOMPARE(evaluateJavaScriptSync(&page, "document.images.length").toInt(), 1);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images[0].width").toInt(), 128);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images[0].height").toInt(), 128);

    // Now we test the opposite: without a baseUrl as a local file, we cannot request local resources.

    page.setHtml(html);
    waitForSignal(&page, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images.length").toInt(), 1);
    QEXPECT_FAIL("", "https://bugs.webkit.org/show_bug.cgi?id=118659", Continue);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images[0].width").toInt(), 0);
    QEXPECT_FAIL("", "https://bugs.webkit.org/show_bug.cgi?id=118659", Continue);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images[0].height").toInt(), 0);
}

void tst_QWebEngineFrame::setHtmlWithStylesheetResource()
{
#if !defined(QWEBENGINEELEMENT)
    QSKIP("QWEBENGINEELEMENT");
#else
    // By default, only security origins of local files can load local resources.
    // So we should specify baseUrl to be a local file in order to be able to download the local stylesheet.

    const char* htmlData =
        "<html>"
            "<head>"
                "<link rel='stylesheet' href='qrc:/style.css' type='text/css' />"
            "</head>"
            "<body>"
                "<p id='idP'>some text</p>"
            "</body>"
        "</html>";
    QLatin1String html(htmlData);
    QWebEnginePage page;
    QWebEngineElement webElement;

    page.setHtml(html, QUrl(QLatin1String("qrc:///file")));
    waitForSignal(&page, SIGNAL(loadFinished(bool)), 200);
    webElement = page.documentElement().findFirst("p");
    QCOMPARE(webElement.styleProperty("color", QWebEngineElement::CascadedStyle), QLatin1String("red"));

    // Now we test the opposite: without a baseUrl as a local file, we cannot request local resources.

    page.setHtml(html, QUrl(QLatin1String("http://www.example.com/")));
    waitForSignal(&page, SIGNAL(loadFinished(bool)), 200);
    webElement = page.documentElement().findFirst("p");
    QEXPECT_FAIL("", "https://bugs.webkit.org/show_bug.cgi?id=118659", Continue);
    QCOMPARE(webElement.styleProperty("color", QWebEngineElement::CascadedStyle), QString());
#endif
}

void tst_QWebEngineFrame::setHtmlWithBaseURL()
{
    // This tests if baseUrl is indeed affecting the relative paths from resources.
    // As we are using a local file as baseUrl, its security origin should be able to load local resources.

    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QDir::setCurrent(TESTS_SOURCE_DIR);

    QString html("<html><body><p>hello world</p><img src='resources/image2.png'/></body></html>");

    QWebEnginePage page;

    // in few seconds, the image should be completey loaded
    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));

    page.setHtml(html, QUrl::fromLocalFile(TESTS_SOURCE_DIR));
    waitForSignal(&page, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy.count(), 1);

    QCOMPARE(evaluateJavaScriptSync(&page, "document.images.length").toInt(), 1);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images[0].width").toInt(), 128);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images[0].height").toInt(), 128);

    // no history item has to be added.
    QCOMPARE(m_view->page()->history()->count(), 0);
}

class MyPage : public QWebEnginePage
{
public:
    MyPage() :  QWebEnginePage(), alerts(0) {}
    int alerts;

protected:
    virtual void javaScriptAlert(const QUrl &securityOrigin, const QString &msg)
    {
        alerts++;
        QCOMPARE(securityOrigin, QUrl(QStringLiteral("http://test.origin.com/")));
        QCOMPARE(msg, QString("foo"));
    }
};

void tst_QWebEngineFrame::setHtmlWithJSAlert()
{
    QString html("<html><head></head><body><script>alert('foo');</script><p>hello world</p></body></html>");
    MyPage page;
    page.setHtml(html, QUrl(QStringLiteral("http://test.origin.com/path#fragment")));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(page.alerts, 1);
    QCOMPARE(toHtmlSync(&page), html);
}

class TestNetworkManager : public QNetworkAccessManager
{
public:
    TestNetworkManager(QObject* parent) : QNetworkAccessManager(parent) {}

    QList<QUrl> requestedUrls;

protected:
    virtual QNetworkReply* createRequest(Operation op, const QNetworkRequest &request, QIODevice* outgoingData) {
        requestedUrls.append(request.url());
        QNetworkRequest redirectedRequest = request;
        redirectedRequest.setUrl(QUrl("data:text/html,<p>hello"));
        return QNetworkAccessManager::createRequest(op, redirectedRequest, outgoingData);
    }
};

void tst_QWebEngineFrame::ipv6HostEncoding()
{
#if !defined(QWEBENGINEPAGE_EVALUATEJAVASCRIPT)
    QSKIP("QWEBENGINEPAGE_EVALUATEJAVASCRIPT");
#else
    TestNetworkManager* networkManager = new TestNetworkManager(m_page);
    m_page->setNetworkAccessManager(networkManager);
    networkManager->requestedUrls.clear();

    QUrl baseUrl = QUrl::fromEncoded("http://[::1]/index.html");
    m_view->setHtml("<p>Hi", baseUrl);
    m_view->page()->evaluateJavaScript("var r = new XMLHttpRequest();"
            "r.open('GET', 'http://[::1]/test.xml', false);"
            "r.send(null);"
            );
    QCOMPARE(networkManager->requestedUrls.count(), 1);
    QCOMPARE(networkManager->requestedUrls.at(0), QUrl::fromEncoded("http://[::1]/test.xml"));
#endif
}

void tst_QWebEngineFrame::metaData()
{
#if !defined(QWEBENGINEPAGE_METADATA)
    QSKIP("QWEBENGINEPAGE_METADATA");
#else
    m_view->setHtml("<html>"
                    "    <head>"
                    "        <meta name=\"description\" content=\"Test description\">"
                    "        <meta name=\"keywords\" content=\"HTML, JavaScript, Css\">"
                    "    </head>"
                    "</html>");

    QMultiMap<QString, QString> metaData = m_view->page()->metaData();

    QCOMPARE(metaData.count(), 2);

    QCOMPARE(metaData.value("description"), QString("Test description"));
    QCOMPARE(metaData.value("keywords"), QString("HTML, JavaScript, Css"));
    QCOMPARE(metaData.value("nonexistent"), QString());

    m_view->setHtml("<html>"
                    "    <head>"
                    "        <meta name=\"samekey\" content=\"FirstValue\">"
                    "        <meta name=\"samekey\" content=\"SecondValue\">"
                    "    </head>"
                    "</html>");

    metaData = m_view->page()->metaData();

    QCOMPARE(metaData.count(), 2);

    QStringList values = metaData.values("samekey");
    QCOMPARE(values.count(), 2);

    QVERIFY(values.contains("FirstValue"));
    QVERIFY(values.contains("SecondValue"));

    QCOMPARE(metaData.value("nonexistent"), QString());
#endif
}

#if !defined(QT_NO_COMBOBOX)
void tst_QWebEngineFrame::popupFocus()
{
#if !defined(QWEBENGINEELEMENT)
    QSKIP("QWEBENGINEELEMENT");
#else
    QWebEngineView view;
    view.setHtml("<html>"
                 "    <body>"
                 "        <select name=\"select\">"
                 "            <option>1</option>"
                 "            <option>2</option>"
                 "        </select>"
                 "        <input type=\"text\"> </input>"
                 "        <textarea name=\"text_area\" rows=\"3\" cols=\"40\">"
                 "This test checks whether showing and hiding a popup"
                 "takes the focus away from the webpage."
                 "        </textarea>"
                 "    </body>"
                 "</html>");
    view.resize(400, 100);
    // Call setFocus before show to work around http://bugreports.qt.nokia.com/browse/QTBUG-14762
    view.setFocus();
    view.show();
    QTest::qWaitForWindowExposed(&view);
    view.activateWindow();
    QTRY_VERIFY(view.hasFocus());

    // open the popup by clicking. check if focus is on the popup
    const QWebEngineElement webCombo = view.page()->documentElement().findFirst(QLatin1String("select[name=select]"));
    QTest::mouseClick(&view, Qt::LeftButton, 0, webCombo.geometry().center());

    QComboBox* combo = view.findChild<QComboBox*>();
    QVERIFY(combo != 0);
    QTRY_VERIFY(!view.hasFocus() && combo->view()->hasFocus()); // Focus should be on the popup

    // hide the popup and check if focus is on the page
    combo->hidePopup();
    QTRY_VERIFY(view.hasFocus()); // Focus should be back on the WebView
#endif
}
#endif

void tst_QWebEngineFrame::inputFieldFocus()
{
#if !defined(QWEBENGINEELEMENT)
    QSKIP("QWEBENGINEELEMENT");
#else
    QWebEngineView view;
    view.setHtml("<html><body><input type=\"text\"></input></body></html>");
    view.resize(400, 100);
    view.show();
    QTest::qWaitForWindowExposed(&view);
    view.activateWindow();
    view.setFocus();
    QTRY_VERIFY(view.hasFocus());

    // double the flashing time, should at least blink once already
    int delay = qApp->cursorFlashTime() * 2;

    // focus the lineedit and check if it blinks
    bool autoSipEnabled = qApp->autoSipEnabled();
    qApp->setAutoSipEnabled(false);
    const QWebEngineElement inputElement = view.page()->documentElement().findFirst(QLatin1String("input[type=text]"));
    QTest::mouseClick(&view, Qt::LeftButton, 0, inputElement.geometry().center());
    m_inputFieldsTestView = &view;
    view.installEventFilter( this );
    QTest::qWait(delay);
    QVERIFY2(m_inputFieldTestPaintCount >= 3,
             "The input field should have a blinking caret");
    qApp->setAutoSipEnabled(autoSipEnabled);
#endif
}

void tst_QWebEngineFrame::hitTestContent()
{
#if !defined(QWEBENGINEELEMENT)
    QSKIP("QWEBENGINEELEMENT");
#else
    QString html("<html><body><p>A paragraph</p><br/><br/><br/><a href=\"about:blank\" target=\"_foo\" id=\"link\">link text</a></body></html>");

    QWebEnginePage page;
    page.setHtml(html);
    page.setViewportSize(QSize(200, 0)); //no height so link is not visible
    const QWebEngineElement linkElement = page.documentElement().findFirst(QLatin1String("a#link"));
    QWebEngineHitTestResult result = page.hitTestContent(linkElement.geometry().center());
    QCOMPARE(result.linkText(), QString("link text"));
    QWebEngineElement link = result.linkElement();
    QCOMPARE(link.attribute("target"), QString("_foo"));
#endif
}

void tst_QWebEngineFrame::baseUrl_data()
{
    QTest::addColumn<QString>("html");
    QTest::addColumn<QUrl>("loadUrl");
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QUrl>("baseUrl");

    QTest::newRow("null") << QString() << QUrl()
                          << QUrl("about:blank") << QUrl("about:blank");

    QTest::newRow("foo") << QString() << QUrl("http://foobar.baz/")
                         << QUrl("http://foobar.baz/") << QUrl("http://foobar.baz/");

    QString html = "<html>"
        "<head>"
            "<base href=\"http://foobaz.bar/\" />"
        "</head>"
    "</html>";
    QTest::newRow("customBaseUrl") << html << QUrl("http://foobar.baz/")
                                   << QUrl("http://foobar.baz/") << QUrl("http://foobaz.bar/");
}

void tst_QWebEngineFrame::baseUrl()
{
    QFETCH(QString, html);
    QFETCH(QUrl, loadUrl);
    QFETCH(QUrl, url);
    QFETCH(QUrl, baseUrl);

    QSignalSpy loadSpy(m_page, SIGNAL(loadFinished(bool)));
    m_page->setHtml(html, loadUrl);
    QTRY_COMPARE(loadSpy.count(), 1);
    QCOMPARE(m_page->url(), url);
    QEXPECT_FAIL("null", "Slight change: We now translate QUrl() to about:blank for the virtual url, but not for the baseUrl", Continue);
    QCOMPARE(baseUrlSync(m_page), baseUrl);
}

void tst_QWebEngineFrame::hasSetFocus()
{
#if !defined(QWEBENGINEFRAME)
    QSKIP("QWEBENGINEFRAME");
#else
    QString html("<html><body><p>top</p>" \
                    "<iframe width='80%' height='30%'/>" \
                 "</body></html>");

    QSignalSpy loadSpy(m_page, SIGNAL(loadFinished(bool)));
    m_page->setHtml(html);

    waitForSignal(m_page, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(loadSpy.size(), 1);

    QList<QWebEngineFrame*> children = m_page->childFrames();
    QWebEngineFrame* frame = children.at(0);
    QString innerHtml("<html><body><p>another iframe</p>" \
                        "<iframe width='80%' height='30%'/>" \
                      "</body></html>");
    frame->setHtml(innerHtml);

    waitForSignal(frame, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(loadSpy.size(), 2);

    m_page->setFocus();
    QTRY_VERIFY(m_page->hasFocus());

    for (int i = 0; i < children.size(); ++i) {
        children.at(i)->setFocus();
        QTRY_VERIFY(children.at(i)->hasFocus());
        QVERIFY(!m_page->hasFocus());
    }

    m_page->setFocus();
    QTRY_VERIFY(m_page->hasFocus());
#endif
}

void tst_QWebEngineFrame::renderGeometry()
{
#if !defined(QWEBENGINEFRAME)
    QSKIP("QWEBENGINEFRAME");
#else
    QString html("<html>" \
                    "<head><style>" \
                       "body, iframe { margin: 0px; border: none; }" \
                    "</style></head>" \
                    "<body><iframe width='100px' height='100px'/></body>" \
                 "</html>");

    QWebEnginePage page;
    page.setHtml(html);

    QList<QWebEngineFrame*> frames = page.childFrames();
    QWebEngineFrame *frame = frames.at(0);
    QString innerHtml("<body style='margin: 0px;'><img src='qrc:/image.png'/></body>");

    // By default, only security origins of local files can load local resources.
    // So we should specify baseUrl to be a local file in order to get a proper origin.
    frame->setHtml(innerHtml, QUrl("file:///path/to/file"));
    waitForSignal(frame, SIGNAL(loadFinished(bool)), 200);

    QPicture picture;

    QSize size = page.contentsSize();
    page.setViewportSize(size);

    // render contents layer only (the iframe is smaller than the image, so it will have scrollbars)
    QPainter painter1(&picture);
    frame->render(&painter1, QWebEngineFrame::ContentsLayer);
    painter1.end();

    QCOMPARE(size.width(), picture.boundingRect().width() + frame->scrollBarGeometry(Qt::Vertical).width());
    QCOMPARE(size.height(), picture.boundingRect().height() + frame->scrollBarGeometry(Qt::Horizontal).height());

    // render everything, should be the size of the iframe
    QPainter painter2(&picture);
    frame->render(&painter2, QWebEngineFrame::AllLayers);
    painter2.end();

    QCOMPARE(size.width(), picture.boundingRect().width());   // width: 100px
    QCOMPARE(size.height(), picture.boundingRect().height()); // height: 100px
#endif
}


class DummyPaintEngine: public QPaintEngine {
public:

    DummyPaintEngine()
        : QPaintEngine(QPaintEngine::AllFeatures)
        , renderHints(0)
    {
    }

    bool begin(QPaintDevice*)
    {
        setActive(true);
        return true;
    }

    bool end()
    {
        setActive(false);
        return false;
    }

    void updateState(const QPaintEngineState& state)
    {
        renderHints = state.renderHints();
    }

    void drawPath(const QPainterPath&) { }
    void drawPixmap(const QRectF&, const QPixmap&, const QRectF&) { }

    QPaintEngine::Type type() const
    {
        return static_cast<QPaintEngine::Type>(QPaintEngine::User + 2);
    }

    QPainter::RenderHints renderHints;
};

class DummyPaintDevice: public QPaintDevice {
public:
    DummyPaintDevice()
        : QPaintDevice()
        , m_engine(new DummyPaintEngine)
    {
    }

    ~DummyPaintDevice()
    {
        delete m_engine;
    }

    QPaintEngine* paintEngine() const
    {
        return m_engine;
    }

    QPainter::RenderHints renderHints() const
    {
        return m_engine->renderHints;
    }

protected:
    int metric(PaintDeviceMetric metric) const;

private:
    DummyPaintEngine* m_engine;
    friend class DummyPaintEngine;
};


int DummyPaintDevice::metric(PaintDeviceMetric metric) const
{
    switch (metric) {
    case PdmWidth:
        return 400;
        break;

    case PdmHeight:
        return 200;
        break;

    case PdmNumColors:
        return INT_MAX;
        break;

    case PdmDepth:
        return 32;
        break;

    default:
        break;
    }
    return 0;
}

void tst_QWebEngineFrame::renderHints()
{
#if !defined(QWEBENGINEPAGE_RENDER)
    QSKIP("QWEBENGINEPAGE_RENDER");
#else
    QString html("<html><body><p>Hello, world!</p></body></html>");

    QWebEnginePage page;
    page.setHtml(html);
    page.setViewportSize(page.contentsSize());

    // We will call frame->render and trap the paint engine state changes
    // to ensure that GraphicsContext does not clobber the render hints.
    DummyPaintDevice buffer;
    QPainter painter(&buffer);

    painter.setRenderHint(QPainter::TextAntialiasing, false);
    page.render(&painter);
    QVERIFY(!(buffer.renderHints() & QPainter::TextAntialiasing));
    QVERIFY(!(buffer.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(buffer.renderHints() & QPainter::HighQualityAntialiasing));

    painter.setRenderHint(QPainter::TextAntialiasing, true);
    page.render(&painter);
    QVERIFY(buffer.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(buffer.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(buffer.renderHints() & QPainter::HighQualityAntialiasing));

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    page.render(&painter);
    QVERIFY(buffer.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(buffer.renderHints() & QPainter::SmoothPixmapTransform);
    QVERIFY(!(buffer.renderHints() & QPainter::HighQualityAntialiasing));

    painter.setRenderHint(QPainter::HighQualityAntialiasing, true);
    page.render(&painter);
    QVERIFY(buffer.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(buffer.renderHints() & QPainter::SmoothPixmapTransform);
    QVERIFY(buffer.renderHints() & QPainter::HighQualityAntialiasing);
#endif
}

void tst_QWebEngineFrame::scrollPosition()
{
#if !defined(QWEBENGINEPAGE_EVALUATEJAVASCRIPT)
    QSKIP("QWEBENGINEPAGE_EVALUATEJAVASCRIPT");
#else
    // enlarged image in a small viewport, to provoke the scrollbars to appear
    QString html("<html><body><img src='qrc:/image.png' height=500 width=500/></body></html>");

    QWebEnginePage page;
    page.setViewportSize(QSize(200, 200));

    page.setHtml(html);
    page.setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
    page.setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

    // try to set the scroll offset programmatically
    page.setScrollPosition(QPoint(23, 29));
    QCOMPARE(page.scrollPosition().x(), 23);
    QCOMPARE(page.scrollPosition().y(), 29);

    int x = page.evaluateJavaScript("window.scrollX").toInt();
    int y = page.evaluateJavaScript("window.scrollY").toInt();
    QCOMPARE(x, 23);
    QCOMPARE(y, 29);
#endif
}

void tst_QWebEngineFrame::scrollToAnchor()
{
#if !defined(QWEBENGINEELEMENT)
    QSKIP("QWEBENGINEELEMENT");
#else
    QWebEnginePage page;
    page.setViewportSize(QSize(480, 800));

    QString html("<html><body><p style=\"margin-bottom: 1500px;\">Hello.</p>"
                 "<p><a id=\"foo\">This</a> is an anchor</p>"
                 "<p style=\"margin-bottom: 1500px;\"><a id=\"bar\">This</a> is another anchor</p>"
                 "</body></html>");
    page.setHtml(html);
    page.setScrollPosition(QPoint(0, 0));
    QCOMPARE(page.scrollPosition().x(), 0);
    QCOMPARE(page.scrollPosition().y(), 0);

    QWebEngineElement fooAnchor = page.findFirstElement("a[id=foo]");

    page.scrollToAnchor("foo");
    QCOMPARE(page.scrollPosition().y(), fooAnchor.geometry().top());

    page.scrollToAnchor("bar");
    page.scrollToAnchor("foo");
    QCOMPARE(page.scrollPosition().y(), fooAnchor.geometry().top());

    page.scrollToAnchor("top");
    QCOMPARE(page.scrollPosition().y(), 0);

    page.scrollToAnchor("bar");
    page.scrollToAnchor("notexist");
    QVERIFY(page.scrollPosition().y() != 0);
#endif
}


void tst_QWebEngineFrame::scrollbarsOff()
{
#if !defined(QWEBENGINEPAGE_EVALUATEJAVASCRIPT)
    QSKIP("QWEBENGINEPAGE_EVALUATEJAVASCRIPT");
#else
    QWebEngineView view;
    QWebEngineFrame* mainFrame = view.page();

    mainFrame->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
    mainFrame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

    QString html("<script>" \
                 "   function checkScrollbar() {" \
                 "       if (innerWidth === document.documentElement.offsetWidth)" \
                 "           document.getElementById('span1').innerText = 'SUCCESS';" \
                 "       else" \
                 "           document.getElementById('span1').innerText = 'FAIL';" \
                 "   }" \
                 "</script>" \
                 "<body>" \
                 "   <div style='margin-top:1000px ; margin-left:1000px'>" \
                 "       <a id='offscreen' href='a'>End</a>" \
                 "   </div>" \
                 "<span id='span1'></span>" \
                 "</body>");


    QSignalSpy loadSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml(html);
    ::waitForSignal(&view, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(loadSpy.count(), 1);

    mainFrame->evaluateJavaScript("checkScrollbar();");
    QCOMPARE(mainFrame->documentElement().findAll("span").at(0).toPlainText(), QString("SUCCESS"));
#endif
}

void tst_QWebEngineFrame::horizontalScrollAfterBack()
{
#if !defined(QWEBENGINESETTINGS)
    QSKIP("QWEBENGINESETTINGS");
#else
    QWebEngineView view;
    QSignalSpy loadSpy(view.page(), SIGNAL(loadFinished(bool)));

    view.page()->settings()->setMaximumPagesInCache(2);
    view.page()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAsNeeded);
    view.page()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAsNeeded);

    view.load(QUrl("qrc:/testiframe2.html"));
    view.resize(200, 200);
    QTRY_COMPARE(loadSpy.count(), 1);
    QTRY_VERIFY((view.page()->scrollBarGeometry(Qt::Horizontal)).height());

    view.load(QUrl("qrc:/testiframe.html"));
    QTRY_COMPARE(loadSpy.count(), 2);

    view.page()->triggerAction(QWebEnginePage::Back);
    QTRY_COMPARE(loadSpy.count(), 3);
    QTRY_VERIFY((view.page()->scrollBarGeometry(Qt::Horizontal)).height());
#endif
}

void tst_QWebEngineFrame::evaluateWillCauseRepaint()
{
#if !defined(QWEBENGINEPAGE_EVALUATEJAVASCRIPT)
    QSKIP("QWEBENGINEPAGE_EVALUATEJAVASCRIPT");
#else
    QWebEngineView view;
    QString html("<html><body>top<div id=\"junk\" style=\"display: block;\">"
                    "junk</div>bottom</body></html>");
    view.setHtml(html);
    view.show();

    QTest::qWaitForWindowExposed(&view);
    view.page()->evaluateJavaScript(
        "document.getElementById('junk').style.display = 'none';");

    ::waitForSignal(view.page(), SIGNAL(repaintRequested(QRect)));
#endif
}

void tst_QWebEngineFrame::setContent_data()
{
    QTest::addColumn<QString>("mimeType");
    QTest::addColumn<QByteArray>("testContents");
    QTest::addColumn<QString>("expected");

    QString str = QString::fromUtf8("ὕαλον ϕαγεῖν δύναμαι· τοῦτο οὔ με βλάπτει");
    QTest::newRow("UTF-8 plain text") << "text/plain; charset=utf-8" << str.toUtf8() << str;

    QTextCodec *utf16 = QTextCodec::codecForName("UTF-16");
    if (utf16)
        QTest::newRow("UTF-16 plain text") << "text/plain; charset=utf-16" << utf16->fromUnicode(str) << str;

    str = QString::fromUtf8("Une chaîne de caractères à sa façon.");
    QTest::newRow("latin-1 plain text") << "text/plain; charset=iso-8859-1" << str.toLatin1() << str;


}

void tst_QWebEngineFrame::setContent()
{
    QFETCH(QString, mimeType);
    QFETCH(QByteArray, testContents);
    QFETCH(QString, expected);
    QSignalSpy loadSpy(m_page, SIGNAL(loadFinished(bool)));
    m_view->setContent(testContents, mimeType);
    QVERIFY(loadSpy.wait());
    QCOMPARE(toPlainTextSync(m_view->page()), expected);
}

class CacheNetworkAccessManager : public QNetworkAccessManager {
public:
    CacheNetworkAccessManager(QObject* parent = 0)
        : QNetworkAccessManager(parent)
        , m_lastCacheLoad(QNetworkRequest::PreferNetwork)
    {
    }

    virtual QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*)
    {
        QVariant cacheLoad = request.attribute(QNetworkRequest::CacheLoadControlAttribute);
        if (cacheLoad.isValid())
            m_lastCacheLoad = static_cast<QNetworkRequest::CacheLoadControl>(cacheLoad.toUInt());
        else
            m_lastCacheLoad = QNetworkRequest::PreferNetwork; // default value
        return new FakeReply(request, this);
    }

    QNetworkRequest::CacheLoadControl lastCacheLoad() const
    {
        return m_lastCacheLoad;
    }

private:
    QNetworkRequest::CacheLoadControl m_lastCacheLoad;
};

void tst_QWebEngineFrame::setCacheLoadControlAttribute()
{
#if !defined(QWEBENGINEPAGE_SETNETWORKACCESSMANAGER)
    QSKIP("QWEBENGINEPAGE_SETNETWORKACCESSMANAGER");
#else
    QWebEnginePage page;
    CacheNetworkAccessManager* manager = new CacheNetworkAccessManager(&page);
    page.setNetworkAccessManager(manager);

    QNetworkRequest request(QUrl("http://abcdef.abcdef/"));

    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysCache);
    page.load(request);
    QCOMPARE(manager->lastCacheLoad(), QNetworkRequest::AlwaysCache);

    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    page.load(request);
    QCOMPARE(manager->lastCacheLoad(), QNetworkRequest::PreferCache);

    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    page.load(request);
    QCOMPARE(manager->lastCacheLoad(), QNetworkRequest::AlwaysNetwork);

    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork);
    page.load(request);
    QCOMPARE(manager->lastCacheLoad(), QNetworkRequest::PreferNetwork);
#endif
}

void tst_QWebEngineFrame::setUrlWithPendingLoads()
{
    QWebEnginePage page;
    page.setHtml("<img src='dummy:'/>");
    page.setUrl(QUrl("about:blank"));
}

void tst_QWebEngineFrame::setUrlWithFragment_data()
{
    QTest::addColumn<QUrl>("previousUrl");
    QTest::newRow("empty") << QUrl();
    QTest::newRow("same URL no fragment") << QUrl("qrc:/test1.html");
    // See comments in setUrlSameUrl about using setUrl() with the same url().
    QTest::newRow("same URL with same fragment") << QUrl("qrc:/test1.html#");
    QTest::newRow("same URL with different fragment") << QUrl("qrc:/test1.html#anotherFragment");
    QTest::newRow("another URL") << QUrl("qrc:/test2.html");
}

// Based on bug report https://bugs.webkit.org/show_bug.cgi?id=32723
void tst_QWebEngineFrame::setUrlWithFragment()
{
    QSKIP("FIXME: https://trello.com/c/3L7F8VZJ/217-take-care-about-the-in-page-navigations-in-the-tests");
    QFETCH(QUrl, previousUrl);

    QWebEnginePage page;

    if (!previousUrl.isEmpty()) {
        page.load(previousUrl);
        ::waitForSignal(&page, SIGNAL(loadFinished(bool)));
        QCOMPARE(page.url(), previousUrl);
    }

    QSignalSpy spy(&page, SIGNAL(urlChanged(QUrl)));
    const QUrl url("qrc:/test1.html#");
    QVERIFY(!url.fragment().isNull());

    page.setUrl(url);
    ::waitForSignal(&page, SIGNAL(urlChanged(QUrl)));

    QCOMPARE(spy.count(), 1);
    QVERIFY(!toPlainTextSync(&page).isEmpty());
    // Slight change: This information now comes from Chromium and the behavior of requestedUrl changed in this case.
    // QCOMPARE(page.requestedUrl(), url);
    QCOMPARE(page.url(), url);
}

void tst_QWebEngineFrame::setUrlToEmpty()
{
    int expectedLoadFinishedCount = 0;
    const QUrl aboutBlank("about:blank");
    const QUrl url("qrc:/test2.html");

    QWebEnginePage page;
    QCOMPARE(page.url(), QUrl());
    QCOMPARE(page.requestedUrl(), QUrl());
    QCOMPARE(baseUrlSync(&page), QUrl());

    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));

    // Set existing url
    page.setUrl(url);
    expectedLoadFinishedCount++;
    ::waitForSignal(&page, SIGNAL(loadFinished(bool)));

    QCOMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(page.url(), url);
    QCOMPARE(page.requestedUrl(), url);
    QCOMPARE(baseUrlSync(&page), url);

    // Set empty url
    page.setUrl(QUrl());
    expectedLoadFinishedCount++;

    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(page.url(), aboutBlank);
    QCOMPARE(page.requestedUrl(), QUrl());
    QCOMPARE(baseUrlSync(&page), aboutBlank);

    // Set existing url
    page.setUrl(url);
    expectedLoadFinishedCount++;

    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(page.url(), url);
    QCOMPARE(page.requestedUrl(), url);
    QCOMPARE(baseUrlSync(&page), url);

    // Load empty url
    page.load(QUrl());
    expectedLoadFinishedCount++;

    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(page.url(), aboutBlank);
    QCOMPARE(page.requestedUrl(), QUrl());
    QCOMPARE(baseUrlSync(&page), aboutBlank);
}

void tst_QWebEngineFrame::setUrlToInvalid()
{
    QEXPECT_FAIL("", "Unsupported: QtWebEngine doesn't adjust invalid URLs.", Abort);
    QVERIFY(false);

    QWebEnginePage page;

    const QUrl invalidUrl("http:/example.com");
    QVERIFY(!invalidUrl.isEmpty());
    QVERIFY(invalidUrl != QUrl());

    // QWebEngineFrame will do its best to accept the URL, possible converting it to a valid equivalent URL.
    const QUrl validUrl("http://example.com/");
    page.setUrl(invalidUrl);
    QCOMPARE(page.url(), validUrl);
    QCOMPARE(page.requestedUrl(), validUrl);
    QCOMPARE(baseUrlSync(&page), validUrl);

    // QUrls equivalent to QUrl() will be treated as such.
    const QUrl aboutBlank("about:blank");
    const QUrl anotherInvalidUrl("1http://bugs.webkit.org");
    QVERIFY(!anotherInvalidUrl.isEmpty()); // and they are not necessarily empty.
    QVERIFY(!anotherInvalidUrl.isValid());
    QCOMPARE(anotherInvalidUrl.toEncoded(), QUrl().toEncoded());

    page.setUrl(anotherInvalidUrl);
    QCOMPARE(page.url(), aboutBlank);
    QCOMPARE(page.requestedUrl().toEncoded(), anotherInvalidUrl.toEncoded());
    QCOMPARE(baseUrlSync(&page), aboutBlank);
}

static QStringList collectHistoryUrls(QWebEngineHistory *history)
{
    QStringList urls;
    foreach (const QWebEngineHistoryItem &i, history->items())
        urls << i.url().toString();
    return urls;
}

void tst_QWebEngineFrame::setUrlHistory()
{
    const QUrl aboutBlank("about:blank");
    QUrl url;
    int expectedLoadFinishedCount = 0;
    QSignalSpy spy(m_page, SIGNAL(loadFinished(bool)));

    QCOMPARE(m_page->history()->count(), 0);

    m_page->setUrl(QUrl());
    expectedLoadFinishedCount++;
    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(m_page->url(), aboutBlank);
    QCOMPARE(m_page->requestedUrl(), QUrl());
    // Chromium stores navigation entry for every successful loads. The load of the empty page is committed and stored as about:blank.
    QCOMPARE(collectHistoryUrls(m_page->history()), QStringList() << aboutBlank.toString());

    url = QUrl("http://non.existent/");
    m_page->setUrl(url);
    expectedLoadFinishedCount++;
    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    // When error page is disabled in case of LoadFail the entry of the unavailable page is not stored.
    // We expect the url of the previously loaded page here.
    QCOMPARE(m_page->url(), aboutBlank);
    QCOMPARE(m_page->requestedUrl(), QUrl());
    // Since the entry of the unavailable page is not stored it will not available in the history.
    QCOMPARE(collectHistoryUrls(m_page->history()), QStringList() << aboutBlank.toString());

    url = QUrl("qrc:/test1.html");
    m_page->setUrl(url);
    expectedLoadFinishedCount++;
    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(m_page->url(), url);
    QCOMPARE(m_page->requestedUrl(), url);
    QCOMPARE(collectHistoryUrls(m_page->history()), QStringList() << aboutBlank.toString() << QStringLiteral("qrc:/test1.html"));

    m_page->setUrl(QUrl());
    expectedLoadFinishedCount++;
    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(m_page->url(), aboutBlank);
    QCOMPARE(m_page->requestedUrl(), QUrl());
    // Chromium stores navigation entry for every successful loads. The load of the empty page is committed and stored as about:blank.
    QCOMPARE(collectHistoryUrls(m_page->history()), QStringList()
                                                        << aboutBlank.toString()
                                                        << QStringLiteral("qrc:/test1.html")
                                                        << aboutBlank.toString());

    url = QUrl("qrc:/test1.html");
    m_page->setUrl(url);
    expectedLoadFinishedCount++;
    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(m_page->url(), url);
    QCOMPARE(m_page->requestedUrl(), url);
    // The history count DOES change since the about:blank is in the list.
    QCOMPARE(collectHistoryUrls(m_page->history()), QStringList()
                                                        << aboutBlank.toString()
                                                        << QStringLiteral("qrc:/test1.html")
                                                        << aboutBlank.toString()
                                                        << QStringLiteral("qrc:/test1.html"));

    url = QUrl("qrc:/test2.html");
    m_page->setUrl(url);
    expectedLoadFinishedCount++;
    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(m_page->url(), url);
    QCOMPARE(m_page->requestedUrl(), url);
    QCOMPARE(collectHistoryUrls(m_page->history()), QStringList()
                                                        << aboutBlank.toString()
                                                        << QStringLiteral("qrc:/test1.html")
                                                        << aboutBlank.toString()
                                                        << QStringLiteral("qrc:/test1.html")
                                                        << QStringLiteral("qrc:/test2.html"));
}

void tst_QWebEngineFrame::setUrlUsingStateObject()
{
    const QUrl aboutBlank("about:blank");
    QUrl url;
    QSignalSpy urlChangedSpy(m_page, SIGNAL(urlChanged(QUrl)));
    int expectedUrlChangeCount = 0;

    QCOMPARE(m_page->history()->count(), 0);

    url = QUrl("qrc:/test1.html");
    m_page->setUrl(url);
    waitForSignal(m_page, SIGNAL(loadFinished(bool)));
    expectedUrlChangeCount++;
    QCOMPARE(urlChangedSpy.count(), expectedUrlChangeCount);
    QCOMPARE(m_page->url(), url);
    QCOMPARE(m_page->history()->count(), 1);

    evaluateJavaScriptSync(m_page, "window.history.pushState(null, 'push', 'navigate/to/here')");
    expectedUrlChangeCount++;
    QCOMPARE(urlChangedSpy.count(), expectedUrlChangeCount);
    QCOMPARE(m_page->url(), QUrl("qrc:/navigate/to/here"));
    QCOMPARE(m_page->history()->count(), 2);
    QVERIFY(m_page->history()->canGoBack());

    evaluateJavaScriptSync(m_page, "window.history.replaceState(null, 'replace', 'another/location')");
    expectedUrlChangeCount++;
    QCOMPARE(urlChangedSpy.count(), expectedUrlChangeCount);
    QCOMPARE(m_page->url(), QUrl("qrc:/navigate/to/another/location"));
    QCOMPARE(m_page->history()->count(), 2);
    QVERIFY(!m_page->history()->canGoForward());
    QVERIFY(m_page->history()->canGoBack());

    evaluateJavaScriptSync(m_page, "window.history.back()");
    QTest::qWait(100);
    expectedUrlChangeCount++;
    QCOMPARE(urlChangedSpy.count(), expectedUrlChangeCount);
    QCOMPARE(m_page->url(), QUrl("qrc:/test1.html"));
    QVERIFY(m_page->history()->canGoForward());
    QVERIFY(!m_page->history()->canGoBack());
}

void tst_QWebEngineFrame::setUrlSameUrl()
{
#if !defined(QWEBENGINEPAGE_SETNETWORKACCESSMANAGER)
    QSKIP("QWEBENGINEPAGE_SETNETWORKACCESSMANAGER");
#else
    const QUrl url1("qrc:/test1.html");
    const QUrl url2("qrc:/test2.html");

    QWebEnginePage page;
    FakeNetworkManager* networkManager = new FakeNetworkManager(&page);
    page.setNetworkAccessManager(networkManager);

    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));

    page.setUrl(url1);
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QVERIFY(page.url() != url1); // Nota bene: our QNAM redirects url1 to url2
    QCOMPARE(page.url(), url2);
    QCOMPARE(spy.count(), 1);

    page.setUrl(url1);
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QVERIFY(page.url() != url1);
    QCOMPARE(page.url(), url2);
    QCOMPARE(spy.count(), 2);

    // Now a case without redirect. The existing behavior we have for setUrl()
    // is more like a "clear(); load()", so the page will be loaded again, even
    // if urlToBeLoaded == url(). This test should be changed if we want to
    // make setUrl() early return in this case.
    page.setUrl(url2);
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(page.url(), url2);
    QCOMPARE(spy.count(), 3);

    page.setUrl(url1);
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(page.url(), url2);
    QCOMPARE(spy.count(), 4);
#endif
}

static inline QUrl extractBaseUrl(const QUrl& url)
{
    return url.resolved(QUrl());
}

void tst_QWebEngineFrame::setUrlThenLoads_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QUrl>("baseUrl");

    QTest::newRow("resource file") << QUrl("qrc:/test1.html") << extractBaseUrl(QUrl("qrc:/test1.html"));
    QTest::newRow("base specified in HTML") << QUrl("data:text/html,<head><base href=\"http://different.base/\"></head>") << QUrl("http://different.base/");
}

void tst_QWebEngineFrame::setUrlThenLoads()
{
    QFETCH(QUrl, url);
    QFETCH(QUrl, baseUrl);
    QSignalSpy urlChangedSpy(m_page, SIGNAL(urlChanged(QUrl)));
    QSignalSpy startedSpy(m_page, SIGNAL(loadStarted()));
    QSignalSpy finishedSpy(m_page, SIGNAL(loadFinished(bool)));

    m_page->setUrl(url);
    QTRY_COMPARE(startedSpy.count(), 1);
    QTRY_COMPARE(urlChangedSpy.count(), 1);
    QTRY_COMPARE(finishedSpy.count(), 1);
    QVERIFY(finishedSpy.at(0).first().toBool());
    QCOMPARE(m_page->url(), url);
    QCOMPARE(m_page->requestedUrl(), url);
    QCOMPARE(baseUrlSync(m_page), baseUrl);

    const QUrl urlToLoad1("qrc:/test2.html");
    const QUrl urlToLoad2("qrc:/test1.html");

    // Just after first load. URL didn't changed yet.
    m_page->load(urlToLoad1);
    QCOMPARE(m_page->url(), url);
    QCOMPARE(m_page->requestedUrl(), urlToLoad1);
    // baseUrlSync spins an event loop and this sometimes return the next result.
    // QCOMPARE(baseUrlSync(m_page), baseUrl);
    QTRY_COMPARE(startedSpy.count(), 2);

    // After first URL changed.
    QTRY_COMPARE(urlChangedSpy.count(), 2);
    QTRY_COMPARE(finishedSpy.count(), 2);
    QVERIFY(finishedSpy.at(1).first().toBool());
    QCOMPARE(m_page->url(), urlToLoad1);
    QCOMPARE(m_page->requestedUrl(), urlToLoad1);
    QCOMPARE(baseUrlSync(m_page), extractBaseUrl(urlToLoad1));

    // Just after second load. URL didn't changed yet.
    m_page->load(urlToLoad2);
    QCOMPARE(m_page->url(), urlToLoad1);
    QCOMPARE(m_page->requestedUrl(), urlToLoad2);
    QCOMPARE(baseUrlSync(m_page), extractBaseUrl(urlToLoad1));
    QTRY_COMPARE(startedSpy.count(), 3);

    // After second URL changed.
    QTRY_COMPARE(urlChangedSpy.count(), 3);
    QTRY_COMPARE(finishedSpy.count(), 3);
    QVERIFY(finishedSpy.at(2).first().toBool());
    QCOMPARE(m_page->url(), urlToLoad2);
    QCOMPARE(m_page->requestedUrl(), urlToLoad2);
    QCOMPARE(baseUrlSync(m_page), extractBaseUrl(urlToLoad2));
}

void tst_QWebEngineFrame::loadFinishedAfterNotFoundError()
{
#if !defined(QWEBENGINEPAGE_SETNETWORKACCESSMANAGER)
    QSKIP("QWEBENGINEPAGE_SETNETWORKACCESSMANAGER");
#else
    QWebEnginePage page;

    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));
    FakeNetworkManager* networkManager = new FakeNetworkManager(&page);
    page.setNetworkAccessManager(networkManager);

    page.setUrl(FakeReply::urlFor404ErrorWithoutContents);
    QTRY_COMPARE(spy.count(), 1);
    const bool wasLoadOk = spy.at(0).at(0).toBool();
    QVERIFY(!wasLoadOk);
#endif
}

class URLSetter : public QObject {
    Q_OBJECT

public:
    enum Signal {
        LoadStarted,
        LoadFinished,
        ProvisionalLoad
    };

    enum Type {
        UseLoad,
        UseSetUrl
    };

    URLSetter(QWebEnginePage*, Signal, Type, const QUrl&);

public Q_SLOTS:
    void execute();

Q_SIGNALS:
    void finished();

private:
    QWebEnginePage* m_page;
    QUrl m_url;
    Type m_type;
};

Q_DECLARE_METATYPE(URLSetter::Signal)
Q_DECLARE_METATYPE(URLSetter::Type)

URLSetter::URLSetter(QWebEnginePage* page, Signal signal, URLSetter::Type type, const QUrl& url)
    : m_page(page), m_url(url), m_type(type)
{
    if (signal == LoadStarted)
        connect(m_page, SIGNAL(loadStarted()), SLOT(execute()));
    else if (signal == LoadFinished)
        connect(m_page, SIGNAL(loadFinished(bool)), SLOT(execute()));
    else
        connect(m_page, SIGNAL(provisionalLoad()), SLOT(execute()));
}

void URLSetter::execute()
{
    // We track only the first emission.
    m_page->disconnect(this);
    if (m_type == URLSetter::UseLoad)
        m_page->load(m_url);
    else
        m_page->setUrl(m_url);
    connect(m_page, SIGNAL(loadFinished(bool)), SIGNAL(finished()));
}

void tst_QWebEngineFrame::loadInSignalHandlers_data()
{
    QSKIP("FIXME: This crashes in content::WebContentsImpl::NavigateToEntry because of reentrancy. Should we require QueuedConnections or do it ourselves to support this?");

    QTest::addColumn<URLSetter::Type>("type");
    QTest::addColumn<URLSetter::Signal>("signal");
    QTest::addColumn<QUrl>("url");

    const QUrl validUrl("qrc:/test2.html");
    const QUrl invalidUrl("qrc:/invalid");

    QTest::newRow("call load() in loadStarted() after valid url") << URLSetter::UseLoad << URLSetter::LoadStarted << validUrl;
    QTest::newRow("call load() in loadStarted() after invalid url") << URLSetter::UseLoad << URLSetter::LoadStarted << invalidUrl;
    QTest::newRow("call load() in loadFinished() after valid url") << URLSetter::UseLoad << URLSetter::LoadFinished << validUrl;
    QTest::newRow("call load() in loadFinished() after invalid url") << URLSetter::UseLoad << URLSetter::LoadFinished << invalidUrl;
    QTest::newRow("call load() in provisionalLoad() after valid url") << URLSetter::UseLoad << URLSetter::ProvisionalLoad << validUrl;
    QTest::newRow("call load() in provisionalLoad() after invalid url") << URLSetter::UseLoad << URLSetter::ProvisionalLoad << invalidUrl;

    QTest::newRow("call setUrl() in loadStarted() after valid url") << URLSetter::UseSetUrl << URLSetter::LoadStarted << validUrl;
    QTest::newRow("call setUrl() in loadStarted() after invalid url") << URLSetter::UseSetUrl << URLSetter::LoadStarted << invalidUrl;
    QTest::newRow("call setUrl() in loadFinished() after valid url") << URLSetter::UseSetUrl << URLSetter::LoadFinished << validUrl;
    QTest::newRow("call setUrl() in loadFinished() after invalid url") << URLSetter::UseSetUrl << URLSetter::LoadFinished << invalidUrl;
    QTest::newRow("call setUrl() in provisionalLoad() after valid url") << URLSetter::UseSetUrl << URLSetter::ProvisionalLoad << validUrl;
    QTest::newRow("call setUrl() in provisionalLoad() after invalid url") << URLSetter::UseSetUrl << URLSetter::ProvisionalLoad << invalidUrl;
}

void tst_QWebEngineFrame::loadInSignalHandlers()
{
    QFETCH(URLSetter::Type, type);
    QFETCH(URLSetter::Signal, signal);
    QFETCH(QUrl, url);

    const QUrl urlForSetter("qrc:/test1.html");
    URLSetter setter(m_page, signal, type, urlForSetter);

    m_page->load(url);
    waitForSignal(&setter, SIGNAL(finished()), 200);
    QCOMPARE(m_page->url(), urlForSetter);
}

QTEST_MAIN(tst_QWebEngineFrame)
#include "tst_qwebengineframe.moc"
