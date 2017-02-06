/*
    Copyright (C) 2015 The Qt Company Ltd.

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

#include <qaccessible.h>
#include <qwebengineview.h>
#include <qwebenginepage.h>
#include <qwidget.h>

class tst_QWebEngineAccessibility : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void noPage();
    void hierarchy();
    void text();
    void value();
};

// This will be called before the first test function is executed.
// It is only called once.
void tst_QWebEngineAccessibility::initTestCase()
{
}

// This will be called after the last test function is executed.
// It is only called once.
void tst_QWebEngineAccessibility::cleanupTestCase()
{
}

// This will be called before each test function is executed.
void tst_QWebEngineAccessibility::init()
{
}

// This will be called after every test function.
void tst_QWebEngineAccessibility::cleanup()
{
}

void tst_QWebEngineAccessibility::noPage()
{
    QWebEngineView webView;
    webView.show();

    QTest::qWait(1000);
    QAccessibleInterface *view = QAccessible::queryAccessibleInterface(&webView);
    QVERIFY(view);
    QCOMPARE(view->role(), QAccessible::Client);
    QCOMPARE(view->childCount(), 1);
    QAccessibleInterface *document = view->child(0);
    QCOMPARE(document->role(), QAccessible::WebDocument);
    QCOMPARE(document->parent(), view);
    QCOMPARE(document->childCount(), 0);
}

void tst_QWebEngineAccessibility::hierarchy()
{
    QWebEngineView webView;
    webView.setHtml("<html><body>" \
        "Hello world" \
        "<input type='text' value='some text'></input>" \
        "</body></html>");
    webView.show();
    ::waitForSignal(&webView, SIGNAL(loadFinished(bool)));

    QAccessibleInterface *view = QAccessible::queryAccessibleInterface(&webView);
    QVERIFY(view);
    QCOMPARE(view->role(), QAccessible::Client);
    QCOMPARE(view->childCount(), 1);
    // Wait for accessibility to be fully initialized
    QTRY_VERIFY(view->child(0)->childCount() == 1);
    QAccessibleInterface *document = view->child(0);
    QCOMPARE(document->role(), QAccessible::WebDocument);
    QCOMPARE(document->parent(), view);
    QCOMPARE(view->indexOfChild(document), 0);
    QCOMPARE(document->childCount(), 1);
    QAccessibleInterface *grouping = document->child(0);
    QVERIFY(grouping);
    QCOMPARE(grouping->parent(), document);
    QCOMPARE(document->indexOfChild(grouping), 0);
    QCOMPARE(grouping->childCount(), 2);

    QAccessibleInterface *text = grouping->child(0);
    QCOMPARE(text->role(), QAccessible::StaticText);
    QCOMPARE(text->parent(), grouping);
    QCOMPARE(grouping->indexOfChild(text), 0);
    QCOMPARE(text->childCount(), 0);
    QCOMPARE(text->text(QAccessible::Name), QStringLiteral("Hello world"));
    QCOMPARE(text->text(QAccessible::Description), QString());
    QCOMPARE(text->text(QAccessible::Value), QString());

    QAccessibleInterface *input = grouping->child(1);
    QCOMPARE(input->role(), QAccessible::EditableText);
    QCOMPARE(input->parent(), grouping);
    QCOMPARE(grouping->indexOfChild(input), 1);
    QCOMPARE(input->childCount(), 0);
    QCOMPARE(input->text(QAccessible::Name), QString());
    QCOMPARE(input->text(QAccessible::Description), QString());
    QCOMPARE(input->text(QAccessible::Value), QStringLiteral("some text"));

    QRect windowRect = webView.geometry();
    QRect inputRect = input->rect();
    QVERIFY(!inputRect.isEmpty());
    QVERIFY(windowRect.contains(inputRect));
    QPoint inputCenter = inputRect.center();
    QAccessibleInterface *hitTest = view;
    QAccessibleInterface *child = Q_NULLPTR;
    while (hitTest) {
        child = hitTest;
        hitTest = hitTest->childAt(inputCenter.x(), inputCenter.y());
    }
    QCOMPARE(input, child);
}

void tst_QWebEngineAccessibility::text()
{
    QWebEngineView webView;
    webView.setHtml("<html><body>" \
        "<input type='text' value='Good morning!'></input>" \
        "<p id='labelName'>Enter your name here:</p>" \
        "<input type='text' value='my name' aria-labelledby='labelName' aria-describedby='explanation'></input>" \
        "<p id='explanation'>Provide both first and last name.</p>" \
        "<input type='text' value='Good day!' placeholder='day'></input>" \
        "</body></html>");
    webView.show();
    ::waitForSignal(&webView, SIGNAL(loadFinished(bool)));

    QAccessibleInterface *view = QAccessible::queryAccessibleInterface(&webView);
    // Wait for accessibility to be fully initialized

    QTRY_VERIFY(view->child(0)->childCount() == 5);
    QAccessibleInterface *document = view->child(0);
    QVERIFY(document);

    // Good morning! [edit]
    QAccessibleInterface *grouping1 = document->child(0);
    QAccessibleInterface *input1 = grouping1->child(0);
    QCOMPARE(input1->role(), QAccessible::EditableText);
    QCOMPARE(input1->text(QAccessible::Name), QString());
    QCOMPARE(input1->text(QAccessible::Description), QString());
    QCOMPARE(input1->text(QAccessible::Value), QStringLiteral("Good morning!"));

    QAccessibleTextInterface *textInterface1 = input1->textInterface();
    QVERIFY(textInterface1);
    QCOMPARE(textInterface1->characterCount(), 13);
    QCOMPARE(textInterface1->selectionCount(), 0);
    QCOMPARE(textInterface1->text(2, 9), QStringLiteral("od morn"));
    int start = -1;
    int end = -1;
    QCOMPARE(textInterface1->textAtOffset(8, QAccessible::WordBoundary, &start, &end), QStringLiteral("morning"));

    // Enter your name here:
    // my name [edit]
    // Provide both first and last name here.
    QAccessibleInterface *grouping2 = document->child(1);
    QAccessibleInterface *label1 = grouping2->child(0);
    QCOMPARE(label1->role(), QAccessible::StaticText);
    QCOMPARE(label1->text(QAccessible::Name), QStringLiteral("Enter your name here:"));
    QCOMPARE(label1->text(QAccessible::Description), QString());
    QCOMPARE(label1->text(QAccessible::Value), QString());
    QAccessibleInterface *grouping3 = document->child(2);
    QAccessibleInterface *input2 = grouping3->child(0);
    QCOMPARE(input2->role(), QAccessible::EditableText);
    QCOMPARE(input2->text(QAccessible::Name), QStringLiteral("Enter your name here:"));
    QCOMPARE(input2->text(QAccessible::Description), QStringLiteral("Provide both first and last name."));
    QCOMPARE(input2->text(QAccessible::Value), QStringLiteral("my name"));
    QAccessibleInterface *grouping4 = document->child(3);
    QAccessibleInterface *label2 = grouping4->child(0);
    QCOMPARE(label2->role(), QAccessible::StaticText);
    QCOMPARE(label2->text(QAccessible::Name), QStringLiteral("Provide both first and last name."));
    QCOMPARE(label2->text(QAccessible::Description), QString());
    QCOMPARE(label2->text(QAccessible::Value), QString());

    // Good day! [edit]
    QAccessibleInterface *grouping5 = document->child(4);
    QAccessibleInterface *input3 = grouping5->child(0);
    QCOMPARE(input3->role(), QAccessible::EditableText);
    QCOMPARE(input3->text(QAccessible::Name), QStringLiteral("day"));
    QCOMPARE(input3->text(QAccessible::Description), QString());
    QCOMPARE(input3->text(QAccessible::Value), QStringLiteral("Good day!"));
}

void tst_QWebEngineAccessibility::value()
{
    QWebEngineView webView;
    webView.setHtml("<html><body>" \
        "<div role='slider' aria-valuenow='4' aria-valuemin='1' aria-valuemax='10'></div>" \
        "<div class='progress' role='progressbar' aria-valuenow='77' aria-valuemin='22' aria-valuemax='99'></div>" \
        "</body></html>");
    webView.show();
    ::waitForSignal(&webView, SIGNAL(loadFinished(bool)));

    QAccessibleInterface *view = QAccessible::queryAccessibleInterface(&webView);
    QTRY_COMPARE(view->child(0)->childCount(), 2);
    QAccessibleInterface *document = view->child(0);
    QCOMPARE(document->childCount(), 2);

    QAccessibleInterface *slider = document->child(0);
    QCOMPARE(slider->role(), QAccessible::Slider);
    QCOMPARE(slider->text(QAccessible::Name), QString());
    QCOMPARE(slider->text(QAccessible::Description), QString());
    QCOMPARE(slider->text(QAccessible::Value), QString());
    QAccessibleValueInterface *valueInterface = slider->valueInterface();
    QVERIFY(valueInterface);
    QCOMPARE(valueInterface->currentValue().toInt(), 4);
    QCOMPARE(valueInterface->minimumValue().toInt(), 1);
    QCOMPARE(valueInterface->maximumValue().toInt(), 10);

    QAccessibleInterface *progressBar = document->child(1);
    QCOMPARE(progressBar->role(), QAccessible::ProgressBar);
    QCOMPARE(progressBar->text(QAccessible::Name), QString());
    QCOMPARE(progressBar->text(QAccessible::Description), QString());
    QCOMPARE(progressBar->text(QAccessible::Value), QString());
    QAccessibleValueInterface *progressBarValueInterface = progressBar->valueInterface();
    QVERIFY(progressBarValueInterface);
    QCOMPARE(progressBarValueInterface->currentValue().toInt(), 77);
    QCOMPARE(progressBarValueInterface->minimumValue().toInt(), 22);
    QCOMPARE(progressBarValueInterface->maximumValue().toInt(), 99);
}

static QByteArrayList params = QByteArrayList()
    << "--force-renderer-accessibility";

W_QTEST_MAIN(tst_QWebEngineAccessibility, params)
#include "tst_qwebengineaccessibility.moc"
