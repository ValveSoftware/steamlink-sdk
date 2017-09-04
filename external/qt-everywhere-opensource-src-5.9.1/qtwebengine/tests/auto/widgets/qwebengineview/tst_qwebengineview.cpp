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

#include <private/qinputmethod_p.h>
#include <qpainter.h>
#include <qpagelayout.h>
#include <qpa/qplatforminputcontext.h>
#include <qwebengineview.h>
#include <qwebenginepage.h>
#include <qwebenginesettings.h>
#include <qnetworkrequest.h>
#include <qdiriterator.h>
#include <qstackedlayout.h>
#include <qtemporarydir.h>
#include <QCompleter>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QQuickItem>
#include <QQuickWidget>
#include <QtWebEngineCore/qwebenginehttprequest.h>
#include <QTcpServer>
#include <QTcpSocket>
#include <QStyle>
#include <QtWidgets/qaction.h>

#define VERIFY_INPUTMETHOD_HINTS(actual, expect) \
    QVERIFY(actual == expect);

#define QTRY_COMPARE_WITH_TIMEOUT_FAIL_BLOCK(__expr, __expected, __timeout, __fail_block) \
do { \
    QTRY_IMPL(((__expr) == (__expected)), __timeout);\
    if (__expr != __expected)\
        __fail_block\
    QCOMPARE((__expr), __expected); \
} while (0)

static QPoint elementCenter(QWebEnginePage *page, const QString &id)
{
    const QString jsCode(
            "(function(){"
            "   var elem = document.getElementById('" + id + "');"
            "   var rect = elem.getBoundingClientRect();"
            "   return [(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2];"
            "})()");
    QVariantList rectList = evaluateJavaScriptSync(page, jsCode).toList();

    if (rectList.count() != 2) {
        qWarning("elementCenter failed.");
        return QPoint();
    }

    return QPoint(rectList.at(0).toInt(), rectList.at(1).toInt());
}

static QRect elementGeometry(QWebEnginePage *page, const QString &id)
{
    const QString jsCode(
                "(function() {"
                "   var elem = document.getElementById('" + id + "');"
                "   var rect = elem.getBoundingClientRect();"
                "   return [rect.left, rect.top, rect.right, rect.bottom];"
                "})()");
    QVariantList coords = evaluateJavaScriptSync(page, jsCode).toList();

    if (coords.count() != 4) {
        qWarning("elementGeometry faield.");
        return QRect();
    }

    return QRect(coords[0].toInt(), coords[1].toInt(), coords[2].toInt(), coords[3].toInt());
}


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
    void focusInternalRenderWidgetHostViewQuickItem();

    void changeLocale();
    void inputMethodsTextFormat_data();
    void inputMethodsTextFormat();
    void keyboardEvents();
    void keyboardFocusAfterPopup();
    void postData();
    void inputFieldOverridesShortcuts();

    void softwareInputPanel();
    void inputMethods();
    void textSelectionInInputField();
    void textSelectionOutOfInputField();
    void hiddenText();
    void emptyInputMethodEvent();
    void imeComposition();
    void imeCompositionQueryEvent_data();
    void imeCompositionQueryEvent();
    void newlineInTextarea();
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
        QSignalSpy spyFinished(view1, &QWebEngineView::loadFinished);
        QVERIFY(spyFinished.wait(2000));
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
    QWebEngineView webView;
    webView.show();
    QTest::qWaitForWindowExposed(&webView);

    QSignalSpy scrollSpy(webView.page(), SIGNAL(scrollPositionChanged(QPointF)));
    QSignalSpy loadFinishedSpy(&webView, SIGNAL(loadFinished(bool)));
    webView.page()->setHtml("<html><body>"
                            "<input type='text' id='input1' value='' maxlength='20'/><br>"
                            "<canvas id='canvas1' width='500' height='500'></canvas>"
                            "<input type='password'/><br>"
                            "<canvas id='canvas2' width='500' height='500'></canvas>"
                            "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    evaluateJavaScriptSync(webView.page(), "document.getElementById('input1').focus()");
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("input1"));

    QTRY_VERIFY(webView.focusProxy()->inputMethodQuery(Qt::ImMicroFocus).isValid());
    QVariant initialMicroFocus = webView.focusProxy()->inputMethodQuery(Qt::ImMicroFocus);

    evaluateJavaScriptSync(webView.page(), "window.scrollBy(0, 50)");
    QVERIFY(scrollSpy.wait());

    QTRY_VERIFY(webView.focusProxy()->inputMethodQuery(Qt::ImMicroFocus).isValid());
    QVariant currentMicroFocus = webView.focusProxy()->inputMethodQuery(Qt::ImMicroFocus);

    QCOMPARE(initialMicroFocus.toRect().translated(QPoint(0,-50)), currentMicroFocus.toRect());
}

void tst_QWebEngineView::focusInputTypes()
{
    QWebEngineView webView;
    webView.show();
    QTest::qWaitForWindowExposed(&webView);

    QSignalSpy loadFinishedSpy(&webView, SIGNAL(loadFinished(bool)));
    webView.load(QUrl("qrc:///resources/input_types.html"));
    QVERIFY(loadFinishedSpy.wait());

    // 'text' field
    QPoint textInputCenter = elementCenter(webView.page(), "textInput");
    QTest::mouseClick(webView.focusProxy(), Qt::LeftButton, 0, textInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("textInput"));
    VERIFY_INPUTMETHOD_HINTS(webView.focusProxy()->inputMethodHints(), Qt::ImhPreferLowercase);
    QVERIFY(webView.focusProxy()->testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    QPoint passwordInputCenter = elementCenter(webView.page(), "passwordInput");
    QTest::mouseClick(webView.focusProxy(), Qt::LeftButton, 0, passwordInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("passwordInput"));
    VERIFY_INPUTMETHOD_HINTS(webView.focusProxy()->inputMethodHints(), (Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase | Qt::ImhHiddenText));
    QVERIFY(webView.focusProxy()->testAttribute(Qt::WA_InputMethodEnabled));

    // 'tel' field
    QPoint telInputCenter = elementCenter(webView.page(), "telInput");
    QTest::mouseClick(webView.focusProxy(), Qt::LeftButton, 0, telInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("telInput"));
    VERIFY_INPUTMETHOD_HINTS(webView.focusProxy()->inputMethodHints(), Qt::ImhDialableCharactersOnly);
    QVERIFY(webView.focusProxy()->testAttribute(Qt::WA_InputMethodEnabled));

    // 'number' field
    QPoint numberInputCenter = elementCenter(webView.page(), "numberInput");
    QTest::mouseClick(webView.focusProxy(), Qt::LeftButton, 0, numberInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("numberInput"));
    VERIFY_INPUTMETHOD_HINTS(webView.focusProxy()->inputMethodHints(), Qt::ImhFormattedNumbersOnly);
    QVERIFY(webView.focusProxy()->testAttribute(Qt::WA_InputMethodEnabled));

    // 'email' field
    QPoint emailInputCenter = elementCenter(webView.page(), "emailInput");
    QTest::mouseClick(webView.focusProxy(), Qt::LeftButton, 0, emailInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("emailInput"));
    VERIFY_INPUTMETHOD_HINTS(webView.focusProxy()->inputMethodHints(), Qt::ImhEmailCharactersOnly);
    QVERIFY(webView.focusProxy()->testAttribute(Qt::WA_InputMethodEnabled));

    // 'url' field
    QPoint urlInputCenter = elementCenter(webView.page(), "urlInput");
    QTest::mouseClick(webView.focusProxy(), Qt::LeftButton, 0, urlInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("urlInput"));
    VERIFY_INPUTMETHOD_HINTS(webView.focusProxy()->inputMethodHints(), (Qt::ImhUrlCharactersOnly | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase));
    QVERIFY(webView.focusProxy()->testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    QTest::mouseClick(webView.focusProxy(), Qt::LeftButton, 0, passwordInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("passwordInput"));
    VERIFY_INPUTMETHOD_HINTS(webView.focusProxy()->inputMethodHints(), (Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase | Qt::ImhHiddenText));
    QVERIFY(webView.focusProxy()->testAttribute(Qt::WA_InputMethodEnabled));

    // 'text' type
    QTest::mouseClick(webView.focusProxy(), Qt::LeftButton, 0, textInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("textInput"));
    VERIFY_INPUTMETHOD_HINTS(webView.focusProxy()->inputMethodHints(), Qt::ImhPreferLowercase);
    QVERIFY(webView.focusProxy()->testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    QTest::mouseClick(webView.focusProxy(), Qt::LeftButton, 0, passwordInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("passwordInput"));
    VERIFY_INPUTMETHOD_HINTS(webView.focusProxy()->inputMethodHints(), (Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase | Qt::ImhHiddenText));
    QVERIFY(webView.focusProxy()->testAttribute(Qt::WA_InputMethodEnabled));

    // 'text area' field
    QPoint textAreaCenter = elementCenter(webView.page(), "textArea");
    QTest::mouseClick(webView.focusProxy(), Qt::LeftButton, 0, textAreaCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("textArea"));
    VERIFY_INPUTMETHOD_HINTS(webView.focusProxy()->inputMethodHints(), (Qt::ImhMultiLine | Qt::ImhPreferLowercase));
    QVERIFY(webView.focusProxy()->testAttribute(Qt::WA_InputMethodEnabled));
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

    QSignalSpy loadFinishedSpy(&webView, SIGNAL(loadFinished(bool)));
    webView.load(QUrl("qrc:///resources/keyboardEvents.html"));
    QVERIFY(loadFinishedSpy.wait());

    evaluateJavaScriptSync(webView.page(), "document.getElementById('first_div').focus()");
    QTRY_COMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("first_div"));

    QTest::sendKeyEvent(QTest::Press, webView.focusProxy(), Qt::Key_Right, QString(), Qt::NoModifier);
    QTest::sendKeyEvent(QTest::Release, webView.focusProxy(), Qt::Key_Right, QString(), Qt::NoModifier);
    // Right arrow key is unhandled thus focus is not changed
    QTRY_COMPARE(parentWidget.releaseEvents.size(), 1);
    QCOMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("first_div"));

    QTest::sendKeyEvent(QTest::Press, webView.focusProxy(), Qt::Key_Tab, QString(), Qt::NoModifier);
    QTest::sendKeyEvent(QTest::Release, webView.focusProxy(), Qt::Key_Tab, QString(), Qt::NoModifier);
    // Tab key is handled thus focus is changed
    QTRY_COMPARE(parentWidget.releaseEvents.size(), 2);
    QCOMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("second_div"));

    QTest::sendKeyEvent(QTest::Press, webView.focusProxy(), Qt::Key_Left, QString(), Qt::NoModifier);
    QTest::sendKeyEvent(QTest::Release, webView.focusProxy(), Qt::Key_Left, QString(), Qt::NoModifier);
    // Left arrow key is unhandled thus focus is not changed
    QTRY_COMPARE(parentWidget.releaseEvents.size(), 3);
    QCOMPARE(evaluateJavaScriptSync(webView.page(), "document.activeElement.id").toString(), QStringLiteral("second_div"));

    // The page will consume the Tab key to change focus between elements while the arrow
    // keys won't be used.
    QCOMPARE(parentWidget.pressEvents.size(), 2);
    QCOMPARE(parentWidget.pressEvents[0].key(), (int)Qt::Key_Right);
    QCOMPARE(parentWidget.pressEvents[1].key(), (int)Qt::Key_Left);

    // Key releases will all come back unconsumed.
    QCOMPARE(parentWidget.releaseEvents[0].key(), (int)Qt::Key_Right);
    QCOMPARE(parentWidget.releaseEvents[1].key(), (int)Qt::Key_Tab);
    QCOMPARE(parentWidget.releaseEvents[2].key(), (int)Qt::Key_Left);
}

void tst_QWebEngineView::horizontalScrollbarTest()
{
    QString html("<html><body>"
                 "<div style='width: 1000px; height: 1000px; background-color: green' />"
                 "</body></html>");

    QWebEngineView view;
    view.setFixedSize(600, 600);
    view.show();

    QTest::qWaitForWindowExposed(&view);

    QSignalSpy loadSpy(view.page(), SIGNAL(loadFinished(bool)));
    view.setHtml(html);
    QTRY_COMPARE(loadSpy.count(), 1);

    QVERIFY(view.page()->scrollPosition() == QPoint(0, 0));
    QSignalSpy scrollSpy(view.page(), SIGNAL(scrollPositionChanged(QPointF)));

    // Note: The test below assumes that the layout direction is Qt::LeftToRight.
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, QPoint(550, 595));
    scrollSpy.wait();
    QVERIFY(view.page()->scrollPosition().x() > 0);

    // Note: The test below assumes that the layout direction is Qt::LeftToRight.
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, QPoint(20, 595));
    scrollSpy.wait();
    QVERIFY(view.page()->scrollPosition() == QPoint(0, 0));
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
    QSignalSpy spyFinished(&view, &QWebEngineView::loadFinished);
    QVERIFY(spyFinished.wait());
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

    QVERIFY(spyFinished.wait());

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

void tst_QWebEngineView::focusInternalRenderWidgetHostViewQuickItem()
{
    // Create a container widget, that will hold a line edit that has initial focus, and a web
    // engine view.
    QScopedPointer<QWidget> containerWidget(new QWidget);
    QLineEdit *label = new QLineEdit;
    label->setText(QString::fromLatin1("Text"));
    label->setFocus();

    // Create the web view, and set its focusOnNavigation property to false, so it doesn't
    // get initial focus.
    QWebEngineView *webView = new QWebEngineView;
    QWebEngineSettings *settings = webView->page()->settings();
    settings->setAttribute(QWebEngineSettings::FocusOnNavigationEnabled, false);
    webView->resize(300, 300);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(label);
    layout->addWidget(webView);

    containerWidget->setLayout(layout);
    containerWidget->show();
    QTest::qWaitForWindowExposed(containerWidget.data());

    // Load the content, and check that focus is not set.
    QSignalSpy loadSpy(webView, SIGNAL(loadFinished(bool)));
    webView->setHtml("<html><head><title>Title</title></head><body>Hello"
                    "<input id=\"input\" type=\"text\"></body></html>");
    QTRY_COMPARE(loadSpy.count(), 1);
    QTRY_COMPARE(webView->hasFocus(), false);

    // Manually trigger focus.
    webView->setFocus();

    // Check that focus is set in QWebEngineView and all internal classes.
    QTRY_COMPARE(webView->hasFocus(), true);

    QQuickWidget *renderWidgetHostViewQtDelegateWidget =
            qobject_cast<QQuickWidget *>(webView->focusProxy());
    QVERIFY(renderWidgetHostViewQtDelegateWidget);
    QTRY_COMPARE(renderWidgetHostViewQtDelegateWidget->hasFocus(), true);

    QQuickItem *renderWidgetHostViewQuickItem =
            renderWidgetHostViewQtDelegateWidget->rootObject();
    QVERIFY(renderWidgetHostViewQuickItem);
    QTRY_COMPARE(renderWidgetHostViewQuickItem->hasFocus(), true);
}

void tst_QWebEngineView::changeLocale()
{
    QStringList errorLines;
    QUrl url("http://non.existent/");

    QLocale::setDefault(QLocale("de"));
    QWebEngineView viewDE;
    QSignalSpy loadFinishedSpyDE(&viewDE, SIGNAL(loadFinished(bool)));
    viewDE.load(url);
    QTRY_COMPARE_WITH_TIMEOUT(loadFinishedSpyDE.count(), 1, 12000);

    QTRY_VERIFY(!toPlainTextSync(viewDE.page()).isEmpty());
    errorLines = toPlainTextSync(viewDE.page()).split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
    QCOMPARE(errorLines.first().toUtf8(), QByteArrayLiteral("Diese Website ist nicht erreichbar"));

    QLocale::setDefault(QLocale("en"));
    QWebEngineView viewEN;
    QSignalSpy loadFinishedSpyEN(&viewEN, SIGNAL(loadFinished(bool)));
    viewEN.load(url);
    QTRY_COMPARE_WITH_TIMEOUT(loadFinishedSpyEN.count(), 1, 12000);

    QTRY_VERIFY(!toPlainTextSync(viewEN.page()).isEmpty());
    errorLines = toPlainTextSync(viewEN.page()).split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
    QCOMPARE(errorLines.first().toUtf8(), QByteArrayLiteral("This site can\xE2\x80\x99t be reached"));

    // Reset error page
    viewDE.load(QUrl("about:blank"));
    QVERIFY(loadFinishedSpyDE.wait());
    loadFinishedSpyDE.clear();

    // Check whether an existing QWebEngineView keeps the language settings after changing the default locale
    viewDE.load(url);
    QTRY_COMPARE_WITH_TIMEOUT(loadFinishedSpyDE.count(), 1, 12000);

    QTRY_VERIFY(!toPlainTextSync(viewDE.page()).isEmpty());
    errorLines = toPlainTextSync(viewDE.page()).split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
    QCOMPARE(errorLines.first().toUtf8(), QByteArrayLiteral("Diese Website ist nicht erreichbar"));
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

    // Setting background color is disabled for Qt WebEngine because some IME manager
    // sets background color to black and there is no API for setting the foreground color.
    // This may result black text on black background. However, we still test it to ensure
    // changing background color doesn't cause any crash.
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
    QTest::keyPress(view.focusProxy(), Qt::Key_C, Qt::NoModifier, 1100 /* blink::typeAheadTimeout + 0.1s */);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('combobox').value").toString(), QStringLiteral("c"));

    // Test the Enter key by loading a page with a hyperlink
    evaluateJavaScriptSync(view.page(), "document.getElementById('first_hyperlink').focus()");
    QTest::keyPress(view.focusProxy(), Qt::Key_Enter);
    QVERIFY(loadFinishedSpy.wait());
}

void tst_QWebEngineView::keyboardFocusAfterPopup()
{
    QScopedPointer<QWidget> containerWidget(new QWidget);

    QLineEdit *urlLine = new QLineEdit(containerWidget.data());
    QStringList urlList;
    urlList << "test";
    QCompleter *completer = new QCompleter(urlList, urlLine);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    urlLine->setCompleter(completer);
    urlLine->setFocus();

    QWebEngineView *webView = new QWebEngineView(containerWidget.data());
    QSignalSpy loadFinishedSpy(webView, SIGNAL(loadFinished(bool)));

    connect(urlLine, &QLineEdit::editingFinished, [=] {
        webView->setHtml("<html><body onload=\"document.getElementById('input1').focus()\">"
                         " <input type='text' id='input1' />"
                         "</body></html>");

        // Check whether the RenderWidgetHostView has the keyboard focus
        QQuickWidget *rwhv = qobject_cast<QQuickWidget *>(webView->focusProxy());
        QVERIFY(rwhv);
        QVERIFY(rwhv->hasFocus());
        QVERIFY(rwhv->rootObject()->hasFocus());
        QVERIFY(rwhv->window()->windowHandle()->isActive());
        QVERIFY(rwhv->rootObject()->hasActiveFocus());
    });

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(urlLine);
    layout->addWidget(webView);

    containerWidget->setLayout(layout);
    containerWidget->show();
    QTest::qWaitForWindowExposed(containerWidget.data());

    // Trigger completer's popup and select the first suggestion
    QTest::keyClick(urlLine, Qt::Key_T);
    qApp->processEvents();
    QTRY_VERIFY(qApp->activePopupWidget());
    QTest::keyClick(qApp->activePopupWidget(), Qt::Key_Down);
    qApp->processEvents();
    QTest::keyClick(qApp->activePopupWidget(), Qt::Key_Enter);
    qApp->processEvents();

    // After the load the focused window should forward the keyboard events to the webView
    QVERIFY(loadFinishedSpy.wait());
    // Wait for active focus on the input field
    QTRY_COMPARE(evaluateJavaScriptSync(webView->page(), "document.activeElement.id").toString(), QStringLiteral("input1"));
    QTest::keyClick(qApp->focusWindow(), Qt::Key_X);
    qApp->processEvents();
    QTRY_COMPARE(evaluateJavaScriptSync(webView->page(), "document.getElementById('input1').value").toString(), QStringLiteral("x"));
}

void tst_QWebEngineView::postData()
{
    QMap<QString, QString> postData;
    // use reserved characters to make the test harder to pass
    postData[QStringLiteral("Spä=m")] = QStringLiteral("ëgg:s");
    postData[QStringLiteral("foo\r\n")] = QStringLiteral("ba&r");

    QEventLoop eventloop;

    // Set up dummy "HTTP" server
    QTcpServer server;
    connect(&server, &QTcpServer::newConnection, this, [this, &server, &eventloop, &postData](){
        QTcpSocket* socket = server.nextPendingConnection();

        connect(socket, &QAbstractSocket::disconnected, this, [&eventloop](){
            eventloop.quit();
        });

        connect(socket, &QIODevice::readyRead, this, [this, socket, &server, &postData](){
            QByteArray rawData = socket->readAll();
            QStringList lines = QString::fromLocal8Bit(rawData).split("\r\n");

            // examine request
            QStringList request = lines[0].split(" ", QString::SkipEmptyParts);
            bool requestOk = request.length() > 2
                          && request[2].toUpper().startsWith("HTTP/")
                          && request[0].toUpper() == "POST"
                          && request[1] == "/";
            if (!requestOk) // POST and HTTP/... can be switched(?)
                requestOk =  request.length() > 2
                          && request[0].toUpper().startsWith("HTTP/")
                          && request[2].toUpper() == "POST"
                          && request[1] == "/";

            // examine headers
            int line = 1;
            bool headersOk = true;
            for (; headersOk && line < lines.length(); line++) {
                QStringList headerParts = lines[line].split(":");
                if (headerParts.length() < 2)
                    break;
                QString headerKey = headerParts[0].trimmed().toLower();
                QString headerValue = headerParts[1].trimmed().toLower();

                if (headerKey == "host")
                    headersOk = headersOk && (headerValue == "127.0.0.1")
                                          && (headerParts.length() == 3)
                                          && (headerParts[2].trimmed()
                                              == QString::number(server.serverPort()));
                if (headerKey == "content-type")
                    headersOk = headersOk && (headerValue == "application/x-www-form-urlencoded");
            }

            // examine body
            bool bodyOk = true;
            if (lines.length() == line+2) {
                QStringList postedFields = lines[line+1].split("&");
                QMap<QString, QString> postedData;
                for (int i = 0; bodyOk && i < postedFields.length(); i++) {
                    QStringList postedField = postedFields[i].split("=");
                    if (postedField.length() == 2)
                        postedData[QUrl::fromPercentEncoding(postedField[0].toLocal8Bit())]
                                 = QUrl::fromPercentEncoding(postedField[1].toLocal8Bit());
                    else
                        bodyOk = false;
                }
                bodyOk = bodyOk && (postedData == postData);
            } else { // no body at all or more than 1 line
                bodyOk = false;
            }

            // send response
            socket->write("HTTP/1.1 200 OK\r\n");
            socket->write("Content-Type: text/html\r\n");
            socket->write("Content-Length: 39\r\n\r\n");
            if (requestOk && headersOk && bodyOk)
                //             6     6     11         7      7      2 = 39 (Content-Length)
                socket->write("<html><body>Test Passed</body></html>\r\n");
            else
                socket->write("<html><body>Test Failed</body></html>\r\n");
            socket->flush();

            if (!requestOk || !headersOk || !bodyOk) {
                qDebug() << "Dummy HTTP Server: received request was not as expected";
                qDebug() << rawData;
                QVERIFY(requestOk); // one of them will yield useful output and make the test fail
                QVERIFY(headersOk);
                QVERIFY(bodyOk);
            }

            socket->close();
        });
    });
    if (!server.listen())
        QFAIL("Dummy HTTP Server: listen() failed");

    // Manual, hard coded client (commented out, but not removed - for reference and just in case)
    /*
    QTcpSocket client;
    connect(&client, &QIODevice::readyRead, this, [&client, &eventloop](){
        qDebug() << "Dummy HTTP client: data received";
        qDebug() << client.readAll();
        eventloop.quit();
    });
    connect(&client, &QAbstractSocket::connected, this, [&client](){
        client.write("HTTP/1.1 / GET\r\n\r\n");
    });
    client.connectToHost(QHostAddress::LocalHost, server.serverPort());
    */

    // send the POST request
    QWebEngineView view;
    QString sPort = QString::number(server.serverPort());
    view.load(QWebEngineHttpRequest::postRequest(QUrl("http://127.0.0.1:"+sPort), postData));

    // timeout after 10 seconds
    QTimer timeoutGuard(this);
    connect(&timeoutGuard, &QTimer::timeout, this, [&eventloop](){
        eventloop.quit();
        QFAIL("Dummy HTTP Server: waiting for data timed out");
    });
    timeoutGuard.setSingleShot(true);
    timeoutGuard.start(10000);

    // start the test
    eventloop.exec();

    timeoutGuard.stop();
    server.close();
}

void tst_QWebEngineView::inputFieldOverridesShortcuts()
{
    bool actionTriggered = false;
    QAction *action = new QAction;
    connect(action, &QAction::triggered, [&actionTriggered] () { actionTriggered = true; });

    QWebEngineView view;
    view.addAction(action);

    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml(QString("<html><body onload=\"input1=document.getElementById('input1')\">"
                         "<button id=\"btn1\" type=\"button\">push it real good</button>"
                         "<input id=\"input1\" type=\"text\" value=\"x\">"
                         "</body></html>"));
    QVERIFY(loadFinishedSpy.wait());

    view.show();
    QTest::qWaitForWindowActive(&view);

    auto inputFieldValue = [&view] () -> QString {
        return evaluateJavaScriptSync(view.page(),
                                      "input1.value").toString();
    };

    // The input form is not focused. The action is triggered on pressing Shift+Delete.
    action->setShortcut(Qt::SHIFT + Qt::Key_Delete);
    QTest::keyClick(view.windowHandle(), Qt::Key_Delete, Qt::ShiftModifier);
    QTRY_VERIFY(actionTriggered);
    QCOMPARE(inputFieldValue(), QString("x"));

    // The input form is not focused. The action is triggered on pressing X.
    action->setShortcut(Qt::Key_X);
    actionTriggered = false;
    QTest::keyClick(view.windowHandle(), Qt::Key_X);
    QTRY_VERIFY(actionTriggered);
    QCOMPARE(inputFieldValue(), QString("x"));

    // The input form is focused. The action is not triggered, and the form's text changed.
    evaluateJavaScriptSync(view.page(), "input1.focus();");
    actionTriggered = false;
    QTest::keyClick(view.windowHandle(), Qt::Key_Y);
    QTRY_COMPARE(inputFieldValue(), QString("yx"));
    QVERIFY(!actionTriggered);

    // The input form is focused. Make sure we don't override all short cuts.
    // A Ctrl-1 action is no default Qt key binding and should be triggerable.
    action->setShortcut(Qt::CTRL + Qt::Key_1);
    QTest::keyClick(view.windowHandle(), Qt::Key_1, Qt::ControlModifier);
    QTRY_VERIFY(actionTriggered);
    QCOMPARE(inputFieldValue(), QString("yx"));

    // Remove focus from the input field. A QKeySequence::Copy action must be triggerable.
    evaluateJavaScriptSync(view.page(), "document.getElementById('btn1').focus();");
    action->setShortcut(QKeySequence::Copy);
    actionTriggered = false;
    QTest::keyClick(view.windowHandle(), Qt::Key_C, Qt::ControlModifier);
    QTRY_VERIFY(actionTriggered);
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

void tst_QWebEngineView::softwareInputPanel()
{
    TestInputContext testContext;
    QWebEngineView view;
    view.show();

    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml("<html><body>"
                 "  <input type='text' id='input1' value='' size='50'/>"
                 "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    QPoint textInputCenter = elementCenter(view.page(), "input1");
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, textInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.activeElement.id").toString(), QStringLiteral("input1"));

    // This part of the test checks if the SIP (Software Input Panel) is triggered,
    // which normally happens on mobile platforms, when a user input form receives
    // a mouse click.
    int inputPanel = view.style()->styleHint(QStyle::SH_RequestSoftwareInputPanel);

    // For non-mobile platforms RequestSoftwareInputPanel event is not called
    // because there is no SIP (Software Input Panel) triggered. In the case of a
    // mobile platform, an input panel, e.g. virtual keyboard, is usually invoked
    // and the RequestSoftwareInputPanel event is called. For these two situations
    // this part of the test can verified as the checks below.
    if (inputPanel)
        QTRY_VERIFY(testContext.isInputPanelVisible());
    else
        QTRY_VERIFY(!testContext.isInputPanelVisible());
    testContext.hideInputPanel();

    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, textInputCenter);
    QTRY_VERIFY(testContext.isInputPanelVisible());

    view.setHtml("<html><body><p id='para'>nothing to input here</p></body></html>");
    QVERIFY(loadFinishedSpy.wait());
    testContext.hideInputPanel();

    QPoint paraCenter = elementCenter(view.page(), "para");
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, paraCenter);

    QVERIFY(!testContext.isInputPanelVisible());

    // Check sending RequestSoftwareInputPanel event
    view.page()->setHtml("<html><body>"
                         "  <input type='text' id='input1' value='QtWebEngine inputMethod'/>"
                         "  <div id='btnDiv' onclick='i=document.getElementById(&quot;input1&quot;); i.focus();'>abc</div>"
                         "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    QPoint btnDivCenter = elementCenter(view.page(), "btnDiv");
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, btnDivCenter);

    QVERIFY(!testContext.isInputPanelVisible());
}

void tst_QWebEngineView::inputMethods()
{
    QWebEngineView view;
    view.show();

    QSignalSpy selectionChangedSpy(&view, SIGNAL(selectionChanged()));
    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.settings()->setFontFamily(QWebEngineSettings::SerifFont, view.settings()->fontFamily(QWebEngineSettings::FixedFont));
    view.setHtml("<html><body>"
                 "  <input type='text' id='input1' style='font-family: serif' value='' maxlength='20' size='50'/>"
                 "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    QPoint textInputCenter = elementCenter(view.page(), "input1");
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, textInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.activeElement.id").toString(), QStringLiteral("input1"));

    // ImCursorRectangle
    QVariant variant = view.focusProxy()->inputMethodQuery(Qt::ImCursorRectangle);
    QVERIFY(elementGeometry(view.page(), "input1").contains(variant.toRect().topLeft()));

    // We assigned the serif font family to be the same as the fixed font family.
    // Then test ImFont on a serif styled element, we should get our fixed font family.
    variant = view.focusProxy()->inputMethodQuery(Qt::ImFont);
    QFont font = variant.value<QFont>();
    QEXPECT_FAIL("", "UNIMPLEMENTED: RenderWidgetHostViewQt::inputMethodQuery(Qt::ImFont)", Continue);
    QCOMPARE(view.settings()->fontFamily(QWebEngineSettings::FixedFont), font.family());

    QList<QInputMethodEvent::Attribute> inputAttributes;

    // Insert text
    {
        QString text = QStringLiteral("QtWebEngine");
        QInputMethodEvent eventText(text, inputAttributes);
        QApplication::sendEvent(view.focusProxy(), &eventText);
        QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString(), text);
        QCOMPARE(selectionChangedSpy.count(), 0);
    }

    {
        QString text = QStringLiteral("QtWebEngine");
        QInputMethodEvent eventText("", inputAttributes);
        eventText.setCommitString(text, 0, 0);
        QApplication::sendEvent(view.focusProxy(), &eventText);
        QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString(), text);
        QCOMPARE(selectionChangedSpy.count(), 0);
    }

    // ImMaximumTextLength
    QEXPECT_FAIL("", "UNIMPLEMENTED: RenderWidgetHostViewQt::inputMethodQuery(Qt::ImMaximumTextLength)", Continue);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImMaximumTextLength).toInt(), 20);

    // Set selection
    inputAttributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 3, 2, QVariant());
    QInputMethodEvent eventSelection1("", inputAttributes);

    QApplication::sendEvent(view.focusProxy(), &eventSelection1);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);

    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 3);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 5);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString("eb"));
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("QtWebEngine"));

    // Set selection with negative length
    inputAttributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 6, -5, QVariant());
    QInputMethodEvent eventSelection2("", inputAttributes);
    QApplication::sendEvent(view.focusProxy(), &eventSelection2);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 2);

    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 1);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 6);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString("tWebE"));
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("QtWebEngine"));

    QList<QInputMethodEvent::Attribute> attributes;
    // Clear the selection, so the next test does not clear any contents.
    QInputMethodEvent::Attribute newSelection(QInputMethodEvent::Selection, 0, 0, QVariant());
    attributes.append(newSelection);
    QInputMethodEvent eventComposition("composition", attributes);
    QApplication::sendEvent(view.focusProxy(), &eventComposition);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 3);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));

    // An ongoing composition should not change the surrounding text before it is committed.
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("QtWebEngine"));

    // Cancel current composition first
    inputAttributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 0, 0, QVariant());
    QInputMethodEvent eventSelection3("", inputAttributes);
    QApplication::sendEvent(view.focusProxy(), &eventSelection3);

    // Cancelling composition should not clear the surrounding text
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("QtWebEngine"));
}

void tst_QWebEngineView::textSelectionInInputField()
{
    QWebEngineView view;
    view.show();

    QSignalSpy selectionChangedSpy(&view, SIGNAL(selectionChanged()));
    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml("<html><body>"
                 "  <input type='text' id='input1' value='QtWebEngine' size='50'/>"
                 "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    // Tests for Selection when the Editor is NOT in Composition mode

    // LEFT to RIGHT selection
    // Mouse click event moves the current cursor to the end of the text
    QPoint textInputCenter = elementCenter(view.page(), "input1");
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, textInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.activeElement.id").toString(), QStringLiteral("input1"));
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 11);
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 11);
    // There was no selection to be changed by the click
    QCOMPARE(selectionChangedSpy.count(), 0);

    QList<QInputMethodEvent::Attribute> attributes;
    QInputMethodEvent event(QString(), attributes);
    event.setCommitString("XXX", 0, 0);
    QApplication::sendEvent(view.focusProxy(), &event);
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("QtWebEngineXXX"));

    event.setCommitString(QString(), -2, 2); // Erase two characters.
    QApplication::sendEvent(view.focusProxy(), &event);
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("QtWebEngineX"));

    event.setCommitString(QString(), -1, 1); // Erase one character.
    QApplication::sendEvent(view.focusProxy(), &event);
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("QtWebEngine"));

    // Move to the start of the line
    QTest::keyClick(view.focusProxy(), Qt::Key_Home);

    // Move 2 characters RIGHT
    for (int j = 0; j < 2; ++j)
        QTest::keyClick(view.focusProxy(), Qt::Key_Right);

    // Select to the end of the line
    QTest::keyClick(view.focusProxy(), Qt::Key_End, Qt::ShiftModifier);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);

    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 2);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 11);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString("WebEngine"));

    // RIGHT to LEFT selection
    // Deselect the selection (this moves the current cursor to the end of the text)
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, textInputCenter);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 2);

    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 11);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 11);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));

    // Move 2 characters LEFT
    for (int i = 0; i < 2; ++i)
        QTest::keyClick(view.focusProxy(), Qt::Key_Left);

    // Select to the start of the line
    QTest::keyClick(view.focusProxy(), Qt::Key_Home, Qt::ShiftModifier);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 3);

    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 9);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 0);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString("QtWebEngi"));
}

void tst_QWebEngineView::textSelectionOutOfInputField()
{
    QWebEngineView view;
    view.show();

    QSignalSpy selectionChangedSpy(&view, SIGNAL(selectionChanged()));
    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml("<html><body>"
                 "  This is a text"
                 "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    QCOMPARE(selectionChangedSpy.count(), 0);
    QVERIFY(!view.hasSelection());
    QVERIFY(view.page()->selectedText().isEmpty());

    // Simple click should not update text selection, however it updates selection bounds in Chromium
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, view.geometry().center());
    QCOMPARE(selectionChangedSpy.count(), 0);
    QVERIFY(!view.hasSelection());
    QVERIFY(view.page()->selectedText().isEmpty());

    // Workaround for macOS: press ctrl+a without key text
    QKeyEvent keyPressCtrlA(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
    QKeyEvent keyReleaseCtrlA(QEvent::KeyRelease, Qt::Key_A, Qt::ControlModifier);

    // Select text by ctrl+a
    QApplication::sendEvent(view.focusProxy(), &keyPressCtrlA);
    QApplication::sendEvent(view.focusProxy(), &keyReleaseCtrlA);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QVERIFY(view.hasSelection());
    QCOMPARE(view.page()->selectedText(), QString("This is a text"));

    // Deselect text by mouse click
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, view.geometry().center());
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 2);
    QVERIFY(!view.hasSelection());
    QVERIFY(view.page()->selectedText().isEmpty());

    selectionChangedSpy.clear();
    view.setHtml("<html><body>"
                 "  This is a text"
                 "  <br>"
                 "  <input type='text' id='input1' value='QtWebEngine' size='50'/>"
                 "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    QCOMPARE(selectionChangedSpy.count(), 0);
    QVERIFY(!view.hasSelection());
    QVERIFY(view.page()->selectedText().isEmpty());

    // Make sure the input field does not have the focus
    evaluateJavaScriptSync(view.page(), "document.getElementById('input1').blur()");
    QTRY_VERIFY(evaluateJavaScriptSync(view.page(), "document.activeElement.id").toString().isEmpty());

    // Select the whole page by ctrl+a
    QApplication::sendEvent(view.focusProxy(), &keyPressCtrlA);
    QApplication::sendEvent(view.focusProxy(), &keyReleaseCtrlA);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QVERIFY(view.hasSelection());
    QVERIFY(view.page()->selectedText().startsWith(QString("This is a text")));

    // Remove selection by clicking into an input field
    QPoint textInputCenter = elementCenter(view.page(), "input1");
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, textInputCenter);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(evaluateJavaScriptSync(view.page(), "document.activeElement.id").toString(), QStringLiteral("input1"));
    QCOMPARE(selectionChangedSpy.count(), 2);
    QVERIFY(!view.hasSelection());
    QVERIFY(view.page()->selectedText().isEmpty());

    // Select the content of the input field by ctrl+a
    QApplication::sendEvent(view.focusProxy(), &keyPressCtrlA);
    QApplication::sendEvent(view.focusProxy(), &keyReleaseCtrlA);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 3);
    QVERIFY(view.hasSelection());
    QCOMPARE(view.page()->selectedText(), QString("QtWebEngine"));

    // Deselect input field's text by mouse click
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, view.geometry().center());
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 4);
    QVERIFY(!view.hasSelection());
    QVERIFY(view.page()->selectedText().isEmpty());
}

void tst_QWebEngineView::hiddenText()
{
    QWebEngineView view;
    view.show();

    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml("<html><body>"
                 "  <input type='text' id='input1' value='QtWebEngine' size='50'/><br>"
                 "  <input type='password' id='password1'/>"
                 "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    QPoint passwordInputCenter = elementCenter(view.page(), "password1");
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, passwordInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.activeElement.id").toString(), QStringLiteral("password1"));

    QVERIFY(view.focusProxy()->testAttribute(Qt::WA_InputMethodEnabled));
    QVERIFY(view.focusProxy()->inputMethodHints() & Qt::ImhHiddenText);

    QPoint textInputCenter = elementCenter(view.page(), "input1");
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, 0, textInputCenter);
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.activeElement.id").toString(), QStringLiteral("input1"));
    QVERIFY(!(view.focusProxy()->inputMethodHints() & Qt::ImhHiddenText));
}

void tst_QWebEngineView::emptyInputMethodEvent()
{
    QWebEngineView view;
    view.show();

    QSignalSpy selectionChangedSpy(&view, SIGNAL(selectionChanged()));
    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml("<html><body>"
                 "  <input type='text' id='input1' value='QtWebEngine'/>"
                 "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    evaluateJavaScriptSync(view.page(), "var inputEle = document.getElementById('input1'); inputEle.focus(); inputEle.select();");
    QTRY_VERIFY(!evaluateJavaScriptSync(view.page(), "window.getSelection().toString()").toString().isEmpty());

    QEXPECT_FAIL("", "https://bugreports.qt.io/browse/QTBUG-53134", Continue);
    QVERIFY(selectionChangedSpy.wait(100));
    QEXPECT_FAIL("", "https://bugreports.qt.io/browse/QTBUG-53134", Continue);
    QCOMPARE(selectionChangedSpy.count(), 1);

    // Send empty QInputMethodEvent
    QInputMethodEvent emptyEvent;
    QApplication::sendEvent(view.focusProxy(), &emptyEvent);

    QString inputValue = evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString();
    QCOMPARE(inputValue, QString("QtWebEngine"));
}

void tst_QWebEngineView::imeComposition()
{
    QWebEngineView view;
    view.show();

    QSignalSpy selectionChangedSpy(&view, SIGNAL(selectionChanged()));
    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml("<html><body>"
                 "  <input type='text' id='input1' value='QtWebEngine inputMethod'/>"
                 "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    evaluateJavaScriptSync(view.page(), "var inputEle = document.getElementById('input1'); inputEle.focus(); inputEle.select();");
    QTRY_VERIFY(!evaluateJavaScriptSync(view.page(), "window.getSelection().toString()").toString().isEmpty());

    QEXPECT_FAIL("", "https://bugreports.qt.io/browse/QTBUG-53134", Continue);
    QVERIFY(selectionChangedSpy.wait(100));
    QEXPECT_FAIL("", "https://bugreports.qt.io/browse/QTBUG-53134", Continue);
    QCOMPARE(selectionChangedSpy.count(), 1);

    // Clear the selection, also cancel the ongoing composition if there is one.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent::Attribute newSelection(QInputMethodEvent::Selection, 0, 0, QVariant());
        attributes.append(newSelection);
        QInputMethodEvent event("", attributes);
        QApplication::sendEvent(view.focusProxy(), &event);
        QTRY_VERIFY(evaluateJavaScriptSync(view.page(), "window.getSelection().toString()").toString().isEmpty());

        QEXPECT_FAIL("", "https://bugreports.qt.io/browse/QTBUG-53134", Continue);
        QVERIFY(selectionChangedSpy.wait(100));
        QEXPECT_FAIL("", "https://bugreports.qt.io/browse/QTBUG-53134", Continue);
        QCOMPARE(selectionChangedSpy.count(), 2);
    }
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("QtWebEngine inputMethod"));
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 0);
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 0);
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));

    selectionChangedSpy.clear();


    // 1. Insert a character to the beginning of the line.
    // Send temporary text, which makes the editor has composition 'm'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("m", attributes);
        QApplication::sendEvent(view.focusProxy(), &event);
    }
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("QtWebEngine inputMethod"));
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 0);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 0);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));
    QCOMPARE(selectionChangedSpy.count(), 0);

    // Send temporary text, which makes the editor has composition 'n'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("n", attributes);
        QApplication::sendEvent(view.focusProxy(), &event);
    }
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("QtWebEngine inputMethod"));
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 0);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 0);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));
    QCOMPARE(selectionChangedSpy.count(), 0);

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("o");
        QApplication::sendEvent(view.focusProxy(), &event);
    }
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("oQtWebEngine inputMethod"));
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 1);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 1);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));
    QCOMPARE(selectionChangedSpy.count(), 0);


    // 2. insert a character to the middle of the line.
    // Send temporary text, which makes the editor has composition 'd'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("d", attributes);
        QApplication::sendEvent(view.focusProxy(), &event);
    }
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("oQtWebEngine inputMethod"));
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 1);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 1);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));
    QCOMPARE(selectionChangedSpy.count(), 0);

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("e");
        QApplication::sendEvent(view.focusProxy(), &event);
    }
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("oeQtWebEngine inputMethod"));
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 2);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 2);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));
    QCOMPARE(selectionChangedSpy.count(), 0);


    // 3. Insert a character to the end of the line.
    QTest::keyClick(view.focusProxy(), Qt::Key_End);
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 25);
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 25);

    // Send temporary text, which makes the editor has composition 't'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("t", attributes);
        QApplication::sendEvent(view.focusProxy(), &event);
    }
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("oeQtWebEngine inputMethod"));
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 25);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 25);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));
    QCOMPARE(selectionChangedSpy.count(), 0);

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("t");
        QApplication::sendEvent(view.focusProxy(), &event);
    }
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("oeQtWebEngine inputMethodt"));
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 26);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 26);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));
    QCOMPARE(selectionChangedSpy.count(), 0);


    // 4. Replace the selection.
#ifndef Q_OS_MACOS
    QTest::keyClick(view.focusProxy(), Qt::Key_Left, Qt::ShiftModifier | Qt::ControlModifier);
#else
    QTest::keyClick(view.focusProxy(), Qt::Key_Left, Qt::ShiftModifier | Qt::AltModifier);
#endif
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);

    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("oeQtWebEngine inputMethodt"));
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 14);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 26);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString("inputMethodt"));

    // Send temporary text, which makes the editor has composition 'w'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("w", attributes);
        QApplication::sendEvent(view.focusProxy(), &event);
        QTRY_VERIFY(evaluateJavaScriptSync(view.page(), "window.getSelection().toString()").toString().isEmpty());
        QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString(), QString("oeQtWebEngine w"));

        QEXPECT_FAIL("", "https://bugreports.qt.io/browse/QTBUG-53134", Continue);
        QVERIFY(selectionChangedSpy.wait(100));
        QEXPECT_FAIL("", "https://bugreports.qt.io/browse/QTBUG-53134", Continue);
        QCOMPARE(selectionChangedSpy.count(), 2);
    }
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("oeQtWebEngine "));
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 14);
    QEXPECT_FAIL("", "https://bugreports.qt.io/browse/QTBUG-53134", Continue);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 14);
    QEXPECT_FAIL("", "https://bugreports.qt.io/browse/QTBUG-53134", Continue);
    QCOMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("2");
        QApplication::sendEvent(view.focusProxy(), &event);
    }
    // There is no text selection to be changed at this point thus we can't wait for selectionChanged signal.
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("oeQtWebEngine 2"));
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 15);
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImAnchorPosition).toInt(), 15);
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCurrentSelection).toString(), QString(""));
}

void tst_QWebEngineView::newlineInTextarea()
{
    QWebEngineView view;
    view.show();

    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.page()->setHtml("<html><body>"
                         "  <textarea rows='5' cols='1' id='input1'></textarea>"
                         "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    evaluateJavaScriptSync(view.page(), "var inputEle = document.getElementById('input1'); inputEle.focus(); inputEle.select();");
    QTRY_VERIFY(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString().isEmpty());

    // Enter Key without key text
    QKeyEvent keyPressEnter(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
    QKeyEvent keyReleaseEnter(QEvent::KeyRelease, Qt::Key_Enter, Qt::NoModifier);
    QApplication::sendEvent(view.focusProxy(), &keyPressEnter);
    QApplication::sendEvent(view.focusProxy(), &keyReleaseEnter);

    QList<QInputMethodEvent::Attribute> attribs;

    QInputMethodEvent eventText(QString(), attribs);
    eventText.setCommitString("\n");
    QApplication::sendEvent(view.focusProxy(), &eventText);

    QInputMethodEvent eventText2(QString(), attribs);
    eventText2.setCommitString("third line");
    QApplication::sendEvent(view.focusProxy(), &eventText2);

    qApp->processEvents();
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString(), QString("\n\nthird line"));
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("\n\nthird line"));

    // Enter Key with key text '\r'
    evaluateJavaScriptSync(view.page(), "var inputEle = document.getElementById('input1'); inputEle.value = ''; inputEle.focus(); inputEle.select();");
    QTRY_VERIFY(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString().isEmpty());

    QKeyEvent keyPressEnterWithCarriageReturn(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier, "\r");
    QKeyEvent keyReleaseEnterWithCarriageReturn(QEvent::KeyRelease, Qt::Key_Enter, Qt::NoModifier);
    QApplication::sendEvent(view.focusProxy(), &keyPressEnterWithCarriageReturn);
    QApplication::sendEvent(view.focusProxy(), &keyReleaseEnterWithCarriageReturn);

    QApplication::sendEvent(view.focusProxy(), &eventText);
    QApplication::sendEvent(view.focusProxy(), &eventText2);

    qApp->processEvents();
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString(), QString("\n\nthird line"));
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("\n\nthird line"));

    // Enter Key with key text '\n'
    evaluateJavaScriptSync(view.page(), "var inputEle = document.getElementById('input1'); inputEle.value = ''; inputEle.focus(); inputEle.select();");
    QTRY_VERIFY(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString().isEmpty());

    QKeyEvent keyPressEnterWithLineFeed(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier, "\n");
    QKeyEvent keyReleaseEnterWithLineFeed(QEvent::KeyRelease, Qt::Key_Enter, Qt::NoModifier, "\n");
    QApplication::sendEvent(view.focusProxy(), &keyPressEnterWithLineFeed);
    QApplication::sendEvent(view.focusProxy(), &keyReleaseEnterWithLineFeed);

    QApplication::sendEvent(view.focusProxy(), &eventText);
    QApplication::sendEvent(view.focusProxy(), &eventText2);

    qApp->processEvents();
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString(), QString("\n\nthird line"));
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("\n\nthird line"));

    // Enter Key with key text "\n\r"
    evaluateJavaScriptSync(view.page(), "var inputEle = document.getElementById('input1'); inputEle.value = ''; inputEle.focus(); inputEle.select();");
    QTRY_VERIFY(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString().isEmpty());

    QKeyEvent keyPressEnterWithLFCR(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier, "\n\r");
    QKeyEvent keyReleaseEnterWithLFCR(QEvent::KeyRelease, Qt::Key_Enter, Qt::NoModifier, "\n\r");
    QApplication::sendEvent(view.focusProxy(), &keyPressEnterWithLFCR);
    QApplication::sendEvent(view.focusProxy(), &keyReleaseEnterWithLFCR);

    QApplication::sendEvent(view.focusProxy(), &eventText);
    QApplication::sendEvent(view.focusProxy(), &eventText2);

    qApp->processEvents();
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString(), QString("\n\nthird line"));
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("\n\nthird line"));

    // Return Key without key text
    evaluateJavaScriptSync(view.page(), "var inputEle = document.getElementById('input1'); inputEle.value = ''; inputEle.focus(); inputEle.select();");
    QTRY_VERIFY(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString().isEmpty());

    QKeyEvent keyPressReturn(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
    QKeyEvent keyReleaseReturn(QEvent::KeyRelease, Qt::Key_Enter, Qt::NoModifier);
    QApplication::sendEvent(view.focusProxy(), &keyPressReturn);
    QApplication::sendEvent(view.focusProxy(), &keyReleaseReturn);

    QApplication::sendEvent(view.focusProxy(), &eventText);
    QApplication::sendEvent(view.focusProxy(), &eventText2);

    qApp->processEvents();
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString(), QString("\n\nthird line"));
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("\n\nthird line"));
}

void tst_QWebEngineView::imeCompositionQueryEvent_data()
{
    QTest::addColumn<QString>("receiverObjectName");
    QTest::newRow("focusObject") << QString("focusObject");
    QTest::newRow("focusProxy") << QString("focusProxy");
    QTest::newRow("focusWidget") << QString("focusWidget");
}

void tst_QWebEngineView::imeCompositionQueryEvent()
{
    QWebEngineView view;
    view.show();

    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.setHtml("<html><body>"
                 "  <input type='text' id='input1' />"
                 "</body></html>");
    QVERIFY(loadFinishedSpy.wait());

    evaluateJavaScriptSync(view.page(), "document.getElementById('input1').focus()");
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.activeElement.id").toString(), QStringLiteral("input1"));

    QObject *input = nullptr;

    QFETCH(QString, receiverObjectName);
    if (receiverObjectName == "focusObject")
        input = qApp->focusObject();
    else if (receiverObjectName == "focusProxy")
        input = view.focusProxy();
    else if (receiverObjectName == "focusWidget")
        input = view.focusWidget();

    QVERIFY(input);

    QInputMethodQueryEvent srrndTextQuery(Qt::ImSurroundingText);
    QInputMethodQueryEvent cursorPosQuery(Qt::ImCursorPosition);
    QInputMethodQueryEvent anchorPosQuery(Qt::ImAnchorPosition);

    // Set composition
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("composition", attributes);
        QApplication::sendEvent(input, &event);
        qApp->processEvents();
    }
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString(), QString("composition"));
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImCursorPosition).toInt(), 11);

    QApplication::sendEvent(input, &srrndTextQuery);
    QApplication::sendEvent(input, &cursorPosQuery);
    QApplication::sendEvent(input, &anchorPosQuery);
    qApp->processEvents();

    QTRY_COMPARE(srrndTextQuery.value(Qt::ImSurroundingText).toString(), QString(""));
    QTRY_COMPARE(cursorPosQuery.value(Qt::ImCursorPosition).toInt(), 11);
    QTRY_COMPARE(anchorPosQuery.value(Qt::ImAnchorPosition).toInt(), 11);

    // Send commit
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("composition");
        QApplication::sendEvent(input, &event);
        qApp->processEvents();
    }
    QTRY_COMPARE(evaluateJavaScriptSync(view.page(), "document.getElementById('input1').value").toString(), QString("composition"));
    QTRY_COMPARE(view.focusProxy()->inputMethodQuery(Qt::ImSurroundingText).toString(), QString("composition"));

    QApplication::sendEvent(input, &srrndTextQuery);
    QApplication::sendEvent(input, &cursorPosQuery);
    QApplication::sendEvent(input, &anchorPosQuery);
    qApp->processEvents();

    QTRY_COMPARE(srrndTextQuery.value(Qt::ImSurroundingText).toString(), QString("composition"));
    QTRY_COMPARE(cursorPosQuery.value(Qt::ImCursorPosition).toInt(), 11);
    QTRY_COMPARE(anchorPosQuery.value(Qt::ImAnchorPosition).toInt(), 11);
}

QTEST_MAIN(tst_QWebEngineView)
#include "tst_qwebengineview.moc"
