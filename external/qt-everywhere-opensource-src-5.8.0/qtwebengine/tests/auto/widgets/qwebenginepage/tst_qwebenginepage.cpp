/*
    Copyright (C) 2016 The Qt Company Ltd.
    Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
    Copyright (C) 2010 Holger Hans Peter Freyther

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

#include "../util.h"
#include <QByteArray>
#include <QClipboard>
#include <QDir>
#include <QGraphicsWidget>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMimeDatabase>
#include <QOpenGLWidget>
#include <QPaintEngine>
#include <QPushButton>
#include <QStateMachine>
#include <QStyle>
#include <QtGui/QClipboard>
#include <QtTest/QtTest>
#include <QTextCharFormat>
#include <QWebChannel>
#include <private/qinputmethod_p.h>
#include <qnetworkcookiejar.h>
#include <qnetworkreply.h>
#include <qnetworkrequest.h>
#include <qpa/qplatforminputcontext.h>
#include <qwebenginedownloaditem.h>
#include <qwebenginefullscreenrequest.h>
#include <qwebenginehistory.h>
#include <qwebenginepage.h>
#include <qwebengineprofile.h>
#include <qwebenginescript.h>
#include <qwebenginescriptcollection.h>
#include <qwebenginesettings.h>
#include <qwebengineview.h>
#include <qimagewriter.h>

static void removeRecursive(const QString& dirname)
{
    QDir dir(dirname);
    QFileInfoList entries(dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot));
    for (int i = 0; i < entries.count(); ++i)
        if (entries[i].isDir())
            removeRecursive(entries[i].filePath());
        else
            dir.remove(entries[i].fileName());
    QDir().rmdir(dirname);
}

class TestInputContext : public QPlatformInputContext
{
public:
    TestInputContext()
    : m_visible(false)
    {
        QInputMethodPrivate* inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
        inputMethodPrivate->testContext = this;
    }

    ~TestInputContext()
    {
        QInputMethodPrivate* inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
        inputMethodPrivate->testContext = 0;
    }

    virtual void showInputPanel()
    {
        m_visible = true;
    }
    virtual void hideInputPanel()
    {
        m_visible = false;
    }
    virtual bool isInputPanelVisible() const
    {
        return m_visible;
    }

    bool m_visible;
};

class tst_QWebEnginePage : public QObject
{
    Q_OBJECT

public:
    tst_QWebEnginePage();
    virtual ~tst_QWebEnginePage();

    bool eventFilter(QObject *watched, QEvent *event);

public Q_SLOTS:
    void init();
    void cleanup();
    void cleanupFiles();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void thirdPartyCookiePolicy();
    void comboBoxPopupPositionAfterMove();
    void comboBoxPopupPositionAfterChildMove();
    void contextMenuCopy();
    void contextMenuPopulatedOnce();
    void acceptNavigationRequest();
    void acceptNavigationRequestNavigationType();
    void geolocationRequestJS_data();
    void geolocationRequestJS();
    void loadFinished();
    void actionStates();
    void pasteImage();
    void popupFormSubmission();
    void userStyleSheet();
    void userStyleSheetFromLocalFileUrl();
    void userStyleSheetFromQrcUrl();
    void loadHtml5Video();
    void modified();
    void contextMenuCrash();
    void updatePositionDependentActionsCrash();
    void createPluginWithPluginsEnabled();
    void createPluginWithPluginsDisabled();
    void destroyPlugin_data();
    void destroyPlugin();
    void createViewlessPlugin_data();
    void createViewlessPlugin();
    void graphicsWidgetPlugin();
    void multiplePageGroupsAndLocalStorage();
    void cursorMovements();
    void textSelection();
    void textEditing();
    void backActionUpdate();
    void protectBindingsRuntimeObjectsFromCollector();
    void testOptionalJSObjects();
    void testLocalStorageVisibility();
    void testEnablePersistentStorage();
    void consoleOutput();
    void inputMethods_data();
    void inputMethods();
    void errorPageExtension();
    void errorPageExtensionLoadFinished();
    void userAgentNewlineStripping();
    void undoActionHaveCustomText();
    void renderWidgetHostViewNotShowTopLevel();
    void getUserMediaRequest();
    void savePage();

    void crashTests_LazyInitializationOfMainFrame();

    void screenshot_data();
    void screenshot();

#if defined(ENABLE_WEBGL) && ENABLE_WEBGL
    void acceleratedWebGLScreenshotWithoutView();
    void unacceleratedWebGLScreenshotWithoutView();
#endif

    void testJSPrompt();
    void testStopScheduledPageRefresh();
    void findText();
    void findTextResult();
    void findTextSuccessiveShouldCallAllCallbacks();
    void supportedContentType();
    // [Qt] tst_QWebEnginePage::infiniteLoopJS() timeouts with DFG JIT
    // https://bugs.webkit.org/show_bug.cgi?id=79040
    // void infiniteLoopJS();
    void deleteQWebEngineViewTwice();
    void renderOnRepaintRequestedShouldNotRecurse();
    void loadSignalsOrder_data();
    void loadSignalsOrder();
    void openWindowDefaultSize();
    void cssMediaTypeGlobalSetting();
    void cssMediaTypePageSetting();

#ifdef Q_OS_MAC
    void macCopyUnicodeToClipboard();
#endif

    void runJavaScript();
    void fullScreenRequested();


    // Tests from tst_QWebEngineFrame
    void horizontalScrollAfterBack();
    void symmetricUrl();
    void progressSignal();
    void urlChange();
    void requestedUrlAfterSetAndLoadFailures();
    void asyncAndDelete();
    void earlyToHtml();
    void setHtml();
    void setHtmlWithImageResource();
    void setHtmlWithStylesheetResource();
    void setHtmlWithBaseURL();
    void setHtmlWithJSAlert();
    void inputFieldFocus();
    void hitTestContent();
    void baseUrl_data();
    void baseUrl();
    void scrollPosition();
    void scrollToAnchor();
    void scrollbarsOff();
    void evaluateWillCauseRepaint();
    void setContent_data();
    void setContent();
    void setCacheLoadControlAttribute();
    void setUrlWithPendingLoads();
    void setUrlToEmpty();
    void setUrlToInvalid();
    void setUrlHistory();
    void setUrlUsingStateObject();
    void setUrlThenLoads_data();
    void setUrlThenLoads();
    void loadFinishedAfterNotFoundError();
    void loadInSignalHandlers_data();
    void loadInSignalHandlers();

    void restoreHistory();
    void toPlainTextLoadFinishedRace_data();
    void toPlainTextLoadFinishedRace();
    void setZoomFactor();
    void mouseButtonTranslation();

    void printToPdf();
    void viewSource();
    void viewSourceURL_data();
    void viewSourceURL();

private:
    static QPoint elementCenter(QWebEnginePage *page, const QString &id);

    QWebEngineView* m_view;
    QWebEnginePage* m_page;
    QWebEngineView* m_inputFieldsTestView;
    int m_inputFieldTestPaintCount;
    QString tmpDirPath() const
    {
        static QString tmpd = QDir::tempPath() + "/tst_qwebenginepage-"
            + QDateTime::currentDateTime().toString(QLatin1String("yyyyMMddhhmmss"));
        return tmpd;
    }
};

tst_QWebEnginePage::tst_QWebEnginePage()
{
}

tst_QWebEnginePage::~tst_QWebEnginePage()
{
}

bool tst_QWebEnginePage::eventFilter(QObject* watched, QEvent* event)
{
    // used on the inputFieldFocus test
    if (watched == m_inputFieldsTestView) {
        if (event->type() == QEvent::Paint)
            m_inputFieldTestPaintCount++;
    }
    return QObject::eventFilter(watched, event);
}

void tst_QWebEnginePage::init()
{
    m_view = new QWebEngineView();
    m_page = m_view->page();
    m_page->settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);
}

void tst_QWebEnginePage::cleanup()
{
    delete m_view;
}

void tst_QWebEnginePage::cleanupFiles()
{
    removeRecursive(tmpDirPath());
}

void tst_QWebEnginePage::initTestCase()
{
    QLocale::setDefault(QLocale("en"));
    cleanupFiles(); // In case there are old files from previous runs

    // Set custom path since the CI doesn't install test plugins.
    // Stolen from qtlocation/tests/auto/positionplugintest.
    QString searchPath = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    searchPath += QStringLiteral("/..");
#endif
    searchPath += QStringLiteral("/../../../plugins");
    QCoreApplication::addLibraryPath(searchPath);
}

void tst_QWebEnginePage::cleanupTestCase()
{
    cleanupFiles(); // Be nice
}

class NavigationRequestOverride : public QWebEnginePage
{
public:
    NavigationRequestOverride(QWebEngineView* parent, bool initialValue) : QWebEnginePage(parent), m_acceptNavigationRequest(initialValue) {}

    bool m_acceptNavigationRequest;
protected:
    virtual bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
    {
        Q_UNUSED(url);
        Q_UNUSED(type);
        Q_UNUSED(isMainFrame);

        return m_acceptNavigationRequest;
    }
};

void tst_QWebEnginePage::acceptNavigationRequest()
{
    QWebEngineView *view = new QWebEngineView();
    QSignalSpy loadSpy(view, SIGNAL(loadFinished(bool)));

    NavigationRequestOverride* newPage = new NavigationRequestOverride(view, false);
    view->setPage(newPage);

    view->setHtml(QString("<html><body><form name='tstform' action='data:text/html,foo'method='get'>"
                            "<input type='text'><input type='submit'></form></body></html>"), QUrl());
    QTRY_COMPARE(loadSpy.count(), 1);

    evaluateJavaScriptSync(view->page(), "tstform.submit();");

    newPage->m_acceptNavigationRequest = true;
    evaluateJavaScriptSync(view->page(), "tstform.submit();");
    QTRY_COMPARE(loadSpy.count(), 2);

    QCOMPARE(toPlainTextSync(view->page()), QString("foo?"));

    delete view;
}

class JSTestPage : public QWebEnginePage
{
Q_OBJECT
public:
    JSTestPage(QObject* parent = 0)
    : QWebEnginePage(parent) {}

    virtual bool shouldInterruptJavaScript()
    {
        return true;
    }
public Q_SLOTS:
    void requestPermission(const QUrl &origin, QWebEnginePage::Feature feature)
    {
        if (m_allowGeolocation)
            setFeaturePermission(origin, feature, PermissionGrantedByUser);
        else
            setFeaturePermission(origin, feature, PermissionDeniedByUser);
    }

public:
    void setGeolocationPermission(bool allow)
    {
        m_allowGeolocation = allow;
    }

private:
    bool m_allowGeolocation;
};

// [Qt] tst_QWebEnginePage::infiniteLoopJS() timeouts with DFG JIT
// https://bugs.webkit.org/show_bug.cgi?id=79040
/*
void tst_QWebEnginePage::infiniteLoopJS()
{
    JSTestPage* newPage = new JSTestPage(m_view);
    m_view->setPage(newPage);
    m_view->setHtml(QString("<html><body>test</body></html>"), QUrl());
    m_view->page()->evaluateJavaScript("var run = true; var a = 1; while (run) { a++; }");
    delete newPage;
}
*/

void tst_QWebEnginePage::geolocationRequestJS_data()
{
    QTest::addColumn<bool>("allowed");
    QTest::addColumn<int>("errorCode");
    QTest::newRow("allowed") << true << 0;
    QTest::newRow("not allowed") << false << 1;
}

void tst_QWebEnginePage::geolocationRequestJS()
{
    QFETCH(bool, allowed);
    QFETCH(int, errorCode);
    QWebEngineView *view = new QWebEngineView;
    JSTestPage *newPage = new JSTestPage(view);
    newPage->setView(view);
    newPage->setGeolocationPermission(allowed);

    connect(newPage, SIGNAL(featurePermissionRequested(const QUrl&, QWebEnginePage::Feature)),
            newPage, SLOT(requestPermission(const QUrl&, QWebEnginePage::Feature)));

    QSignalSpy spyLoadFinished(newPage, SIGNAL(loadFinished(bool)));
    newPage->setHtml(QString("<html><body>test</body></html>"), QUrl("qrc://secure/origin"));
    QTRY_COMPARE(spyLoadFinished.count(), 1);
    if (evaluateJavaScriptSync(newPage, QLatin1String("!navigator.geolocation")).toBool()) {
        delete view;
        W_QSKIP("Geolocation is not supported.", SkipSingle);
    }

    evaluateJavaScriptSync(newPage, "var errorCode = 0; var done = false; function error(err) { errorCode = err.code; done = true; } function success(pos) { done = true; } navigator.geolocation.getCurrentPosition(success, error)");

    QTRY_VERIFY(evaluateJavaScriptSync(newPage, "done").toBool());
    int result = evaluateJavaScriptSync(newPage, "errorCode").toInt();
    if (result == 2)
        QEXPECT_FAIL("", "No location service available.", Continue);
    QCOMPARE(result, errorCode);

    delete view;
}

void tst_QWebEnginePage::loadFinished()
{
    QWebEnginePage page;
    QSignalSpy spyLoadStarted(&page, SIGNAL(loadStarted()));
    QSignalSpy spyLoadFinished(&page, SIGNAL(loadFinished(bool)));

    page.load(QUrl("data:text/html,<frameset cols=\"25%,75%\"><frame src=\"data:text/html,"
                                           "<head><meta http-equiv='refresh' content='1'></head>foo \">"
                                           "<frame src=\"data:text/html,bar\"></frameset>"));
    QTRY_COMPARE(spyLoadFinished.count(), 1);

    QEXPECT_FAIL("", "Behavior change: Load signals are emitted only for the main frame in QtWebEngine.", Continue);
    QTRY_VERIFY_WITH_TIMEOUT(spyLoadStarted.count() > 1, 100);
    QEXPECT_FAIL("", "Behavior change: Load signals are emitted only for the main frame in QtWebEngine.", Continue);
    QTRY_VERIFY_WITH_TIMEOUT(spyLoadFinished.count() > 1, 100);

    spyLoadFinished.clear();

    page.load(QUrl("data:text/html,<frameset cols=\"25%,75%\"><frame src=\"data:text/html,"
                                           "foo \"><frame src=\"data:text/html,bar\"></frameset>"));
    QTRY_COMPARE(spyLoadFinished.count(), 1);
    QCOMPARE(spyLoadFinished.count(), 1);
}

void tst_QWebEnginePage::actionStates()
{
    m_page->load(QUrl("qrc:///resources/script.html"));

    QAction* reloadAction = m_page->action(QWebEnginePage::Reload);
    QAction* stopAction = m_page->action(QWebEnginePage::Stop);

    QTRY_VERIFY(reloadAction->isEnabled());
    QTRY_VERIFY(!stopAction->isEnabled());
}

static QImage imageWithoutAlpha(const QImage &image)
{
    QImage result = image;
    QPainter painter(&result);
    painter.fillRect(result.rect(), Qt::green);
    painter.drawImage(0, 0, image);
    return result;
}

void tst_QWebEnginePage::pasteImage()
{
    // Pixels with an alpha value of 0 will have different RGB values after the
    // test -> clipboard -> webengine -> test roundtrip.
    // Clear the alpha channel to make QCOMPARE happy.
    const QImage origImage = imageWithoutAlpha(QImage(":/resources/image.png"));
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setImage(origImage);
    QWebEnginePage *page = m_view->page();
    page->load(QUrl("qrc:///resources/pasteimage.html"));
    QVERIFY(waitForSignal(m_view, SIGNAL(loadFinished(bool))));
    page->triggerAction(QWebEnginePage::Paste);
    QTRY_VERIFY(evaluateJavaScriptSync(page,
            "window.myImageDataURL ? window.myImageDataURL.length : 0").toInt() > 0);
    QByteArray data = evaluateJavaScriptSync(page, "window.myImageDataURL").toByteArray();
    data.remove(0, data.indexOf(";base64,") + 8);
    const QImage image = QImage::fromData(QByteArray::fromBase64(data), "PNG");
    QCOMPARE(image, origImage);
}

class ConsolePage : public QWebEnginePage
{
public:
    ConsolePage(QObject* parent = 0) : QWebEnginePage(parent) {}

    virtual void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID)
    {
        levels.append(level);
        messages.append(message);
        lineNumbers.append(lineNumber);
        sourceIDs.append(sourceID);
    }

    QList<int> levels;
    QStringList messages;
    QList<int> lineNumbers;
    QStringList sourceIDs;
};

void tst_QWebEnginePage::consoleOutput()
{
    ConsolePage page;
    // We don't care about the result but want this to be synchronous
    evaluateJavaScriptSync(&page, "this is not valid JavaScript");
    QCOMPARE(page.messages.count(), 1);
    QCOMPARE(page.lineNumbers.at(0), 1);
}

class TestPage : public QWebEnginePage {
    Q_OBJECT
public:
    TestPage(QObject* parent = 0) : QWebEnginePage(parent)
    {
        connect(this, SIGNAL(geometryChangeRequested(QRect)), this, SLOT(slotGeometryChangeRequested(QRect)));
    }

    struct Navigation {
        NavigationType type;
        QUrl url;
        bool isMainFrame;
    };
    QList<Navigation> navigations;

    virtual bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
    {
        Navigation n;
        n.url = url;
        n.type = type;
        n.isMainFrame = isMainFrame;
        navigations.append(n);
        return true;
    }

    QList<TestPage*> createdWindows;
    virtual QWebEnginePage* createWindow(WebWindowType) {
        TestPage* page = new TestPage(this);
        createdWindows.append(page);
        emit windowCreated();
        return page;
    }

    QRect requestedGeometry;

signals:
    void windowCreated();

private Q_SLOTS:
    void slotGeometryChangeRequested(const QRect& geom) {
        requestedGeometry = geom;
    }
};

void tst_QWebEnginePage::acceptNavigationRequestNavigationType()
{

    TestPage page;
    QSignalSpy loadSpy(&page, SIGNAL(loadFinished(bool)));

    page.load(QUrl("qrc:///resources/script.html"));
    QTRY_COMPARE(loadSpy.count(), 1);
    QTRY_COMPARE(page.navigations.count(), 1);

    page.load(QUrl("qrc:///resources/content.html"));
    QTRY_COMPARE(loadSpy.count(), 2);
    QTRY_COMPARE(page.navigations.count(), 2);

    page.triggerAction(QWebEnginePage::Stop);
    QVERIFY(page.history()->canGoBack());
    page.triggerAction(QWebEnginePage::Back);

    QTRY_COMPARE(loadSpy.count(), 3);
    QTRY_COMPARE(page.navigations.count(), 3);

    page.triggerAction(QWebEnginePage::Reload);
    QTRY_COMPARE(loadSpy.count(), 4);
    QTRY_COMPARE(page.navigations.count(), 4);

    QList<QWebEnginePage::NavigationType> expectedList;
    expectedList << QWebEnginePage::NavigationTypeTyped
        << QWebEnginePage::NavigationTypeTyped
        << QWebEnginePage::NavigationTypeBackForward
        << QWebEnginePage::NavigationTypeReload;
    QVERIFY(expectedList.count() == page.navigations.count());
    for (int i = 0; i < expectedList.count(); ++i) {
        QCOMPARE(page.navigations[i].type, expectedList[i]);
    }
}

void tst_QWebEnginePage::popupFormSubmission()
{
    TestPage page;
    QSignalSpy loadFinishedSpy(&page, SIGNAL(loadFinished(bool)));
    QSignalSpy windowCreatedSpy(&page, SIGNAL(windowCreated()));

    page.settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    page.setHtml("<form name='form1' method=get action='' target='myNewWin'>"
                 "  <input type='hidden' name='foo' value='bar'>"
                 "</form>");
    QTRY_COMPARE(loadFinishedSpy.count(), 1);

    page.runJavaScript("window.open('', 'myNewWin', 'width=500,height=300,toolbar=0');");
    evaluateJavaScriptSync(&page, "document.form1.submit();");
    QTRY_COMPARE(windowCreatedSpy.count(), 1);

    // The number of popup created should be one.
    QVERIFY(page.createdWindows.size() == 1);

    QTRY_VERIFY(!page.createdWindows[0]->url().isEmpty());
    QString url = page.createdWindows[0]->url().toString();

    // Check if the form submission was OK.
    QVERIFY(url.contains("?foo=bar"));
}

class TestNetworkManager : public QNetworkAccessManager
{
public:
    TestNetworkManager(QObject* parent) : QNetworkAccessManager(parent) {}

    QList<QUrl> requestedUrls;
    QList<QNetworkRequest> requests;

protected:
    virtual QNetworkReply* createRequest(Operation op, const QNetworkRequest &request, QIODevice* outgoingData) {
        requests.append(request);
        requestedUrls.append(request.url());
        return QNetworkAccessManager::createRequest(op, request, outgoingData);
    }
};

void tst_QWebEnginePage::userStyleSheet()
{
#if !defined(QWEBENGINEPAGE_SETNETWORKACCESSMANAGER)
    QSKIP("QWEBENGINEPAGE_SETNETWORKACCESSMANAGER");
#else
    TestNetworkManager* networkManager = new TestNetworkManager(m_page);
    m_page->setNetworkAccessManager(networkManager);

    m_page->settings()->setUserStyleSheetUrl(QUrl("data:text/css;charset=utf-8;base64,"
            + QByteArray("p { background-image: url('http://does.not/exist.png');}").toBase64()));
    m_view->setHtml("<p>hello world</p>");
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    QVERIFY(networkManager->requestedUrls.count() >= 1);
    QCOMPARE(networkManager->requestedUrls.at(0), QUrl("http://does.not/exist.png"));
#endif
}

void tst_QWebEnginePage::userStyleSheetFromLocalFileUrl()
{
#if !defined(QWEBENGINEPAGE_SETNETWORKACCESSMANAGER)
    QSKIP("QWEBENGINEPAGE_SETNETWORKACCESSMANAGER");
#else
    TestNetworkManager* networkManager = new TestNetworkManager(m_page);
    m_page->setNetworkAccessManager(networkManager);

    QUrl styleSheetUrl = QUrl::fromLocalFile(TESTS_SOURCE_DIR + QLatin1String("qwebenginepage/resources/user.css"));
    m_page->settings()->setUserStyleSheetUrl(styleSheetUrl);
    m_view->setHtml("<p>hello world</p>");
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    QVERIFY(networkManager->requestedUrls.count() >= 1);
    QCOMPARE(networkManager->requestedUrls.at(0), QUrl("http://does.not/exist.png"));
#endif
}

void tst_QWebEnginePage::userStyleSheetFromQrcUrl()
{
#if !defined(QWEBENGINEPAGE_SETNETWORKACCESSMANAGER)
    QSKIP("QWEBENGINEPAGE_SETNETWORKACCESSMANAGER");
#else
    TestNetworkManager* networkManager = new TestNetworkManager(m_page);
    m_page->setNetworkAccessManager(networkManager);

    m_page->settings()->setUserStyleSheetUrl(QUrl("qrc:///resources/user.css"));
    m_view->setHtml("<p>hello world</p>");
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    QVERIFY(networkManager->requestedUrls.count() >= 1);
    QCOMPARE(networkManager->requestedUrls.at(0), QUrl("http://does.not/exist.png"));
#endif
}

void tst_QWebEnginePage::loadHtml5Video()
{
#if defined(WTF_USE_QT_MULTIMEDIA) && WTF_USE_QT_MULTIMEDIA
    QByteArray url("http://does.not/exist?a=1%2Cb=2");
    m_view->setHtml("<p><video id ='video' src='" + url + "' autoplay/></p>");
    QTest::qWait(2000);
    QUrl mUrl = DumpRenderTreeSupportQt::mediaContentUrlByElementId(m_page->mainFrame()->handle(), "video");
    QEXPECT_FAIL("", "https://bugs.webkit.org/show_bug.cgi?id=65452", Continue);
    QCOMPARE(mUrl.toEncoded(), url);
#else
    W_QSKIP("This test requires Qt Multimedia", SkipAll);
#endif
}

void tst_QWebEnginePage::modified()
{
#if !defined(QWEBENGINEPAGE_ISMODIFIED)
    QSKIP("QWEBENGINEPAGE_ISMODIFIED");
#else
    m_page->setUrl(QUrl("data:text/html,<body>blub"));
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    m_page->setUrl(QUrl("data:text/html,<body id=foo contenteditable>blah"));
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    QVERIFY(!m_page->isModified());

    m_page->runJavaScript("document.getElementById('foo').focus()");
    evaluateJavaScriptSync(m_page, "document.execCommand('InsertText', true, 'Test');");

    QVERIFY(m_page->isModified());

    evaluateJavaScriptSync(m_page, "document.execCommand('Undo', true);");

    QVERIFY(!m_page->isModified());

    evaluateJavaScriptSync(m_page, "document.execCommand('Redo', true);");

    QVERIFY(m_page->isModified());

    QVERIFY(m_page->history()->canGoBack());
    QVERIFY(!m_page->history()->canGoForward());
    QCOMPARE(m_page->history()->count(), 2);
    QVERIFY(m_page->history()->backItem().isValid());
    QVERIFY(!m_page->history()->forwardItem().isValid());

    m_page->history()->back();
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    QVERIFY(!m_page->history()->canGoBack());
    QVERIFY(m_page->history()->canGoForward());

    QVERIFY(!m_page->isModified());

    QCOMPARE(m_page->history()->currentItemIndex(), 0);

    m_page->history()->setMaximumItemCount(3);
    QCOMPARE(m_page->history()->maximumItemCount(), 3);

    QVariant variant("string test");
    m_page->history()->currentItem().setUserData(variant);
    QVERIFY(m_page->history()->currentItem().userData().toString() == "string test");

    m_page->setUrl(QUrl("data:text/html,<body>This is second page"));
    m_page->setUrl(QUrl("data:text/html,<body>This is third page"));
    QCOMPARE(m_page->history()->count(), 2);
    m_page->setUrl(QUrl("data:text/html,<body>This is fourth page"));
    QCOMPARE(m_page->history()->count(), 2);
    m_page->setUrl(QUrl("data:text/html,<body>This is fifth page"));
    QVERIFY(::waitForSignal(m_page, SIGNAL(saveFrameStateRequested(QWebEngineFrame*,QWebEngineHistoryItem*))));
#endif
}

// https://bugs.webkit.org/show_bug.cgi?id=51331
void tst_QWebEnginePage::updatePositionDependentActionsCrash()
{
#if !defined(QWEBENGINEPAGE_UPDATEPOSITIONDEPENDENTACTIONS)
    QSKIP("QWEBENGINEPAGE_UPDATEPOSITIONDEPENDENTACTIONS");
#else
    QWebEngineView view;
    view.setHtml("<p>test");
    QPoint pos(0, 0);
    view.page()->updatePositionDependentActions(pos);
    QMenu* contextMenu = 0;
    foreach (QObject* child, view.children()) {
        contextMenu = qobject_cast<QMenu*>(child);
        if (contextMenu)
            break;
    }
    QVERIFY(!contextMenu);
#endif
}

// https://bugs.webkit.org/show_bug.cgi?id=20357
void tst_QWebEnginePage::contextMenuCrash()
{
#if !defined(QWEBENGINEPAGE_SWALLOWCONTEXTMENUEVENT)
    QSKIP("QWEBENGINEPAGE_SWALLOWCONTEXTMENUEVENT");
#else
    QWebEngineView view;
    view.setHtml("<p>test");
    QPoint pos(0, 0);
    QContextMenuEvent event(QContextMenuEvent::Mouse, pos);
    view.page()->swallowContextMenuEvent(&event);
    view.page()->updatePositionDependentActions(pos);
    QMenu* contextMenu = 0;
    foreach (QObject* child, view.children()) {
        contextMenu = qobject_cast<QMenu*>(child);
        if (contextMenu)
            break;
    }
    QVERIFY(contextMenu);
    delete contextMenu;
#endif
}

#if defined(QWEBENGINEPAGE_CREATEPLUGIN)
class PluginPage : public QWebEnginePage
{
public:
    PluginPage(QObject *parent = 0)
        : QWebEnginePage(parent) {}

    struct CallInfo
    {
        CallInfo(const QString &c, const QUrl &u,
                 const QStringList &pn, const QStringList &pv,
                 QObject *r)
            : classid(c), url(u), paramNames(pn),
              paramValues(pv), returnValue(r)
            {}
        QString classid;
        QUrl url;
        QStringList paramNames;
        QStringList paramValues;
        QObject *returnValue;
    };

    QList<CallInfo> calls;

protected:
    virtual QObject *createPlugin(const QString &classid, const QUrl &url,
                                  const QStringList &paramNames,
                                  const QStringList &paramValues)
    {
        QObject *result = 0;
        if (classid == "pushbutton")
            result = new QPushButton();
#ifndef QT_NO_INPUTDIALOG
        else if (classid == "lineedit")
            result = new QLineEdit();
#endif
        else if (classid == "graphicswidget")
            result = new QGraphicsWidget();
        if (result)
            result->setObjectName(classid);
        calls.append(CallInfo(classid, url, paramNames, paramValues, result));
        return result;
    }
};

static void createPlugin(QWebEngineView *view)
{
    QSignalSpy loadSpy(view, SIGNAL(loadFinished(bool)));

    PluginPage* newPage = new PluginPage(view);
    view->setPage(newPage);

    // type has to be application/x-qt-plugin
    view->setHtml(QString("<html><body><object type='application/x-foobarbaz' classid='pushbutton' id='mybutton'/></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 1);
    QCOMPARE(newPage->calls.count(), 0);

    view->setHtml(QString("<html><body><object type='application/x-qt-plugin' classid='pushbutton' id='mybutton'/></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 2);
    QCOMPARE(newPage->calls.count(), 1);
    {
        PluginPage::CallInfo ci = newPage->calls.takeFirst();
        QCOMPARE(ci.classid, QString::fromLatin1("pushbutton"));
        QCOMPARE(ci.url, QUrl());
        QCOMPARE(ci.paramNames.count(), 3);
        QCOMPARE(ci.paramValues.count(), 3);
        QCOMPARE(ci.paramNames.at(0), QString::fromLatin1("type"));
        QCOMPARE(ci.paramValues.at(0), QString::fromLatin1("application/x-qt-plugin"));
        QCOMPARE(ci.paramNames.at(1), QString::fromLatin1("classid"));
        QCOMPARE(ci.paramValues.at(1), QString::fromLatin1("pushbutton"));
        QCOMPARE(ci.paramNames.at(2), QString::fromLatin1("id"));
        QCOMPARE(ci.paramValues.at(2), QString::fromLatin1("mybutton"));
        QVERIFY(ci.returnValue != 0);
        QVERIFY(ci.returnValue->inherits("QPushButton"));
    }
    // test JS bindings
    QCOMPARE(evaluateJavaScriptSync(newPage, "document.getElementById('mybutton').toString()").toString(),
             QString::fromLatin1("[object HTMLObjectElement]"));
    QCOMPARE(evaluateJavaScriptSync(newPage, "mybutton.toString()").toString(),
             QString::fromLatin1("[object HTMLObjectElement]"));
    QCOMPARE(evaluateJavaScriptSync(newPage, "typeof mybutton.objectName").toString(),
             QString::fromLatin1("string"));
    QCOMPARE(evaluateJavaScriptSync(newPage, "mybutton.objectName").toString(),
             QString::fromLatin1("pushbutton"));
    QCOMPARE(evaluateJavaScriptSync(newPage, "typeof mybutton.clicked").toString(),
             QString::fromLatin1("function"));
    QCOMPARE(evaluateJavaScriptSync(newPage, "mybutton.clicked.toString()").toString(),
             QString::fromLatin1("function clicked() {\n    [native code]\n}"));

    view->setHtml(QString("<html><body><table>"
                            "<tr><object type='application/x-qt-plugin' classid='lineedit' id='myedit'/></tr>"
                            "<tr><object type='application/x-qt-plugin' classid='pushbutton' id='mybutton'/></tr>"
                            "</table></body></html>"), QUrl("http://foo.bar.baz"));
    QTRY_COMPARE(loadSpy.count(), 3);
    QCOMPARE(newPage->calls.count(), 2);
    {
        PluginPage::CallInfo ci = newPage->calls.takeFirst();
        QCOMPARE(ci.classid, QString::fromLatin1("lineedit"));
        QCOMPARE(ci.url, QUrl());
        QCOMPARE(ci.paramNames.count(), 3);
        QCOMPARE(ci.paramValues.count(), 3);
        QCOMPARE(ci.paramNames.at(0), QString::fromLatin1("type"));
        QCOMPARE(ci.paramValues.at(0), QString::fromLatin1("application/x-qt-plugin"));
        QCOMPARE(ci.paramNames.at(1), QString::fromLatin1("classid"));
        QCOMPARE(ci.paramValues.at(1), QString::fromLatin1("lineedit"));
        QCOMPARE(ci.paramNames.at(2), QString::fromLatin1("id"));
        QCOMPARE(ci.paramValues.at(2), QString::fromLatin1("myedit"));
        QVERIFY(ci.returnValue != 0);
        QVERIFY(ci.returnValue->inherits("QLineEdit"));
    }
    {
        PluginPage::CallInfo ci = newPage->calls.takeFirst();
        QCOMPARE(ci.classid, QString::fromLatin1("pushbutton"));
        QCOMPARE(ci.url, QUrl());
        QCOMPARE(ci.paramNames.count(), 3);
        QCOMPARE(ci.paramValues.count(), 3);
        QCOMPARE(ci.paramNames.at(0), QString::fromLatin1("type"));
        QCOMPARE(ci.paramValues.at(0), QString::fromLatin1("application/x-qt-plugin"));
        QCOMPARE(ci.paramNames.at(1), QString::fromLatin1("classid"));
        QCOMPARE(ci.paramValues.at(1), QString::fromLatin1("pushbutton"));
        QCOMPARE(ci.paramNames.at(2), QString::fromLatin1("id"));
        QCOMPARE(ci.paramValues.at(2), QString::fromLatin1("mybutton"));
        QVERIFY(ci.returnValue != 0);
        QVERIFY(ci.returnValue->inherits("QPushButton"));
    }
}
#endif

void tst_QWebEnginePage::graphicsWidgetPlugin()
{
#if !defined(QWEBENGINEPAGE_CREATEPLUGIN)
    QSKIP("QWEBENGINEPAGE_CREATEPLUGIN");
#else
    m_view->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    QGraphicsWebView webView;

    QSignalSpy loadSpy(&webView, SIGNAL(loadFinished(bool)));

    PluginPage* newPage = new PluginPage(&webView);
    webView.setPage(newPage);

    // type has to be application/x-qt-plugin
    webView.setHtml(QString("<html><body><object type='application/x-foobarbaz' classid='graphicswidget' id='mygraphicswidget'/></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 1);
    QCOMPARE(newPage->calls.count(), 0);

    webView.setHtml(QString("<html><body><object type='application/x-qt-plugin' classid='graphicswidget' id='mygraphicswidget'/></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 2);
    QCOMPARE(newPage->calls.count(), 1);
    {
        PluginPage::CallInfo ci = newPage->calls.takeFirst();
        QCOMPARE(ci.classid, QString::fromLatin1("graphicswidget"));
        QCOMPARE(ci.url, QUrl());
        QCOMPARE(ci.paramNames.count(), 3);
        QCOMPARE(ci.paramValues.count(), 3);
        QCOMPARE(ci.paramNames.at(0), QString::fromLatin1("type"));
        QCOMPARE(ci.paramValues.at(0), QString::fromLatin1("application/x-qt-plugin"));
        QCOMPARE(ci.paramNames.at(1), QString::fromLatin1("classid"));
        QCOMPARE(ci.paramValues.at(1), QString::fromLatin1("graphicswidget"));
        QCOMPARE(ci.paramNames.at(2), QString::fromLatin1("id"));
        QCOMPARE(ci.paramValues.at(2), QString::fromLatin1("mygraphicswidget"));
        QVERIFY(ci.returnValue);
        QVERIFY(ci.returnValue->inherits("QGraphicsWidget"));
    }
    // test JS bindings
    QCOMPARE(evaluateJavaScriptSync(newPage, "document.getElementById('mygraphicswidget').toString()").toString(),
             QString::fromLatin1("[object HTMLObjectElement]"));
    QCOMPARE(evaluateJavaScriptSync(newPage, "mygraphicswidget.toString()").toString(),
             QString::fromLatin1("[object HTMLObjectElement]"));
    QCOMPARE(evaluateJavaScriptSync(newPage, "typeof mygraphicswidget.objectName").toString(),
             QString::fromLatin1("string"));
    QCOMPARE(evaluateJavaScriptSync(newPage, "mygraphicswidget.objectName").toString(),
             QString::fromLatin1("graphicswidget"));
    QCOMPARE(evaluateJavaScriptSync(newPage, "typeof mygraphicswidget.geometryChanged").toString(),
             QString::fromLatin1("function"));
    QCOMPARE(evaluateJavaScriptSync(newPage, "mygraphicswidget.geometryChanged.toString()").toString(),
             QString::fromLatin1("function geometryChanged() {\n    [native code]\n}"));
#endif
}

void tst_QWebEnginePage::createPluginWithPluginsEnabled()
{
#if !defined(QWEBENGINEPAGE_CREATEPLUGIN)
    QSKIP("QWEBENGINEPAGE_CREATEPLUGIN");
#else
    m_view->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    createPlugin(m_view);
#endif
}

void tst_QWebEnginePage::createPluginWithPluginsDisabled()
{
#if !defined(QWEBENGINEPAGE_CREATEPLUGIN)
    QSKIP("QWEBENGINEPAGE_CREATEPLUGIN");
#else
    // Qt Plugins should be loaded by QtWebEngine even when PluginsEnabled is
    // false. The client decides whether a Qt plugin is enabled or not when
    // it decides whether or not to instantiate it.
    m_view->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    createPlugin(m_view);
#endif
}

#if defined(QWEBENGINEPAGE_CREATEPLUGIN)
// Standard base class for template PluginTracerPage. In tests it is used as interface.
class PluginCounterPage : public QWebEnginePage {
public:
    int m_count;
    QPointer<QObject> m_widget;
    QObject* m_pluginParent;
    PluginCounterPage(QObject* parent = 0)
        : QWebEnginePage(parent)
        , m_count(0)
        , m_pluginParent(0)
    {
       settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    }
    ~PluginCounterPage()
    {
        if (m_pluginParent)
            m_pluginParent->deleteLater();
    }
};

template<class T>
class PluginTracerPage : public PluginCounterPage {
public:
    PluginTracerPage(QObject* parent = 0)
        : PluginCounterPage(parent)
    {
        // this is a dummy parent object for the created plugin
        m_pluginParent = new T;
    }
    virtual QObject* createPlugin(const QString&, const QUrl&, const QStringList&, const QStringList&)
    {
        m_count++;
        m_widget = new T;
        // need a cast to the specific type, as QObject::setParent cannot be called,
        // because it is not virtual. Instead it is necessary to call QWidget::setParent,
        // which also takes a QWidget* instead of a QObject*. Therefore we need to
        // upcast to T*, which is a QWidget.
        static_cast<T*>(m_widget.data())->setParent(static_cast<T*>(m_pluginParent));
        return m_widget.data();
    }
};

class PluginFactory {
public:
    enum FactoredType {QWidgetType, QGraphicsWidgetType};
    static PluginCounterPage* create(FactoredType type, QObject* parent = 0)
    {
        PluginCounterPage* result = 0;
        switch (type) {
        case QWidgetType:
            result = new PluginTracerPage<QWidget>(parent);
            break;
        case QGraphicsWidgetType:
            result = new PluginTracerPage<QGraphicsWidget>(parent);
            break;
        default: {/*Oops*/};
        }
        return result;
    }

    static void prepareTestData()
    {
        QTest::addColumn<int>("type");
        QTest::newRow("QWidget") << (int)PluginFactory::QWidgetType;
        QTest::newRow("QGraphicsWidget") << (int)PluginFactory::QGraphicsWidgetType;
    }
};
#endif

void tst_QWebEnginePage::destroyPlugin_data()
{
#if defined(QWEBENGINEPAGE_CREATEPLUGIN)
    PluginFactory::prepareTestData();
#endif
}

void tst_QWebEnginePage::destroyPlugin()
{
#if !defined(QWEBENGINEPAGE_CREATEPLUGIN)
    QSKIP("QWEBENGINEPAGE_CREATEPLUGIN");
#else
    QFETCH(int, type);
    PluginCounterPage* page = PluginFactory::create((PluginFactory::FactoredType)type, m_view);
    m_view->setPage(page);

    // we create the plugin, so the widget should be constructed
    QString content("<html><body><object type=\"application/x-qt-plugin\" classid=\"QProgressBar\"></object></body></html>");
    m_view->setHtml(content);
    QVERIFY(page->m_widget);
    QCOMPARE(page->m_count, 1);

    // navigate away, the plugin widget should be destructed
    m_view->setHtml("<html><body>Hi</body></html>");
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(!page->m_widget);
#endif
}

void tst_QWebEnginePage::createViewlessPlugin_data()
{
#if defined(QWEBENGINEPAGE_CREATEPLUGIN)
    PluginFactory::prepareTestData();
#endif
}

void tst_QWebEnginePage::createViewlessPlugin()
{
#if !defined(QWEBENGINEPAGE_CREATEPLUGIN)
    QSKIP("QWEBENGINEPAGE_CREATEPLUGIN");
#else
    QFETCH(int, type);
    PluginCounterPage* page = PluginFactory::create((PluginFactory::FactoredType)type);
    QString content("<html><body><object type=\"application/x-qt-plugin\" classid=\"QProgressBar\"></object></body></html>");
    page->setHtml(content);
    QCOMPARE(page->m_count, 1);
    QVERIFY(page->m_widget);
    QVERIFY(page->m_pluginParent);
    QVERIFY(page->m_widget.data()->parent() == page->m_pluginParent);
    delete page;
#endif
}

void tst_QWebEnginePage::multiplePageGroupsAndLocalStorage()
{
#if !defined(QWEBENGINESETTINGS_SETLOCALSTORAGEPATH)
    QSKIP("QWEBENGINESETTINGS_SETLOCALSTORAGEPATH");
#else
    QDir dir(tmpDirPath());
    dir.mkdir("path1");
    dir.mkdir("path2");

    QWebEngineView view1;
    QWebEngineView view2;

    view1.page()->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    view1.page()->settings()->setLocalStoragePath(QDir::toNativeSeparators(tmpDirPath() + "/path1"));
    DumpRenderTreeSupportQt::webPageSetGroupName(view1.page()->handle(), "group1");
    view2.page()->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    view2.page()->settings()->setLocalStoragePath(QDir::toNativeSeparators(tmpDirPath() + "/path2"));
    DumpRenderTreeSupportQt::webPageSetGroupName(view2.page()->handle(), "group2");
    QCOMPARE(DumpRenderTreeSupportQt::webPageGroupName(view1.page()->handle()), QString("group1"));
    QCOMPARE(DumpRenderTreeSupportQt::webPageGroupName(view2.page()->handle()), QString("group2"));


    view1.setHtml(QString("<html><body> </body></html>"), QUrl("http://www.myexample.com"));
    view2.setHtml(QString("<html><body> </body></html>"), QUrl("http://www.myexample.com"));

    evaluateJavaScriptSync(view1.page(), "localStorage.test='value1';");
    evaluateJavaScriptSync(view2.page(), "localStorage.test='value2';");

    view1.setHtml(QString("<html><body> </body></html>"), QUrl("http://www.myexample.com"));
    view2.setHtml(QString("<html><body> </body></html>"), QUrl("http://www.myexample.com"));

    QVariant s1 = evaluateJavaScriptSync(view1.page(), "localStorage.test");
    QCOMPARE(s1.toString(), QString("value1"));

    QVariant s2 = evaluateJavaScriptSync(view2.page(), "localStorage.test");
    QCOMPARE(s2.toString(), QString("value2"));

    QTest::qWait(1000);

    QFile::remove(QDir::toNativeSeparators(tmpDirPath() + "/path1/http_www.myexample.com_0.localstorage"));
    QFile::remove(QDir::toNativeSeparators(tmpDirPath() + "/path2/http_www.myexample.com_0.localstorage"));
    dir.rmdir(QDir::toNativeSeparators("./path1"));
    dir.rmdir(QDir::toNativeSeparators("./path2"));
#endif
}

class CursorTrackedPage : public QWebEnginePage
{
public:

    CursorTrackedPage(QWidget *parent = 0): QWebEnginePage(parent) {
    }

    QString selectedText() {
        return evaluateJavaScriptSync(this, "window.getSelection().toString()").toString();
    }

    int selectionStartOffset() {
        return evaluateJavaScriptSync(this, "window.getSelection().getRangeAt(0).startOffset").toInt();
    }

    int selectionEndOffset() {
        return evaluateJavaScriptSync(this, "window.getSelection().getRangeAt(0).endOffset").toInt();
    }

    // true if start offset == end offset, i.e. no selected text
    int isSelectionCollapsed() {
        return evaluateJavaScriptSync(this, "window.getSelection().getRangeAt(0).collapsed").toBool();
    }
    bool hasSelection()
    {
        return !selectedText().isEmpty();
    }
};

void tst_QWebEnginePage::cursorMovements()
{
#if !defined(QWEBENGINEPAGE_SELECTEDTEXT)
    QSKIP("QWEBENGINEPAGE_SELECTEDTEXT");
#else
    CursorTrackedPage* page = new CursorTrackedPage;
    QString content("<html><body><p id=one>The quick brown fox</p><p id=two>jumps over the lazy dog</p><p>May the source<br/>be with you!</p></body></html>");
    page->setHtml(content);

    // this will select the first paragraph
    QString script = "var range = document.createRange(); " \
        "var node = document.getElementById(\"one\"); " \
        "range.selectNode(node); " \
        "getSelection().addRange(range);";
    evaluateJavaScriptSync(page, script);
    QCOMPARE(page->selectedText().trimmed(), QString::fromLatin1("The quick brown fox"));

    QRegExp regExp(" style=\".*\"");
    regExp.setMinimal(true);
    QCOMPARE(page->selectedHtml().trimmed().replace(regExp, ""), QString::fromLatin1("<p id=\"one\">The quick brown fox</p>"));

    // these actions must exist
    QVERIFY(page->action(QWebEnginePage::MoveToNextChar) != 0);
    QVERIFY(page->action(QWebEnginePage::MoveToPreviousChar) != 0);
    QVERIFY(page->action(QWebEnginePage::MoveToNextWord) != 0);
    QVERIFY(page->action(QWebEnginePage::MoveToPreviousWord) != 0);
    QVERIFY(page->action(QWebEnginePage::MoveToNextLine) != 0);
    QVERIFY(page->action(QWebEnginePage::MoveToPreviousLine) != 0);
    QVERIFY(page->action(QWebEnginePage::MoveToStartOfLine) != 0);
    QVERIFY(page->action(QWebEnginePage::MoveToEndOfLine) != 0);
    QVERIFY(page->action(QWebEnginePage::MoveToStartOfBlock) != 0);
    QVERIFY(page->action(QWebEnginePage::MoveToEndOfBlock) != 0);
    QVERIFY(page->action(QWebEnginePage::MoveToStartOfDocument) != 0);
    QVERIFY(page->action(QWebEnginePage::MoveToEndOfDocument) != 0);

    // right now they are disabled because contentEditable is false
    QCOMPARE(page->action(QWebEnginePage::MoveToNextChar)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::MoveToPreviousChar)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::MoveToNextWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::MoveToPreviousWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::MoveToNextLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::MoveToPreviousLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::MoveToStartOfLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::MoveToEndOfLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::MoveToStartOfBlock)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::MoveToEndOfBlock)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::MoveToStartOfDocument)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::MoveToEndOfDocument)->isEnabled(), false);

    // make it editable before navigating the cursor
    page->setContentEditable(true);

    // here the actions are enabled after contentEditable is true
    QCOMPARE(page->action(QWebEnginePage::MoveToNextChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::MoveToPreviousChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::MoveToNextWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::MoveToPreviousWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::MoveToNextLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::MoveToPreviousLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::MoveToStartOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::MoveToEndOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::MoveToStartOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::MoveToEndOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::MoveToStartOfDocument)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::MoveToEndOfDocument)->isEnabled(), true);

    // cursor will be before the word "jump"
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // cursor will be between 'j' and 'u' in the word "jump"
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 1);

    // cursor will be between 'u' and 'm' in the word "jump"
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 2);

    // cursor will be after the word "jump"
    page->triggerAction(QWebEnginePage::MoveToNextWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 5);

    // cursor will be after the word "lazy"
    page->triggerAction(QWebEnginePage::MoveToNextWord);
    page->triggerAction(QWebEnginePage::MoveToNextWord);
    page->triggerAction(QWebEnginePage::MoveToNextWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 19);

    // cursor will be between 'z' and 'y' in "lazy"
    page->triggerAction(QWebEnginePage::MoveToPreviousChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 18);

    // cursor will be between 'a' and 'z' in "lazy"
    page->triggerAction(QWebEnginePage::MoveToPreviousChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 17);

    // cursor will be before the word "lazy"
    page->triggerAction(QWebEnginePage::MoveToPreviousWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 15);

    // cursor will be before the word "quick"
    page->triggerAction(QWebEnginePage::MoveToPreviousWord);
    page->triggerAction(QWebEnginePage::MoveToPreviousWord);
    page->triggerAction(QWebEnginePage::MoveToPreviousWord);
    page->triggerAction(QWebEnginePage::MoveToPreviousWord);
    page->triggerAction(QWebEnginePage::MoveToPreviousWord);
    page->triggerAction(QWebEnginePage::MoveToPreviousWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 4);

    // cursor will be between 'p' and 's' in the word "jumps"
    page->triggerAction(QWebEnginePage::MoveToNextWord);
    page->triggerAction(QWebEnginePage::MoveToNextWord);
    page->triggerAction(QWebEnginePage::MoveToNextWord);
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 4);

    // cursor will be before the word "jumps"
    page->triggerAction(QWebEnginePage::MoveToStartOfLine);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // cursor will be after the word "dog"
    page->triggerAction(QWebEnginePage::MoveToEndOfLine);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 23);

    // cursor will be between 'w' and 'n' in "brown"
    page->triggerAction(QWebEnginePage::MoveToStartOfLine);
    page->triggerAction(QWebEnginePage::MoveToPreviousWord);
    page->triggerAction(QWebEnginePage::MoveToPreviousWord);
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 14);

    // cursor will be after the word "fox"
    page->triggerAction(QWebEnginePage::MoveToEndOfLine);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 19);

    // cursor will be before the word "The"
    page->triggerAction(QWebEnginePage::MoveToStartOfDocument);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // cursor will be after the word "you!"
    page->triggerAction(QWebEnginePage::MoveToEndOfDocument);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 12);

    // cursor will be before the word "be"
    page->triggerAction(QWebEnginePage::MoveToStartOfBlock);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // cursor will be after the word "you!"
    page->triggerAction(QWebEnginePage::MoveToEndOfBlock);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 12);

    // try to move before the document start
    page->triggerAction(QWebEnginePage::MoveToStartOfDocument);
    page->triggerAction(QWebEnginePage::MoveToPreviousChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);
    page->triggerAction(QWebEnginePage::MoveToStartOfDocument);
    page->triggerAction(QWebEnginePage::MoveToPreviousWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // try to move past the document end
    page->triggerAction(QWebEnginePage::MoveToEndOfDocument);
    page->triggerAction(QWebEnginePage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 12);
    page->triggerAction(QWebEnginePage::MoveToEndOfDocument);
    page->triggerAction(QWebEnginePage::MoveToNextWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 12);

    delete page;
#endif
}

void tst_QWebEnginePage::textSelection()
{
    QWebEngineView *view = new QWebEngineView;
    CursorTrackedPage *page = new CursorTrackedPage(view);
    QString content("<html><body><p id=one>The quick brown fox</p>" \
        "<p id=two>jumps over the lazy dog</p>" \
        "<p>May the source<br/>be with you!</p></body></html>");
    page->setView(view);
    QSignalSpy loadSpy(view, SIGNAL(loadFinished(bool)));
    page->setHtml(content);
    QTRY_COMPARE(loadSpy.count(), 1);

    // these actions must exist
    QVERIFY(page->action(QWebEnginePage::SelectAll) != 0);
#if defined(QWEBENGINEPAGE_SELECTACTIONS)
    QVERIFY(page->action(QWebEnginePage::SelectNextChar) != 0);
    QVERIFY(page->action(QWebEnginePage::SelectPreviousChar) != 0);
    QVERIFY(page->action(QWebEnginePage::SelectNextWord) != 0);
    QVERIFY(page->action(QWebEnginePage::SelectPreviousWord) != 0);
    QVERIFY(page->action(QWebEnginePage::SelectNextLine) != 0);
    QVERIFY(page->action(QWebEnginePage::SelectPreviousLine) != 0);
    QVERIFY(page->action(QWebEnginePage::SelectStartOfLine) != 0);
    QVERIFY(page->action(QWebEnginePage::SelectEndOfLine) != 0);
    QVERIFY(page->action(QWebEnginePage::SelectStartOfBlock) != 0);
    QVERIFY(page->action(QWebEnginePage::SelectEndOfBlock) != 0);
    QVERIFY(page->action(QWebEnginePage::SelectStartOfDocument) != 0);
    QVERIFY(page->action(QWebEnginePage::SelectEndOfDocument) != 0);

    // right now they are disabled because contentEditable is false and
    // there isn't an existing selection to modify
    QCOMPARE(page->action(QWebEnginePage::SelectNextChar)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SelectPreviousChar)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SelectNextWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SelectPreviousWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SelectNextLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SelectPreviousLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SelectStartOfLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SelectEndOfLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SelectStartOfBlock)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SelectEndOfBlock)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SelectStartOfDocument)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SelectEndOfDocument)->isEnabled(), false);
#endif

    // ..but SelectAll is awalys enabled
    QCOMPARE(page->action(QWebEnginePage::SelectAll)->isEnabled(), true);

    // Verify hasSelection returns false since there is no selection yet...
    QCOMPARE(page->hasSelection(), false);

    // this will select the first paragraph
    QString selectScript = "var range = document.createRange(); " \
        "var node = document.getElementById(\"one\"); " \
        "range.selectNode(node); " \
        "getSelection().addRange(range);";
    evaluateJavaScriptSync(page, selectScript);
    QCOMPARE(page->selectedText().trimmed(), QString::fromLatin1("The quick brown fox"));
#if defined(QWEBENGINEPAGE_SELECTEDHTML)
    QRegExp regExp(" style=\".*\"");
    regExp.setMinimal(true);
    QCOMPARE(page->selectedHtml().trimmed().replace(regExp, ""), QString::fromLatin1("<p id=\"one\">The quick brown fox</p>"));
#endif
    // Make sure hasSelection returns true, since there is selected text now...
    QCOMPARE(page->hasSelection(), true);

#if defined(QWEBENGINEPAGE_SELECTACTIONS)
    // here the actions are enabled after a selection has been created
    QCOMPARE(page->action(QWebEnginePage::SelectNextChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectPreviousChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectNextWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectPreviousWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectNextLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectPreviousLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectStartOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectEndOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectStartOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectEndOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectStartOfDocument)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectEndOfDocument)->isEnabled(), true);

    // make it editable before navigating the cursor
    page->setContentEditable(true);

    // cursor will be before the word "The", this makes sure there is a charet
    page->triggerAction(QWebEnginePage::MoveToStartOfDocument);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // here the actions are enabled after contentEditable is true
    QCOMPARE(page->action(QWebEnginePage::SelectNextChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectPreviousChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectNextWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectPreviousWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectNextLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectPreviousLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectStartOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectEndOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectStartOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectEndOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectStartOfDocument)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SelectEndOfDocument)->isEnabled(), true);
#endif

    delete view;
}

void tst_QWebEnginePage::textEditing()
{
#if !defined(QWEBENGINEPAGE_EVALUATEJAVASCRIPT)
    QSKIP("QWEBENGINEPAGE_EVALUATEJAVASCRIPT");
#else
    CursorTrackedPage* page = new CursorTrackedPage;
    QString content("<html><body><p id=one>The quick brown fox</p>" \
        "<p id=two>jumps over the lazy dog</p>" \
        "<p>May the source<br/>be with you!</p></body></html>");
    page->setHtml(content);

    // these actions must exist
    QVERIFY(page->action(QWebEnginePage::Cut) != 0);
    QVERIFY(page->action(QWebEnginePage::Copy) != 0);
    QVERIFY(page->action(QWebEnginePage::Paste) != 0);
    QVERIFY(page->action(QWebEnginePage::DeleteStartOfWord) != 0);
    QVERIFY(page->action(QWebEnginePage::DeleteEndOfWord) != 0);
    QVERIFY(page->action(QWebEnginePage::SetTextDirectionDefault) != 0);
    QVERIFY(page->action(QWebEnginePage::SetTextDirectionLeftToRight) != 0);
    QVERIFY(page->action(QWebEnginePage::SetTextDirectionRightToLeft) != 0);
    QVERIFY(page->action(QWebEnginePage::ToggleBold) != 0);
    QVERIFY(page->action(QWebEnginePage::ToggleItalic) != 0);
    QVERIFY(page->action(QWebEnginePage::ToggleUnderline) != 0);
    QVERIFY(page->action(QWebEnginePage::InsertParagraphSeparator) != 0);
    QVERIFY(page->action(QWebEnginePage::InsertLineSeparator) != 0);
    QVERIFY(page->action(QWebEnginePage::PasteAndMatchStyle) != 0);
    QVERIFY(page->action(QWebEnginePage::RemoveFormat) != 0);
    QVERIFY(page->action(QWebEnginePage::ToggleStrikethrough) != 0);
    QVERIFY(page->action(QWebEnginePage::ToggleSubscript) != 0);
    QVERIFY(page->action(QWebEnginePage::ToggleSuperscript) != 0);
    QVERIFY(page->action(QWebEnginePage::InsertUnorderedList) != 0);
    QVERIFY(page->action(QWebEnginePage::InsertOrderedList) != 0);
    QVERIFY(page->action(QWebEnginePage::Indent) != 0);
    QVERIFY(page->action(QWebEnginePage::Outdent) != 0);
    QVERIFY(page->action(QWebEnginePage::AlignCenter) != 0);
    QVERIFY(page->action(QWebEnginePage::AlignJustified) != 0);
    QVERIFY(page->action(QWebEnginePage::AlignLeft) != 0);
    QVERIFY(page->action(QWebEnginePage::AlignRight) != 0);

    // right now they are disabled because contentEditable is false
    QCOMPARE(page->action(QWebEnginePage::Cut)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::Paste)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::DeleteStartOfWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::DeleteEndOfWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SetTextDirectionDefault)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SetTextDirectionLeftToRight)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::SetTextDirectionRightToLeft)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::ToggleBold)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::ToggleItalic)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::ToggleUnderline)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::InsertParagraphSeparator)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::InsertLineSeparator)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::PasteAndMatchStyle)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::RemoveFormat)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::ToggleStrikethrough)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::ToggleSubscript)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::ToggleSuperscript)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::InsertUnorderedList)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::InsertOrderedList)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::Indent)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::Outdent)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::AlignCenter)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::AlignJustified)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::AlignLeft)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::AlignRight)->isEnabled(), false);

    // Select everything
    page->triggerAction(QWebEnginePage::SelectAll);

    // make sure it is enabled since there is a selection
    QCOMPARE(page->action(QWebEnginePage::Copy)->isEnabled(), true);

    // make it editable before navigating the cursor
    page->setContentEditable(true);

    // clear the selection
    page->triggerAction(QWebEnginePage::MoveToStartOfDocument);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // make sure it is disabled since there isn't a selection
    QCOMPARE(page->action(QWebEnginePage::Copy)->isEnabled(), false);

    // here the actions are enabled after contentEditable is true
    QCOMPARE(page->action(QWebEnginePage::Paste)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::DeleteStartOfWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::DeleteEndOfWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SetTextDirectionDefault)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SetTextDirectionLeftToRight)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::SetTextDirectionRightToLeft)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::ToggleBold)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::ToggleItalic)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::ToggleUnderline)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::InsertParagraphSeparator)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::InsertLineSeparator)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::PasteAndMatchStyle)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::ToggleStrikethrough)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::ToggleSubscript)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::ToggleSuperscript)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::InsertUnorderedList)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::InsertOrderedList)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::Indent)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::Outdent)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::AlignCenter)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::AlignJustified)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::AlignLeft)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::AlignRight)->isEnabled(), true);

    // make sure these are disabled since there isn't a selection
    QCOMPARE(page->action(QWebEnginePage::Cut)->isEnabled(), false);
    QCOMPARE(page->action(QWebEnginePage::RemoveFormat)->isEnabled(), false);

    // make sure everything is selected
    page->triggerAction(QWebEnginePage::SelectAll);

    // this is only true if there is an editable selection
    QCOMPARE(page->action(QWebEnginePage::Cut)->isEnabled(), true);
    QCOMPARE(page->action(QWebEnginePage::RemoveFormat)->isEnabled(), true);

    delete page;
#endif
}

void tst_QWebEnginePage::backActionUpdate()
{
    QWebEngineView view;
    QWebEnginePage *page = view.page();
    QAction *action = page->action(QWebEnginePage::Back);
    QVERIFY(!action->isEnabled());
    QSignalSpy loadSpy(page, SIGNAL(loadFinished(bool)));
    QUrl url = QUrl("qrc:///resources/framedindex.html");
    page->load(url);
    QTRY_COMPARE(loadSpy.count(), 1);
    QVERIFY(!action->isEnabled());
    QTest::mouseClick(&view, Qt::LeftButton, 0, QPoint(10, 10));
    QEXPECT_FAIL("", "Behavior change: Load signals are emitted only for the main frame in QtWebEngine.", Continue);
    QTRY_COMPARE_WITH_TIMEOUT(loadSpy.count(), 2, 100);

    QEXPECT_FAIL("", "FIXME: Mouse events aren't passed from the QWebEngineView down to the RWHVQtDelegateWidget", Continue);
    QVERIFY(action->isEnabled());
}

void tst_QWebEnginePage::inputMethods_data()
{
    QTest::addColumn<QString>("viewType");
    QTest::newRow("QWebEngineView") << "QWebEngineView";
    QTest::newRow("QGraphicsWebView") << "QGraphicsWebView";
}

#if defined(QWEBENGINEPAGE_INPUTMETHODQUERY)
static Qt::InputMethodHints inputMethodHints(QObject* object)
{
    if (QGraphicsObject* o = qobject_cast<QGraphicsObject*>(object))
        return o->inputMethodHints();
    if (QWidget* w = qobject_cast<QWidget*>(object))
        return w->inputMethodHints();
    return Qt::InputMethodHints();
}

static bool inputMethodEnabled(QObject* object)
{
    if (QGraphicsObject* o = qobject_cast<QGraphicsObject*>(object))
        return o->flags() & QGraphicsItem::ItemAcceptsInputMethod;
    if (QWidget* w = qobject_cast<QWidget*>(object))
        return w->testAttribute(Qt::WA_InputMethodEnabled);
    return false;
}

static void clickOnPage(QWebEnginePage* page, const QPoint& position)
{
    QMouseEvent evpres(QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    page->event(&evpres);
    QMouseEvent evrel(QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    page->event(&evrel);
}
#endif

void tst_QWebEnginePage::inputMethods()
{
#if !defined(QWEBENGINEPAGE_INPUTMETHODQUERY)
    QSKIP("QWEBENGINEPAGE_INPUTMETHODQUERY");
#else
    QFETCH(QString, viewType);
    QWebEnginePage* page = new QWebEnginePage;
    QObject* view = 0;
    QObject* container = 0;
    if (viewType == "QWebEngineView") {
        QWebEngineView* wv = new QWebEngineView;
        wv->setPage(page);
        view = wv;
        container = view;
    } else if (viewType == "QGraphicsWebView") {
        QGraphicsWebView* wv = new QGraphicsWebView;
        wv->setPage(page);
        view = wv;

        QGraphicsView* gv = new QGraphicsView;
        QGraphicsScene* scene = new QGraphicsScene(gv);
        gv->setScene(scene);
        scene->addItem(wv);
        wv->setGeometry(QRect(0, 0, 500, 500));

        container = gv;
    } else
        QVERIFY2(false, "Unknown view type");

    page->settings()->setFontFamily(QWebEngineSettings::SerifFont, page->settings()->fontFamily(QWebEngineSettings::FixedFont));
    page->setHtml("<html><body>" \
                                            "<input type='text' id='input1' style='font-family: serif' value='' maxlength='20'/><br>" \
                                            "<input type='password'/>" \
                                            "</body></html>");
    page->mainFrame()->setFocus();

    TestInputContext testContext;

    QWebEngineElementCollection inputs = page->mainFrame()->documentElement().findAll("input");
    QPoint textInputCenter = inputs.at(0).geometry().center();

    clickOnPage(page, textInputCenter);

    // This part of the test checks if the SIP (Software Input Panel) is triggered,
    // which normally happens on mobile platforms, when a user input form receives
    // a mouse click.
    int  inputPanel = 0;
    if (viewType == "QWebEngineView") {
        if (QWebEngineView* wv = qobject_cast<QWebEngineView*>(view))
            inputPanel = wv->style()->styleHint(QStyle::SH_RequestSoftwareInputPanel);
    } else if (viewType == "QGraphicsWebView") {
        if (QGraphicsWebView* wv = qobject_cast<QGraphicsWebView*>(view))
            inputPanel = wv->style()->styleHint(QStyle::SH_RequestSoftwareInputPanel);
    }

    // For non-mobile platforms RequestSoftwareInputPanel event is not called
    // because there is no SIP (Software Input Panel) triggered. In the case of a
    // mobile platform, an input panel, e.g. virtual keyboard, is usually invoked
    // and the RequestSoftwareInputPanel event is called. For these two situations
    // this part of the test can verified as the checks below.
    if (inputPanel)
        QVERIFY(testContext.isInputPanelVisible());
    else
        QVERIFY(!testContext.isInputPanelVisible());
    testContext.hideInputPanel();

    clickOnPage(page, textInputCenter);
    QVERIFY(testContext.isInputPanelVisible());

    //ImMicroFocus
    QVariant variant = page->inputMethodQuery(Qt::ImMicroFocus);
    QVERIFY(inputs.at(0).geometry().contains(variant.toRect().topLeft()));

    // We assigned the serif font famility to be the same as the fixef font family.
    // Then test ImFont on a serif styled element, we should get our fixef font family.
    variant = page->inputMethodQuery(Qt::ImFont);
    QFont font = variant.value<QFont>();
    QCOMPARE(page->settings()->fontFamily(QWebEngineSettings::FixedFont), font.family());

    QList<QInputMethodEvent::Attribute> inputAttributes;

    //Insert text.
    {
        QInputMethodEvent eventText("QtWebEngine", inputAttributes);
        QSignalSpy signalSpy(page, SIGNAL(microFocusChanged()));
        page->event(&eventText);
        QCOMPARE(signalSpy.count(), 0);
    }

    {
        QInputMethodEvent eventText("", inputAttributes);
        eventText.setCommitString(QString("QtWebEngine"), 0, 0);
        page->event(&eventText);
    }

    //ImMaximumTextLength
    variant = page->inputMethodQuery(Qt::ImMaximumTextLength);
    QCOMPARE(20, variant.toInt());

    //Set selection
    inputAttributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 3, 2, QVariant());
    QInputMethodEvent eventSelection("",inputAttributes);
    page->event(&eventSelection);

    //ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    int anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 3);

    //ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    int cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 5);

    //ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    QString selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString("eb"));

    //Set selection with negative length
    inputAttributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 6, -5, QVariant());
    QInputMethodEvent eventSelection3("",inputAttributes);
    page->event(&eventSelection3);

    //ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 1);

    //ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 6);

    //ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString("tWebK"));

    //ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    QString value = variant.value<QString>();
    QCOMPARE(value, QString("QtWebEngine"));

    {
        QList<QInputMethodEvent::Attribute> attributes;
        // Clear the selection, so the next test does not clear any contents.
        QInputMethodEvent::Attribute newSelection(QInputMethodEvent::Selection, 0, 0, QVariant());
        attributes.append(newSelection);
        QInputMethodEvent event("composition", attributes);
        page->event(&event);
    }

    // A ongoing composition should not change the surrounding text before it is committed.
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    value = variant.value<QString>();
    QCOMPARE(value, QString("QtWebEngine"));

    // Cancel current composition first
    inputAttributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 0, 0, QVariant());
    QInputMethodEvent eventSelection4("", inputAttributes);
    page->event(&eventSelection4);

    // START - Tests for Selection when the Editor is NOT in Composition mode

    // LEFT to RIGHT selection
    // Deselect the selection by sending MouseButtonPress events
    // This moves the current cursor to the end of the text
    clickOnPage(page, textInputCenter);

    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event(QString(), attributes);
        event.setCommitString("XXX", 0, 0);
        page->event(&event);
        event.setCommitString(QString(), -2, 2); // Erase two characters.
        page->event(&event);
        event.setCommitString(QString(), -1, 1); // Erase one character.
        page->event(&event);
        variant = page->inputMethodQuery(Qt::ImSurroundingText);
        value = variant.value<QString>();
        QCOMPARE(value, QString("QtWebEngine"));
    }

    //Move to the start of the line
    page->triggerAction(QWebEnginePage::MoveToStartOfLine);

    QKeyEvent keyRightEventPress(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    QKeyEvent keyRightEventRelease(QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier);

    //Move 2 characters RIGHT
    for (int j = 0; j < 2; ++j) {
        page->event(&keyRightEventPress);
        page->event(&keyRightEventRelease);
    }

    //Select to the end of the line
    page->triggerAction(QWebEnginePage::SelectEndOfLine);

    //ImAnchorPosition QtWebEngine
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 2);

    //ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 8);

    //ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString("WebKit"));

    //RIGHT to LEFT selection
    //Deselect the selection (this moves the current cursor to the end of the text)
    clickOnPage(page, textInputCenter);

    //ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 8);

    //ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 8);

    //ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    QKeyEvent keyLeftEventPress(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
    QKeyEvent keyLeftEventRelease(QEvent::KeyRelease, Qt::Key_Left, Qt::NoModifier);

    //Move 2 characters LEFT
    for (int i = 0; i < 2; ++i) {
        page->event(&keyLeftEventPress);
        page->event(&keyLeftEventRelease);
    }

    //Select to the start of the line
    page->triggerAction(QWebEnginePage::SelectStartOfLine);

    //ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 6);

    //ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 0);

    //ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString("QtWebK"));

    //END - Tests for Selection when the Editor is not in Composition mode

    //ImhHiddenText
    QPoint passwordInputCenter = inputs.at(1).geometry().center();
    clickOnPage(page, passwordInputCenter);

    QVERIFY(inputMethodEnabled(view));
    QVERIFY(inputMethodHints(view) & Qt::ImhHiddenText);

    clickOnPage(page, textInputCenter);
    QVERIFY(!(inputMethodHints(view) & Qt::ImhHiddenText));

    page->setHtml("<html><body><p>nothing to input here");
    testContext.hideInputPanel();

    QWebEngineElement para = page->mainFrame()->findFirstElement("p");
    clickOnPage(page, para.geometry().center());

    QVERIFY(!testContext.isInputPanelVisible());

    //START - Test for sending empty QInputMethodEvent
    page->setHtml("<html><body>" \
                                            "<input type='text' id='input3' value='QtWebEngine2'/>" \
                                            "</body></html>");
    evaluateJavaScriptSync(page, "var inputEle = document.getElementById('input3'); inputEle.focus(); inputEle.select();");

    //Send empty QInputMethodEvent
    QInputMethodEvent emptyEvent;
    page->event(&emptyEvent);

    QString inputValue = evaluateJavaScriptSync(page, "document.getElementById('input3').value").toString();
    QCOMPARE(inputValue, QString("QtWebEngine2"));
    //END - Test for sending empty QInputMethodEvent

    page->setHtml("<html><body>" \
                                            "<input type='text' id='input4' value='QtWebEngine inputMethod'/>" \
                                            "</body></html>");
    evaluateJavaScriptSync(page, "var inputEle = document.getElementById('input4'); inputEle.focus(); inputEle.select();");

    // Clear the selection, also cancel the ongoing composition if there is one.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent::Attribute newSelection(QInputMethodEvent::Selection, 0, 0, QVariant());
        attributes.append(newSelection);
        QInputMethodEvent event("", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    QString surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("QtWebEngine inputMethod"));

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 0);

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 0);

    // 1. Insert a character to the beginning of the line.
    // Send temporary text, which makes the editor has composition 'm'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("m", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("QtWebEngine inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 0);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 0);

    // Send temporary text, which makes the editor has composition 'n'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("n", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("QtWebEngine inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 0);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 0);

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("o");
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oQtWebEngine inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 1);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 1);

    // 2. insert a character to the middle of the line.
    // Send temporary text, which makes the editor has composition 'd'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("d", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oQtWebEngine inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 1);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 1);

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("e");
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebEngine inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 2);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 2);

    // 3. Insert a character to the end of the line.
    page->triggerAction(QWebEnginePage::MoveToEndOfLine);

    // Send temporary text, which makes the editor has composition 't'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("t", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebEngine inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 22);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 22);

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("t");
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebEngine inputMethodt"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 23);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 23);

    // 4. Replace the selection.
    page->triggerAction(QWebEnginePage::SelectPreviousWord);

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString("inputMethodt"));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebEngine inputMethodt"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 11);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 23);

    // Send temporary text, which makes the editor has composition 'w'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("w", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebEngine "));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 11);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 11);

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("2");
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebEngine 2"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 12);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 12);

    // Check sending RequestSoftwareInputPanel event
    page->setHtml("<html><body>" \
                                            "<input type='text' id='input5' value='QtWebEngine inputMethod'/>" \
                                            "<div id='btnDiv' onclick='i=document.getElementById(&quot;input5&quot;); i.focus();'>abc</div>"\
                                            "</body></html>");
    QWebEngineElement inputElement = page->mainFrame()->findFirstElement("div");
    clickOnPage(page, inputElement.geometry().center());

    QVERIFY(!testContext.isInputPanelVisible());

    // START - Newline test for textarea
    qApp->processEvents();
    page->setHtml("<html><body>" \
                                            "<textarea rows='5' cols='1' id='input5' value=''/>" \
                                            "</body></html>");
    evaluateJavaScriptSync(page, "var inputEle = document.getElementById('input5'); inputEle.focus(); inputEle.select();");

    // Enter Key without key text
    QKeyEvent keyEnter(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
    page->event(&keyEnter);
    QList<QInputMethodEvent::Attribute> attribs;

    QInputMethodEvent eventText(QString(), attribs);
    eventText.setCommitString("\n");
    page->event(&eventText);

    QInputMethodEvent eventText2(QString(), attribs);
    eventText2.setCommitString("third line");
    page->event(&eventText2);
    qApp->processEvents();

    QString inputValue2 = evaluateJavaScriptSync(page, "document.getElementById('input5').value").toString();
    QCOMPARE(inputValue2, QString("\n\nthird line"));

    // Enter Key with key text '\r'
    evaluateJavaScriptSync(page, "var inputEle = document.getElementById('input5'); inputEle.value = ''; inputEle.focus(); inputEle.select();");
    inputValue2 = evaluateJavaScriptSync(page, "document.getElementById('input5').value").toString();
    QCOMPARE(inputValue2, QString(""));

    QKeyEvent keyEnterWithCarriageReturn(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier, "\r");
    page->event(&keyEnterWithCarriageReturn);
    page->event(&eventText);
    page->event(&eventText2);
    qApp->processEvents();

    inputValue2 = evaluateJavaScriptSync(page, "document.getElementById('input5').value").toString();
    QCOMPARE(inputValue2, QString("\n\nthird line"));

    // Enter Key with key text '\n'
    page->runJavaScript("var inputEle = document.getElementById('input5'); inputEle.value = ''; inputEle.focus(); inputEle.select();");
    inputValue2 = evaluateJavaScriptSync(page, "document.getElementById('input5').value").toString();
    QCOMPARE(inputValue2, QString(""));

    QKeyEvent keyEnterWithLineFeed(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier, "\n");
    page->event(&keyEnterWithLineFeed);
    page->event(&eventText);
    page->event(&eventText2);
    qApp->processEvents();

    inputValue2 = evaluateJavaScriptSync(page, "document.getElementById('input5').value").toString();
    QCOMPARE(inputValue2, QString("\n\nthird line"));

    // Enter Key with key text "\n\r"
    page->runJavaScript("var inputEle = document.getElementById('input5'); inputEle.value = ''; inputEle.focus(); inputEle.select();");
    inputValue2 = evaluateJavaScriptSync(page, "document.getElementById('input5').value").toString();
    QCOMPARE(inputValue2, QString(""));

    QKeyEvent keyEnterWithLFCR(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier, "\n\r");
    page->event(&keyEnterWithLFCR);
    page->event(&eventText);
    page->event(&eventText2);
    qApp->processEvents();

    inputValue2 = evaluateJavaScriptSync(page, "document.getElementById('input5').value").toString();
    QCOMPARE(inputValue2, QString("\n\nthird line"));

    // Return Key without key text
    page->runJavaScript("var inputEle = document.getElementById('input5'); inputEle.value = ''; inputEle.focus(); inputEle.select();");
    inputValue2 = evaluateJavaScriptSync(page, "document.getElementById('input5').value").toString();
    QCOMPARE(inputValue2, QString(""));

    QKeyEvent keyReturn(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    page->event(&keyReturn);
    page->event(&eventText);
    page->event(&eventText2);
    qApp->processEvents();

    inputValue2 = evaluateJavaScriptSync(page, "document.getElementById('input5').value").toString();
    QCOMPARE(inputValue2, QString("\n\nthird line"));

    // END - Newline test for textarea

    delete container;
#endif
}

void tst_QWebEnginePage::protectBindingsRuntimeObjectsFromCollector()
{
#if !defined(QWEBENGINEPAGE_CREATEPLUGIN)
    QSKIP("QWEBENGINEPAGE_CREATEPLUGIN");
#else
    QSignalSpy loadSpy(m_view, SIGNAL(loadFinished(bool)));

    PluginPage* newPage = new PluginPage(m_view);
    m_view->setPage(newPage);

    m_view->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);

    m_view->setHtml(QString("<html><body><object type='application/x-qt-plugin' classid='lineedit' id='mylineedit'/></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 1);

    newPage->runJavaScript("function testme(text) { var lineedit = document.getElementById('mylineedit'); lineedit.setText(text); lineedit.selectAll(); }");

    evaluateJavaScriptSync(newPage, "testme('foo')");

    DumpRenderTreeSupportQt::garbageCollectorCollect();

    // don't crash!
    evaluateJavaScriptSync(newPage, "testme('bar')");
#endif
}

#if defined(QWEBENGINEPAGE_SETTINGS)
static inline bool testFlag(QWebEnginePage& webPage, QWebEngineSettings::WebAttribute settingAttribute, const QString& jsObjectName, bool settingValue)
{
    webPage.settings()->setAttribute(settingAttribute, settingValue);
    return evaluateJavaScriptSync(&webPage, QString("(window.%1 != undefined)").arg(jsObjectName)).toBool();
}
#endif

void tst_QWebEnginePage::testOptionalJSObjects()
{
#if !defined(QWEBENGINESETTINGS)
    QSKIP("QWEBENGINSETTINGS");
#else
    // Once a feature is enabled and the JS object is accessed turning off the setting will not turn off
    // the visibility of the JS object any more. For this reason this test uses two QWebEnginePage instances.
    // Part of the test is to make sure that the QWebEnginePage instances do not interfere with each other so turning on
    // a feature for one instance will not turn it on for another.

    QWebEnginePage webPage1;
    QWebEnginePage webPage2;

    webPage1.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("http://www.example.com/"));
    webPage2.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("http://www.example.com/"));

    QEXPECT_FAIL("","Feature enabled/disabled checking problem. Look at bugs.webkit.org/show_bug.cgi?id=29867", Continue);
    QCOMPARE(testFlag(webPage1, QWebEngineSettings::OfflineWebApplicationCacheEnabled, "applicationCache", false), false);
    QCOMPARE(testFlag(webPage2, QWebEngineSettings::OfflineWebApplicationCacheEnabled, "applicationCache", true),  true);
    QEXPECT_FAIL("","Feature enabled/disabled checking problem. Look at bugs.webkit.org/show_bug.cgi?id=29867", Continue);
    QCOMPARE(testFlag(webPage1, QWebEngineSettings::OfflineWebApplicationCacheEnabled, "applicationCache", false), false);
    QCOMPARE(testFlag(webPage2, QWebEngineSettings::OfflineWebApplicationCacheEnabled, "applicationCache", false), true);

    QCOMPARE(testFlag(webPage1, QWebEngineSettings::LocalStorageEnabled, "localStorage", false), false);
    QCOMPARE(testFlag(webPage2, QWebEngineSettings::LocalStorageEnabled, "localStorage", true),  true);
    QCOMPARE(testFlag(webPage1, QWebEngineSettings::LocalStorageEnabled, "localStorage", false), false);
    QCOMPARE(testFlag(webPage2, QWebEngineSettings::LocalStorageEnabled, "localStorage", false), true);
#endif
}

#if defined(QWEBENGINEPAGE_SETTINGS)
static inline bool checkLocalStorageVisibility(QWebEnginePage& webPage, bool localStorageEnabled)
{
    webPage.settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, localStorageEnabled);
    return evaluateJavaScriptSync(&webPage, QString("(window.localStorage != undefined)")).toBool();
}
#endif

void tst_QWebEnginePage::testLocalStorageVisibility()
{
#if !defined(QWEBENGINEPAGE_SETTINGS)
    QSKIP("QWEBENGINEPAGE_SETTINGS");
#else
    // Local storage's visibility depends on its security origin, which depends on base url.
    // Initially, it will test it with base urls that get a globally unique origin, which may not
    // be able to use local storage even if the feature is enabled. Then later the same test is
    // done but with urls that would get a valid origin, so local storage could be used.
    // Before every test case it checks if local storage is not already visible.

    QWebEnginePage webPage;

    webPage.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl());

    QCOMPARE(checkLocalStorageVisibility(webPage, false), false);
    QCOMPARE(checkLocalStorageVisibility(webPage, true), false);

    webPage.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("invalid"));

    QCOMPARE(checkLocalStorageVisibility(webPage, false), false);
    QCOMPARE(checkLocalStorageVisibility(webPage, true), false);

    webPage.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("://misparsed.com"));

    QCOMPARE(checkLocalStorageVisibility(webPage, false), false);
    QCOMPARE(checkLocalStorageVisibility(webPage, true), false);

    webPage.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("http://"));

    QCOMPARE(checkLocalStorageVisibility(webPage, false), false);
    QCOMPARE(checkLocalStorageVisibility(webPage, true), false);

    webPage.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("about:blank"));

    QCOMPARE(checkLocalStorageVisibility(webPage, false), false);
    QCOMPARE(checkLocalStorageVisibility(webPage, true), false);

    webPage.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("data:text/html,test"));

    QCOMPARE(checkLocalStorageVisibility(webPage, false), false);
    QCOMPARE(checkLocalStorageVisibility(webPage, true), false);

    webPage.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("file:///"));

    QCOMPARE(checkLocalStorageVisibility(webPage, false), false);
    QCOMPARE(checkLocalStorageVisibility(webPage, true), true);

    webPage.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("http://www.example.com"));

    QCOMPARE(checkLocalStorageVisibility(webPage, false), false);
    QCOMPARE(checkLocalStorageVisibility(webPage, true), true);

    webPage.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("https://www.example.com"));

    QCOMPARE(checkLocalStorageVisibility(webPage, false), false);
    QCOMPARE(checkLocalStorageVisibility(webPage, true), true);

    webPage.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("ftp://files.example.com"));

    QCOMPARE(checkLocalStorageVisibility(webPage, false), false);
    QCOMPARE(checkLocalStorageVisibility(webPage, true), true);

    webPage.currentFrame()->setHtml(QString("<html><body>test</body></html>"), QUrl("file:///path/to/index.html"));

    QCOMPARE(checkLocalStorageVisibility(webPage, false), false);
    QCOMPARE(checkLocalStorageVisibility(webPage, true), true);
#endif
}

void tst_QWebEnginePage::testEnablePersistentStorage()
{
#if !defined(QWEBENGINESETTINGS)
    QSKIP("QWEBENGINESETTINGS");
#else
    QWebEnginePage webPage;

    // By default all persistent options should be disabled
    QCOMPARE(webPage.settings()->testAttribute(QWebEngineSettings::LocalStorageEnabled), false);
    QCOMPARE(webPage.settings()->testAttribute(QWebEngineSettings::OfflineStorageDatabaseEnabled), false);
    QCOMPARE(webPage.settings()->testAttribute(QWebEngineSettings::OfflineWebApplicationCacheEnabled), false);
    QVERIFY(webPage.settings()->iconDatabasePath().isEmpty());

    QWebEngineSettings::enablePersistentStorage();


    QTRY_COMPARE(webPage.settings()->testAttribute(QWebEngineSettings::LocalStorageEnabled), true);
    QTRY_COMPARE(webPage.settings()->testAttribute(QWebEngineSettings::OfflineStorageDatabaseEnabled), true);
    QTRY_COMPARE(webPage.settings()->testAttribute(QWebEngineSettings::OfflineWebApplicationCacheEnabled), true);

    QTRY_VERIFY(!webPage.settings()->offlineStoragePath().isEmpty());
    QTRY_VERIFY(!webPage.settings()->offlineWebApplicationCachePath().isEmpty());
    QTRY_VERIFY(!webPage.settings()->iconDatabasePath().isEmpty());
#endif
}


#if defined(QWEBENGINEPAGE_ERRORPAGEEXTENSION)
class ErrorPage : public QWebEnginePage
{
public:

    ErrorPage(QWidget* parent = 0): QWebEnginePage(parent)
    {
    }

    virtual bool supportsExtension(Extension extension) const
    {
        return extension == ErrorPageExtension;
    }

    virtual bool extension(Extension, const ExtensionOption* option, ExtensionReturn* output)
    {
        ErrorPageExtensionReturn* errorPage = static_cast<ErrorPageExtensionReturn*>(output);

        errorPage->contentType = "text/html";
        errorPage->content = "error";
        return true;
    }
};
#endif

void tst_QWebEnginePage::errorPageExtension()
{
#if !defined(QWEBENGINEPAGE_ERRORPAGEEXTENSION)
    QSKIP("QWEBENGINEPAGE_ERRORPAGEEXTENSION");
#else
    ErrorPage page;
    m_view->setPage(&page);

    QSignalSpy spyLoadFinished(m_view, SIGNAL(loadFinished(bool)));

    m_view->setUrl(QUrl("data:text/html,foo"));
    QTRY_COMPARE(spyLoadFinished.count(), 1);

    page.setUrl(QUrl("http://non.existent/url"));
    QTRY_COMPARE(spyLoadFinished.count(), 2);
    QCOMPARE(toPlainTextSync(&page), QString("error"));
    QCOMPARE(page.history()->count(), 2);
    QCOMPARE(page.history()->currentItem().url(), QUrl("http://non.existent/url"));
    QCOMPARE(page.history()->canGoBack(), true);
    QCOMPARE(page.history()->canGoForward(), false);

    page.triggerAction(QWebEnginePage::Back);
    QTRY_COMPARE(page.history()->canGoBack(), false);
    QTRY_COMPARE(page.history()->canGoForward(), true);

    page.triggerAction(QWebEnginePage::Forward);
    QTRY_COMPARE(page.history()->canGoBack(), true);
    QTRY_COMPARE(page.history()->canGoForward(), false);

    page.triggerAction(QWebEnginePage::Back);
    QTRY_COMPARE(page.history()->canGoBack(), false);
    QTRY_COMPARE(page.history()->canGoForward(), true);
    QTRY_COMPARE(page.history()->currentItem().url(), QUrl("data:text/html,foo"));

    m_view->setPage(0);
#endif
}

void tst_QWebEnginePage::errorPageExtensionLoadFinished()
{
#if !defined(QWEBENGINEPAGE_ERRORPAGEEXTENSION)
    QSKIP("QWEBENGINEPAGE_ERRORPAGEEXTENSION");
#else
    ErrorPage page;
    m_view->setPage(&page);

    QSignalSpy spyLoadFinished(m_view, SIGNAL(loadFinished(bool)));
    QSignalSpy spyFrameLoadFinished(m_view->page(), SIGNAL(loadFinished(bool)));

    m_view->setUrl(QUrl("data:text/html,foo"));
    QTRY_COMPARE(spyLoadFinished.count(), 1);
    QTRY_COMPARE(spyFrameLoadFinished.count(), 1);

    const bool loadSucceded = spyLoadFinished.at(0).at(0).toBool();
    QVERIFY(loadSucceded);
    const bool frameLoadSucceded = spyFrameLoadFinished.at(0).at(0).toBool();
    QVERIFY(frameLoadSucceded);

    m_view->page()->setUrl(QUrl("http://non.existent/url"));
    QTRY_COMPARE(spyLoadFinished.count(), 2);
    QTRY_COMPARE(spyFrameLoadFinished.count(), 2);

    const bool nonExistantLoadSucceded = spyLoadFinished.at(1).at(0).toBool();
    QVERIFY(nonExistantLoadSucceded);
    const bool nonExistantFrameLoadSucceded = spyFrameLoadFinished.at(1).at(0).toBool();
    QVERIFY(nonExistantFrameLoadSucceded);

    m_view->setPage(0);
#endif
}

void tst_QWebEnginePage::userAgentNewlineStripping()
{
    QWebEngineProfile profile;
    QWebEnginePage page(&profile);

    profile.setHttpUserAgent(QStringLiteral("My User Agent\nX-New-Http-Header: Oh Noes!"));
    // The user agent will be updated after a page load.
    page.load(QUrl("about:blank"));

    QCOMPARE(evaluateJavaScriptSync(&page, "navigator.userAgent").toString(), QStringLiteral("My User Agent X-New-Http-Header: Oh Noes!"));
}

void tst_QWebEnginePage::crashTests_LazyInitializationOfMainFrame()
{
    {
        QWebEnginePage webPage;
    }
    {
        QWebEnginePage webPage;
        webPage.selectedText();
    }
    {
#if defined(QWEBENGINEPAGE_SELECTEDHTML)
        QWebEnginePage webPage;
        webPage.selectedHtml();
#endif
    }
    {
        QWebEnginePage webPage;
        webPage.triggerAction(QWebEnginePage::Back, true);
    }
    {
#if defined(QWEBENGINEPAGE_UPDATEPOSITIONDEPENDENTACTIONS)
        QWebEnginePage webPage;
        QPoint pos(10,10);
        webPage.updatePositionDependentActions(pos);
#endif
    }
}

#if defined(QWEBENGINEPAGE_RENDER)
static void takeScreenshot(QWebEnginePage* page)
{
    page->setViewportSize(page->contentsSize());
    QImage image(page->viewportSize(), QImage::Format_ARGB32);
    QPainter painter(&image);
    page->render(&painter);
    painter.end();
}
#endif

void tst_QWebEnginePage::screenshot_data()
{
    QTest::addColumn<QString>("html");
    QTest::newRow("WithoutPlugin") << "<html><body id='b'>text</body></html>";
    QTest::newRow("WindowedPlugin") << QString("<html><body id='b'>text<embed src='resources/test.swf'></embed></body></html>");
    QTest::newRow("WindowlessPlugin") << QString("<html><body id='b'>text<embed src='resources/test.swf' wmode='transparent'></embed></body></html>");
}

void tst_QWebEnginePage::screenshot()
{
#if !defined(QWEBENGINESETTINGS)
    QSKIP("QWEBENGINESETTINGS");
#else
    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QDir::setCurrent(TESTS_SOURCE_DIR);

    QFETCH(QString, html);
    QWebEnginePage* page = new QWebEnginePage;
    page->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    page->setHtml(html, QUrl::fromLocalFile(TESTS_SOURCE_DIR));
    ::waitForSignal(page, SIGNAL(loadFinished(bool)), 2000);

    // take screenshot without a view
    takeScreenshot(page);

    QWebEngineView* view = new QWebEngineView;
    view->setPage(page);

    // take screenshot when attached to a view
    takeScreenshot(page);

    delete page;
    delete view;

    QDir::setCurrent(QApplication::applicationDirPath());
#endif
}

#if defined(ENABLE_WEBGL) && ENABLE_WEBGL
// https://bugs.webkit.org/show_bug.cgi?id=54138
static void webGLScreenshotWithoutView(bool accelerated)
{
    QWebEnginePage page;
    page.settings()->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    page.settings()->setAttribute(QWebEngineSettings::AcceleratedCompositingEnabled, accelerated);
    page.setHtml("<html><body>"
                       "<canvas id='webgl' width='300' height='300'></canvas>"
                       "<script>document.getElementById('webgl').getContext('experimental-webgl')</script>"
                       "</body></html>");

    takeScreenshot(&page);
}

void tst_QWebEnginePage::acceleratedWebGLScreenshotWithoutView()
{
    webGLScreenshotWithoutView(true);
}

void tst_QWebEnginePage::unacceleratedWebGLScreenshotWithoutView()
{
    webGLScreenshotWithoutView(false);
}
#endif

/**
 * Test fixups for https://bugs.webkit.org/show_bug.cgi?id=30914
 *
 * From JS we test the following conditions.
 *
 *   OK     + QString() => SUCCESS, empty string (but not null)
 *   OK     + "text"    => SUCCESS, "text"
 *   CANCEL + QString() => CANCEL, null string
 *   CANCEL + "text"    => CANCEL, null string
 */
class JSPromptPage : public QWebEnginePage {
    Q_OBJECT
public:
    JSPromptPage()
    {}

    bool javaScriptPrompt(const QUrl &securityOrigin, const QString& msg, const QString& defaultValue, QString* result)
    {
        if (msg == QLatin1String("test1")) {
            *result = QString();
            return true;
        } else if (msg == QLatin1String("test2")) {
            *result = QLatin1String("text");
            return true;
        } else if (msg == QLatin1String("test3")) {
            *result = QString();
            return false;
        } else if (msg == QLatin1String("test4")) {
            *result = QLatin1String("text");
            return false;
        }

        qFatal("Unknown msg.");
        return QWebEnginePage::javaScriptPrompt(securityOrigin, msg, defaultValue, result);
    }
};

void tst_QWebEnginePage::testJSPrompt()
{
    JSPromptPage page;
    bool res;
    QSignalSpy loadSpy(&page, SIGNAL(loadFinished(bool)));
    page.setHtml(QStringLiteral("<html><body></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 1);

    // OK + QString()
    res = evaluateJavaScriptSync(&page,
            "var retval = prompt('test1');"
            "retval=='' && retval.length == 0;").toBool();
    QVERIFY(res);

    // OK + "text"
    res = evaluateJavaScriptSync(&page,
            "var retval = prompt('test2');"
            "retval=='text' && retval.length == 4;").toBool();
    QVERIFY(res);

    // Cancel + QString()
    res = evaluateJavaScriptSync(&page,
            "var retval = prompt('test3');"
            "retval===null;").toBool();
    QVERIFY(res);

    // Cancel + "text"
    res = evaluateJavaScriptSync(&page,
            "var retval = prompt('test4');"
            "retval===null;").toBool();
    QVERIFY(res);
}

void tst_QWebEnginePage::testStopScheduledPageRefresh()
{
#if !defined(QWEBENGINEPAGE_SETNETWORKACCESSMANAGER)
    QSKIP("QWEBENGINEPAGE_SETNETWORKACCESSMANAGER");
#else
    // Without QWebEnginePage::StopScheduledPageRefresh
    QWebEnginePage page1;
    page1.setNetworkAccessManager(new TestNetworkManager(&page1));
    page1.setHtml("<html><head>"
                                "<meta http-equiv=\"refresh\"content=\"0;URL=qrc:///resources/index.html\">"
                                "</head><body><h1>Page redirects immediately...</h1>"
                                "</body></html>");
    QVERIFY(::waitForSignal(&page1, SIGNAL(loadFinished(bool))));
    QTest::qWait(500);
    QCOMPARE(page1.url(), QUrl(QLatin1String("qrc:///resources/index.html")));

    // With QWebEnginePage::StopScheduledPageRefresh
    QWebEnginePage page2;
    page2.setNetworkAccessManager(new TestNetworkManager(&page2));
    page2.setHtml("<html><head>"
                               "<meta http-equiv=\"refresh\"content=\"1;URL=qrc:///resources/index.html\">"
                               "</head><body><h1>Page redirect test with 1 sec timeout...</h1>"
                               "</body></html>");
    page2.triggerAction(QWebEnginePage::StopScheduledPageRefresh);
    QTest::qWait(1500);
    QEXPECT_FAIL("", "https://bugs.webkit.org/show_bug.cgi?id=118673", Continue);
    QCOMPARE(page2.url().toString(), QLatin1String("about:blank"));
#endif
}

void tst_QWebEnginePage::findText()
{
    QSignalSpy loadSpy(m_page, SIGNAL(loadFinished(bool)));
    m_page->setHtml(QString("<html><head></head><body><div>foo bar</div></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 1);

    // Select whole page contents.
    m_page->triggerAction(QWebEnginePage::SelectAll);
    QTRY_COMPARE(m_page->hasSelection(), true);

    // Invoke a stopFinding() operation, which should clear the currently selected text.
    m_page->findText("");
    QTRY_VERIFY(m_page->selectedText().isEmpty());

    QStringList words = (QStringList() << "foo" << "bar");
    foreach (QString subString, words) {
        // Invoke a find operation, which should clear the currently selected text, should
        // highlight all the found ocurrences, but should not update the selected text to the
        // searched for string.
        m_page->findText(subString);
        QTRY_VERIFY(m_page->selectedText().isEmpty());

        // Search highlights should be cleared, selected text should still be empty.
        m_page->findText("");
        QTRY_VERIFY(m_page->selectedText().isEmpty());
    }
}

void tst_QWebEnginePage::findTextResult()
{
    // findText will abort in blink if the view has an empty size.
    m_view->resize(800, 600);
    m_view->show();

    QSignalSpy loadSpy(m_view, SIGNAL(loadFinished(bool)));
    m_view->setHtml(QString("<html><head></head><body><div>foo bar</div></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 1);

    QCOMPARE(findTextSync(m_page, ""), false);

    QStringList words = (QStringList() << "foo" << "bar");
    foreach (QString subString, words) {
        QCOMPARE(findTextSync(m_page, subString), true);
        QCOMPARE(findTextSync(m_page, ""), false);
    }

    QCOMPARE(findTextSync(m_page, "blahhh"), false);
    QCOMPARE(findTextSync(m_page, ""), false);
}

void tst_QWebEnginePage::findTextSuccessiveShouldCallAllCallbacks()
{
    CallbackSpy<bool> spy1;
    CallbackSpy<bool> spy2;
    CallbackSpy<bool> spy3;
    CallbackSpy<bool> spy4;
    CallbackSpy<bool> spy5;
    QSignalSpy loadSpy(m_view, SIGNAL(loadFinished(bool)));
    m_view->setHtml(QString("<html><head></head><body><div>abcdefg abcdefg abcdefg abcdefg abcdefg</div></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 1);
    m_page->findText("abcde", 0, spy1.ref());
    m_page->findText("abcd", 0, spy2.ref());
    m_page->findText("abc", 0, spy3.ref());
    m_page->findText("ab", 0, spy4.ref());
    m_page->findText("a", 0, spy5.ref());
    spy5.waitForResult();
    QVERIFY(spy1.wasCalled());
    QVERIFY(spy2.wasCalled());
    QVERIFY(spy3.wasCalled());
    QVERIFY(spy4.wasCalled());
    QVERIFY(spy5.wasCalled());
}

#if defined(QWEBENGINEPAGE_SUPPORTEDCONTENTTYPES)
static QString getMimeTypeForExtension(const QString &ext)
{
    QMimeType mimeType = QMimeDatabase().mimeTypeForFile(QStringLiteral("filename.") + ext.toLower(), QMimeDatabase::MatchExtension);
    if (mimeType.isValid() && !mimeType.isDefault())
        return mimeType.name();

    return QString();
}
#endif

void tst_QWebEnginePage::supportedContentType()
{
#if !defined(QWEBENGINEPAGE_SUPPORTEDCONTENTTYPES)
    QSKIP("QWEBENGINEPAGE_SUPPORTEDCONTENTTYPES");
#else
    QStringList contentTypes;

    // Add supported non image types...
    contentTypes << "text/html" << "text/xml" << "text/xsl" << "text/plain" << "text/"
                 << "application/xml" << "application/xhtml+xml" << "application/vnd.wap.xhtml+xml"
                 << "application/rss+xml" << "application/atom+xml" << "application/json";

#if ENABLE_MHTML
    contentTypes << "application/x-mimearchive";
#endif

    // Add supported image types...
    Q_FOREACH (const QByteArray& imageType, QImageWriter::supportedImageFormats()) {
        const QString mimeType = getMimeTypeForExtension(imageType);
        if (!mimeType.isEmpty())
            contentTypes << mimeType;
    }

    // Get the mime types supported by webengine...
    const QStringList supportedContentTypes = m_page->supportedContentTypes();

    Q_FOREACH (const QString& mimeType, contentTypes)
        QVERIFY2(supportedContentTypes.contains(mimeType), QString("'%1' is not a supported content type!").arg(mimeType).toLatin1());

    Q_FOREACH (const QString& mimeType, contentTypes)
        QVERIFY2(m_page->supportsContentType(mimeType), QString("Cannot handle content types '%1'!").arg(mimeType).toLatin1());
#endif
}

void tst_QWebEnginePage::thirdPartyCookiePolicy()
{
#if !defined(DUMPRENDERTREESUPPORTQT)
    QSKIP("DUMPRENDERTREESUPPORTQT");
#else
    QWebEngineSettings::globalSettings()->setThirdPartyCookiePolicy(QWebEngineSettings::AlwaysBlockThirdPartyCookies);
    m_page->networkAccessManager()->setCookieJar(new QNetworkCookieJar());
    QVERIFY(m_page->networkAccessManager()->cookieJar());

    // These are all first-party cookies, so should pass.
    QVERIFY(DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://www.example.com"), QUrl("http://example.com")));
    QVERIFY(DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://www.example.com"), QUrl("http://doc.example.com")));
    QVERIFY(DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://aaa.www.example.com"), QUrl("http://doc.example.com")));
    QVERIFY(DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://example.com"), QUrl("http://www.example.com")));
    QVERIFY(DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://www.example.co.uk"), QUrl("http://example.co.uk")));
    QVERIFY(DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://www.example.co.uk"), QUrl("http://doc.example.co.uk")));
    QVERIFY(DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://aaa.www.example.co.uk"), QUrl("http://doc.example.co.uk")));
    QVERIFY(DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://example.co.uk"), QUrl("http://www.example.co.uk")));

    // These are all third-party cookies, so should fail.
    QVERIFY(!DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://www.example.com"), QUrl("http://slashdot.org")));
    QVERIFY(!DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://example.com"), QUrl("http://anotherexample.com")));
    QVERIFY(!DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://anotherexample.com"), QUrl("http://example.com")));
    QVERIFY(!DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://www.example.co.uk"), QUrl("http://slashdot.co.uk")));
    QVERIFY(!DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://example.co.uk"), QUrl("http://anotherexample.co.uk")));
    QVERIFY(!DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(m_page->handle(),
            QUrl("http://anotherexample.co.uk"), QUrl("http://example.co.uk")));
#endif
}

static QWindow *findNewTopLevelWindow(const QWindowList &oldTopLevelWindows)
{
    const auto tlws = QGuiApplication::topLevelWindows();
    for (auto w : tlws) {
        if (!oldTopLevelWindows.contains(w)) {
            return w;
        }
    }
    return nullptr;
}

void tst_QWebEnginePage::comboBoxPopupPositionAfterMove()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QWebEngineView view;
    view.move(screen->availableGeometry().topLeft());
    view.resize(640, 480);
    view.show();

    QSignalSpy loadSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml(QLatin1String("<html><head></head><body><select id='foo'>"
                               "<option>fran</option><option>troz</option>"
                               "</select></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 1);
    const auto oldTlws = QGuiApplication::topLevelWindows();
    QWindow *window = view.windowHandle();
    QTest::mouseClick(window, Qt::LeftButton, Qt::KeyboardModifiers(),
                      elementCenter(view.page(), "foo"));

    QWindow *popup = nullptr;
    QTRY_VERIFY(popup = findNewTopLevelWindow(oldTlws));
    QPoint popupPos = popup->position();

    // Close the popup by clicking somewhere into the page.
    QTest::mouseClick(window, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(1, 1));
    QTRY_VERIFY(!QGuiApplication::topLevelWindows().contains(popup));

    // Move the top-level QWebEngineView a little and check the popup's position.
    const QPoint offset(12, 13);
    view.move(screen->availableGeometry().topLeft() + offset);
    QTest::mouseClick(window, Qt::LeftButton, Qt::KeyboardModifiers(),
                      elementCenter(view.page(), "foo"));
    QTRY_VERIFY(popup = findNewTopLevelWindow(oldTlws));
    QCOMPARE(popupPos + offset, popup->position());
}

void tst_QWebEnginePage::comboBoxPopupPositionAfterChildMove()
{
    QWidget mainWidget;
    mainWidget.setLayout(new QHBoxLayout);

    QWidget spacer;
    spacer.setMinimumWidth(50);
    mainWidget.layout()->addWidget(&spacer);

    QWebEngineView view;
    mainWidget.layout()->addWidget(&view);

    QScreen *screen = QGuiApplication::primaryScreen();
    mainWidget.move(screen->availableGeometry().topLeft());
    mainWidget.resize(640, 480);
    mainWidget.show();

    QSignalSpy loadSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml(QLatin1String("<html><head></head><body><select autofocus id='foo'>"
                               "<option value=\"narf\">narf</option><option>zort</option>"
                               "</select></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 1);
    const auto oldTlws = QGuiApplication::topLevelWindows();
    QWindow *window = view.window()->windowHandle();
    QTest::mouseClick(window, Qt::LeftButton, Qt::KeyboardModifiers(),
                      view.mapTo(view.window(), elementCenter(view.page(), "foo")));

    QWindow *popup = nullptr;
    QTRY_VERIFY(popup = findNewTopLevelWindow(oldTlws));
    QPoint popupPos = popup->position();

    // Close the popup by clicking somewhere into the page.
    QTest::mouseClick(window, Qt::LeftButton, Qt::KeyboardModifiers(),
                      view.mapTo(view.window(), QPoint(1, 1)));
    QTRY_VERIFY(!QGuiApplication::topLevelWindows().contains(popup));

    // Resize the "spacer" widget, and implicitly change the global position of the QWebEngineView.
    spacer.setMinimumWidth(100);
    QTest::mouseClick(window, Qt::LeftButton, Qt::KeyboardModifiers(),
                      view.mapTo(view.window(), elementCenter(view.page(), "foo")));
    QTRY_VERIFY(popup = findNewTopLevelWindow(oldTlws));
    QCOMPARE(popupPos + QPoint(50, 0), popup->position());
}

#ifdef Q_OS_MAC
void tst_QWebEnginePage::macCopyUnicodeToClipboard()
{
    QString unicodeText = QString::fromUtf8("αβγδεζηθικλμπ");
    m_page->setHtml(QString("<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /></head><body>%1</body></html>").arg(unicodeText));
    m_page->triggerAction(QWebEnginePage::SelectAll);
    m_page->triggerAction(QWebEnginePage::Copy);

    QString clipboardData = QString::fromUtf8(QApplication::clipboard()->mimeData()->data(QLatin1String("text/html")));

    QVERIFY(clipboardData.contains(QLatin1String("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />")));
    QVERIFY(clipboardData.contains(unicodeText));
}
#endif

void tst_QWebEnginePage::contextMenuCopy()
{
#if !defined(QWEBENGINEELEMENT)
    QSKIP("QWEBENGINEELEMENT");
#else
    QWebEngineView view;

    view.setHtml("<a href=\"http://www.google.com\">You cant miss this</a>");

    view.page()->triggerAction(QWebEnginePage::SelectAll);
    QVERIFY(!view.page()->selectedText().isEmpty());

    QWebEngineElement link = view.page()->mainFrame()->findFirstElement("a");
    QPoint pos(link.geometry().center());
    QContextMenuEvent event(QContextMenuEvent::Mouse, pos);
    view.page()->swallowContextMenuEvent(&event);
    view.page()->updatePositionDependentActions(pos);

    QList<QMenu*> contextMenus = view.findChildren<QMenu*>();
    QVERIFY(!contextMenus.isEmpty());
    QMenu* contextMenu = contextMenus.first();
    QVERIFY(contextMenu);

    QList<QAction *> list = contextMenu->actions();
    int index = list.indexOf(view.page()->action(QWebEnginePage::Copy));
    QVERIFY(index != -1);
#endif
}

// https://bugs.webkit.org/show_bug.cgi?id=62139
void tst_QWebEnginePage::contextMenuPopulatedOnce()
{
#if !defined(QWEBENGINEELEMENT)
    QSKIP("QWEBENGINEELEMENT");
#else
    QWebEngineView view;

    view.setHtml("<input type=\"text\">");

    QWebEngineElement link = view.page()->mainFrame()->findFirstElement("input");
    QPoint pos(link.geometry().center());
    QContextMenuEvent event(QContextMenuEvent::Mouse, pos);
    view.page()->swallowContextMenuEvent(&event);
    view.page()->updatePositionDependentActions(pos);

    QList<QMenu*> contextMenus = view.findChildren<QMenu*>();
    QVERIFY(!contextMenus.isEmpty());
    QMenu* contextMenu = contextMenus.first();
    QVERIFY(contextMenu);

    QList<QAction *> list = contextMenu->actions();
    QStringList entries;
    while (!list.isEmpty()) {
        QString entry = list.takeFirst()->text();
        QVERIFY(!entries.contains(entry));
        entries << entry;
    }
#endif
}

void tst_QWebEnginePage::deleteQWebEngineViewTwice()
{
    for (int i = 0; i < 2; ++i) {
        QMainWindow mainWindow;
        QWebEngineView* webView = new QWebEngineView(&mainWindow);
        mainWindow.setCentralWidget(webView);
        webView->load(QUrl("qrc:///resources/frame_a.html"));
        mainWindow.show();
        QVERIFY(::waitForSignal(webView, SIGNAL(loadFinished(bool))));
    }
}

#if defined(QWEBENGINEPAGE_RENDER)
class RepaintRequestedRenderer : public QObject {
    Q_OBJECT
public:
    RepaintRequestedRenderer(QWebEnginePage* page, QPainter* painter)
        : m_page(page)
        , m_painter(painter)
        , m_recursionCount(0)
    {
        connect(m_page, SIGNAL(repaintRequested(QRect)), this, SLOT(onRepaintRequested(QRect)));
    }

Q_SIGNALS:
    void finished();

private Q_SLOTS:
    void onRepaintRequested(const QRect& rect)
    {
        QCOMPARE(m_recursionCount, 0);

        m_recursionCount++;
        m_page->render(m_painter, rect);
        m_recursionCount--;

        QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
    }

private:
    QWebEnginePage* m_page;
    QPainter* m_painter;
    int m_recursionCount;
};
#endif

void tst_QWebEnginePage::renderOnRepaintRequestedShouldNotRecurse()
{
#if !defined(QWEBENGINEPAGE_RENDER)
    QSKIP("QWEBENGINEPAGE_RENDER");
#else
    QSize viewportSize(720, 576);
    QWebEnginePage page;

    QImage image(viewportSize, QImage::Format_ARGB32);
    QPainter painter(&image);

    page.setPreferredContentsSize(viewportSize);
    page.setViewportSize(viewportSize);
    RepaintRequestedRenderer r(&page, &painter);

    page.setHtml("zalan loves trunk", QUrl());

    QVERIFY(::waitForSignal(&r, SIGNAL(finished())));
#endif
}

class SpyForLoadSignalsOrder : public QStateMachine {
    Q_OBJECT
public:
    SpyForLoadSignalsOrder(QWebEnginePage* page, QObject* parent = 0)
        : QStateMachine(parent)
    {
        connect(page, SIGNAL(loadProgress(int)), SLOT(onLoadProgress(int)));

        QState* waitingForLoadStarted = new QState(this);
        QState* waitingForLastLoadProgress = new QState(this);
        QState* waitingForLoadFinished = new QState(this);
        QFinalState* final = new QFinalState(this);

        waitingForLoadStarted->addTransition(page, SIGNAL(loadStarted()), waitingForLastLoadProgress);
        waitingForLastLoadProgress->addTransition(this, SIGNAL(lastLoadProgress()), waitingForLoadFinished);
        waitingForLoadFinished->addTransition(page, SIGNAL(loadFinished(bool)), final);

        setInitialState(waitingForLoadStarted);
        start();
    }
    bool isFinished() const
    {
        return !isRunning();
    }
public Q_SLOTS:
    void onLoadProgress(int progress)
    {
        if (progress == 100)
            emit lastLoadProgress();
    }
Q_SIGNALS:
    void lastLoadProgress();
};

void tst_QWebEnginePage::loadSignalsOrder_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::newRow("inline data") << QUrl("data:text/html,This is first page");
    QTest::newRow("simple page") << QUrl("qrc:///resources/content.html");
    QTest::newRow("frameset page") << QUrl("qrc:///resources/index.html");
}

void tst_QWebEnginePage::loadSignalsOrder()
{
    QFETCH(QUrl, url);
    QWebEnginePage page;
    SpyForLoadSignalsOrder loadSpy(&page);
    waitForSignal(&loadSpy, SIGNAL(started()), 500);
    page.load(url);
    QTRY_VERIFY(loadSpy.isFinished());
}

void tst_QWebEnginePage::undoActionHaveCustomText()
{
#if !defined(QWEBENGINEPAGE_UNDOACTION)
    QSKIP("QWEBENGINEPAGE_UNDOACTION");
#else
    m_page->setHtml("<div id=test contenteditable></div>");
    evaluateJavaScriptSync(m_page, "document.getElementById('test').focus()");

    evaluateJavaScriptSync(m_page, "document.execCommand('insertText', true, 'Test');");
    QString typingActionText = m_page->action(QWebEnginePage::Undo)->text();

    evaluateJavaScriptSync(m_page, "document.execCommand('indent', true);");
    QString alignActionText = m_page->action(QWebEnginePage::Undo)->text();

    QVERIFY(typingActionText != alignActionText);
#endif
}

void tst_QWebEnginePage::renderWidgetHostViewNotShowTopLevel()
{
    QWebEnginePage page;
    QSignalSpy spyLoadFinished(&page, SIGNAL(loadFinished(bool)));

    page.load(QUrl("http://qt-project.org"));
    if (!spyLoadFinished.wait(10000) || !spyLoadFinished.at(0).at(0).toBool())
        QSKIP("Couldn't load page from network, skipping test.");
    spyLoadFinished.clear();

    // Loading a different domain will force the creation of a separate render
    // process and should therefore create a new RenderWidgetHostViewQtDelegateWidget.
    page.load(QUrl("http://www.wikipedia.org/"));
    if (!spyLoadFinished.wait(10000) || !spyLoadFinished.at(0).at(0).toBool())
        QSKIP("Couldn't load page from network, skipping test.");

    // Make sure that RenderWidgetHostViewQtDelegateWidgets are not shown as top-level.
    // They should only be made visible when parented to a QWebEngineView.
    foreach (QWidget *widget, QApplication::topLevelWidgets())
        QCOMPARE(widget->isVisible(), false);
}

class GetUserMediaTestPage : public QWebEnginePage {
Q_OBJECT

public:
    GetUserMediaTestPage()
        : m_gotRequest(false)
    {
        connect(this, &QWebEnginePage::featurePermissionRequested, this, &GetUserMediaTestPage::onFeaturePermissionRequested);
    }

    void rejectPendingRequest()
    {
        setFeaturePermission(m_requestSecurityOrigin, m_requestedFeature, QWebEnginePage::PermissionDeniedByUser);
        m_gotRequest = false;
    }
    void acceptPendingRequest()
    {
        setFeaturePermission(m_requestSecurityOrigin, m_requestedFeature, QWebEnginePage::PermissionGrantedByUser);
        m_gotRequest = false;
    }

    bool gotFeatureRequest(QWebEnginePage::Feature feature)
    {
        return m_gotRequest && m_requestedFeature == feature;
    }

private Q_SLOTS:
    void onFeaturePermissionRequested(const QUrl &securityOrigin, QWebEnginePage::Feature feature)
    {
        m_requestedFeature = feature;
        m_requestSecurityOrigin = securityOrigin;
        m_gotRequest = true;
    }

private:
    bool m_gotRequest;
    QWebEnginePage::Feature m_requestedFeature;
    QUrl m_requestSecurityOrigin;

};


void tst_QWebEnginePage::getUserMediaRequest()
{
    GetUserMediaTestPage *page = new GetUserMediaTestPage();

    // We need to load content from a resource in order for the securityOrigin to be valid.
    QSignalSpy loadSpy(page, SIGNAL(loadFinished(bool)));
    page->load(QUrl("qrc:///resources/content.html"));
    QTRY_COMPARE(loadSpy.count(), 1);

    QVERIFY(evaluateJavaScriptSync(page, QStringLiteral("!!navigator.webkitGetUserMedia")).toBool());
    evaluateJavaScriptSync(page, QStringLiteral("navigator.webkitGetUserMedia({audio: true}, function() {}, function(){})"));
    QTRY_VERIFY(page->gotFeatureRequest(QWebEnginePage::MediaAudioCapture));
    // Might end up failing due to the lack of physical media devices deeper in the content layer, so the JS callback is not guaranteed to be called,
    // but at least we go through that code path, potentially uncovering failing assertions.
    page->acceptPendingRequest();

    page->runJavaScript(QStringLiteral("errorCallbackCalled = false;"));
    evaluateJavaScriptSync(page, QStringLiteral("navigator.webkitGetUserMedia({audio: true, video: true}, function() {}, function(){errorCallbackCalled = true;})"));
    QTRY_VERIFY(page->gotFeatureRequest(QWebEnginePage::MediaAudioVideoCapture));
    page->rejectPendingRequest(); // Should always end up calling the error callback in JS.
    QTRY_VERIFY(evaluateJavaScriptSync(page, QStringLiteral("errorCallbackCalled;")).toBool());
    delete page;
}

void tst_QWebEnginePage::savePage()
{
    QWebEngineView view;
    QWebEnginePage *page = view.page();

    connect(page->profile(), &QWebEngineProfile::downloadRequested,
            [] (QWebEngineDownloadItem *item)
    {
        connect(item, &QWebEngineDownloadItem::finished,
                &QTestEventLoop::instance(), &QTestEventLoop::exitLoop, Qt::QueuedConnection);
    });

    const QString urlPrefix = QStringLiteral("data:text/html,<h1>");
    const QString text = QStringLiteral("There is Thingumbob shouting!");
    page->load(QUrl(urlPrefix + text));
    waitForSignal(page, SIGNAL(loadFinished(bool)));
    QCOMPARE(toPlainTextSync(page), text);

    // Save the loaded page as HTML.
    QTemporaryDir tempDir(QDir::tempPath() + "/tst_qwebengineview-XXXXXX");
    const QString filePath = tempDir.path() + "/thingumbob.html";
    page->save(filePath, QWebEngineDownloadItem::CompleteHtmlSaveFormat);
    QTestEventLoop::instance().enterLoop(10);

    // Load something else.
    page->load(QUrl(urlPrefix + QLatin1String("It's a Snark!")));
    waitForSignal(page, SIGNAL(loadFinished(bool)));
    QVERIFY(toPlainTextSync(page) != text);

    // Load the saved page and compare the contents.
    page->load(QUrl::fromLocalFile(filePath));
    waitForSignal(page, SIGNAL(loadFinished(bool)));
    QCOMPARE(toPlainTextSync(page), text);
}

void tst_QWebEnginePage::openWindowDefaultSize()
{
    TestPage page;
    QSignalSpy windowCreatedSpy(&page, SIGNAL(windowCreated()));
    QWebEngineView view;
    page.setView(&view);
    view.show();

    page.settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    // Open a default window.
    page.runJavaScript("window.open()");
    QTRY_COMPARE(windowCreatedSpy.count(), 1);
    // Open a too small window.
    evaluateJavaScriptSync(&page, "window.open('','about:blank','width=10,height=10')");
    QTRY_COMPARE(windowCreatedSpy.count(), 2);

    // The number of popups created should be two.
    QCOMPARE(page.createdWindows.size(), 2);

    QRect requestedGeometry = page.createdWindows[0]->requestedGeometry;
    // Check default size has been requested.
    QCOMPARE(requestedGeometry.width(), 0);
    QCOMPARE(requestedGeometry.height(), 0);

    requestedGeometry = page.createdWindows[1]->requestedGeometry;
    // Check minimum size has been requested.
    QCOMPARE(requestedGeometry.width(), 100);
    QCOMPARE(requestedGeometry.height(), 100);
}

void tst_QWebEnginePage::cssMediaTypeGlobalSetting()
{
#if !defined(QWEBENGINESETTINGS_SETCSSMEDIATYPE)
    QSKIP("QWEBENGINESETTINGS_SETCSSMEDIATYPE");
#else
    QString testHtml("<style>@media tv {body{background-color:red;}}@media handheld {body{background-color:green;}}@media screen {body{background-color:blue;}}</style>");
    QSignalSpy loadSpy(m_view, SIGNAL(loadFinished(bool)));

    QWebEngineSettings::globalSettings()->setCSSMediaType("tv");
    // Clear page specific setting to read from global setting
    m_view->page()->settings()->setCSSMediaType(QString());
    m_view->setHtml(testHtml);
    QTRY_COMPARE(loadSpy.count(), 1);
    QVERIFY(evaluateJavaScriptSync(m_view->page(), "window.matchMedia('tv').matches == true").toBool());
    QVERIFY(QWebEngineSettings::globalSettings()->cssMediaType() == "tv");

    QWebEngineSettings::globalSettings()->setCSSMediaType("handheld");
    // Clear page specific setting to read from global setting
    m_view->page()->settings()->setCSSMediaType(QString());
    m_view->setHtml(testHtml);
    QTRY_COMPARE(loadSpy.count(), 2);
    QVERIFY(evaluateJavaScriptSync(m_view->page(), "window.matchMedia('handheld').matches == true").toBool());
    QVERIFY(QWebEngineSettings::globalSettings()->cssMediaType() == "handheld");

    QWebEngineSettings::globalSettings()->setCSSMediaType("screen");
    // Clear page specific setting to read from global setting
    m_view->page()->settings()->setCSSMediaType(QString());
    m_view->setHtml(testHtml);
    QTRY_COMPARE(loadSpy.count(), 3);
    QVERIFY(evaluateJavaScriptSync(m_view->page(), "window.matchMedia('screen').matches == true").toBool());
    QVERIFY(QWebEngineSettings::globalSettings()->cssMediaType() == "screen");
#endif
}

void tst_QWebEnginePage::cssMediaTypePageSetting()
{
#if !defined(QWEBENGINESETTINGS_SETCSSMEDIATYPE)
    QSKIP("QWEBENGINESETTINGS_SETCSSMEDIATYPE");
#else
    QString testHtml("<style>@media tv {body{background-color:red;}}@media handheld {body{background-color:green;}}@media screen {body{background-color:blue;}}</style>");
    QSignalSpy loadSpy(m_view, SIGNAL(loadFinished(bool)));

    m_view->page()->settings()->setCSSMediaType("tv");
    m_view->setHtml(testHtml);
    QTRY_COMPARE(loadSpy.count(), 1);
    QVERIFY(evaluateJavaScriptSync(m_view->page(), "window.matchMedia('tv').matches == true").toBool());
    QVERIFY(m_view->page()->settings()->cssMediaType() == "tv");

    m_view->page()->settings()->setCSSMediaType("handheld");
    m_view->setHtml(testHtml);
    QTRY_COMPARE(loadSpy.count(), 2);
    QVERIFY(evaluateJavaScriptSync(m_view->page(), "window.matchMedia('handheld').matches == true").toBool());
    QVERIFY(m_view->page()->settings()->cssMediaType() == "handheld");

    m_view->page()->settings()->setCSSMediaType("screen");
    m_view->setHtml(testHtml);
    QTRY_COMPARE(loadSpy.count(), 3);
    QVERIFY(evaluateJavaScriptSync(m_view->page(), "window.matchMedia('screen').matches == true").toBool());
    QVERIFY(m_view->page()->settings()->cssMediaType() == "screen");
#endif
}

class JavaScriptCallbackBase
{
public:
    JavaScriptCallbackBase()
    {
        if (watcher)
            QMetaObject::invokeMethod(watcher, "add");
    }

    void operator() (const QVariant &result)
    {
        check(result);
        if (watcher)
            QMetaObject::invokeMethod(watcher, "notify");
    }

protected:
    virtual void check(const QVariant &result) = 0;

private:
    friend class JavaScriptCallbackWatcher;
    static QPointer<QObject> watcher;
};

QPointer<QObject> JavaScriptCallbackBase::watcher = 0;

class JavaScriptCallback : public JavaScriptCallbackBase
{
public:
    JavaScriptCallback() { }
    JavaScriptCallback(const QVariant& _expected) : expected(_expected) { }

    void check(const QVariant& result) Q_DECL_OVERRIDE
    {
        QVERIFY(result.isValid());
        QCOMPARE(result, expected);
    }

private:
    QVariant expected;
};

class JavaScriptCallbackNull : public JavaScriptCallbackBase
{
public:
    void check(const QVariant& result) Q_DECL_OVERRIDE
    {
        QVERIFY(result.isNull());
// FIXME: Returned null values are currently invalid QVariants.
//        QVERIFY(result.isValid());
    }
};

class JavaScriptCallbackUndefined : public JavaScriptCallbackBase
{
public:
    void check(const QVariant& result) Q_DECL_OVERRIDE
    {
        QVERIFY(result.isNull());
        QVERIFY(!result.isValid());
    }
};

class JavaScriptCallbackWatcher : public QObject
{
    Q_OBJECT
public:
    JavaScriptCallbackWatcher()
    {
        Q_ASSERT(!JavaScriptCallbackBase::watcher);
        JavaScriptCallbackBase::watcher = this;
    }

    Q_INVOKABLE void add()
    {
        available++;
    }

    Q_INVOKABLE void notify()
    {
        called++;
        if (called == available)
            emit allCalled();
    }

    bool wait(int maxSeconds = 30)
    {
        if (called == available)
            return true;

        QTestEventLoop loop;
        connect(this, SIGNAL(allCalled()), &loop, SLOT(exitLoop()));
        loop.enterLoop(maxSeconds);
        return !loop.timeout();
    }

signals:
    void allCalled();

private:
    int available = 0;
    int called = 0;
};


void tst_QWebEnginePage::runJavaScript()
{
    TestPage page;
    JavaScriptCallbackWatcher watcher;

    JavaScriptCallback callbackBool(QVariant(false));
    page.runJavaScript("false", QWebEngineCallback<const QVariant&>(callbackBool));

    JavaScriptCallback callbackInt(QVariant(2));
    page.runJavaScript("2", QWebEngineCallback<const QVariant&>(callbackInt));

    JavaScriptCallback callbackDouble(QVariant(2.5));
    page.runJavaScript("2.5", QWebEngineCallback<const QVariant&>(callbackDouble));

    JavaScriptCallback callbackString(QVariant(QStringLiteral("Test")));
    page.runJavaScript("\"Test\"", QWebEngineCallback<const QVariant&>(callbackString));

    QVariantList list;
    JavaScriptCallback callbackList(list);
    page.runJavaScript("[]", QWebEngineCallback<const QVariant&>(callbackList));

    QVariantMap map;
    map.insert(QStringLiteral("test"), QVariant(2));
    JavaScriptCallback callbackMap(map);
    page.runJavaScript("var el = {\"test\": 2}; el", QWebEngineCallback<const QVariant&>(callbackMap));

    JavaScriptCallbackNull callbackNull;
    page.runJavaScript("null", QWebEngineCallback<const QVariant&>(callbackNull));

    JavaScriptCallbackUndefined callbackUndefined;
    page.runJavaScript("undefined", QWebEngineCallback<const QVariant&>(callbackUndefined));

    QVERIFY(watcher.wait());
}

void tst_QWebEnginePage::fullScreenRequested()
{
    JavaScriptCallbackWatcher watcher;
    QWebEngineView* view = new QWebEngineView;
    QWebEnginePage* page = view->page();
    view->show();

    page->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);

    QSignalSpy loadSpy(view, SIGNAL(loadFinished(bool)));
    page->load(QUrl("qrc:///resources/fullscreen.html"));
    QTRY_COMPARE(loadSpy.count(), 1);

    page->runJavaScript("document.webkitFullscreenEnabled", JavaScriptCallback(true));
    page->runJavaScript("document.webkitIsFullScreen", JavaScriptCallback(false));
    QVERIFY(watcher.wait());

    // FullscreenRequest must be a user gesture
    bool acceptRequest = true;
    connect(page, &QWebEnginePage::fullScreenRequested,
        [&acceptRequest](QWebEngineFullScreenRequest request) {
        if (acceptRequest) request.accept(); else request.reject();
    });

    QTest::keyPress(view->focusProxy(), Qt::Key_Space);
    QTRY_VERIFY(evaluateJavaScriptSync(page, "document.webkitIsFullScreen").toBool());
    page->runJavaScript("document.webkitExitFullscreen()", JavaScriptCallbackUndefined());
    QVERIFY(watcher.wait());

    acceptRequest = false;

    page->runJavaScript("document.webkitFullscreenEnabled", JavaScriptCallback(true));
    QTest::keyPress(view->focusProxy(), Qt::Key_Space);
    QVERIFY(watcher.wait());
    page->runJavaScript("document.webkitIsFullScreen", JavaScriptCallback(false));
    QVERIFY(watcher.wait());

    delete view;
}

void tst_QWebEnginePage::symmetricUrl()
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

    // setUrl(dataUrl3) might override the pending load for dataUrl2. Or not.
    QTRY_VERIFY(loadFinishedSpy.count() >= 2);
    QTRY_VERIFY(loadFinishedSpy.count() <= 3);

    // setUrl(dataUrl3) might stop Chromium from adding a navigation entry for dataUrl2,
    // depending on whether the load of dataUrl2 could be completed in time.
    QVERIFY(view.history()->count() >= 2);
    QVERIFY(view.history()->count() <= 3);

    QCOMPARE(toPlainTextSync(view.page()), QString("Test3"));
}

void tst_QWebEnginePage::progressSignal()
{
    QSignalSpy progressSpy(m_view, SIGNAL(loadProgress(int)));

    QUrl dataUrl("data:text/html,<h1>Test");
    m_view->setUrl(dataUrl);

    ::waitForSignal(m_view, SIGNAL(loadFinished(bool)));

    QVERIFY(progressSpy.size() >= 2);
    int previousValue = -1;
    for (QSignalSpy::ConstIterator it = progressSpy.begin(); it < progressSpy.end(); ++it) {
        int current = (*it).first().toInt();
        QVERIFY(current >= previousValue);
        previousValue = current;
    }

    // But we always end at 100%
    QCOMPARE(progressSpy.last().first().toInt(), 100);
}

void tst_QWebEnginePage::urlChange()
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

void tst_QWebEnginePage::requestedUrlAfterSetAndLoadFailures()
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

void tst_QWebEnginePage::asyncAndDelete()
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

void tst_QWebEnginePage::earlyToHtml()
{
    QString html("<html><head></head><body></body></html>");
    QCOMPARE(toHtmlSync(m_view->page()), html);
}

void tst_QWebEnginePage::setHtml()
{
    QString html("<html><head></head><body><p>hello world</p></body></html>");
    QSignalSpy spy(m_view->page(), SIGNAL(loadFinished(bool)));
    m_view->page()->setHtml(html);
    QVERIFY(spy.wait());
    QCOMPARE(toHtmlSync(m_view->page()), html);
}

void tst_QWebEnginePage::setHtmlWithImageResource()
{
    // We allow access to qrc resources from any security origin, including local and anonymous

    QLatin1String html("<html><body><p>hello world</p><img src='qrc:/resources/image.png'/></body></html>");
    QWebEnginePage page;

    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));
    page.setHtml(html, QUrl("file:///path/to/file"));
    QTRY_COMPARE(spy.count(), 1);

    QCOMPARE(evaluateJavaScriptSync(&page, "document.images.length").toInt(), 1);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images[0].width").toInt(), 128);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images[0].height").toInt(), 128);

    // Now we test the opposite: without a baseUrl as a local file, we can still request qrc resources.

    page.setHtml(html);
    QTRY_COMPARE(spy.count(), 2);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images.length").toInt(), 1);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images[0].width").toInt(), 128);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.images[0].height").toInt(), 128);
}

void tst_QWebEnginePage::setHtmlWithStylesheetResource()
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

void tst_QWebEnginePage::setHtmlWithBaseURL()
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
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
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

void tst_QWebEnginePage::setHtmlWithJSAlert()
{
    QString html("<html><head></head><body><script>alert('foo');</script><p>hello world</p></body></html>");
    MyPage page;
    page.setHtml(html, QUrl(QStringLiteral("http://test.origin.com/path#fragment")));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(page.alerts, 1);
    QCOMPARE(toHtmlSync(&page), html);
}

void tst_QWebEnginePage::inputFieldFocus()
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

void tst_QWebEnginePage::hitTestContent()
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

void tst_QWebEnginePage::baseUrl_data()
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

void tst_QWebEnginePage::baseUrl()
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

void tst_QWebEnginePage::scrollPosition()
{
    // enlarged image in a small viewport, to provoke the scrollbars to appear
    QString html("<html><body><img src='qrc:/image.png' height=500 width=500/></body></html>");

    QWebEngineView view;
    view.setFixedSize(200,200);
    view.show();

    QTest::qWaitForWindowExposed(&view);

    QSignalSpy loadSpy(view.page(), SIGNAL(loadFinished(bool)));
    view.setHtml(html);
    QTRY_COMPARE(loadSpy.count(), 1);

    // try to set the scroll offset programmatically
    view.page()->runJavaScript("window.scrollTo(23, 29);");
    QTRY_COMPARE(view.page()->scrollPosition().x(), qreal(23));
    QCOMPARE(view.page()->scrollPosition().y(), qreal(29));

    int x = evaluateJavaScriptSync(view.page(), "window.scrollX").toInt();
    int y = evaluateJavaScriptSync(view.page(), "window.scrollY").toInt();
    QCOMPARE(x, 23);
    QCOMPARE(y, 29);
}

void tst_QWebEnginePage::scrollToAnchor()
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


void tst_QWebEnginePage::scrollbarsOff()
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

void tst_QWebEnginePage::horizontalScrollAfterBack()
{
#if !defined(QWEBENGINESETTINGS)
    QSKIP("QWEBENGINESETTINGS");
#else
    QWebEngineView view;
    QSignalSpy loadSpy(view.page(), SIGNAL(loadFinished(bool)));

    view.page()->settings()->setMaximumPagesInCache(2);
    view.page()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAsNeeded);
    view.page()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAsNeeded);

    view.load(QUrl("qrc:/resources/testiframe2.html"));
    view.resize(200, 200);
    QTRY_COMPARE(loadSpy.count(), 1);
    QTRY_VERIFY((view.page()->scrollBarGeometry(Qt::Horizontal)).height());

    view.load(QUrl("qrc:/resources/testiframe.html"));
    QTRY_COMPARE(loadSpy.count(), 2);

    view.page()->triggerAction(QWebEnginePage::Back);
    QTRY_COMPARE(loadSpy.count(), 3);
    QTRY_VERIFY((view.page()->scrollBarGeometry(Qt::Horizontal)).height());
#endif
}

class WebView : public QWebEngineView
{
    Q_OBJECT
signals:
    void repaintRequested();

protected:
    bool event(QEvent *event) {
        if (event->type() == QEvent::UpdateRequest)
            emit repaintRequested();

        return QWebEngineView::event(event);
    }
};

void tst_QWebEnginePage::evaluateWillCauseRepaint()
{
    WebView view;
    view.show();
    QTest::qWaitForWindowExposed(&view);

    QString html("<html><body>"
                 "  top"
                 "  <div id=\"junk\" style=\"display: block;\">junk</div>"
                 "  bottom"
                 "</body></html>");

    QSignalSpy loadSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml(html);
    QTRY_COMPARE(loadSpy.count(), 1);

    evaluateJavaScriptSync(view.page(), "document.getElementById('junk').style.display = 'none';");
    ::waitForSignal(&view, SIGNAL(repaintRequested()));
}

void tst_QWebEnginePage::setContent_data()
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

void tst_QWebEnginePage::setContent()
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

void tst_QWebEnginePage::setCacheLoadControlAttribute()
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

void tst_QWebEnginePage::setUrlWithPendingLoads()
{
    QWebEnginePage page;
    page.setHtml("<img src='dummy:'/>");
    page.setUrl(QUrl("about:blank"));
}

void tst_QWebEnginePage::setUrlToEmpty()
{
    QSKIP("FIXME: [0908/090526:FATAL:navigation_controller_impl.cc(927)] Check failed: active_entry->site_instance() == rfh->GetSiteInstance().");

    int expectedLoadFinishedCount = 0;
    const QUrl aboutBlank("about:blank");
    const QUrl url("qrc:/resources/test2.html");

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

void tst_QWebEnginePage::setUrlToInvalid()
{
    QEXPECT_FAIL("", "Unsupported: QtWebEngine doesn't adjust invalid URLs.", Abort);
    QVERIFY(false);

    QWebEnginePage page;

    const QUrl invalidUrl("http:/example.com");
    QVERIFY(!invalidUrl.isEmpty());
    QVERIFY(invalidUrl != QUrl());

    // QWebEnginePage will do its best to accept the URL, possible converting it to a valid equivalent URL.
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

void tst_QWebEnginePage::setUrlHistory()
{
    QSKIP("FIXME: [0908/090526:FATAL:navigation_controller_impl.cc(927)] Check failed: active_entry->site_instance() == rfh->GetSiteInstance().");

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

    url = QUrl("qrc:/resources/test1.html");
    m_page->setUrl(url);
    expectedLoadFinishedCount++;
    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(m_page->url(), url);
    QCOMPARE(m_page->requestedUrl(), url);
    QCOMPARE(collectHistoryUrls(m_page->history()), QStringList() << aboutBlank.toString() << QStringLiteral("qrc:/resources/test1.html"));

    m_page->setUrl(QUrl());
    expectedLoadFinishedCount++;
    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(m_page->url(), aboutBlank);
    QCOMPARE(m_page->requestedUrl(), QUrl());
    // Chromium stores navigation entry for every successful loads. The load of the empty page is committed and stored as about:blank.
    QCOMPARE(collectHistoryUrls(m_page->history()), QStringList()
                                                        << aboutBlank.toString()
                                                        << QStringLiteral("qrc:/resources/test1.html")
                                                        << aboutBlank.toString());

    url = QUrl("qrc:/resources/test1.html");
    m_page->setUrl(url);
    expectedLoadFinishedCount++;
    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(m_page->url(), url);
    QCOMPARE(m_page->requestedUrl(), url);
    // The history count DOES change since the about:blank is in the list.
    QCOMPARE(collectHistoryUrls(m_page->history()), QStringList()
                                                        << aboutBlank.toString()
                                                        << QStringLiteral("qrc:/resources/test1.html")
                                                        << aboutBlank.toString()
                                                        << QStringLiteral("qrc:/resources/test1.html"));

    url = QUrl("qrc:/resources/test2.html");
    m_page->setUrl(url);
    expectedLoadFinishedCount++;
    QTRY_COMPARE(spy.count(), expectedLoadFinishedCount);
    QCOMPARE(m_page->url(), url);
    QCOMPARE(m_page->requestedUrl(), url);
    QCOMPARE(collectHistoryUrls(m_page->history()), QStringList()
                                                        << aboutBlank.toString()
                                                        << QStringLiteral("qrc:/resources/test1.html")
                                                        << aboutBlank.toString()
                                                        << QStringLiteral("qrc:/resources/test1.html")
                                                        << QStringLiteral("qrc:/resources/test2.html"));
}

void tst_QWebEnginePage::setUrlUsingStateObject()
{
    const QUrl aboutBlank("about:blank");
    QUrl url;
    QSignalSpy urlChangedSpy(m_page, SIGNAL(urlChanged(QUrl)));
    int expectedUrlChangeCount = 0;

    QCOMPARE(m_page->history()->count(), 0);

    url = QUrl("qrc:/resources/test1.html");
    m_page->setUrl(url);
    waitForSignal(m_page, SIGNAL(loadFinished(bool)));
    expectedUrlChangeCount++;
    QCOMPARE(urlChangedSpy.count(), expectedUrlChangeCount);
    QCOMPARE(m_page->url(), url);
    QCOMPARE(m_page->history()->count(), 1);

    evaluateJavaScriptSync(m_page, "window.history.pushState(null, 'push', 'navigate/to/here')");
    expectedUrlChangeCount++;
    QCOMPARE(urlChangedSpy.count(), expectedUrlChangeCount);
    QCOMPARE(m_page->url(), QUrl("qrc:/resources/navigate/to/here"));
    QCOMPARE(m_page->history()->count(), 2);
    QVERIFY(m_page->history()->canGoBack());

    evaluateJavaScriptSync(m_page, "window.history.replaceState(null, 'replace', 'another/location')");
    expectedUrlChangeCount++;
    QCOMPARE(urlChangedSpy.count(), expectedUrlChangeCount);
    QCOMPARE(m_page->url(), QUrl("qrc:/resources/navigate/to/another/location"));
    QCOMPARE(m_page->history()->count(), 2);
    QVERIFY(!m_page->history()->canGoForward());
    QVERIFY(m_page->history()->canGoBack());

    evaluateJavaScriptSync(m_page, "window.history.back()");
    QTest::qWait(100);
    expectedUrlChangeCount++;
    QCOMPARE(urlChangedSpy.count(), expectedUrlChangeCount);
    QCOMPARE(m_page->url(), QUrl("qrc:/resources/test1.html"));
    QVERIFY(m_page->history()->canGoForward());
    QVERIFY(!m_page->history()->canGoBack());
}

static inline QUrl extractBaseUrl(const QUrl& url)
{
    return url.resolved(QUrl());
}

void tst_QWebEnginePage::setUrlThenLoads_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QUrl>("baseUrl");

    QTest::newRow("resource file") << QUrl("qrc:/resources/test1.html") << extractBaseUrl(QUrl("qrc:/resources/test1.html"));
    QTest::newRow("base specified in HTML") << QUrl("data:text/html,<head><base href=\"http://different.base/\"></head>") << QUrl("http://different.base/");
}

void tst_QWebEnginePage::setUrlThenLoads()
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

    const QUrl urlToLoad1("qrc:/resources/test2.html");
    const QUrl urlToLoad2("qrc:/resources/test1.html");

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

void tst_QWebEnginePage::loadFinishedAfterNotFoundError()
{
    QWebEnginePage page;
    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));

    page.settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);
    page.setUrl(QUrl("http://non.existent/url"));
    QTRY_COMPARE(spy.count(), 1);

    page.settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, true);
    page.setUrl(QUrl("http://another.non.existent/url"));
    QTRY_COMPARE(spy.count(), 2);
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

void tst_QWebEnginePage::loadInSignalHandlers_data()
{
    QSKIP("FIXME: This crashes in content::WebContentsImpl::NavigateToEntry because of reentrancy. Should we require QueuedConnections or do it ourselves to support this?");

    QTest::addColumn<URLSetter::Type>("type");
    QTest::addColumn<URLSetter::Signal>("signal");
    QTest::addColumn<QUrl>("url");

    const QUrl validUrl("qrc:/resources/test2.html");
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

void tst_QWebEnginePage::loadInSignalHandlers()
{
    QFETCH(URLSetter::Type, type);
    QFETCH(URLSetter::Signal, signal);
    QFETCH(QUrl, url);

    const QUrl urlForSetter("qrc:/resources/test1.html");
    URLSetter setter(m_page, signal, type, urlForSetter);

    m_page->load(url);
    waitForSignal(&setter, SIGNAL(finished()));
    QCOMPARE(m_page->url(), urlForSetter);
}

void tst_QWebEnginePage::restoreHistory()
{
    QWebChannel *channel = new QWebChannel;
    QWebEnginePage *page = new QWebEnginePage;
    page->setWebChannel(channel);

    QWebEngineScript script;
    script.setName(QStringLiteral("script"));
    page->scripts().insert(script);

    QSignalSpy spy(page, SIGNAL(loadFinished(bool)));
    page->load(QUrl(QStringLiteral("qrc:/resources/test1.html")));
    QTRY_COMPARE(spy.count(), 1);

    QCOMPARE(page->webChannel(), channel);
    QVERIFY(page->scripts().contains(script));

    QByteArray data;
    QDataStream out(&data, QIODevice::ReadWrite);
    out << *page->history();
    QDataStream in(&data, QIODevice::ReadOnly);
    in >> *page->history();
    QTRY_COMPARE(spy.count(), 2);

    QCOMPARE(page->webChannel(), channel);
    QVERIFY(page->scripts().contains(script));

    delete page;
    delete channel;
}

void tst_QWebEnginePage::toPlainTextLoadFinishedRace_data()
{
    QTest::addColumn<bool>("enableErrorPage");
    QTest::newRow("disableErrorPage") << false;
    QTest::newRow("enableErrorPage") << true;
}

void tst_QWebEnginePage::toPlainTextLoadFinishedRace()
{
    QFETCH(bool, enableErrorPage);

    QWebEnginePage *page = new QWebEnginePage;
    page->settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, enableErrorPage);
    QSignalSpy spy(page, SIGNAL(loadFinished(bool)));

    page->load(QUrl("data:text/plain,foobarbaz"));
    QTRY_VERIFY(spy.count() == 1);
    QCOMPARE(toPlainTextSync(page), QString("foobarbaz"));

    page->load(QUrl("fail:unknown/scheme"));
    QTRY_VERIFY(spy.count() == 2);
    QString s = toPlainTextSync(page);
    QVERIFY(s.contains("foobarbaz") == !enableErrorPage);

    page->load(QUrl("data:text/plain,lalala"));
    QTRY_VERIFY(spy.count() == 3);
    QCOMPARE(toPlainTextSync(page), QString("lalala"));
    delete page;
    QVERIFY(spy.count() == 3);
}

void tst_QWebEnginePage::setZoomFactor()
{
    QWebEnginePage *page = new QWebEnginePage;

    QVERIFY(qFuzzyCompare(page->zoomFactor(), 1.0));
    page->setZoomFactor(2.5);
    QVERIFY(qFuzzyCompare(page->zoomFactor(), 2.5));

    const QUrl urlToLoad("qrc:/resources/test1.html");

    QSignalSpy finishedSpy(m_page, SIGNAL(loadFinished(bool)));
    m_page->setUrl(urlToLoad);
    QTRY_COMPARE(finishedSpy.count(), 1);
    QVERIFY(finishedSpy.at(0).first().toBool());
    QVERIFY(qFuzzyCompare(page->zoomFactor(), 2.5));

    page->setZoomFactor(5.5);
    QVERIFY(qFuzzyCompare(page->zoomFactor(), 2.5));

    page->setZoomFactor(0.1);
    QVERIFY(qFuzzyCompare(page->zoomFactor(), 2.5));

    delete page;
}

void tst_QWebEnginePage::printToPdf()
{
#if !defined(QWEBENGINEPAGE_PDFPRINTINGENABLED)
    QSKIP("QWEBENGINEPAGE_PDFPRINTINGENABLED");
#else
    QTemporaryDir tempDir(QDir::tempPath() + "/tst_qwebengineview-XXXXXX");
    QVERIFY(tempDir.isValid());
    QWebEnginePage page;
    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));
    page.load(QUrl("qrc:///resources/basic_printing_page.html"));
    QTRY_VERIFY(spy.count() == 1);

    QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF(0.0, 0.0, 0.0, 0.0));
    QString path = tempDir.path() + "/print_1_success.pdf";
    page.printToPdf(path, layout);
    QTRY_VERIFY(QFile::exists(path));

#if !defined(Q_OS_WIN)
    path = tempDir.path() + "/print_//2_failed.pdf";
#else
    path = tempDir.path() + "/print_|2_failed.pdf";
#endif
    page.printToPdf(path, QPageLayout());
    QTRY_VERIFY(!QFile::exists(path));

    CallbackSpy<QByteArray> successfulSpy;
    page.printToPdf(successfulSpy.ref(), layout);
    QVERIFY(successfulSpy.waitForResult().length() > 0);

    CallbackSpy<QByteArray> failedInvalidLayoutSpy;
    page.printToPdf(failedInvalidLayoutSpy.ref(), QPageLayout());
    QCOMPARE(failedInvalidLayoutSpy.waitForResult().length(), 0);
#endif
}

void tst_QWebEnginePage::mouseButtonTranslation()
{
    QWebEngineView *view = new QWebEngineView;

    QSignalSpy spy(view, SIGNAL(loadFinished(bool)));
    view->setHtml(QStringLiteral(
                      "<html><head><script>\
                           var lastEvent = { 'button' : -1 }; \
                           function saveLastEvent(event) { console.log(event); lastEvent = event; }; \
                      </script></head>\
                      <body>\
                      <div style=\"height:600px;\" onmousedown=\"saveLastEvent(event)\">\
                      </div>\
                      </body></html>"));
    view->show();
    QTest::qWaitForWindowExposed(view);
    QTRY_VERIFY(spy.count() == 1);

    QVERIFY(view->focusProxy() != nullptr);

    QMouseEvent evpres(QEvent::MouseButtonPress, view->rect().center(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QGuiApplication::sendEvent(view->focusProxy(), &evpres);

    QTRY_COMPARE(evaluateJavaScriptSync(view->page(), "lastEvent.button").toInt(), 0);
    QCOMPARE(evaluateJavaScriptSync(view->page(), "lastEvent.buttons").toInt(), 1);

    QMouseEvent evpres2(QEvent::MouseButtonPress, view->rect().center(), Qt::RightButton, Qt::LeftButton | Qt::RightButton, Qt::NoModifier);
    QGuiApplication::sendEvent(view->focusProxy(), &evpres2);

    QTRY_COMPARE(evaluateJavaScriptSync(view->page(), "lastEvent.button").toInt(), 2);
    QCOMPARE(evaluateJavaScriptSync(view->page(), "lastEvent.buttons").toInt(), 3);

    delete view;
}

QPoint tst_QWebEnginePage::elementCenter(QWebEnginePage *page, const QString &id)
{
    QVariantList rectList = evaluateJavaScriptSync(page,
            "(function(){"
            "var elem = document.getElementById('" + id + "');"
            "var rect = elem.getBoundingClientRect();"
            "return [(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2];"
            "})()").toList();

    if (rectList.count() != 2) {
        qWarning("elementCenter failed.");
        return QPoint();
    }

    return QPoint(rectList.at(0).toInt(), rectList.at(1).toInt());
}

void tst_QWebEnginePage::viewSource()
{
    TestPage page;
    QSignalSpy loadFinishedSpy(&page, SIGNAL(loadFinished(bool)));
    QSignalSpy windowCreatedSpy(&page, SIGNAL(windowCreated()));
    const QUrl url("qrc:/resources/test1.html");

    page.load(url);
    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QCOMPARE(page.title(), QStringLiteral("Test page 1"));
    QVERIFY(page.action(QWebEnginePage::ViewSource)->isEnabled());

    page.triggerAction(QWebEnginePage::ViewSource);
    QTRY_COMPARE(windowCreatedSpy.count(), 1);
    QCOMPARE(page.createdWindows.size(), 1);

    QTRY_COMPARE(page.createdWindows[0]->url().toString(), QStringLiteral("view-source:%1").arg(url.toString()));
    // The requested URL should not be about:blank if the qrc scheme is supported
    QTRY_COMPARE(page.createdWindows[0]->requestedUrl(), url);
    QTRY_COMPARE(page.createdWindows[0]->title(), QStringLiteral("view-source:%1").arg(url.toString()));
    QVERIFY(!page.createdWindows[0]->action(QWebEnginePage::ViewSource)->isEnabled());
}

void tst_QWebEnginePage::viewSourceURL_data()
{
    QTest::addColumn<QUrl>("userInputUrl");
    QTest::addColumn<bool>("loadSucceed");
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QUrl>("requestedUrl");
    QTest::addColumn<QString>("title");

    QTest::newRow("view-source:") << QUrl("view-source:") << true << QUrl("view-source:") << QUrl("about:blank") << QString("view-source:");
    QTest::newRow("view-source:about:blank") << QUrl("view-source:about:blank") << true << QUrl("view-source:about:blank") << QUrl("about:blank") << QString("view-source:about:blank");

    QString localFilePath = QString("%1qwebenginepage/resources/test1.html").arg(TESTS_SOURCE_DIR);
    QUrl testLocalUrl = QUrl(QString("view-source:%1").arg(QUrl::fromLocalFile(localFilePath).toString()));
    QUrl testLocalUrlWithoutScheme = QUrl(QString("view-source:%1").arg(localFilePath));
    QTest::newRow(testLocalUrl.toString().toStdString().c_str()) << testLocalUrl << true << testLocalUrl << QUrl::fromLocalFile(localFilePath) << QString("test1.html");
    QTest::newRow(testLocalUrlWithoutScheme.toString().toStdString().c_str()) << testLocalUrlWithoutScheme << true << testLocalUrl << QUrl::fromLocalFile(localFilePath) << QString("test1.html");

    QString resourcePath = QLatin1String("qrc:/resources/test1.html");
    QUrl testResourceUrl = QUrl(QString("view-source:%1").arg(resourcePath));
    QTest::newRow(testResourceUrl.toString().toStdString().c_str()) << testResourceUrl << true << testResourceUrl << QUrl(resourcePath) << testResourceUrl.toString();

    QTest::newRow("view-source:http://non.existent") << QUrl("view-source:non.existent") << false << QUrl("view-source:http://non.existent/") << QUrl("http://non.existent/") << QString("http://non.existent/ is not available");
    QTest::newRow("view-source:non.existent") << QUrl("view-source:non.existent") << false << QUrl("view-source:http://non.existent/") << QUrl("http://non.existent/") << QString("http://non.existent/ is not available");
}

void tst_QWebEnginePage::viewSourceURL()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QFETCH(QUrl, userInputUrl);
    QFETCH(bool, loadSucceed);
    QFETCH(QUrl, url);
    QFETCH(QUrl, requestedUrl);
    QFETCH(QString, title);

    QWebEnginePage *page = new QWebEnginePage;
    QSignalSpy loadFinishedSpy(page, SIGNAL(loadFinished(bool)));

    page->load(userInputUrl);
    QTRY_COMPARE(loadFinishedSpy.count(), 1);
    QList<QVariant> arguments = loadFinishedSpy.takeFirst();

    QCOMPARE(arguments.at(0).toBool(), loadSucceed);
    QCOMPARE(page->url(), url);
    QCOMPARE(page->requestedUrl(), requestedUrl);
    QCOMPARE(page->title(), title);
    QVERIFY(!page->action(QWebEnginePage::ViewSource)->isEnabled());

    delete page;
}

QTEST_MAIN(tst_QWebEnginePage)
#include "tst_qwebenginepage.moc"
