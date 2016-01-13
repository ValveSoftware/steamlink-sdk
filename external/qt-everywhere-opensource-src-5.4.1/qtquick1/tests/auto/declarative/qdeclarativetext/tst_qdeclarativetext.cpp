/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <qtest.h>
#include <QTextDocument>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <private/qdeclarativetext_p.h>
#include <private/qdeclarativetext_p_p.h>
#include <private/qdeclarativevaluetype_p.h>
#include <QFontMetrics>
#include <QGraphicsSceneMouseEvent>
#include <qmath.h>
#include <QDeclarativeView>
#include <private/qapplication_p.h>
#include <limits.h>

#include "testhttpserver.h"

class tst_qdeclarativetext : public QObject

{
    Q_OBJECT
public:
    tst_qdeclarativetext();

private slots:
    void text();
    void width();
    void wrap();
    void elide();
    void textFormat();

    void alignments_data();
    void alignments();

    void embeddedImages_data();
    void embeddedImages();

    void lineCount();
    void lineHeight();

    // ### these tests may be trivial
    void horizontalAlignment();
    void horizontalAlignment_RightToLeft();
    void verticalAlignment();
    void font();
    void style();
    void color();
    void smooth();

    // QDeclarativeFontValueType
    void weight();
    void underline();
    void overline();
    void strikeout();
    void capitalization();
    void letterSpacing();
    void wordSpacing();

    void clickLink();

    void QTBUG_12291();
    void implicitSize_data();
    void implicitSize();
    void testQtQuick11Attributes();
    void testQtQuick11Attributes_data();

    void qtbug_14734();
private:
    QStringList standard;
    QStringList richText;

    QStringList horizontalAlignmentmentStrings;
    QStringList verticalAlignmentmentStrings;

    QList<Qt::Alignment> verticalAlignmentments;
    QList<Qt::Alignment> horizontalAlignmentments;

    QStringList styleStrings;
    QList<QDeclarativeText::TextStyle> styles;

    QStringList colorStrings;

    QDeclarativeEngine engine;

    QDeclarativeView *createView(const QString &filename);
};

tst_qdeclarativetext::tst_qdeclarativetext()
{
    standard << "the quick brown fox jumped over the lazy dog"
            << "the quick brown fox\n jumped over the lazy dog";

    richText << "<i>the <b>quick</b> brown <a href=\\\"http://www.google.com\\\">fox</a> jumped over the <b>lazy</b> dog</i>"
            << "<i>the <b>quick</b> brown <a href=\\\"http://www.google.com\\\">fox</a><br>jumped over the <b>lazy</b> dog</i>";

    horizontalAlignmentmentStrings << "AlignLeft"
            << "AlignRight"
            << "AlignHCenter";

    verticalAlignmentmentStrings << "AlignTop"
            << "AlignBottom"
            << "AlignVCenter";

    horizontalAlignmentments << Qt::AlignLeft
            << Qt::AlignRight
            << Qt::AlignHCenter;

    verticalAlignmentments << Qt::AlignTop
            << Qt::AlignBottom
            << Qt::AlignVCenter;

    styleStrings << "Normal"
            << "Outline"
            << "Raised"
            << "Sunken";

    styles << QDeclarativeText::Normal
            << QDeclarativeText::Outline
            << QDeclarativeText::Raised
            << QDeclarativeText::Sunken;

    colorStrings << "aliceblue"
            << "antiquewhite"
            << "aqua"
            << "darkkhaki"
            << "darkolivegreen"
            << "dimgray"
            << "palevioletred"
            << "lightsteelblue"
            << "#000000"
            << "#AAAAAA"
            << "#FFFFFF"
            << "#2AC05F";
    //
    // need a different test to do alpha channel test
    // << "#AA0011DD"
    // << "#00F16B11";
    //
}

QDeclarativeView *tst_qdeclarativetext::createView(const QString &filename)
{
    QDeclarativeView *canvas = new QDeclarativeView(0);

    canvas->setSource(QUrl::fromLocalFile(filename));
    return canvas;
}

void tst_qdeclarativetext::text()
{
    {
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData("import QtQuick 1.0\nText { text: \"\" }", QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->text(), QString(""));
        QVERIFY(textObject->width() == 0);

        delete textObject;
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"" + standard.at(i) + "\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));

        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->text(), standard.at(i));
        QVERIFY(textObject->width() > 0);
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"" + richText.at(i) + "\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QString expected = richText.at(i);
        QCOMPARE(textObject->text(), expected.replace("\\\"", "\""));
        QVERIFY(textObject->width() > 0);
    }
}

void tst_qdeclarativetext::width()
{
    // uses Font metrics to find the width for standard and document to find the width for rich
    {
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData("import QtQuick 1.0\nText { text: \"\" }", QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->width(), 0.);
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QVERIFY(!Qt::mightBeRichText(standard.at(i))); // self-test

        QFont f;
        QFontMetricsF fm(f);
        qreal metricWidth = fm.size(Qt::TextExpandTabs && Qt::TextShowMnemonic, standard.at(i)).width();
        metricWidth = qCeil(metricWidth);

        QString componentStr = "import QtQuick 1.0\nText { text: \"" + standard.at(i) + "\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QVERIFY(textObject->boundingRect().width() > 0);
        QCOMPARE(textObject->width(), qreal(metricWidth));
        QVERIFY(textObject->textFormat() == QDeclarativeText::AutoText); // setting text doesn't change format
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QVERIFY(Qt::mightBeRichText(richText.at(i))); // self-test

        QTextDocument document;
        document.setHtml(richText.at(i));
        document.setDocumentMargin(0);

        int documentWidth = document.idealWidth();

        QString componentStr = "import QtQuick 1.0\nText { text: \"" + richText.at(i) + "\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->width(), qreal(documentWidth));
        QVERIFY(textObject->textFormat() == QDeclarativeText::AutoText); // setting text doesn't change format
    }
}

void tst_qdeclarativetext::wrap()
{
    int textHeight = 0;
    // for specified width and wrap set true
    {
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData("import QtQuick 1.0\nText { text: \"Hello\"; wrapMode: Text.WordWrap; width: 300 }", QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());
        textHeight = textObject->height();

        QVERIFY(textObject != 0);
        QVERIFY(textObject->wrapMode() == QDeclarativeText::WordWrap);
        QCOMPARE(textObject->width(), 300.);
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nText { wrapMode: Text.WordWrap; width: 30; text: \"" + standard.at(i) + "\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->width(), 30.);
        QVERIFY(textObject->height() > textHeight);

        int oldHeight = textObject->height();
        textObject->setWidth(100);
        QVERIFY(textObject->height() < oldHeight);
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nText { wrapMode: Text.WordWrap; width: 30; text: \"" + richText.at(i) + "\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->width(), 30.);
        QVERIFY(textObject->height() > textHeight);

        qreal oldHeight = textObject->height();
        textObject->setWidth(100);
        QVERIFY(textObject->height() < oldHeight);
    }

    // richtext again with a fixed height
    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nText { wrapMode: Text.WordWrap; width: 30; height: 50; text: \"" + richText.at(i) + "\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->width(), 30.);
        QVERIFY(textObject->implicitHeight() > textHeight);

        qreal oldHeight = textObject->implicitHeight();
        textObject->setWidth(100);
        QVERIFY(textObject->implicitHeight() < oldHeight);
    }
}

void tst_qdeclarativetext::elide()
{
    for (QDeclarativeText::TextElideMode m = QDeclarativeText::ElideLeft; m<=QDeclarativeText::ElideNone; m=QDeclarativeText::TextElideMode(int(m)+1)) {
        const char* elidename[]={"ElideLeft", "ElideRight", "ElideMiddle", "ElideNone"};
        QString elide = "elide: Text." + QString(elidename[int(m)]) + ";";

        // XXX Poor coverage.

        {
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(("import QtQuick 1.0\nText { text: \"\"; "+elide+" width: 100 }").toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

            QCOMPARE(textObject->elideMode(), m);
            QCOMPARE(textObject->width(), 100.);
        }

        for (int i = 0; i < standard.size(); i++)
        {
            QString componentStr = "import QtQuick 1.0\nText { "+elide+" width: 100; text: \"" + standard.at(i) + "\" }";
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

            QCOMPARE(textObject->elideMode(), m);
            QCOMPARE(textObject->width(), 100.);
        }

        // richtext - does nothing
        for (int i = 0; i < richText.size(); i++)
        {
            QString componentStr = "import QtQuick 1.0\nText { "+elide+" width: 100; text: \"" + richText.at(i) + "\" }";
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

            QCOMPARE(textObject->elideMode(), m);
            QCOMPARE(textObject->width(), 100.);
        }
    }

    // QTBUG-18627
    QUrl qmlfile = QUrl::fromLocalFile(SRCDIR "/data/elideimplicitwidth.qml");
    QDeclarativeComponent textComponent(&engine, qmlfile);
    QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(textComponent.create());
    QVERIFY(item != 0);
    QVERIFY(item->implicitWidth() > item->width());
}

void tst_qdeclarativetext::textFormat()
{
    {
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData("import QtQuick 1.0\nText { text: \"Hello\"; textFormat: Text.RichText }", QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QVERIFY(textObject->textFormat() == QDeclarativeText::RichText);
    }
    {
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData("import QtQuick 1.0\nText { text: \"<b>Hello</b>\"; textFormat: Text.PlainText }", QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QVERIFY(textObject->textFormat() == QDeclarativeText::PlainText);
    }
}


void tst_qdeclarativetext::alignments_data()
{
    QTest::addColumn<int>("hAlign");
    QTest::addColumn<int>("vAlign");
    QTest::addColumn<QString>("expectfile");

    QTest::newRow("LT") << int(Qt::AlignLeft) << int(Qt::AlignTop) << SRCDIR "/data/alignments_lt.png";
    QTest::newRow("RT") << int(Qt::AlignRight) << int(Qt::AlignTop) << SRCDIR "/data/alignments_rt.png";
    QTest::newRow("CT") << int(Qt::AlignHCenter) << int(Qt::AlignTop) << SRCDIR "/data/alignments_ct.png";

    QTest::newRow("LB") << int(Qt::AlignLeft) << int(Qt::AlignBottom) << SRCDIR "/data/alignments_lb.png";
    QTest::newRow("RB") << int(Qt::AlignRight) << int(Qt::AlignBottom) << SRCDIR "/data/alignments_rb.png";
    QTest::newRow("CB") << int(Qt::AlignHCenter) << int(Qt::AlignBottom) << SRCDIR "/data/alignments_cb.png";

    QTest::newRow("LC") << int(Qt::AlignLeft) << int(Qt::AlignVCenter) << SRCDIR "/data/alignments_lc.png";
    QTest::newRow("RC") << int(Qt::AlignRight) << int(Qt::AlignVCenter) << SRCDIR "/data/alignments_rc.png";
    QTest::newRow("CC") << int(Qt::AlignHCenter) << int(Qt::AlignVCenter) << SRCDIR "/data/alignments_cc.png";
}


void tst_qdeclarativetext::alignments()
{
    QFETCH(int, hAlign);
    QFETCH(int, vAlign);
    QFETCH(QString, expectfile);

#ifdef Q_WS_X11
    // Font-specific, but not likely platform-specific, so only test on one platform
    QFont fn;
    fn.setRawName("-misc-fixed-medium-r-*-*-8-*-*-*-*-*-*-*");
    QApplication::setFont(fn);
#endif

    QDeclarativeView *canvas = createView(SRCDIR "/data/alignments.qml");

    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas, 10000));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    QObject *ob = canvas->rootObject();
    QVERIFY(ob != 0);
    ob->setProperty("horizontalAlignment",hAlign);
    ob->setProperty("verticalAlignment",vAlign);
    QTRY_COMPARE(ob->property("running").toBool(),false);
    QImage actual(canvas->width(), canvas->height(), QImage::Format_RGB32);
    actual.fill(qRgb(255,255,255));
    QPainter p(&actual);
    canvas->render(&p);

    QImage expect(expectfile);

#ifdef Q_WS_X11
    // Font-specific, but not likely platform-specific, so only test on one platform
    if (QApplicationPrivate::graphics_system_name == "raster" || QApplicationPrivate::graphics_system_name == "") {
        QCOMPARE(actual,expect);
    }
#endif

    delete canvas;
}

//the alignment tests may be trivial o.oa
void tst_qdeclarativetext::horizontalAlignment()
{
    //test one align each, and then test if two align fails.

    for (int i = 0; i < standard.size(); i++)
    {
        for (int j=0; j < horizontalAlignmentmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 1.0\nText { horizontalAlignment: \"" + horizontalAlignmentmentStrings.at(j) + "\"; text: \"" + standard.at(i) + "\" }";
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

            QCOMPARE((int)textObject->hAlign(), (int)horizontalAlignmentments.at(j));
        }
    }

    for (int i = 0; i < richText.size(); i++)
    {
        for (int j=0; j < horizontalAlignmentmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 1.0\nText { horizontalAlignment: \"" + horizontalAlignmentmentStrings.at(j) + "\"; text: \"" + richText.at(i) + "\" }";
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

            QCOMPARE((int)textObject->hAlign(), (int)horizontalAlignmentments.at(j));
        }
    }

}

void tst_qdeclarativetext::horizontalAlignment_RightToLeft()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/horizontalAlignment_RightToLeft.qml");
    QDeclarativeText *text = canvas->rootObject()->findChild<QDeclarativeText*>("text");
    QVERIFY(text != 0);
    canvas->show();

    QDeclarativeTextPrivate *textPrivate = QDeclarativeTextPrivate::get(text);
    QVERIFY(textPrivate != 0);

    // implicit alignment should follow the reading direction of RTL text
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() > canvas->width()/2);

    // explicitly left aligned text
    text->setHAlign(QDeclarativeText::AlignLeft);
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignLeft);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() < canvas->width()/2);

    // explicitly right aligned text
    text->setHAlign(QDeclarativeText::AlignRight);
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() > canvas->width()/2);

    // change to rich text
    QString textString = text->text();
    text->setText(QString("<i>") + textString + QString("</i>"));
    text->setTextFormat(QDeclarativeText::RichText);
    text->resetHAlign();

    // implicitly aligned rich text should follow the reading direction of text
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->textDocument()->defaultTextOption().alignment() & Qt::AlignLeft);

    // explicitly left aligned rich text
    text->setHAlign(QDeclarativeText::AlignLeft);
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignLeft);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->textDocument()->defaultTextOption().alignment() & Qt::AlignRight);

    // explicitly right aligned rich text
    text->setHAlign(QDeclarativeText::AlignRight);
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->textDocument()->defaultTextOption().alignment() & Qt::AlignLeft);

    text->setText(textString);
    text->setTextFormat(QDeclarativeText::PlainText);

    // explicitly center aligned
    text->setHAlign(QDeclarativeText::AlignHCenter);
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignHCenter);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() < canvas->width()/2);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().right() > canvas->width()/2);

    // reseted alignment should go back to following the text reading direction
    text->resetHAlign();
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignRight);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() > canvas->width()/2);

    // mirror the text item
    QDeclarativeItemPrivate::get(text)->setLayoutMirror(true);

    // mirrored implicit alignment should continue to follow the reading direction of the text
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), QDeclarativeText::AlignRight);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() > canvas->width()/2);

    // mirrored explicitly right aligned behaves as left aligned
    text->setHAlign(QDeclarativeText::AlignRight);
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), QDeclarativeText::AlignLeft);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() < canvas->width()/2);

    // mirrored explicitly left aligned behaves as right aligned
    text->setHAlign(QDeclarativeText::AlignLeft);
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignLeft);
    QCOMPARE(text->effectiveHAlign(), QDeclarativeText::AlignRight);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() > canvas->width()/2);

    // disable mirroring
    QDeclarativeItemPrivate::get(text)->setLayoutMirror(false);
    text->resetHAlign();

    // English text should be implicitly left aligned
    text->setText("Hello world!");
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignLeft);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() < canvas->width()/2);

#ifndef Q_OS_MAC    // QTBUG-18040
    // empty text with implicit alignment follows the system locale-based
    // keyboard input direction from QApplication::keyboardInputDirection
    text->setText("");
    QCOMPARE(text->hAlign(), QApplication::keyboardInputDirection() == Qt::LeftToRight ?
                                  QDeclarativeText::AlignLeft : QDeclarativeText::AlignRight);
    text->setHAlign(QDeclarativeText::AlignRight);
    QCOMPARE(text->hAlign(), QDeclarativeText::AlignRight);
#endif

    delete canvas;

#ifndef Q_OS_MAC    // QTBUG-18040
    // alignment of Text with no text set to it
    QString componentStr = "import QtQuick 1.0\nText {}";
    QDeclarativeComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());
    QCOMPARE(textObject->hAlign(), QApplication::keyboardInputDirection() == Qt::LeftToRight ?
                                  QDeclarativeText::AlignLeft : QDeclarativeText::AlignRight);
    delete textObject;
#endif
}

void tst_qdeclarativetext::verticalAlignment()
{
    //test one align each, and then test if two align fails.

    for (int i = 0; i < standard.size(); i++)
    {
        for (int j=0; j < verticalAlignmentmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 1.0\nText { verticalAlignment: \"" + verticalAlignmentmentStrings.at(j) + "\"; text: \"" + standard.at(i) + "\" }";
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

            QVERIFY(textObject != 0);
            QCOMPARE((int)textObject->vAlign(), (int)verticalAlignmentments.at(j));
        }
    }

    for (int i = 0; i < richText.size(); i++)
    {
        for (int j=0; j < verticalAlignmentmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 1.0\nText { verticalAlignment: \"" + verticalAlignmentmentStrings.at(j) + "\"; text: \"" + richText.at(i) + "\" }";
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

            QVERIFY(textObject != 0);
            QCOMPARE((int)textObject->vAlign(), (int)verticalAlignmentments.at(j));
        }
    }

    //confirm that bounding rect is correctly positioned.
    QString componentStr = "import QtQuick 1.0\nText { height: 80; text: \"Hello\" }";
    QDeclarativeComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());
    QVERIFY(textObject != 0);
    QRectF br = textObject->boundingRect();
    QVERIFY(br.y() == 0);

    textObject->setVAlign(QDeclarativeText::AlignVCenter);
    br = textObject->boundingRect();
    QCOMPARE(qFloor(br.y()), qFloor((80.0 - br.height())/2));

    textObject->setVAlign(QDeclarativeText::AlignBottom);
    br = textObject->boundingRect();
    QCOMPARE(qFloor(br.y()), qFloor(80.0 - br.height()));

    delete textObject;
}

void tst_qdeclarativetext::font()
{
    //test size, then bold, then italic, then family
    {
        QString componentStr = "import QtQuick 1.0\nText { font.pointSize: 40; text: \"Hello World\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QCOMPARE(textObject->font().pointSize(), 40);
        QCOMPARE(textObject->font().bold(), false);
        QCOMPARE(textObject->font().italic(), false);
    }

    {
        QString componentStr = "import QtQuick 1.0\nText { font.pixelSize: 40; text: \"Hello World\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QCOMPARE(textObject->font().pixelSize(), 40);
        QCOMPARE(textObject->font().bold(), false);
        QCOMPARE(textObject->font().italic(), false);
    }

    {
        QString componentStr = "import QtQuick 1.0\nText { font.bold: true; text: \"Hello World\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QCOMPARE(textObject->font().bold(), true);
        QCOMPARE(textObject->font().italic(), false);
    }

    {
        QString componentStr = "import QtQuick 1.0\nText { font.italic: true; text: \"Hello World\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QCOMPARE(textObject->font().italic(), true);
        QCOMPARE(textObject->font().bold(), false);
    }

    {
        QString componentStr = "import QtQuick 1.0\nText { font.family: \"Helvetica\"; text: \"Hello World\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QCOMPARE(textObject->font().family(), QString("Helvetica"));
        QCOMPARE(textObject->font().bold(), false);
        QCOMPARE(textObject->font().italic(), false);
    }

    {
        QString componentStr = "import QtQuick 1.0\nText { font.family: \"\"; text: \"Hello World\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QCOMPARE(textObject->font().family(), QString(""));
    }
}

void tst_qdeclarativetext::style()
{
    //test style
    for (int i = 0; i < styles.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nText { style: \"" + styleStrings.at(i) + "\"; styleColor: \"white\"; text: \"Hello World\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QCOMPARE((int)textObject->style(), (int)styles.at(i));
        QCOMPARE(textObject->styleColor(), QColor("white"));
    }
    QString componentStr = "import QtQuick 1.0\nText { text: \"Hello World\" }";
    QDeclarativeComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

    QRectF brPre = textObject->boundingRect();
    textObject->setStyle(QDeclarativeText::Outline);
    QRectF brPost = textObject->boundingRect();

    QVERIFY(brPre.width() < brPost.width());
    QVERIFY(brPre.height() < brPost.height());
}

void tst_qdeclarativetext::color()
{
    //test style
    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nText { color: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QCOMPARE(textObject->color(), QColor(colorStrings.at(i)));
        QCOMPARE(textObject->styleColor(), QColor());
    }

    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nText { styleColor: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QCOMPARE(textObject->styleColor(), QColor(colorStrings.at(i)));
        // default color to black?
        QCOMPARE(textObject->color(), QColor("black"));
    }

    for (int i = 0; i < colorStrings.size(); i++)
    {
        for (int j = 0; j < colorStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 1.0\nText { color: \"" + colorStrings.at(i) + "\"; styleColor: \"" + colorStrings.at(j) + "\"; text: \"Hello World\" }";
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

            QCOMPARE(textObject->color(), QColor(colorStrings.at(i)));
            QCOMPARE(textObject->styleColor(), QColor(colorStrings.at(j)));
        }
    }
    {
        QString colorStr = "#AA001234";
        QColor testColor("#001234");
        testColor.setAlpha(170);

        QString componentStr = "import QtQuick 1.0\nText { color: \"" + colorStr + "\"; text: \"Hello World\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QCOMPARE(textObject->color(), testColor);
    }
}

void tst_qdeclarativetext::smooth()
{
    for (int i = 0; i < standard.size(); i++)
    {
        {
            QString componentStr = "import QtQuick 1.0\nText { smooth: true; text: \"" + standard.at(i) + "\" }";
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());
            QCOMPARE(textObject->smooth(), true);
        }
        {
            QString componentStr = "import QtQuick 1.0\nText { text: \"" + standard.at(i) + "\" }";
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());
            QCOMPARE(textObject->smooth(), false);
        }
    }
    for (int i = 0; i < richText.size(); i++)
    {
        {
            QString componentStr = "import QtQuick 1.0\nText { smooth: true; text: \"" + richText.at(i) + "\" }";
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());
            QCOMPARE(textObject->smooth(), true);
        }
        {
            QString componentStr = "import QtQuick 1.0\nText { text: \"" + richText.at(i) + "\" }";
            QDeclarativeComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());
            QCOMPARE(textObject->smooth(), false);
        }
    }
}

void tst_qdeclarativetext::weight()
{
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().weight(), (int)QDeclarativeFontValueType::Normal);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { font.weight: \"Bold\"; text: \"Hello world!\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().weight(), (int)QDeclarativeFontValueType::Bold);
    }
}

void tst_qdeclarativetext::underline()
{
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().underline(), false);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { font.underline: true; text: \"Hello world!\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().underline(), true);
    }
}

void tst_qdeclarativetext::overline()
{
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().overline(), false);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { font.overline: true; text: \"Hello world!\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().overline(), true);
    }
}

void tst_qdeclarativetext::strikeout()
{
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().strikeOut(), false);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { font.strikeout: true; text: \"Hello world!\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().strikeOut(), true);
    }
}

void tst_qdeclarativetext::capitalization()
{
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().capitalization(), (int)QDeclarativeFontValueType::MixedCase);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\"; font.capitalization: \"AllUppercase\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().capitalization(), (int)QDeclarativeFontValueType::AllUppercase);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\"; font.capitalization: \"AllLowercase\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().capitalization(), (int)QDeclarativeFontValueType::AllLowercase);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\"; font.capitalization: \"SmallCaps\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().capitalization(), (int)QDeclarativeFontValueType::SmallCaps);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\"; font.capitalization: \"Capitalize\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().capitalization(), (int)QDeclarativeFontValueType::Capitalize);
    }
}

void tst_qdeclarativetext::letterSpacing()
{
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().letterSpacing(), 0.0);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\"; font.letterSpacing: -2 }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().letterSpacing(), -2.);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\"; font.letterSpacing: 3 }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().letterSpacing(), 3.);
    }
}

void tst_qdeclarativetext::wordSpacing()
{
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().wordSpacing(), 0.0);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\"; font.wordSpacing: -50 }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().wordSpacing(), -50.);
    }
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"Hello world!\"; font.wordSpacing: 200 }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().wordSpacing(), 200.);
    }
}

void tst_qdeclarativetext::QTBUG_12291()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/rotated.qml");

    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    QObject *ob = canvas->rootObject();
    QVERIFY(ob != 0);

    QDeclarativeText *text = ob->findChild<QDeclarativeText*>("text");
    QVERIFY(text);
    QVERIFY(text->boundingRect().isValid());

    delete canvas;
}

class EventSender : public QGraphicsItem
{
public:
    void sendEvent(QEvent *event) { sceneEvent(event); }
};

class LinkTest : public QObject
{
    Q_OBJECT
public:
    LinkTest() {}

    QString link;

public slots:
    void linkClicked(QString l) { link = l; }
};

void tst_qdeclarativetext::clickLink()
{
    {
        QString componentStr = "import QtQuick 1.0\nText { text: \"<a href=\\\"http://www.qt-project.org\\\">Hello world!</a>\" }";
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

        QVERIFY(textObject != 0);

        LinkTest test;
        QObject::connect(textObject, SIGNAL(linkActivated(QString)), &test, SLOT(linkClicked(QString)));

        {
            QGraphicsSceneMouseEvent me(QEvent::GraphicsSceneMousePress);
            me.setPos(QPointF(textObject->x()/2, textObject->y()/2));
            me.setButton(Qt::LeftButton);
            static_cast<EventSender*>(static_cast<QGraphicsItem*>(textObject))->sendEvent(&me);
        }

        {
            QGraphicsSceneMouseEvent me(QEvent::GraphicsSceneMouseRelease);
            me.setPos(QPointF(textObject->x()/2, textObject->y()/2));
            me.setButton(Qt::LeftButton);
            static_cast<EventSender*>(static_cast<QGraphicsItem*>(textObject))->sendEvent(&me);
        }

        QCOMPARE(test.link, QLatin1String("http://www.qt-project.org"));
    }
}

void tst_qdeclarativetext::embeddedImages_data()
{
    QTest::addColumn<QUrl>("qmlfile");
    QTest::addColumn<QString>("error");
    QTest::newRow("local") << QUrl::fromLocalFile(SRCDIR "/data/embeddedImagesLocal.qml") << "";
    QTest::newRow("local-error") << QUrl::fromLocalFile(SRCDIR "/data/embeddedImagesLocalError.qml")
        << QUrl::fromLocalFile(SRCDIR "/data/embeddedImagesLocalError.qml").toString()+":3:1: QML Text: Cannot open: " + QUrl::fromLocalFile(SRCDIR "/data/http/notexists.png").toString();
    QTest::newRow("remote") << QUrl::fromLocalFile(SRCDIR "/data/embeddedImagesRemote.qml") << "";
    QTest::newRow("remote-error") << QUrl::fromLocalFile(SRCDIR "/data/embeddedImagesRemoteError.qml")
        << QUrl::fromLocalFile(SRCDIR "/data/embeddedImagesRemoteError.qml").toString()+":3:1: QML Text: Error downloading http://127.0.0.1:14453/notexists.png - server replied: Not found";
}

void tst_qdeclarativetext::embeddedImages()
{
    // Tests QTBUG-9900

    QFETCH(QUrl, qmlfile);
    QFETCH(QString, error);

    TestHTTPServer server(14453);
    server.serveDirectory(SRCDIR "/data/http");

    if (!error.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, error.toLatin1());

    QDeclarativeComponent textComponent(&engine, qmlfile);
    QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

    QVERIFY(textObject != 0);

    QTRY_COMPARE(textObject->resourcesLoading(), 0);

    QPixmap pm(SRCDIR "/data/http/exists.png");
    if (error.isEmpty()) {
        QCOMPARE(textObject->width(), double(pm.width()));
        QCOMPARE(textObject->height(), double(pm.height()));
    } else {
        QVERIFY(16 != pm.width()); // check test is effective
        QCOMPARE(textObject->width(), 16.0); // default size of QTextDocument broken image icon
        QCOMPARE(textObject->height(), 16.0);
    }

    delete textObject;
}

void tst_qdeclarativetext::lineCount()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/lineCount.qml");

    QDeclarativeText *myText = canvas->rootObject()->findChild<QDeclarativeText*>("myText");
    QVERIFY(myText != 0);

    QVERIFY(myText->lineCount() > 1);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->maximumLineCount(), INT_MAX);

    myText->setMaximumLineCount(2);
    QCOMPARE(myText->lineCount(), 2);
    QCOMPARE(myText->truncated(), true);
    QCOMPARE(myText->maximumLineCount(), 2);

    myText->resetMaximumLineCount();
    QCOMPARE(myText->maximumLineCount(), INT_MAX);
    QCOMPARE(myText->truncated(), false);

    myText->setElideMode(QDeclarativeText::ElideRight);
    myText->setMaximumLineCount(2);
    QCOMPARE(myText->lineCount(), 2);
    QCOMPARE(myText->truncated(), true);
    QCOMPARE(myText->maximumLineCount(), 2);

    delete canvas;
}

void tst_qdeclarativetext::lineHeight()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/lineHeight.qml");

    QDeclarativeText *myText = canvas->rootObject()->findChild<QDeclarativeText*>("myText");
    QVERIFY(myText != 0);

    QVERIFY(myText->lineHeight() == 1);
    QVERIFY(myText->lineHeightMode() == QDeclarativeText::ProportionalHeight);

    qreal h = myText->height();
    myText->setLineHeight(1.5);
    QVERIFY(myText->height() == h * 1.5);

    myText->setLineHeightMode(QDeclarativeText::FixedHeight);
    myText->setLineHeight(20);
    QCOMPARE(myText->height(), myText->lineCount() * 20.0);

    myText->setText("Lorem ipsum sit <b>amet</b>, consectetur adipiscing elit. Integer felis nisl, varius in pretium nec, venenatis non erat. Proin lobortis interdum dictum.");
    myText->setTextFormat(QDeclarativeText::StyledText);
    myText->setLineHeightMode(QDeclarativeText::ProportionalHeight);
    myText->setLineHeight(1.0);

    qreal h2 = myText->height();
    myText->setLineHeight(2.0);
    QVERIFY(myText->height() == h2 * 2.0);

    myText->setLineHeightMode(QDeclarativeText::FixedHeight);
    myText->setLineHeight(10);
    QCOMPARE(myText->height(), myText->lineCount() * 10.0);

    delete canvas;
}

void tst_qdeclarativetext::implicitSize_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("wrap");
    QTest::newRow("plain") << "The quick red fox jumped over the lazy brown dog" << "Text.NoWrap";
    QTest::newRow("richtext") << "<b>The quick red fox jumped over the lazy brown dog</b>" << "Text.NoWrap";
    QTest::newRow("plain_wrap") << "The quick red fox jumped over the lazy brown dog" << "Text.Wrap";
    QTest::newRow("richtext_wrap") << "<b>The quick red fox jumped over the lazy brown dog</b>" << "Text.Wrap";
}

void tst_qdeclarativetext::implicitSize()
{
    QFETCH(QString, text);
    QFETCH(QString, wrap);
    QString componentStr = "import QtQuick 1.1\nText { text: \"" + text + "\"; width: 50; wrapMode: " + wrap + " }";
    QDeclarativeComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeText *textObject = qobject_cast<QDeclarativeText*>(textComponent.create());

    QVERIFY(textObject->width() < textObject->implicitWidth());
    QVERIFY(textObject->height() == textObject->implicitHeight());

    textObject->resetWidth();
    QVERIFY(textObject->width() == textObject->implicitWidth());
    QVERIFY(textObject->height() == textObject->implicitHeight());
}

void tst_qdeclarativetext::testQtQuick11Attributes()
{
    QFETCH(QString, code);
    QFETCH(QString, warning);
    QFETCH(QString, error);

    QDeclarativeEngine engine;
    QObject *obj;

    QDeclarativeComponent valid(&engine);
    valid.setData("import QtQuick 1.1; Text { " + code.toUtf8() + " }", QUrl(""));
    obj = valid.create();
    QVERIFY(obj);
    QVERIFY(valid.errorString().isEmpty());
    delete obj;

    QDeclarativeComponent invalid(&engine);
    invalid.setData("import QtQuick 1.0; Text { " + code.toUtf8() + " }", QUrl(""));
    QTest::ignoreMessage(QtWarningMsg, warning.toUtf8());
    obj = invalid.create();
    QCOMPARE(invalid.errorString(), error);
    delete obj;
}

void tst_qdeclarativetext::testQtQuick11Attributes_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("warning");
    QTest::addColumn<QString>("error");

    QTest::newRow("maximumLineCount") << "maximumLineCount: 4"
        << "QDeclarativeComponent: Component is not ready"
        << ":1 \"Text.maximumLineCount\" is not available in QtQuick 1.0.\n";

    QTest::newRow("lineHeight") << "lineHeight: 2"
        << "QDeclarativeComponent: Component is not ready"
        << ":1 \"Text.lineHeight\" is not available in QtQuick 1.0.\n";

    QTest::newRow("lineHeightMode") << "lineHeightMode: Text.ProportionalHeight"
        << "QDeclarativeComponent: Component is not ready"
        << ":1 \"Text.lineHeightMode\" is not available in QtQuick 1.0.\n";

    QTest::newRow("lineCount") << "property int foo: lineCount"
        << "<Unknown File>:1: ReferenceError: Can't find variable: lineCount"
        << "";

    QTest::newRow("truncated") << "property bool foo: truncated"
        << "<Unknown File>:1: ReferenceError: Can't find variable: truncated"
        << "";
}

void tst_qdeclarativetext::qtbug_14734()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/qtbug_14734.qml");
    QVERIFY(canvas);

    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    delete canvas;
}

QTEST_MAIN(tst_qdeclarativetext)

#include "tst_qdeclarativetext.moc"
