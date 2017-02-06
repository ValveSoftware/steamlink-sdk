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

#include "testwindow.h"
#include "util.h"

#include <QScopedPointer>
#include <QtCore/qelapsedtimer.h>
#include <QtGui/qpa/qwindowsysteminterface.h>
#include <QtQml/QQmlEngine>
#include <QtTest/QtTest>
#include <private/qquickwebengineview_p.h>

#include <functional>

class tst_QQuickWebEngineView : public QObject {
    Q_OBJECT
public:
    tst_QQuickWebEngineView();

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
    void showWebEngineView();
    void removeFromCanvas();
    void multipleWebEngineViewWindows();
    void multipleWebEngineViews();
    void titleUpdate();
    void transparentWebEngineViews();

    void inputMethod();
    void inputMethodHints();
    void basicRenderingSanity();
    void setZoomFactor();
    void printToPdf();
    void stopSettingFocusWhenDisabled();
    void stopSettingFocusWhenDisabled_data();
    void inputEventForwardingDisabledWhenActiveFocusOnPressDisabled();

    void changeLocale();

private:
    inline QQuickWebEngineView *newWebEngineView();
    inline QQuickWebEngineView *webEngineView() const;
    QUrl urlFromTestPath(const char *localFilePath);
    void runJavaScript(const QString &script);
    QString m_testSourceDirPath;
    QScopedPointer<TestWindow> m_window;
    QScopedPointer<QQmlComponent> m_component;
};

tst_QQuickWebEngineView::tst_QQuickWebEngineView()
{
    QtWebEngine::initialize();

    m_testSourceDirPath = QString::fromLocal8Bit(TESTS_SOURCE_DIR);
    if (!m_testSourceDirPath.endsWith(QLatin1Char('/')))
        m_testSourceDirPath.append(QLatin1Char('/'));

    static QQmlEngine *engine = new QQmlEngine(this);
    m_component.reset(new QQmlComponent(engine, this));
    m_component->setData(QByteArrayLiteral("import QtQuick 2.0\n"
                                           "import QtWebEngine 1.2\n"
                                           "WebEngineView {}")
                         , QUrl());
}

QQuickWebEngineView *tst_QQuickWebEngineView::newWebEngineView()
{
    QObject *viewInstance = m_component->create();
    QQuickWebEngineView *webEngineView = qobject_cast<QQuickWebEngineView*>(viewInstance);
    return webEngineView;
}

void tst_QQuickWebEngineView::init()
{
    m_window.reset(new TestWindow(newWebEngineView()));
}

void tst_QQuickWebEngineView::cleanup()
{
    m_window.reset();
}

inline QQuickWebEngineView *tst_QQuickWebEngineView::webEngineView() const
{
    return static_cast<QQuickWebEngineView*>(m_window->webEngineView.data());
}

QUrl tst_QQuickWebEngineView::urlFromTestPath(const char *localFilePath)
{
    return QUrl::fromLocalFile(m_testSourceDirPath + QString::fromUtf8(localFilePath));
}

void tst_QQuickWebEngineView::runJavaScript(const QString &script)
{
    webEngineView()->runJavaScript(script);
}

void tst_QQuickWebEngineView::navigationStatusAtStartup()
{
    QCOMPARE(webEngineView()->canGoBack(), false);

    QCOMPARE(webEngineView()->canGoForward(), false);

    QCOMPARE(webEngineView()->isLoading(), false);
}

void tst_QQuickWebEngineView::stopEnabledAfterLoadStarted()
{
    QCOMPARE(webEngineView()->isLoading(), false);

    LoadStartedCatcher catcher(webEngineView());
    webEngineView()->setUrl(urlFromTestPath("html/basic_page.html"));
    waitForSignal(&catcher, SIGNAL(finished()));

    QCOMPARE(webEngineView()->isLoading(), true);

    QVERIFY(waitForLoadSucceeded(webEngineView()));
}

void tst_QQuickWebEngineView::baseUrl()
{
    // Test the url is in a well defined state when instanciating the view, but before loading anything.
    QVERIFY(webEngineView()->url().isEmpty());
}

void tst_QQuickWebEngineView::loadEmptyUrl()
{
    webEngineView()->setUrl(QUrl());
    webEngineView()->setUrl(QUrl(QLatin1String("")));
}

void tst_QQuickWebEngineView::loadEmptyPageViewVisible()
{
    m_window->show();
    loadEmptyPageViewHidden();
}

void tst_QQuickWebEngineView::loadEmptyPageViewHidden()
{
    QSignalSpy loadSpy(webEngineView(), SIGNAL(loadingChanged(QQuickWebEngineLoadRequest*)));

    webEngineView()->setUrl(urlFromTestPath("html/basic_page.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView()));

    QCOMPARE(loadSpy.size(), 2);
}

void tst_QQuickWebEngineView::loadNonexistentFileUrl()
{
    QSignalSpy loadSpy(webEngineView(), SIGNAL(loadingChanged(QQuickWebEngineLoadRequest*)));

    webEngineView()->setUrl(urlFromTestPath("html/file_that_does_not_exist.html"));
    QVERIFY(waitForLoadFailed(webEngineView()));

    QCOMPARE(loadSpy.size(), 2);
}

void tst_QQuickWebEngineView::backAndForward()
{
    webEngineView()->setUrl(urlFromTestPath("html/basic_page.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView()));

    QCOMPARE(webEngineView()->url(), urlFromTestPath("html/basic_page.html"));

    webEngineView()->setUrl(urlFromTestPath("html/basic_page2.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView()));

    QCOMPARE(webEngineView()->url(), urlFromTestPath("html/basic_page2.html"));

    webEngineView()->goBack();
    QVERIFY(waitForLoadSucceeded(webEngineView()));

    QCOMPARE(webEngineView()->url(), urlFromTestPath("html/basic_page.html"));

    webEngineView()->goForward();
    QVERIFY(waitForLoadSucceeded(webEngineView()));

    QCOMPARE(webEngineView()->url(), urlFromTestPath("html/basic_page2.html"));
}

void tst_QQuickWebEngineView::reload()
{
    webEngineView()->setUrl(urlFromTestPath("html/basic_page.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView()));

    QCOMPARE(webEngineView()->url(), urlFromTestPath("html/basic_page.html"));

    webEngineView()->reload();
    QVERIFY(waitForLoadSucceeded(webEngineView()));

    QCOMPARE(webEngineView()->url(), urlFromTestPath("html/basic_page.html"));
}

void tst_QQuickWebEngineView::stop()
{
    webEngineView()->setUrl(urlFromTestPath("html/basic_page.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView()));

    QCOMPARE(webEngineView()->url(), urlFromTestPath("html/basic_page.html"));

    webEngineView()->stop();
}

void tst_QQuickWebEngineView::loadProgress()
{
    QCOMPARE(webEngineView()->loadProgress(), 0);

    webEngineView()->setUrl(urlFromTestPath("html/basic_page.html"));
    QSignalSpy loadProgressChangedSpy(webEngineView(), SIGNAL(loadProgressChanged()));
    QVERIFY(waitForLoadSucceeded(webEngineView()));

    loadProgressChangedSpy.wait();
    QTRY_COMPARE(webEngineView()->loadProgress(), 100);
}

void tst_QQuickWebEngineView::show()
{
    // This should not crash.
    m_window->show();
    QTest::qWait(200);
    m_window->hide();
}

void tst_QQuickWebEngineView::showWebEngineView()
{
    webEngineView()->setUrl(urlFromTestPath("html/direct-image-compositing.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView()));
    m_window->show();
    // This should not crash.
    webEngineView()->setVisible(true);
    QTest::qWait(200);
    webEngineView()->setVisible(false);
    QTest::qWait(200);
}

void tst_QQuickWebEngineView::removeFromCanvas()
{
    showWebEngineView();

    // This should not crash.
    QQuickItem *parent = webEngineView()->parentItem();
    QQuickItem noCanvasItem;
    webEngineView()->setParentItem(&noCanvasItem);
    QTest::qWait(200);
    webEngineView()->setParentItem(parent);
    webEngineView()->setVisible(true);
    QTest::qWait(200);
}

void tst_QQuickWebEngineView::multipleWebEngineViewWindows()
{
    showWebEngineView();

    // This should not crash.
    QQuickWebEngineView *webEngineView1 = newWebEngineView();
    QScopedPointer<TestWindow> window1(new TestWindow(webEngineView1));
    QQuickWebEngineView *webEngineView2 = newWebEngineView();
    QScopedPointer<TestWindow> window2(new TestWindow(webEngineView2));

    webEngineView1->setUrl(urlFromTestPath("html/scroll.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView1));
    window1->show();
    webEngineView1->setVisible(true);

    webEngineView2->setUrl(urlFromTestPath("html/basic_page.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView2));
    window2->show();
    webEngineView2->setVisible(true);
    QTest::qWait(200);
}

void tst_QQuickWebEngineView::multipleWebEngineViews()
{
    showWebEngineView();

    // This should not crash.
    QScopedPointer<QQuickWebEngineView> webEngineView1(newWebEngineView());
    webEngineView1->setParentItem(m_window->contentItem());
    QScopedPointer<QQuickWebEngineView> webEngineView2(newWebEngineView());
    webEngineView2->setParentItem(m_window->contentItem());

    webEngineView1->setSize(QSizeF(300, 400));
    webEngineView1->setUrl(urlFromTestPath("html/scroll.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView1.data()));
    webEngineView1->setVisible(true);

    webEngineView2->setSize(QSizeF(300, 400));
    webEngineView2->setUrl(urlFromTestPath("html/basic_page.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView2.data()));
    webEngineView2->setVisible(true);
    QTest::qWait(200);
}

QImage tryToGrabWindowUntil(QQuickWindow *window, std::function<bool(const QImage &)> checkImage,
                            int timeout = 5000)
{
    QImage grabbed;
    QElapsedTimer t;
    t.start();
    do {
        QTest::qWait(200);
        grabbed = window->grabWindow();
        if (checkImage(grabbed))
            break;
    } while (!t.hasExpired(timeout));
    return grabbed;
}

void tst_QQuickWebEngineView::basicRenderingSanity()
{
    showWebEngineView();

    webEngineView()->setUrl(QUrl(QString::fromUtf8("data:text/html,<html><body bgcolor=\"#00ff00\"></body></html>")));
    QVERIFY(waitForLoadSucceeded(webEngineView()));

    // This should not crash.
    webEngineView()->setVisible(true);

    QRgb testColor = qRgba(0, 0xff, 0, 0xff);
    const QImage grabbedWindow = tryToGrabWindowUntil(m_window.data(),
                                                      [testColor] (const QImage &image) {
        return image.pixel(10, 10) == testColor;
    });
    QVERIFY(grabbedWindow.pixel(10, 10) == testColor);
    QVERIFY(grabbedWindow.pixel(100, 10) == testColor);
    QVERIFY(grabbedWindow.pixel(10, 100) == testColor);
    QVERIFY(grabbedWindow.pixel(100, 100) == testColor);
}

void tst_QQuickWebEngineView::titleUpdate()
{
    QSignalSpy titleSpy(webEngineView(), SIGNAL(titleChanged()));

    // Load page with no title
    webEngineView()->setUrl(urlFromTestPath("html/basic_page2.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView()));
    QCOMPARE(titleSpy.size(), 1);

    titleSpy.clear();

    // No titleChanged signal for failed load
    webEngineView()->setUrl(urlFromTestPath("html/file_that_does_not_exist.html"));
    QVERIFY(waitForLoadFailed(webEngineView()));
    QCOMPARE(titleSpy.size(), 0);
}

void tst_QQuickWebEngineView::transparentWebEngineViews()
{
    showWebEngineView();

    // This should not crash.
    QScopedPointer<QQuickWebEngineView> webEngineView1(newWebEngineView());
    webEngineView1->setParentItem(m_window->contentItem());
    QScopedPointer<QQuickWebEngineView> webEngineView2(newWebEngineView());
    webEngineView2->setParentItem(m_window->contentItem());

    QVERIFY(webEngineView1->backgroundColor() != Qt::transparent);
    webEngineView2->setBackgroundColor(Qt::transparent);
    QVERIFY(webEngineView2->backgroundColor() == Qt::transparent);

    webEngineView1->setSize(QSizeF(300, 400));
    webEngineView1->loadHtml("<html><body bgcolor=\"red\"></body></html>");
    QVERIFY(waitForLoadSucceeded(webEngineView1.data()));
    webEngineView1->setVisible(true);

    webEngineView2->setSize(QSizeF(300, 400));
    webEngineView2->setUrl(urlFromTestPath("/html/basic_page.html"));
    QVERIFY(waitForLoadSucceeded(webEngineView2.data()));

    // Result image: black text on red background.
    const QImage grabbedWindow = tryToGrabWindowUntil(m_window.data(), [] (const QImage &image) {
        return image.pixelColor(0, 0) == QColor(Qt::red);
    });

    QSet<int> redComponents;
    for (int i = 0, width = grabbedWindow.width(); i < width; i++) {
        for (int j = 0, height = grabbedWindow.height(); j < height; j++) {
            QColor color(grabbedWindow.pixel(i, j));
            redComponents.insert(color.red());
            // There are no green or blue components between red and black.
            QVERIFY(color.green() == 0);
            QVERIFY(color.blue() == 0);
        }
    }

    QVERIFY(redComponents.count() > 1);
    QVERIFY(redComponents.contains(0));     // black
    QVERIFY(redComponents.contains(255));   // red
}

void tst_QQuickWebEngineView::inputMethod()
{
#if !defined(QQUICKWEBENGINEVIEW_ITEMACCEPTSINPUTMETHOD)
    QSKIP("QQUICKWEBENGINEVIEW_ITEMACCEPTSINPUTMETHOD");
#else
    QQuickWebEngineView *view = webEngineView();
    view->setUrl(urlFromTestPath("html/inputmethod.html"));
    QVERIFY(waitForLoadSucceeded(view));

    QVERIFY(!view->flags().testFlag(QQuickItem::ItemAcceptsInputMethod));
    runJavaScript("document.getElementById('inputField').focus();");
    QVERIFY(view->flags().testFlag(QQuickItem::ItemAcceptsInputMethod));
    runJavaScript("document.getElementById('inputField').blur();");
    QVERIFY(!view->flags().testFlag(QQuickItem::ItemAcceptsInputMethod));
#endif
}

void tst_QQuickWebEngineView::inputMethodHints()
{
#if !defined(QQUICKWEBENGINEVIEW_ITEMACCEPTSINPUTMETHOD)
    QSKIP("QQUICKWEBENGINEVIEW_ITEMACCEPTSINPUTMETHOD");
#else
    QQuickWebEngineView *view = webEngineView();

    view->setUrl(urlFromTestPath("html/inputmethod.html"));
    QVERIFY(waitForLoadSucceeded(view));

    // Setting focus on an input element results in an element in its shadow tree becoming the focus node.
    // Input hints should not be set from this shadow tree node but from the input element itself.
    runJavaScript("document.getElementById('emailInputField').focus();");
    QVERIFY(view->flags().testFlag(QQuickItem::ItemAcceptsInputMethod));
    QInputMethodQueryEvent query(Qt::ImHints);
    QGuiApplication::sendEvent(view, &query);
    Qt::InputMethodHints hints(query.value(Qt::ImHints).toUInt() & Qt::ImhExclusiveInputMask);
    QCOMPARE(hints, Qt::ImhEmailCharactersOnly);

    // The focus of an editable DIV is given directly to it, so no shadow root element
    // is necessary. This tests the WebPage::editorState() method ability to get the
    // right element without breaking.
    runJavaScript("document.getElementById('editableDiv').focus();");
    QVERIFY(view->flags().testFlag(QQuickItem::ItemAcceptsInputMethod));
    query = QInputMethodQueryEvent(Qt::ImHints);
    QGuiApplication::sendEvent(view, &query);
    hints = Qt::InputMethodHints(query.value(Qt::ImHints).toUInt());
    QCOMPARE(hints, Qt::ImhNone);
#endif
}

void tst_QQuickWebEngineView::setZoomFactor()
{
    QQuickWebEngineView *view = webEngineView();

    QVERIFY(qFuzzyCompare(view->zoomFactor(), 1.0));
    view->setZoomFactor(2.5);
    QVERIFY(qFuzzyCompare(view->zoomFactor(), 2.5));

    view->setUrl(urlFromTestPath("html/basic_page.html"));
    QVERIFY(waitForLoadSucceeded(view));
    QVERIFY(qFuzzyCompare(view->zoomFactor(), 2.5));

    view->setZoomFactor(0.1);
    QVERIFY(qFuzzyCompare(view->zoomFactor(), 2.5));

    view->setZoomFactor(5.5);
    QVERIFY(qFuzzyCompare(view->zoomFactor(), 2.5));
}

void tst_QQuickWebEngineView::printToPdf()
{
    QTemporaryDir tempDir(QDir::tempPath() + "/tst_qwebengineview-XXXXXX");
    QVERIFY(tempDir.isValid());
    QQuickWebEngineView *view = webEngineView();
    view->setUrl(urlFromTestPath("html/basic_page.html"));
    QVERIFY(waitForLoadSucceeded(view));

    QString path = tempDir.path() + "/print_success.pdf";
    view->printToPdf(path, QQuickWebEngineView::A4, QQuickWebEngineView::Portrait);
    QTRY_VERIFY(QFile::exists(path));

#if !defined(Q_OS_WIN)
    path = tempDir.path() + "/print_//fail.pdf";
#else
    path = tempDir.path() + "/print_|fail.pdf";
#endif // #if !defined(Q_OS_WIN)
    view->printToPdf(path, QQuickWebEngineView::A4, QQuickWebEngineView::Portrait);
    QTRY_VERIFY(!QFile::exists(path));
}

void tst_QQuickWebEngineView::stopSettingFocusWhenDisabled()
{
    QFETCH(bool, viewEnabled);
    QFETCH(bool, activeFocusResult);

    QQuickWebEngineView *view = webEngineView();
    m_window->show();
    view->setSize(QSizeF(640, 480));
    view->setEnabled(viewEnabled);
    view->loadHtml("<html><head><title>Title</title></head><body>Hello"
                   "<input id=\"input\" type=\"text\"></body></html>");
    QVERIFY(waitForLoadSucceeded(view));

    // When enabled, the view should get active focus after the page is loaded.
    QTRY_COMPARE_WITH_TIMEOUT(view->hasActiveFocus(), activeFocusResult, 1000);
    view->runJavaScript("document.getElementById(\"input\").focus()");
    QTRY_COMPARE_WITH_TIMEOUT(view->hasActiveFocus(), activeFocusResult, 1000);
}

void tst_QQuickWebEngineView::stopSettingFocusWhenDisabled_data()
{
    QTest::addColumn<bool>("viewEnabled");
    QTest::addColumn<bool>("activeFocusResult");

    QTest::newRow("enabled view gets active focus") << true << true;
    QTest::newRow("disabled view does not get active focus") << false << false;
}

class MouseTouchEventRecordingItem : public QQuickItem {
    Q_OBJECT
public:
    explicit MouseTouchEventRecordingItem(QQuickItem* child, QQuickItem *parent = 0) :
        QQuickItem(parent), m_eventCounter(0), m_child(child) {
        setFlag(ItemHasContents);
        setAcceptedMouseButtons(Qt::AllButtons);
        setAcceptHoverEvents(true);
    }

    bool event(QEvent *event) Q_DECL_OVERRIDE
    {
        switch (event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletRelease:
        case QEvent::TabletMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            ++m_eventCounter;
            event->accept();
            return true;
        default:
            break;
        }
        return QQuickItem::event(event);
    }

    void clearEventCount()
    {
        m_eventCounter = 0;
    }

    int eventCount()
    {
        return m_eventCounter;
    }

public Q_SLOTS:
    void changeWidth() {
        if (m_child)
            setWidth(m_child->width());
    }

    void changeHeight() {
        if (m_child)
            setHeight(m_child->height());
    }

private:
    int m_eventCounter;
    QQuickItem *m_child;
};

void tst_QQuickWebEngineView::inputEventForwardingDisabledWhenActiveFocusOnPressDisabled()
{
    QQuickWebEngineView *view = webEngineView();
    MouseTouchEventRecordingItem item(view);
    item.setParentItem(m_window->contentItem());

    // Resize the event recorder whenever the view is resized, so that all event positions
    // are contained in both of the item regions.
    QObject::connect(view, &QQuickItem::widthChanged, &item,
                     &MouseTouchEventRecordingItem::changeWidth);
    QObject::connect(view, &QQuickItem::heightChanged, &item,
                     &MouseTouchEventRecordingItem::changeHeight);
    view->setParentItem(&item);
    view->setSize(QSizeF(640, 480));
    m_window->show();

    // Simulate click and move of mouse, so that last known position in the application
    // is updated, thus a mouse move event is not generated when we don't expect it.
    QTest::mouseClick(view->window(), Qt::LeftButton);
    QTRY_COMPARE(item.eventCount(), 2);
    item.clearEventCount();

    // First disable view, so it does not receive focus on page load.
    view->setEnabled(false);
    view->setActiveFocusOnPress(false);
    view->loadHtml("<html><head>"
                   "<script>"
                   "window.onload = function() { document.getElementById(\"input\").focus(); }"
                   "</script>"
                   "<title>Title</title></head><body>Hello"
                   "<input id=\"input\" type=\"text\"></body></html>");
    QVERIFY(waitForLoadSucceeded(view));
    QTRY_COMPARE_WITH_TIMEOUT(view->hasActiveFocus(), false, 1000);

    // Enable the view back so we can try to interact with it.
    view->setEnabled(true);

    // Click on the view, to try and set focus.
    QTest::mouseClick(view->window(), Qt::LeftButton);

    // View should not have focus after click, because setActiveFocusOnPress is false.
    QTRY_COMPARE_WITH_TIMEOUT(view->hasActiveFocus(), false, 1000);

    // Now test sending various input events, to check that indeed all the input events are not
    // forwarded to Chromium, but rather processed and accepted by the view's parent item.
    QTest::mousePress(view->window(), Qt::LeftButton);
    QTest::mouseRelease(view->window(), Qt::LeftButton);

    QTouchDevice *device = new QTouchDevice;
    device->setType(QTouchDevice::TouchScreen);
    QWindowSystemInterface::registerTouchDevice(device);

    QTest::touchEvent(view->window(), device).press(0, QPoint(0,0), view->window());
    QTest::touchEvent(view->window(), device).move(0, QPoint(1, 1), view->window());
    QTest::touchEvent(view->window(), device).release(0, QPoint(1, 1), view->window());

    // We expect to catch 7 events - click = 2, press + release = 2, touches = 3.
    QCOMPARE(item.eventCount(), 7);

    // Manually forcing focus should still be possible.
    view->forceActiveFocus();
    QTRY_COMPARE_WITH_TIMEOUT(view->hasActiveFocus(), true, 1000);
}

void tst_QQuickWebEngineView::changeLocale()
{
    QUrl url("http://non.existent/");

    QLocale::setDefault(QLocale("de"));
    QQuickWebEngineView *viewDE = newWebEngineView();
    QSignalSpy titleSpyHU(viewDE, SIGNAL(titleChanged()));

    viewDE->setUrl(url);
    QVERIFY(waitForLoadFailed(viewDE));
    QTRY_COMPARE(titleSpyHU.size(), 2);
    QCOMPARE(viewDE->title(), QStringLiteral("Nicht verf\u00FCgbar: %1").arg(url.toString()));

    QLocale::setDefault(QLocale("en"));
    QQuickWebEngineView *viewEN = newWebEngineView();
    QSignalSpy titleSpyEN(viewEN, SIGNAL(titleChanged()));

    viewEN->setUrl(url);
    QVERIFY(waitForLoadFailed(viewEN));
    QTRY_COMPARE(titleSpyEN.size(), 2);
    QCOMPARE(viewEN->title(), QStringLiteral("%1 is not available").arg(url.toString()));

    viewDE->setUrl(QUrl("about:blank"));
    QVERIFY(waitForLoadSucceeded(viewDE));
    titleSpyHU.clear();

    viewDE->setUrl(url);
    QVERIFY(waitForLoadFailed(viewDE));
    QTRY_COMPARE(titleSpyHU.size(), 2);
    QCOMPARE(viewDE->title(), QStringLiteral("Nicht verf\u00FCgbar: %1").arg(url.toString()));

    delete viewDE;
    delete viewEN;
}

QTEST_MAIN(tst_QQuickWebEngineView)
#include "tst_qquickwebengineview.moc"
