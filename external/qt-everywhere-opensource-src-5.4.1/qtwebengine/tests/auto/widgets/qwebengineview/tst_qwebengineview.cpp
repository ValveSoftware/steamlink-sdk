/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
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
#include <qwebengineview.h>
#include <qwebenginepage.h>
#include <qnetworkrequest.h>
#include <qdiriterator.h>

#define VERIFY_INPUTMETHOD_HINTS(actual, expect) \
    QVERIFY(actual == expect);

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
    void horizontalScrollbarTest();

    void crashTests();
#if !(defined(WTF_USE_QT_MOBILE_THEME) && WTF_USE_QT_MOBILE_THEME)
    void setPalette_data();
    void setPalette();
#endif
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
#if defined(QWEBENGINESETTINGS)
    page.data()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
#endif
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

QTEST_MAIN(tst_QWebEngineView)
#include "tst_qwebengineview.moc"
