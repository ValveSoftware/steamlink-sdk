/*
    Copyright (C) 2016 The Qt Company Ltd.
    Copyright (C) 2009 Torch Mobile Inc.
    Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>

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

#include <qtest.h>
#include "../util.h"

#include <qpainter.h>
#include <qpagelayout.h>
#include <qwebengineview.h>
#include <qwebenginepage.h>
#include <qwebenginesettings.h>
#include <qnetworkrequest.h>
#include <qdiriterator.h>
#include <qstackedlayout.h>
#include <qtemporarydir.h>
#include <QLineEdit>
#include <QHBoxLayout>

#define VERIFY_INPUTMETHOD_HINTS(actual, expect) \
    QVERIFY(actual == expect);

#define QTRY_COMPARE_WITH_TIMEOUT_FAIL_BLOCK(__expr, __expected, __timeout, __fail_block) \
do { \
    QTRY_IMPL(((__expr) == (__expected)), __timeout);\
    if (__expr != __expected)\
        __fail_block\
    QCOMPARE((__expr), __expected); \
} while (0)

class tst_QWebEngineView : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void renderingAfterMaxAndBack();
    void renderHints();
    void getWebKitVersion();

    void reusePage_data();
    void reusePage();
    void microFocusCoordinates();
    void focusInputTypes();
    void unhandledKeyEventPropagation();
    void horizontalScrollbarTest();

    void crashTests();
#if !(defined(WTF_USE_QT_MOBILE_THEME) && WTF_USE_QT_MOBILE_THEME)
    void setPalette_data();
    void setPalette();
#endif
    void doNotSendMouseKeyboardEventsWhenDisabled();
    void doNotSendMouseKeyboardEventsWhenDisabled_data();
    void stopSettingFocusWhenDisabled();
    void stopSettingFocusWhenDisabled_data();
    void focusOnNavigation_data();
    void focusOnNavigation();

    void changeLocale();
    void inputMethodsTextFormat_data();
    void inputMethodsTextFormat();
    void keyboardEvents();
};

// This will be called before the first test function is executed.
// It is only called once.
void tst_QWebEngineView::initTestCase()
{
}

// This will be called after the last test function is executed.
// It is only called once.
void tst_QWebEngineView::cleanupTestCase()
{
}

// This will be called before each test function is executed.
void tst_QWebEngineView::init()
{
}

// This will be called after every test function.
void tst_QWebEngineView::cleanup()
{
}

void tst_QWebEngineView::renderHints()
{
#if !defined(QWEBENGINEVIEW_RENDERHINTS)
    QSKIP("QWEBENGINEVIEW_RENDERHINTS");
#else
    QWebEngineView webView;

    // default is only text antialiasing + smooth pixmap transform
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(webView.renderHints() & QPainter::SmoothPixmapTransform);
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::Antialiasing, true);
    QVERIFY(webView.renderHints() & QPainter::Antialiasing);
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(webView.renderHints() & QPainter::SmoothPixmapTransform);
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::Antialiasing, false);
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(webView.renderHints() & QPainter::SmoothPixmapTransform);
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(webView.renderHints() & QPainter::SmoothPixmapTransform);
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::SmoothPixmapTransform, false);
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(webView.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));
#endif
}

void tst_QWebEngineView::getWebKitVersion()
{
#if !defined(QWEBENGINEVERSION)
    QSKIP("QWEBENGINEVERSION");
#else
    QVERIFY(qWebKitVersion().toDouble() > 0);
#endif
}

void tst_QWebEngineView::reusePage_data()
{
    QTest::addColumn<QString>("html");
    QTest::newRow("WithoutPlugin") << "<html><body id='b'>text</body></html>";
    QTest::newRow("WindowedPlugin") << QString("<html><body id='b'>text<embed src='resources/test.swf'></embed></body></html>");
    QTest::newRow("WindowlessPlugin") << QString("<html><body id='b'>text<embed src='resources/test.swf' wmode=\"transparent\"></embed></body></html>");
}

void tst_QWebEngineView::reusePage()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        W_QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QDir::setCurrent(TESTS_SOURCE_DIR);

    QFETCH(QString, html);
    QWebEngineView* view1 = new QWebEngineView;
    QPointer<QWebEnginePage> page = new QWebEnginePage;
    view1->setPage(page.data());
    page.data()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    page->setHtml(html, QUrl::fromLocalFile(TESTS_SOURCE_DIR));
    if (html.contains("</embed>")) {
        // some reasonable time for the PluginStream to feed test.swf to flash and start painting
        waitForSignal(view1, SIGNAL(loadFinished(bool)), 2000);
    }

    view1->show();
    QTest::qWaitForWindowExposed(view1);
    delete view1;
    QVERIFY(page != 0); // deleting view must not have deleted the page, since it's not a child of view

    QWebEngineView *view2 = new QWebEngineView;
    view2->setPage(page.data());
    view2->show(); // in Windowless mode, you should still be able to see the plugin here
    QTest::qWaitForWindowExposed(view2);
    delete view2;

    delete page.data(); // must not crash

    QDir::setCurrent(QApplication::applicationDirPath());
}

// Class used in crashTests
class WebViewCrashTest : public QObject {
    Q_OBJECT
    QWebEngineView* m_view;
public:
    bool m_executed;


    WebViewCrashTest(QWebEngineView* view)
      : m_view(view)
      , m_executed(false)
    {
        view->connect(view, SIGNAL(loadProgress(int)), this, SLOT(loading(int)));
    }

private Q_SLOTS:
    void loading(int progress)
    {
        if (progress > 1 && progress < 100) {
            QVERIFY(!m_executed);
            m_view->stop();
            m_executed = true;
        }
    }
};


// Should not crash.
void tst_QWebEngineView::crashTests()
{
    // Test if loading can be stopped in loadProgress handler without crash.
    // Test page should have frames.
    QWebEngineView view;
    WebViewCrashTest tester(&view);
    QUrl url("qrc:///resources/index.html");
    view.load(url);
    QTRY_VERIFY(tester.m_executed); // If fail it means that the test wasn't executed.
}

void tst_QWebEngineView::microFocusCoordinates()
{
#if !defined(QWEBENGINEPAGE_INPUTMETHODQUERY)
    QSKIP("QWEBENGINEPAGE_INPUTMETHODQUERY");
#else
    QWebEnginePage* page = new QWebEnginePage;
    QWebEngineView* webView = new QWebEngineView;
    webView->setPage( page );

    page->setHtml("<html><body>" \
        "<input type='text' id='input1' style='font--family: serif' value='' maxlength='20'/><br>" \
        "<canvas id='canvas1' width='500' height='500'></canvas>" \
        "<input type='password'/><br>" \
        "<canvas id='canvas2' width='500' height='500'></canvas>" \
        "</body></html>");

#if defined(QWEBENGINEFRAME)
    page->mainFrame()->setFocus();
#endif

    QVariant initialMicroFocus = page->inputMethodQuery(Qt::ImMicroFocus);
    QVERIFY(initialMicroFocus.isValid());

    page->scroll(0,50);

    QVariant currentMicroFocus = page->inputMethodQuery(Qt::ImMicroFocus);
    QVERIFY(currentMicroFocus.isValid());

    QCOMPARE(initialMicroFocus.toRect().translated(QPoint(0,-50)), currentMicroFocus.toRect());
#endif
}

void tst_QWebEngineView::focusInputTypes()
{
#if !defined(QWEBENGINEELEMENT)
    QSKIP("QWEBENGINEELEMENT");
#else
    QWebEngineView webView;
    webView.show();
    QTest::qWaitForWindowExposed(&webView);

    QUrl url("qrc:///resources/input_types.html");
    QWebEngineFrame* const mainFrame = webView.page()->mainFrame();
    webView.load(url);
    mainFrame->setFocus();

    QVERIFY(waitForSignal(&webView, SIGNAL(loadFinished(bool))));

    // 'text' type
    QWebEngineElement inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=text]"));
    QTest::mouseClick(&webView, Qt::LeftButton, 0, inputElement.geometry().center());
    QVERIFY(webView.inputMethodHints() == Qt::ImhNone);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=password]"));
    QTest::mouseClick(&webView, Qt::LeftButton, 0, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhHiddenText);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'tel' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=tel]"));
    QTest::mouseClick(&webView, Qt::LeftButton, 0, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhDialableCharactersOnly);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'number' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=number]"));
    QTest::mouseClick(&webView, Qt::LeftButton, 0, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhDigitsOnly);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'email' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=email]"));
    QTest::mouseClick(&webView, Qt::LeftButton, 0, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhEmailCharactersOnly);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'url' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=url]"));
    QTest::mouseClick(&webView, Qt::LeftButton, 0, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhUrlCharactersOnly);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=password]"));
    QTest::mouseClick(&webView, Qt::LeftButton, 0, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhHiddenText);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'text' type
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=text]"));
    QTest::mouseClick(&webView, Qt::LeftButton, 0, inputElement.geometry().center());
    QVERIFY(webView.inputMethodHints() == Qt::ImhNone);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=password]"));
    QTest::mouseClick(&webView, Qt::LeftButton, 0, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhHiddenText);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'text area' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("textarea"));
    QTest::mouseClick(&webView, Qt::LeftButton, 0, inputElement.geometry().center());
    QVERIFY(webView.inputMethodHints() == Qt::ImhNone);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));
#endif
}

class KeyEventRecordingWidget : public QWidget {
public:
    QList<QKeyEvent> pressEvents;
    QList<QKeyEvent> releaseEvents;
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE { pressEvents << *e; }
    void keyReleaseEvent(QKeyEvent *e) Q_DECL_OVERRIDE { releaseEvents << *e; }
};

void tst_QWebEngineView::unhandledKeyEventPropagation()
{
    KeyEventRecordingWidget parentWidget;
    QWebEngineView webView(&parentWidget);
    parentWidget.show();
    QTest::qWaitForWindowExposed(&webView);

    QSignalSpy loadSpy(&webView, SIGNAL(loadFinished(bool)));
    webView.setHtml("<input type='text'/>");
    QTRY_COMPARE(loadSpy.count(), 1);

    evaluateJavaScriptSync(webView.page(), "document.body.firstChild.focus()");

    QTest::sendKeyEvent(QTest::Press, webView.focusProxy(), Qt::Key_A, 'a', Qt::NoModifier);
    QTest::sendKeyEvent(QTest::Release, webView.focusProxy(), Qt::Key_A, 'a', Qt::NoModifier);
    QTest::sendKeyEvent(QTest::Press, webView.focusProxy(), Qt::Key_Left, QString(), Qt::NoModifier);
    QTest::sendKeyEvent(QTest::Release, webView.focusProxy(), Qt::Key_Left, QString(), Qt::NoModifier);
    QTest::sendKeyEvent(QTest::Press, webView.focusProxy(), Qt::Key_Left, QString(), Qt::NoModifier);
    QTest::sendKeyEvent(QTest::Release, webView.focusProxy(), Qt::Key_Left, QString(), Qt::NoModifier);

    // All this happens asychronously, wait for the last release event to know when we're done.
    QTRY_COMPARE(parentWidget.releaseEvents.size(), 3);

    // The page will consume the 'a' and the first left key presses, the second left won't be
    // used since the cursor will already be at the left end of the text input.
    // Key releases will all come back unconsumed.
    QCOMPARE(parentWidget.pressEvents.size(), 1);
    QCOMPARE(parentWidget.pressEvents[0].key(), (int)Qt::Key_Left);
    QCOMPARE(parentWidget.releaseEvents[0].key(), (int)Qt::Key_A);
    QCOMPARE(parentWidget.releaseEvents[1].key(), (int)Qt::Key_Left);
    QCOMPARE(parentWidget.releaseEvents[2].key(), (int)Qt::Key_Left);
}

void tst_QWebEngineView::horizontalScrollbarTest()
{
#if !defined(QWEBENGINEPAGE_SCROLL)
    QSKIP("QWEBENGINEPAGE_SCROLL");
#else
    QWebEngineView webView;
    webView.resize(600, 600);
    webView.show();
    QTest::qWaitForWindowExposed(&webView);

    QUrl url("qrc:///resources/scrolltest_page.html");
    webView.page()->load(url);
    webView.page()->setFocus();

    QVERIFY(waitForSignal(&webView, SIGNAL(loadFinished(bool))));

    QVERIFY(webView.page()->scrollPosition() == QPoint(0, 0));

    // Note: The test below assumes that the layout direction is Qt::LeftToRight.
    QTest::mouseClick(&webView, Qt::LeftButton, 0, QPoint(550, 595));
    QVERIFY(webView.page()->scrollPosition().x() > 0);

    // Note: The test below assumes that the layout direction is Qt::LeftToRight.
    QTest::mouseClick(&webView, Qt::LeftButton, 0, QPoint(20, 595));
    QVERIFY(webView.page()->scrollPosition() == QPoint(0, 0));
#endif
}


#if !(defined(WTF_USE_QT_MOBILE_THEME) && WTF_USE_QT_MOBILE_THEME)
void tst_QWebEngineView::setPalette_data()
{
    QTest::addColumn<bool>("active");
    QTest::addColumn<bool>("background");
    QTest::newRow("activeBG") << true << true;
    QTest::newRow("activeFG") << true << false;
    QTest::newRow("inactiveBG") << false << true;
    QTest::newRow("inactiveFG") << false << false;
}

// Render a QWebEngineView to a QImage twice, each time with a different palette set,
// verify that images rendered are not the same, confirming WebCore usage of
// custom palette on selections.
void tst_QWebEngineView::setPalette()
{
#if !defined(QWEBCONTENTVIEW_SETPALETTE)
    QSKIP("QWEBCONTENTVIEW_SETPALETTE");
#else
    QString html = "<html><head></head>"
                   "<body>"
                   "Some text here"
                   "</body>"
                   "</html>";

    QFETCH(bool, active);
    QFETCH(bool, background);

    QWidget* activeView = 0;

    // Use controlView to manage active/inactive state of test views by raising
    // or lowering their position in the window stack.
    QWebEngineView controlView;
    controlView.setHtml(html);

    QWebEngineView view1;

    QPalette palette1;
    QBrush brush1(Qt::red);
    brush1.setStyle(Qt::SolidPattern);
    if (active && background) {
        // Rendered image must have red background on an active QWebEngineView.
        palette1.setBrush(QPalette::Active, QPalette::Highlight, brush1);
    } else if (active && !background) {
        // Rendered image must have red foreground on an active QWebEngineView.
        palette1.setBrush(QPalette::Active, QPalette::HighlightedText, brush1);
    } else if (!active && background) {
        // Rendered image must have red background on an inactive QWebEngineView.
        palette1.setBrush(QPalette::Inactive, QPalette::Highlight, brush1);
    } else if (!active && !background) {
        // Rendered image must have red foreground on an inactive QWebEngineView.
        palette1.setBrush(QPalette::Inactive, QPalette::HighlightedText, brush1);
    }

    view1.setPalette(palette1);
    view1.setHtml(html);
    view1.page()->setViewportSize(view1.page()->contentsSize());
    view1.show();

    QTest::qWaitForWindowExposed(&view1);

    if (!active) {
        controlView.show();
        QTest::qWaitForWindowExposed(&controlView);
        activeView = &controlView;
        controlView.activateWindow();
    } else {
        view1.activateWindow();
        activeView = &view1;
    }

    QTRY_COMPARE(QApplication::activeWindow(), activeView);

    view1.page()->triggerAction(QWebEnginePage::SelectAll);

    QImage img1(view1.page()->viewportSize(), QImage::Format_ARGB32);
    QPainter painter1(&img1);
    view1.page()->render(&painter1);
    painter1.end();
    view1.close();
    controlView.close();

    QWebEngineView view2;

    QPalette palette2;
    QBrush brush2(Qt::blue);
    brush2.setStyle(Qt::SolidPattern);
    if (active && background) {
        // Rendered image must have blue background on an active QWebEngineView.
        palette2.setBrush(QPalette::Active, QPalette::Highlight, brush2);
    } else if (active && !background) {
        // Rendered image must have blue foreground on an active QWebEngineView.
        palette2.setBrush(QPalette::Active, QPalette::HighlightedText, brush2);
    } else if (!active && background) {
        // Rendered image must have blue background on an inactive QWebEngineView.
        palette2.setBrush(QPalette::Inactive, QPalette::Highlight, brush2);
    } else if (!active && !background) {
        // Rendered image must have blue foreground on an inactive QWebEngineView.
        palette2.setBrush(QPalette::Inactive, QPalette::HighlightedText, brush2);
    }

    view2.setPalette(palette2);
    view2.setHtml(html);
    view2.page()->setViewportSize(view2.page()->contentsSize());
    view2.show();

    QTest::qWaitForWindowExposed(&view2);

    if (!active) {
        controlView.show();
        QTest::qWaitForWindowExposed(&controlView);
        activeView = &controlView;
        controlView.activateWindow();
    } else {
        view2.activateWindow();
        activeView = &view2;
    }

    QTRY_COMPARE(QApplication::activeWindow(), activeView);

    view2.page()->triggerAction(QWebEnginePage::SelectAll);

    QImage img2(view2.page()->viewportSize(), QImage::Format_ARGB32);
    QPainter painter2(&img2);
    view2.page()->render(&painter2);
    painter2.end();

    view2.close();
    controlView.close();

    QVERIFY(img1 != img2);
#endif
}
#endif

void tst_QWebEngineView::renderingAfterMaxAndBack()
{
#if !defined(QWEBENGINEPAGE_RENDER)
    QSKIP("QWEBENGINEPAGE_RENDER");
#else
    QUrl url = QUrl("data:text/html,<html><head></head>"
                   "<body width=1024 height=768 bgcolor=red>"
                   "</body>"
                   "</html>");

    QWebEngineView view;
    view.page()->load(url);
    QVERIFY(waitForSignal(&view, SIGNAL(loadFinished(bool))));
    view.show();

    view.page()->settings()->setMaximumPagesInCache(3);

    QTest::qWaitForWindowExposed(&view);

    QPixmap reference(view.page()->viewportSize());
    reference.fill(Qt::red);

    QPixmap image(view.page()->viewportSize());
    QPainter painter(&image);
    view.page()->render(&painter);

    QCOMPARE(image, reference);

    QUrl url2 = QUrl("data:text/html,<html><head></head>"
                     "<body width=1024 height=768 bgcolor=blue>"
                     "</body>"
                     "</html>");
    view.page()->load(url2);

    QVERIFY(waitForSignal(&view, SIGNAL(loadFinished(bool))));

    view.showMaximized();

    QTest::qWaitForWindowExposed(&view);

    QPixmap reference2(view.page()->viewportSize());
    reference2.fill(Qt::blue);

    QPixmap image2(view.page()->viewportSize());
    QPainter painter2(&image2);
    view.page()->render(&painter2);

    QCOMPARE(image2, reference2);

    view.back();

    QPixmap reference3(view.page()->viewportSize());
    reference3.fill(Qt::red);
    QPixmap image3(view.page()->viewportSize());
    QPainter painter3(&image3);
    view.page()->render(&painter3);

    QCOMPARE(image3, reference3);
#endif
}

class KeyboardAndMouseEventRecordingWidget : public QWidget {
public:
    explicit KeyboardAndMouseEventRecordingWidget(QWidget *parent = 0) :
        QWidget(parent), m_eventCounter(0) {}

    bool event(QEvent *event) Q_DECL_OVERRIDE
    {
        QString eventString;
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
        case QEvent::ContextMenu:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
#ifndef QT_NO_WHEELEVENT
        case QEvent::Wheel:
#endif
            ++m_eventCounter;
            event->setAccepted(true);
            QDebug(&eventString) << event;
            m_eventHistory.append(eventString);
            return true;
        default:
            break;
        }
        return QWidget::event(event);
    }

    void clearEventCount()
    {
        m_eventCounter = 0;
    }

    int eventCount()
    {
        return m_eventCounter;
    }

    void printEventHistory()
    {
        qDebug() << "Received events are:";
        for (int i = 0; i < m_eventHistory.size(); ++i) {
            qDebug() << m_eventHistory[i];
        }
    }

private:
    int m_eventCounter;
    QVector<QString> m_eventHistory;
};

void tst_QWebEngineView::doNotSendMouseKeyboardEventsWhenDisabled()
{
    QFETCH(bool, viewEnabled);
    QFETCH(int, resultEventCount);

    KeyboardAndMouseEventRecordingWidget parentWidget;
    QWebEngineView webView(&parentWidget);
    webView.setEnabled(viewEnabled);
    parentWidget.setLayout(new QStackedLayout);
    parentWidget.layout()->addWidget(&webView);
    webView.resize(640, 480);
    parentWidget.show();
    QTest::qWaitForWindowExposed(&webView);

    QSignalSpy loadSpy(&webView, SIGNAL(loadFinished(bool)));
    webView.setHtml("<html><head><title>Title</title></head><body>Hello"
                    "<input id=\"input\" type=\"text\"></body></html>");
    QTRY_COMPARE(loadSpy.count(), 1);

    // When the webView is enabled, the events are swallowed by it, and the parent widget
    // does not receive any events, otherwise all events are processed by the parent widget.
    parentWidget.clearEventCount();
    QTest::mousePress(parentWidget.windowHandle(), Qt::LeftButton);
    QTest::mouseMove(parentWidget.windowHandle(), QPoint(100, 100));
    QTest::mouseRelease(parentWidget.windowHandle(), Qt::LeftButton,
                        Qt::KeyboardModifiers(), QPoint(100, 100));

    // Wait a bit for the mouse events to be processed, so they don't interfere with the js focus
    // below.
    QTest::qWait(100);
    evaluateJavaScriptSync(webView.page(), "document.getElementById(\"input\").focus()");
    QTest::keyPress(parentWidget.windowHandle(), Qt::Key_H);

    // Wait a bit for the key press to be handled. We have to do it, because the compare
    // below could immediately finish successfully, without alloing for the events to be handled.
    QTest::qWait(100);
    QTRY_COMPARE_WITH_TIMEOUT_FAIL_BLOCK(parentWidget.eventCount(), resultEventCount,
                                         1000, parentWidget.printEventHistory(););
}

void tst_QWebEngineView::doNotSendMouseKeyboardEventsWhenDisabled_data()
{
    QTest::addColumn<bool>("viewEnabled");
    QTest::addColumn<int>("resultEventCount");

    QTest::newRow("enabled view receives events") << true << 0;
    QTest::newRow("disabled view does not receive events") << false << 4;
}

void tst_QWebEngineView::stopSettingFocusWhenDisabled()
{
    QFETCH(bool, viewEnabled);
    QFETCH(bool, focusResult);

    QWebEngineView webView;
    webView.resize(640, 480);
    webView.show();
    webView.setEnabled(viewEnabled);
    QTest::qWaitForWindowExposed(&webView);

    QSignalSpy loadSpy(&webView, SIGNAL(loadFinished(bool)));
    webView.setHtml("<html><head><title>Title</title></head><body>Hello"
                    "<input id=\"input\" type=\"text\"></body></html>");
    QTRY_COMPARE(loadSpy.count(), 1);

    QTRY_COMPARE_WITH_TIMEOUT(webView.hasFocus(), focusResult, 1000);
    evaluateJavaScriptSync(webView.page(), "document.getElementById(\"input\").focus()");
    QTRY_COMPARE_WITH_TIMEOUT(webView.hasFocus(), focusResult, 1000);
}

void tst_QWebEngineView::stopSettingFocusWhenDisabled_data()
{
    QTest::addColumn<bool>("viewEnabled");
    QTest::addColumn<bool>("focusResult");

    QTest::newRow("enabled view gets focus") << true << true;
    QTest::newRow("disabled view does not get focus") << false << false;
}

void tst_QWebEngineView::focusOnNavigation_data()
{
    QTest::addColumn<bool>("focusOnNavigation");
    QTest::addColumn<bool>("viewReceivedFocus");
    QTest::newRow("focusOnNavigation true") << true << true;
    QTest::newRow("focusOnNavigation false") << false << false;
}

void tst_QWebEngineView::focusOnNavigation()
{
    QFETCH(bool, focusOnNavigation);
    QFETCH(bool, viewReceivedFocus);

#define triggerJavascriptFocus()\
    evaluateJavaScriptSync(webView->page(), "document.getElementById(\"input\").focus()");
#define loadAndTriggerFocusAndCompare()\
    QTRY_COMPARE(loadSpy.count(), 1);\
    triggerJavascriptFocus();\
    QTRY_COMPARE(webView->hasFocus(), viewReceivedFocus);

    // Create a container widget, that will hold a line edit that has initial focus, and a web
    // engine view.
    QScopedPointer<QWidget> containerWidget(new QWidget);
    QLineEdit *label = new QLineEdit;
    label->setText(QString::fromLatin1("Text"));
    label->setFocus();

    // Create the web view, and set its focusOnNavigation property.
    QWebEngineView *webView = new QWebEngineView;
    QWebEngineSettings *settings = webView->page()->settings();
    settings->setAttribute(QWebEngineSettings::FocusOnNavigationEnabled, focusOnNavigation);
    webView->resize(300, 300);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(label);
    layout->addWidget(webView);

    containerWidget->setLayout(layout);
    containerWidget->show();
    QTest::qWaitForWindowExposed(containerWidget.data());

    // Load the content, invoke javascript focus on the view, and check which widget has focus.
    QSignalSpy loadSpy(webView, SIGNAL(loadFinished(bool)));
    webView->setHtml("<html><head><title>Title</title></head><body>Hello"
                    "<input id=\"input\" type=\"text\"></body></html>");
    loadAndTriggerFocusAndCompare();

    // Load a different page, and check focus.
    loadSpy.clear();
    webView->setHtml("<html><head><title>Title</title></head><body>Hello 2"
                    "<input id=\"input\" type=\"text\"></body></html>");
    loadAndTriggerFocusAndCompare();

    // Navigate to previous page in history, check focus.
    loadSpy.clear();
    webView->triggerPageAction(QWebEnginePage::Back);
    loadAndTriggerFocusAndCompare();

    // Navigate to next page in history, check focus.
    loadSpy.clear();
    webView->triggerPageAction(QWebEnginePage::Forward);
    loadAndTriggerFocusAndCompare();

    // Reload page, check focus.
    loadSpy.clear();
    webView->triggerPageAction(QWebEnginePage::Reload);
    loadAndTriggerFocusAndCompare();

    // Reload page bypassing cache, check focus.
    loadSpy.clear();
    webView->triggerPageAction(QWebEnginePage::ReloadAndBypassCache);
    loadAndTriggerFocusAndCompare();

    // Manually forcing focus on web view should work.
    webView->setFocus();
    QTRY_COMPARE(webView->hasFocus(), true);

    // Clean up.
#undef loadAndTriggerFocusAndCompare
#undef triggerJavascriptFocus
}

void tst_QWebEngineView::changeLocale()
{
    QUrl url("http://non.existent/");

    QLocale::setDefault(QLocale("de"));
    QWebEngineView viewDE;
    viewDE.setUrl(url);

    QVERIFY(waitForSignal(&viewDE, SIGNAL(titleChanged(QString))));
    QVERIFY(waitForSignal(&viewDE, SIGNAL(loadFinished(bool))));
    QCOMPARE(viewDE.title(), QStringLiteral("Nicht verf\u00FCgbar: %1").arg(url.toString()));

    QLocale::setDefault(QLocale("en"));
    QWebEngineView viewEN;
    viewEN.setUrl(url);

    QVERIFY(waitForSignal(&viewEN, SIGNAL(titleChanged(QString))));
    QVERIFY(waitForSignal(&viewEN, SIGNAL(loadFinished(bool))));
    QCOMPARE(viewEN.title(), QStringLiteral("%1 is not available").arg(url.toString()));

    viewDE.setUrl(QUrl("about:blank"));
    QVERIFY(waitForSignal(&viewDE, SIGNAL(loadFinished(bool))));

    viewDE.setUrl(url);

    QVERIFY(waitForSignal(&viewDE, SIGNAL(titleChanged(QString))));
    QVERIFY(waitForSignal(&viewDE, SIGNAL(loadFinished(bool))));
    QCOMPARE(viewDE.title(), QStringLiteral("Nicht verf\u00FCgbar: %1").arg(url.toString()));
}

void tst_QWebEngineView::inputMethodsTextFormat_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<int>("start");
    QTest::addColumn<int>("length");
    QTest::addColumn<int>("underlineStyle");
    QTest::addColumn<QColor>("underlineColor");
    QTest::addColumn<QColor>("backgroundColor");

    QTest::newRow("") << QString("") << 0 << 0 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("Q") << QString("Q") << 0 << 1 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("Qt") << QString("Qt") << 0 << 1 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("Qt") << QString("Qt") << 0 << 2 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("Qt") << QString("Qt") << 1 << 1 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("Qt ") << QString("Qt ") << 0 << 1 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("Qt ") << QString("Qt ") << 1 << 1 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("Qt ") << QString("Qt ") << 2 << 1 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("Qt ") << QString("Qt ") << 2 << -1 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("Qt ") << QString("Qt ") << -2 << 3 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("Qt ") << QString("Qt ") << -1 << -1 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("Qt ") << QString("Qt ") << 0 << 3 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("The Qt") << QString("The Qt") << 0 << 1 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("The Qt Company") << QString("The Qt Company") << 0 << 1 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("red") << QColor();
    QTest::newRow("The Qt Company") << QString("The Qt Company") << 0 << 3 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("green") << QColor();
    QTest::newRow("The Qt Company") << QString("The Qt Company") << 4 << 2 << static_cast<int>(QTextCharFormat::SingleUnderline) << QColor("green") << QColor("red");
    QTest::newRow("The Qt Company") << QString("The Qt Company") << 7 << 7 << static_cast<int>(QTextCharFormat::NoUnderline) << QColor("green") << QColor("red");
    QTest::newRow("The Qt Company") << QString("The Qt Company") << 7 << 7 << static_cast<int>(QTextCharFormat::NoUnderline) << QColor() << QColor("red");
}


void tst_QWebEngineView::inputMethodsTextFormat()
{
    QWebEngineView view;
    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));

    view.setHtml("<html><body>"
                 " <input type='text' id='input1' style='font-family: serif' value='' maxlength='20'/>"
                 "</body></html>");
    QTRY_COMPARE(loadFinishedSpy.count(), 1);

    evaluateJavaScriptSync(view.page(), "document.getElementById('input1').focus()");
    view.show();

    QFETCH(QString, string);
    QFETCH(int, start);
    QFETCH(int, length);
    QFETCH(int, underlineStyle);
    QFETCH(QColor, underlineColor);
    QFETCH(QColor, backgroundColor);

    QList<QInputMethodEvent::Attribute> attrs;
    QTextCharFormat format;
    format.setUnderlineStyle(static_cast<QTextCharFormat::UnderlineStyle>(underlineStyle));
    format.setUnderlineColor(underlineColor);
    if (backgroundColor.isValid())
        format.setBackground(QBrush(backgroundColor));
    attrs.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, start, length, format));

    QInputMethodEvent im(string, attrs);
    QVERIFY(QApplication::sendEvent(view.focusProxy(), &im));
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString(), string);
}

void tst_QWebEngineView::keyboardEvents()
{
    QWebEngineView view;
    view.show();
    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.load(QUrl("qrc:///resources/keyboardEvents.html"));
    QVERIFY(loadFinishedSpy.wait());

    QStringList elements;
    elements << "first_div" << "second_div";
    elements << "text_input" << "radio1" << "checkbox1" << "checkbox2";
    elements << "number_input" << "range_input" << "search_input";
    elements << "submit_button" << "combobox" << "first_hyperlink" << "second_hyperlink";

    // Iterate over the elements of the test page with the Tab key. This tests whether any
    // element blocks the in-page navigation by Tab.
    for (const QString &elementId : elements) {
        QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.activeElement.id").toString(), elementId);
        QTest::keyPress(view.focusProxy(), Qt::Key_Tab);
    }

    // Move back to the radio buttons with the Shift+Tab key combination
    for (int i = 0; i < 10; ++i)
        QTest::keyPress(view.focusProxy(), Qt::Key_Tab, Qt::ShiftModifier);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.activeElement.id").toString(), QStringLiteral("radio2"));

    // Test the Space key by checking a radio button
    QVERIFY(!evaluateJavaScriptSync(view.page(), "document.getElementById('radio2').checked").toBool());
    QTest::keyClick(view.focusProxy(), Qt::Key_Space);
    QTRY_VERIFY(evaluateJavaScriptSync(view.page(), "document.getElementById('radio2').checked").toBool());

    // Test the Left key by switching the radio button
    QVERIFY(!evaluateJavaScriptSync(view.page(), "document.getElementById('radio1').checked").toBool());
    QTest::keyPress(view.focusProxy(), Qt::Key_Left);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.activeElement.id").toString(), QStringLiteral("radio1"));
    QVERIFY(!evaluateJavaScriptSync(view.page(), "document.getElementById('radio2').checked").toBool());
    QVERIFY(evaluateJavaScriptSync(view.page(), "document.getElementById('radio1').checked").toBool());

    // Test the Space key by unchecking a checkbox
    evaluateJavaScriptSync(view.page(), "document.getElementById('checkbox1').focus()");
    QVERIFY(evaluateJavaScriptSync(view.page(), "document.getElementById('checkbox1').checked").toBool());
    QTest::keyClick(view.focusProxy(), Qt::Key_Space);
    QTRY_VERIFY(!evaluateJavaScriptSync(view.page(), "document.getElementById('checkbox1').checked").toBool());

    // Test the Up and Down keys by changing the value of a spinbox
    evaluateJavaScriptSync(view.page(), "document.getElementById('number_input').focus()");
    QCOMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('number_input').value").toInt(), 5);
    QTest::keyPress(view.focusProxy(), Qt::Key_Up);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('number_input').value").toInt(), 6);
    QTest::keyPress(view.focusProxy(), Qt::Key_Down);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('number_input').value").toInt(), 5);

    // Test the Left, Right, Home, PageUp, End and PageDown keys by changing the value of a slider
    evaluateJavaScriptSync(view.page(), "document.getElementById('range_input').focus()");
    QCOMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('range_input').value").toString(), QStringLiteral("5"));
    QTest::keyPress(view.focusProxy(), Qt::Key_Left);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('range_input').value").toString(), QStringLiteral("4"));
    QTest::keyPress(view.focusProxy(), Qt::Key_Right);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('range_input').value").toString(), QStringLiteral("5"));
    QTest::keyPress(view.focusProxy(), Qt::Key_Home);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('range_input').value").toString(), QStringLiteral("0"));
    QTest::keyPress(view.focusProxy(), Qt::Key_PageUp);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('range_input').value").toString(), QStringLiteral("1"));
    QTest::keyPress(view.focusProxy(), Qt::Key_End);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('range_input').value").toString(), QStringLiteral("10"));
    QTest::keyPress(view.focusProxy(), Qt::Key_PageDown);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('range_input').value").toString(), QStringLiteral("9"));

    // Test the Escape key by removing the content of a search field
    evaluateJavaScriptSync(view.page(), "document.getElementById('search_input').focus()");
    QCOMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('search_input').value").toString(), QStringLiteral("test"));
    QTest::keyPress(view.focusProxy(), Qt::Key_Escape);
    QTRY_VERIFY(evaluateJavaScriptSync(view.page(), "document.getElementById('search_input').value").toString().isEmpty());

    // Test the alpha keys by changing the values in a combobox
    evaluateJavaScriptSync(view.page(), "document.getElementById('combobox').focus()");
    QCOMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('combobox').value").toString(), QStringLiteral("a"));
    QTest::keyPress(view.focusProxy(), Qt::Key_B);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('combobox').value").toString(), QStringLiteral("b"));
    // Must wait with the second key press to simulate selection of another element
    QTest::keyPress(view.focusProxy(), Qt::Key_C, Qt::NoModifier, 1000);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('combobox').value").toString(), QStringLiteral("c"));

    // Test the Enter key by loading a page with a hyperlink
    evaluateJavaScriptSync(view.page(), "document.getElementById('first_hyperlink').focus()");
    QTest::keyPress(view.focusProxy(), Qt::Key_Enter);
    QVERIFY(loadFinishedSpy.wait());
}

QTEST_MAIN(tst_QWebEngineView)
#include "tst_qwebengineview.moc"
