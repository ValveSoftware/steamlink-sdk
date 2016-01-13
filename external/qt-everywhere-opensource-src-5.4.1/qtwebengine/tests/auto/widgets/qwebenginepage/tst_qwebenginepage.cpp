/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
#include <QClipboard>
#include <QDir>
#include <QGraphicsWidget>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMimeDatabase>
#include <QPushButton>
#include <QStateMachine>
#include <QStyle>
#include <QtTest/QtTest>
#include <QTextCharFormat>
#include <private/qinputmethod_p.h>
#include <qnetworkcookiejar.h>
#include <qnetworkreply.h>
#include <qnetworkrequest.h>
#include <qpa/qplatforminputcontext.h>
#include <qwebenginehistory.h>
#include <qwebenginepage.h>
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

public Q_SLOTS:
    void init();
    void cleanup();
    void cleanupFiles();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void thirdPartyCookiePolicy();
    void contextMenuCopy();
    void contextMenuPopulatedOnce();
    void acceptNavigationRequest();
    void geolocationRequestJS();
    void loadFinished();
    void actionStates();
    void popupFormSubmission();
    void acceptNavigationRequestWithNewWindow();
    void userStyleSheet();
    void userStyleSheetFromLocalFileUrl();
    void userStyleSheetFromQrcUrl();
    void loadHtml5Video();
    void modified();
    void contextMenuCrash();
    void updatePositionDependentActionsCrash();
    void database();
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
    void frameAt();
    void requestCache();
    void loadCachedPage();
    void protectBindingsRuntimeObjectsFromCollector();
    void localURLSchemes();
    void testOptionalJSObjects();
    void testLocalStorageVisibility();
    void testEnablePersistentStorage();
    void consoleOutput();
    void inputMethods_data();
    void inputMethods();
    void inputMethodsTextFormat_data();
    void inputMethodsTextFormat();
    void defaultTextEncoding();
    void errorPageExtension();
    void errorPageExtensionInIFrames();
    void errorPageExtensionInFrameset();
    void errorPageExtensionLoadFinished();
    void userAgentApplicationName();
    void userAgentNewlineStripping();
    void undoActionHaveCustomText();
    void renderWidgetHostViewNotShowTopLevel();
    void getUserMediaRequest();

    void viewModes();

    void crashTests_LazyInitializationOfMainFrame();

    void screenshot_data();
    void screenshot();

#if defined(ENABLE_WEBGL) && ENABLE_WEBGL
    void acceleratedWebGLScreenshotWithoutView();
    void unacceleratedWebGLScreenshotWithoutView();
#endif

    void originatingObjectInNetworkRequests();
    void networkReplyParentDidntChange();
    void destroyQNAMBeforeAbortDoesntCrash();
    void testJSPrompt();
    void testStopScheduledPageRefresh();
    void findText();
    void findTextResult();
    void findTextSuccessiveShouldCallAllCallbacks();
    void supportedContentType();
    // [Qt] tst_QWebEnginePage::infiniteLoopJS() timeouts with DFG JIT
    // https://bugs.webkit.org/show_bug.cgi?id=79040
    // void infiniteLoopJS();
    void navigatorCookieEnabled();
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

private:
    QWebEngineView* m_view;
    QWebEnginePage* m_page;
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

void tst_QWebEnginePage::init()
{
    m_view = new QWebEngineView();
    m_page = m_view->page();
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
    cleanupFiles(); // In case there are old files from previous runs
}

void tst_QWebEnginePage::cleanupTestCase()
{
    cleanupFiles(); // Be nice
}

#if defined(QWEBENGINEPAGE_ACCEPTNAVIGATIONREQUEST)
class NavigationRequestOverride : public QWebEnginePage
{
public:
    NavigationRequestOverride(QWebEngineView* parent, bool initialValue) : QWebEnginePage(parent), m_acceptNavigationRequest(initialValue) {}

    bool m_acceptNavigationRequest;
protected:
    virtual bool acceptNavigationRequest(QWebEngineFrame* frame, const QNetworkRequest &request, QWebEnginePage::NavigationType type) {
        Q_UNUSED(frame);
        Q_UNUSED(request);
        Q_UNUSED(type);

        return m_acceptNavigationRequest;
    }
};
#endif

void tst_QWebEnginePage::acceptNavigationRequest()
{
#if !defined(QWEBENGINEPAGE_ACCEPTNAVIGATIONREQUEST)
    QSKIP("QWEBENGINEPAGE_ACCEPTNAVIGATIONREQUEST");
#else
    QSignalSpy loadSpy(m_view, SIGNAL(loadFinished(bool)));

    NavigationRequestOverride* newPage = new NavigationRequestOverride(m_view, false);
    m_view->setPage(newPage);

    m_view->setHtml(QString("<html><body><form name='tstform' action='data:text/html,foo'method='get'>"
                            "<input type='text'><input type='submit'></form></body></html>"), QUrl());
    QTRY_COMPARE(loadSpy.count(), 1);

    evaluateJavaScriptSync(m_view->page(), "tstform.submit();");

    newPage->m_acceptNavigationRequest = true;
    evaluateJavaScriptSync(m_view->page(), "tstform.submit();");
    QTRY_COMPARE(loadSpy.count(), 2);

    QCOMPARE(m_view->page()->toPlainText(), QString("foo?"));

    // Restore default page
    m_view->setPage(0);
#endif
}

#if defined(QWEBENGINEPAGE_SETFEATUREPERMISSION)
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
    void requestPermission(QWebEngineFrame* frame, QWebEnginePage::Feature feature)
    {
        if (m_allowGeolocation)
            setFeaturePermission(frame, feature, PermissionGrantedByUser);
        else
            setFeaturePermission(frame, feature, PermissionDeniedByUser);
    }

public:
    void setGeolocationPermission(bool allow)
    {
        m_allowGeolocation = allow;
    }

private:
    bool m_allowGeolocation;
};
#endif

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

void tst_QWebEnginePage::geolocationRequestJS()
{
#if !defined(QWEBENGINEPAGE_SETFEATUREPERMISSION)
    QSKIP("QWEBENGINEPAGE_SETFEATUREPERMISSION");
#else
    JSTestPage* newPage = new JSTestPage(m_view);

    if (evaluateJavaScriptSync(newPage, QLatin1String("!navigator.geolocation")).toBool()) {
        delete newPage;
        W_QSKIP("Geolocation is not supported.", SkipSingle);
    }

    connect(newPage, SIGNAL(featurePermissionRequested(QWebEngineFrame*, QWebEnginePage::Feature)),
            newPage, SLOT(requestPermission(QWebEngineFrame*, QWebEnginePage::Feature)));

    newPage->setGeolocationPermission(false);
    m_view->setPage(newPage);
    m_view->setHtml(QString("<html><body>test</body></html>"), QUrl());
    evaluateJavaScriptSync(m_view->page(), "var errorCode = 0; function error(err) { errorCode = err.code; } function success(pos) { } navigator.geolocation.getCurrentPosition(success, error)");
    QTest::qWait(2000);
    QVariant empty = evaluateJavaScriptSync(m_view->page(), "errorCode");

    QEXPECT_FAIL("", "https://bugs.webkit.org/show_bug.cgi?id=102235", Continue);
    QVERIFY(empty.type() == QVariant::Double && empty.toInt() != 0);

    newPage->setGeolocationPermission(true);
    evaluateJavaScriptSync(m_view->page(), "errorCode = 0; navigator.geolocation.getCurrentPosition(success, error);");
    empty = evaluateJavaScriptSync(m_view->page(), "errorCode");

    //http://dev.w3.org/geo/api/spec-source.html#position
    //PositionError: const unsigned short PERMISSION_DENIED = 1;
    QVERIFY(empty.type() == QVariant::Double && empty.toInt() != 1);
    delete newPage;
#endif
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
    QWebEnginePage* page = m_view->page();

    page->load(QUrl("qrc:///resources/script.html"));

    QAction* reloadAction = page->action(QWebEnginePage::Reload);
    QAction* stopAction = page->action(QWebEnginePage::Stop);

    QTRY_VERIFY(reloadAction->isEnabled());
    QTRY_VERIFY(!stopAction->isEnabled());
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

#if defined(QWEBENGINEPAGE_ACCEPTNAVIGATIONREQUEST)
    struct Navigation {
        QWebEngineFrame *frame;
        QNetworkRequest request;
        NavigationType type;
    };
    QList<Navigation> navigations;

    virtual bool acceptNavigationRequest(QWebEngineFrame* frame, const QNetworkRequest &request, NavigationType type) {
        Navigation n;
        n.frame = frame;
        n.request = request;
        n.type = type;
        navigations.append(n);
        return true;
    }
#endif

    QList<TestPage*> createdWindows;
    virtual QWebEnginePage* createWindow(WebWindowType) {
        TestPage* page = new TestPage(this);
        createdWindows.append(page);
        return page;
    }

    QRect requestedGeometry;
private Q_SLOTS:
    void slotGeometryChangeRequested(const QRect& geom) {
        requestedGeometry = geom;
    }
};

void tst_QWebEnginePage::popupFormSubmission()
{
    TestPage page;
    page.settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    page.setHtml("<form name=form1 method=get action='' target=myNewWin>"\
                                "<input type=hidden name=foo value='bar'>"\
                                "</form>");
    page.runJavaScript("window.open('', 'myNewWin', 'width=500,height=300,toolbar=0')");
    evaluateJavaScriptSync(&page, "document.form1.submit();");

    QTest::qWait(500);
    // The number of popup created should be one.
    QVERIFY(page.createdWindows.size() == 1);

    QString url = page.createdWindows.takeFirst()->url().toString();
    // Check if the form submission was OK.
    QEXPECT_FAIL("", "https://bugs.webkit.org/show_bug.cgi?id=118597", Continue);
    QVERIFY(url.contains("?foo=bar"));
}

void tst_QWebEnginePage::acceptNavigationRequestWithNewWindow()
{
#if !defined(QWEBENGINESETTINGS)
    QSKIP("QWEBENGINESETTINGS");
#else
    TestPage* page = new TestPage(m_view);
    page->settings()->setAttribute(QWebEngineSettings::LinksIncludedInFocusChain, true);
    m_page = page;
    m_view->setPage(m_page);

    m_view->setUrl(QString("data:text/html,<a href=\"data:text/html,Reached\" target=\"_blank\">Click me</a>"));
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    QFocusEvent fe(QEvent::FocusIn);
    m_page->event(&fe);

    QVERIFY(m_page->focusNextPrevChild(/*next*/ true));

    QKeyEvent keyEnter(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
    m_page->event(&keyEnter);

    QCOMPARE(page->navigations.count(), 2);

    TestPage::Navigation n = page->navigations.at(1);
    QVERIFY(!n.frame);
    QCOMPARE(n.request.url().toString(), QString("data:text/html,Reached"));
    QVERIFY(n.type == QWebEnginePage::NavigationTypeLinkClicked);

    QCOMPARE(page->createdWindows.count(), 1);
#endif
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

void tst_QWebEnginePage::viewModes()
{
#if !defined(QWEBENGINEPAGE_VIEW_MODES)
    QSKIP("QWEBENGINEPAGE_VIEW_MODES");
#else
    m_view->setHtml("<body></body>");
    m_page->setProperty("_q_viewMode", "minimized");

    QVariant empty = evaluateJavaScriptSync(m_page, "window.styleMedia.matchMedium(\"(-webengine-view-mode)\")");
    QVERIFY(empty.type() == QVariant::Bool && empty.toBool());

    QVariant minimized = evaluateJavaScriptSync(m_page, "window.styleMedia.matchMedium(\"(-webengine-view-mode: minimized)\")");
    QVERIFY(minimized.type() == QVariant::Bool && minimized.toBool());

    QVariant maximized = evaluateJavaScriptSync(m_page, "window.styleMedia.matchMedium(\"(-webengine-view-mode: maximized)\")");
    QVERIFY(maximized.type() == QVariant::Bool && !maximized.toBool());
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

void tst_QWebEnginePage::database()
{
#if !defined(QWEBENGINEDATABASE)
    QSKIP("QWEBENGINEDATABASE");
#else
    QString path = tmpDirPath();
    m_page->settings()->setOfflineStoragePath(path);
    QVERIFY(m_page->settings()->offlineStoragePath() == path);

    QWebEngineSettings::setOfflineStorageDefaultQuota(1024 * 1024);
    QVERIFY(QWebEngineSettings::offlineStorageDefaultQuota() == 1024 * 1024);

    m_page->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    m_page->settings()->setAttribute(QWebEngineSettings::OfflineStorageDatabaseEnabled, true);

    QString dbFileName = path + "Databases.db";

    if (QFile::exists(dbFileName))
        QFile::remove(dbFileName);

    qRegisterMetaType<QWebEngineFrame*>("QWebEngineFrame*");
    QSignalSpy spy(m_page, SIGNAL(databaseQuotaExceeded(QWebEngineFrame*,QString)));
    m_view->setHtml(QString("<html><head><script>var db; db=openDatabase('testdb', '1.0', 'test database API', 50000); </script></head><body><div></div></body></html>"), QUrl("http://www.myexample.com"));
    QTRY_COMPARE(spy.count(), 1);
    evaluateJavaScriptSync(m_page, "var db2; db2=openDatabase('testdb', '1.0', 'test database API', 50000);");
    QTRY_COMPARE(spy.count(),1);

    evaluateJavaScriptSync(m_page, "localStorage.test='This is a test for local storage';");
    m_view->setHtml(QString("<html><body id='b'>text</body></html>"), QUrl("http://www.myexample.com"));

    QVariant s1 = evaluateJavaScriptSync(m_page, "localStorage.test");
    QCOMPARE(s1.toString(), QString("This is a test for local storage"));

    evaluateJavaScriptSync(m_page, "sessionStorage.test='This is a test for session storage';");
    m_view->setHtml(QString("<html><body id='b'>text</body></html>"), QUrl("http://www.myexample.com"));
    QVariant s2 = evaluateJavaScriptSync(m_page, "sessionStorage.test");
    QCOMPARE(s2.toString(), QString("This is a test for session storage"));

    m_view->setHtml(QString("<html><head></head><body><div></div></body></html>"), QUrl("http://www.myexample.com"));
    evaluateJavaScriptSync(m_page, "var db3; db3=openDatabase('testdb', '1.0', 'test database API', 50000);db3.transaction(function(tx) { tx.executeSql('CREATE TABLE IF NOT EXISTS Test (text TEXT)', []); }, function(tx, result) { }, function(tx, error) { });");
    QTest::qWait(200);

    // Remove all databases.
    QWebEngineSecurityOrigin origin = m_page->mainFrame()->securityOrigin();
    QList<QWebEngineDatabase> dbs = origin.databases();
    for (int i = 0; i < dbs.count(); i++) {
        QString fileName = dbs[i].fileName();
        QVERIFY(QFile::exists(fileName));
        QWebEngineDatabase::removeDatabase(dbs[i]);
        QVERIFY(!QFile::exists(fileName));
    }
    QVERIFY(!origin.databases().size());
    // Remove removed test :-)
    QWebEngineDatabase::removeAllDatabases();
    QVERIFY(!origin.databases().size());
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
#if !defined(QWEBENGINEPAGE_EVALUATEJAVASCRIPT)
    QSKIP("QWEBENGINEPAGE_EVALUATEJAVASCRIPT");
#else
    CursorTrackedPage* page = new CursorTrackedPage;
    QString content("<html><body><p id=one>The quick brown fox</p>" \
        "<p id=two>jumps over the lazy dog</p>" \
        "<p>May the source<br/>be with you!</p></body></html>");
    page->setHtml(content);

    // these actions must exist
    QVERIFY(page->action(QWebEnginePage::SelectAll) != 0);
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
    QRegExp regExp(" style=\".*\"");
    regExp.setMinimal(true);
    QCOMPARE(page->selectedHtml().trimmed().replace(regExp, ""), QString::fromLatin1("<p id=\"one\">The quick brown fox</p>"));

    // Make sure hasSelection returns true, since there is selected text now...
    QCOMPARE(page->hasSelection(), true);

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

    delete page;
#endif
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

void tst_QWebEnginePage::requestCache()
{
#if !defined(ACCEPTNAVIGATIONREQUEST)
    QSKIP("ACCEPTNAVIGATIONREQUEST");
#else
    TestPage page;
    QSignalSpy loadSpy(&page, SIGNAL(loadFinished(bool)));

    page.setUrl(QString("data:text/html,<a href=\"data:text/html,Reached\" target=\"_blank\">Click me</a>"));
    QTRY_COMPARE(loadSpy.count(), 1);
    QTRY_COMPARE(page.navigations.count(), 1);

    page.setUrl(QString("data:text/html,<a href=\"data:text/html,Reached\" target=\"_blank\">Click me2</a>"));
    QTRY_COMPARE(loadSpy.count(), 2);
    QTRY_COMPARE(page.navigations.count(), 2);

    page.triggerAction(QWebEnginePage::Stop);
    QVERIFY(page.history()->canGoBack());
    page.triggerAction(QWebEnginePage::Back);

    QTRY_COMPARE(loadSpy.count(), 3);
    QTRY_COMPARE(page.navigations.count(), 3);
    QCOMPARE(page.navigations.at(0).request.attribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork).toInt(),
             (int)QNetworkRequest::PreferNetwork);
    QCOMPARE(page.navigations.at(1).request.attribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork).toInt(),
             (int)QNetworkRequest::PreferNetwork);
    QCOMPARE(page.navigations.at(2).request.attribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork).toInt(),
             (int)QNetworkRequest::PreferCache);
#endif
}

void tst_QWebEnginePage::loadCachedPage()
{
#if !defined(QWEBENGINESETTINGS)
    QSKIP("QWEBENGINESETTINGS");
#else
    TestPage page;
    QSignalSpy loadSpy(&page, SIGNAL(loadFinished(bool)));
    page.settings()->setMaximumPagesInCache(3);

    page.load(QUrl("data:text/html,This is first page"));

    QTRY_COMPARE(loadSpy.count(), 1);
    QTRY_COMPARE(page.navigations.count(), 1);

    QUrl firstPageUrl = page.url();
    page.load(QUrl("data:text/html,This is second page"));

    QTRY_COMPARE(loadSpy.count(), 2);
    QTRY_COMPARE(page.navigations.count(), 2);

    page.triggerAction(QWebEnginePage::Stop);
    QVERIFY(page.history()->canGoBack());

    QSignalSpy urlSpy(&page, SIGNAL(urlChanged(QUrl)));
    QVERIFY(urlSpy.isValid());

    page.triggerAction(QWebEnginePage::Back);
    ::waitForSignal(&page, SIGNAL(urlChanged(QUrl)));
    QCOMPARE(urlSpy.size(), 1);

    QList<QVariant> arguments1 = urlSpy.takeFirst();
    QCOMPARE(arguments1.at(0).toUrl(), firstPageUrl);
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

#if defined(QWEBENGINEFRAME)
void frameAtHelper(QWebEnginePage* webPage, QWebEngineFrame* webFrame, QPoint framePosition)
{
    if (!webFrame)
        return;

    framePosition += QPoint(webFrame->pos());
    QList<QWebEngineFrame*> children = webFrame->childFrames();
    for (int i = 0; i < children.size(); ++i) {
        if (children.at(i)->childFrames().size() > 0)
            frameAtHelper(webPage, children.at(i), framePosition);

        QRect frameRect(children.at(i)->pos() + framePosition, children.at(i)->geometry().size());
        QVERIFY(children.at(i) == webPage->frameAt(frameRect.topLeft()));
    }
}
#endif

void tst_QWebEnginePage::frameAt()
{
#if !defined(QWEBENGINEFRAME)
    QSKIP("QWEBENGINEFRAME");
#else
    QWebEngineView webView;
    QWebEnginePage* webPage = webView.page();
    QSignalSpy loadSpy(webPage, SIGNAL(loadFinished(bool)));
    QUrl url = QUrl("qrc:///resources/iframe.html");
    webPage->load(url);
    QTRY_COMPARE(loadSpy.count(), 1);
    frameAtHelper(webPage, webPage->mainFrame(), webPage->mainFrame()->pos());
#endif
}

void tst_QWebEnginePage::inputMethods_data()
{
    QTest::addColumn<QString>("viewType");
    QTest::newRow("QWebEngineView") << "QWebEngineView";
    QTest::newRow("QGraphicsWebView") << "QGraphicsWebView";
}

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

void tst_QWebEnginePage::inputMethodsTextFormat_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<int>("start");
    QTest::addColumn<int>("length");

    QTest::newRow("") << QString("") << 0 << 0;
    QTest::newRow("Q") << QString("Q") << 0 << 1;
    QTest::newRow("Qt") << QString("Qt") << 0 << 1;
    QTest::newRow("Qt") << QString("Qt") << 0 << 2;
    QTest::newRow("Qt") << QString("Qt") << 1 << 1;
    QTest::newRow("Qt ") << QString("Qt ") << 0 << 1;
    QTest::newRow("Qt ") << QString("Qt ") << 1 << 1;
    QTest::newRow("Qt ") << QString("Qt ") << 2 << 1;
    QTest::newRow("Qt ") << QString("Qt ") << 2 << -1;
    QTest::newRow("Qt ") << QString("Qt ") << -2 << 3;
    QTest::newRow("Qt ") << QString("Qt ") << 0 << 3;
    QTest::newRow("Qt by") << QString("Qt by") << 0 << 1;
    QTest::newRow("Qt by Nokia") << QString("Qt by Nokia") << 0 << 1;
}


void tst_QWebEnginePage::inputMethodsTextFormat()
{
#if !defined(QINPUTMETHODEVENT_TEXTFORMAT)
    QSKIP("QINPUTMETHODEVENT_TEXTFORMAT");
#else
    QWebEnginePage* page = new QWebEnginePage;
    QWebEngineView* view = new QWebEngineView;
    view->setPage(page);
    page->settings()->setFontFamily(QWebEngineSettings::SerifFont, "FooSerifFont");
    page->setHtml("<html><body>" \
                                            "<input type='text' id='input1' style='font-family: serif' value='' maxlength='20'/>");
    evaluateJavaScriptSync(page, "document.getElementById('input1').focus()");
    page->mainFrame()->setFocus();
    view->show();

    QFETCH(QString, string);
    QFETCH(int, start);
    QFETCH(int, length);

    QList<QInputMethodEvent::Attribute> attrs;
    QTextCharFormat format;
    format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    format.setUnderlineColor(Qt::red);
    attrs.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, start, length, format));
    QInputMethodEvent im(string, attrs);
    page->event(&im);

    QTest::qWait(1000);

    delete view;
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

void tst_QWebEnginePage::localURLSchemes()
{
#if !defined(QWEBENGINESECURITYORIGIN)
    QSKIP("QWEBENGINESECURITYORIGIN");
#else
    int i = QWebEngineSecurityOrigin::localSchemes().size();

    QWebEngineSecurityOrigin::removeLocalScheme("file");
    QTRY_COMPARE(QWebEngineSecurityOrigin::localSchemes().size(), i);
    QWebEngineSecurityOrigin::addLocalScheme("file");
    QTRY_COMPARE(QWebEngineSecurityOrigin::localSchemes().size(), i);

    QWebEngineSecurityOrigin::removeLocalScheme("qrc");
    QTRY_COMPARE(QWebEngineSecurityOrigin::localSchemes().size(), i - 1);
    QWebEngineSecurityOrigin::addLocalScheme("qrc");
    QTRY_COMPARE(QWebEngineSecurityOrigin::localSchemes().size(), i);

    QString myscheme = "myscheme";
    QWebEngineSecurityOrigin::addLocalScheme(myscheme);
    QTRY_COMPARE(QWebEngineSecurityOrigin::localSchemes().size(), i + 1);
    QVERIFY(QWebEngineSecurityOrigin::localSchemes().contains(myscheme));
    QWebEngineSecurityOrigin::removeLocalScheme(myscheme);
    QTRY_COMPARE(QWebEngineSecurityOrigin::localSchemes().size(), i);
    QWebEngineSecurityOrigin::removeLocalScheme(myscheme);
    QTRY_COMPARE(QWebEngineSecurityOrigin::localSchemes().size(), i);
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

void tst_QWebEnginePage::defaultTextEncoding()
{
    QString defaultCharset = evaluateJavaScriptSync(m_page, "document.defaultCharset").toString();
    QVERIFY(!defaultCharset.isEmpty());
    QCOMPARE(QWebEngineSettings::globalSettings()->defaultTextEncoding(), defaultCharset);

    m_page->settings()->setDefaultTextEncoding(QString("utf-8"));
    QCoreApplication::processEvents();
    QString charset = evaluateJavaScriptSync(m_page, "document.defaultCharset").toString();
    QCOMPARE(charset, QString("utf-8"));
    QCOMPARE(m_page->settings()->defaultTextEncoding(), charset);

    m_page->settings()->setDefaultTextEncoding(QString());
    QCoreApplication::processEvents();
    charset = evaluateJavaScriptSync(m_page, "document.defaultCharset").toString();
    QVERIFY(!charset.isEmpty());
    QCOMPARE(charset, defaultCharset);

    QWebEngineSettings::globalSettings()->setDefaultTextEncoding(QString("utf-8"));
    QCoreApplication::processEvents();
    charset = evaluateJavaScriptSync(m_page, "document.defaultCharset").toString();
    QCOMPARE(charset, QString("utf-8"));
    QCOMPARE(QWebEngineSettings::globalSettings()->defaultTextEncoding(), charset);
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

void tst_QWebEnginePage::errorPageExtensionInIFrames()
{
#if !defined(QWEBENGINEFRAME)
    QSKIP("QWEBENGINEFRAME");
#else
    ErrorPage page;
    m_view->setPage(&page);

    m_view->page()->load(QUrl(
        "data:text/html,"
        "<h1>h1</h1>"
        "<iframe src='data:text/html,<p/>p'></iframe>"
        "<iframe src='http://non.existent/url'></iframe>"));
    QSignalSpy spyLoadFinished(m_view, SIGNAL(loadFinished(bool)));
    QTRY_COMPARE(spyLoadFinished.count(), 1);

    QCOMPARE(page.mainFrame()->childFrames()[1]->toPlainText(), QString("error"));

    m_view->setPage(0);
#endif
}

void tst_QWebEnginePage::errorPageExtensionInFrameset()
{
#if !defined(QWEBENGINEFRAME)
    QSKIP("QWEBENGINEFRAME");
#else
    ErrorPage page;
    m_view->setPage(&page);

    m_view->load(QUrl("qrc:///resources/index.html"));

    QSignalSpy spyLoadFinished(m_view, SIGNAL(loadFinished(bool)));
    QTRY_COMPARE(spyLoadFinished.count(), 1);
    QCOMPARE(page.mainFrame()->childFrames().count(), 2);
    QCOMPARE(page.mainFrame()->childFrames()[1]->toPlainText(), QString("error"));

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

class FriendlyWebPage : public QWebEnginePage
{
public:
    friend class tst_QWebEnginePage;
};

void tst_QWebEnginePage::userAgentApplicationName()
{
#if !defined(QWEBENGINEPAGE_USERAGENTFORURL)
    QSKIP("QWEBENGINEPAGE_USERAGENTFORURL");
#else
    const QString oldApplicationName = QCoreApplication::applicationName();
    FriendlyWebPage page;

    const QString applicationNameMarker = QString::fromUtf8("StrangeName\342\210\236");
    QCoreApplication::setApplicationName(applicationNameMarker);
    QVERIFY(page.userAgentForUrl(QUrl()).contains(applicationNameMarker));

    QCoreApplication::setApplicationName(oldApplicationName);
#endif
}

class CustomUserAgentWebPage : public QWebEnginePage
{
public:
    static const QLatin1String filteredUserAgent;
protected:
    virtual QString userAgentForUrl(const QUrl& url) const
    {
        return QString("My User Agent\nX-New-Http-Header: Oh Noes!");
    }
};
const QLatin1String CustomUserAgentWebPage::filteredUserAgent("My User AgentX-New-Http-Header: Oh Noes!");

void tst_QWebEnginePage::userAgentNewlineStripping()
{
#if !defined(QWEBENGINEPAGE_USERAGENTFORURL)
    QSKIP("QWEBENGINEPAGE_USERAGENTFORURL");
#else
    CustomUserAgentWebPage page;
    page.setHtml("<html><body></body></html>");
    QCOMPARE(evaluateJavaScriptSync(&page, "navigator.userAgent").toString(), CustomUserAgentWebPage::filteredUserAgent);
#endif
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

void tst_QWebEnginePage::originatingObjectInNetworkRequests()
{
#if !defined(QWEBENGINEFRAME)
    QSKIP("QWEBENGINEFRAME");
#else
    TestNetworkManager* networkManager = new TestNetworkManager(m_page);
    m_page->setNetworkAccessManager(networkManager);
    networkManager->requests.clear();

    m_view->setHtml(QString("<frameset cols=\"25%,75%\"><frame src=\"data:text/html,"
                            "<head><meta http-equiv='refresh' content='1'></head>foo \">"
                            "<frame src=\"data:text/html,bar\"></frameset>"), QUrl());
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    QCOMPARE(networkManager->requests.count(), 2);

    QList<QWebEngineFrame*> childFrames = m_page->mainFrame()->childFrames();
    QEXPECT_FAIL("", "https://bugs.webkit.org/show_bug.cgi?id=118660", Continue);
    QCOMPARE(childFrames.count(), 2);

    for (int i = 0; i < 2; ++i)
        QVERIFY(qobject_cast<QWebEngineFrame*>(networkManager->requests.at(i).originatingObject()) == childFrames.at(i));
#endif
}

void tst_QWebEnginePage::networkReplyParentDidntChange()
{
#if !defined(QWEBENGINEPAGE_SETNETWORKACCESSMANAGER)
    QSKIP("QWEBENGINEPAGE_SETNETWORKACCESSMANAGER");
#else
    TestNetworkManager* networkManager = new TestNetworkManager(m_page);
    m_page->setNetworkAccessManager(networkManager);
    networkManager->requests.clear();

    // Trigger a load and check that pending QNetworkReplies haven't been reparented before returning to the event loop.
    m_view->load(QUrl("qrc:///resources/content.html"));

    QVERIFY(networkManager->requests.count() > 0);
    QVERIFY(networkManager->findChildren<QNetworkReply*>().size() > 0);
#endif
}

void tst_QWebEnginePage::destroyQNAMBeforeAbortDoesntCrash()
{
#if !defined(QWEBENGINEPAGE_SETNETWORKACCESSMANAGER)
    QSKIP("QWEBENGINEPAGE_SETNETWORKACCESSMANAGER");
#else
    QNetworkAccessManager* networkManager = new QNetworkAccessManager;
    m_page->setNetworkAccessManager(networkManager);

    m_view->load(QUrl("qrc:///resources/content.html"));
    delete networkManager;
    // This simulates what PingLoader does with its QNetworkReply when it times out.
    // PingLoader isn't attached to a QWebEnginePage and can be kept alive
    // for 60000 seconds (~16.7 hours) to then cancel its ResourceHandle.
    m_view->stop();
#endif
}

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
    m_view->setHtml(QString("<html><head></head><body><div>foo bar</div></body></html>"));
#if defined(QWEBENGINEPAGE_TRIGGERACTION_SELECTALL)
    m_page->triggerAction(QWebEnginePage::SelectAll);
    QVERIFY(!m_page->selectedText().isEmpty());
    QVERIFY(!m_page->selectedHtml().isEmpty());
#endif
    m_page->findText("");
    QVERIFY(m_page->selectedText().isEmpty());
#if defined(QWEBENGINEPAGE_SELECTEDHTML)
    QVERIFY(m_page->selectedHtml().isEmpty());
#endif
    QStringList words = (QStringList() << "foo" << "bar");
    foreach (QString subString, words) {
        m_page->findText(subString);
        QEXPECT_FAIL("", "Unsupported: findText only highlights and doesn't update the selection.", Continue);
        QCOMPARE(m_page->selectedText(), subString);
#if defined(QWEBENGINEPAGE_SELECTEDHTML)
        QVERIFY(m_page->selectedHtml().contains(subString));
#endif
        m_page->findText("");
        QVERIFY(m_page->selectedText().isEmpty());
#if defined(QWEBENGINEPAGE_SELECTEDHTML)
        QVERIFY(m_page->selectedHtml().isEmpty());
#endif
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

static QString getMimeTypeForExtension(const QString &ext)
{
    QMimeType mimeType = QMimeDatabase().mimeTypeForFile(QStringLiteral("filename.") + ext.toLower(), QMimeDatabase::MatchExtension);
    if (mimeType.isValid() && !mimeType.isDefault())
        return mimeType.name();

    return QString();
}

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


void tst_QWebEnginePage::navigatorCookieEnabled()
{
#if !defined(QWEBENGINEPAGE_NETWORKACCESSMANAGER)
    QSKIP("QWEBENGINEPAGE_NETWORKACCESSMANAGER");
#else
    m_page->networkAccessManager()->setCookieJar(0);
    QVERIFY(!m_page->networkAccessManager()->cookieJar());
    QVERIFY(!evaluateJavaScriptSync(m_page, "navigator.cookieEnabled").toBool());

    m_page->networkAccessManager()->setCookieJar(new QNetworkCookieJar());
    QVERIFY(m_page->networkAccessManager()->cookieJar());
    QVERIFY(evaluateJavaScriptSync(m_page, "navigator.cookieEnabled").toBool());
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
    waitForSignal(&loadSpy, SIGNAL(started()));
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
    page->load(QUrl("qrc:///resources/content.html"));

    QVERIFY(evaluateJavaScriptSync(page, QStringLiteral("!!navigator.webkitGetUserMedia")).toBool());
    evaluateJavaScriptSync(page, QStringLiteral("navigator.webkitGetUserMedia({audio: true}, function() {}, function(){})"));
    QTRY_VERIFY_WITH_TIMEOUT(page->gotFeatureRequest(QWebEnginePage::MediaAudioCapture), 100);
    // Might end up failing due to the lack of physical media devices deeper in the content layer, so the JS callback is not guaranteed to be called,
    // but at least we go through that code path, potentially uncovering failing assertions.
    page->acceptPendingRequest();

    page->runJavaScript(QStringLiteral("errorCallbackCalled = false;"));
    evaluateJavaScriptSync(page, QStringLiteral("navigator.webkitGetUserMedia({audio: true, video: true}, function() {}, function(){errorCallbackCalled = true;})"));
    QTRY_VERIFY_WITH_TIMEOUT(page->gotFeatureRequest(QWebEnginePage::MediaAudioVideoCapture), 100);
    page->rejectPendingRequest(); // Should always end up calling the error callback in JS.
    QTRY_VERIFY_WITH_TIMEOUT(evaluateJavaScriptSync(page, QStringLiteral("errorCallbackCalled;")).toBool(), 100);
    delete page;
}

void tst_QWebEnginePage::openWindowDefaultSize()
{
    TestPage page;
    QWebEngineView view;
    page.setView(&view);
    view.show();

    page.settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    // Open a default window.
    page.runJavaScript("window.open()");
    // Open a too small window.
    evaluateJavaScriptSync(&page, "window.open('', '', 'width=10,height=10')");

    QTest::qWait(500);
    // The number of popups created should be two.
    QVERIFY(page.createdWindows.size() == 2);

    QRect requestedGeometry = page.createdWindows[0]->requestedGeometry;
    // Check default size has been requested.
    QVERIFY(requestedGeometry.width() == 0);
    QVERIFY(requestedGeometry.height() == 0);

    requestedGeometry = page.createdWindows[1]->requestedGeometry;
    // Check minimum size has been requested.
    QVERIFY(requestedGeometry.width() == 100);
    QVERIFY(requestedGeometry.height() == 100);
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

class JavaScriptCallback
{
public:
    JavaScriptCallback() { }
    JavaScriptCallback(const QVariant& _expected) : expected(_expected) { }
    virtual void operator() (const QVariant& result) {
        QVERIFY(result.isValid());
        QCOMPARE(result, expected);
    }
private:
    QVariant expected;
};

class JavaScriptCallbackNull
{
public:
    virtual void operator() (const QVariant& result) {
        QVERIFY(result.isNull());
// FIXME: Returned null values are currently invalid QVariants.
//        QVERIFY(result.isValid());
    }
};

class JavaScriptCallbackUndefined
{
public:
    virtual void operator() (const QVariant& result) {
        QVERIFY(result.isNull());
        QVERIFY(!result.isValid());
    }
};

void tst_QWebEnginePage::runJavaScript()
{
    TestPage page;

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

    JavaScriptCallbackNull callbackUndefined;
    page.runJavaScript("undefined", QWebEngineCallback<const QVariant&>(callbackUndefined));

    QTest::qWait(100);
}

QTEST_MAIN(tst_QWebEnginePage)
#include "tst_qwebenginepage.moc"
