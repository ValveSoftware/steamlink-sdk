/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
#include <qtest.h>
#include <QtTest/QSignalSpy>
#include <QTextDocument>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQuick/private/qquickmousearea_p.h>
#include <private/qquicktext_p_p.h>
#include <private/qquicktextdocument_p.h>
#include <private/qquickvaluetypes_p.h>
#include <QFontMetrics>
#include <qmath.h>
#include <QtQuick/QQuickView>
#include <private/qguiapplication_p.h>
#include <limits.h>
#include <QtGui/QMouseEvent>
#include "../../shared/util.h"
#include "testhttpserver.h"

DEFINE_BOOL_CONFIG_OPTION(qmlDisableDistanceField, QML_DISABLE_DISTANCEFIELD)

Q_DECLARE_METATYPE(QQuickText::TextFormat)

QT_BEGIN_NAMESPACE
extern void qt_setQtEnableTestFont(bool value);
QT_END_NAMESPACE

class tst_qquicktext : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquicktext();

private slots:
    void cleanup();
    void text();
    void width();
    void wrap();
    void elide();
    void multilineElide_data();
    void multilineElide();
    void implicitElide_data();
    void implicitElide();
    void textFormat();

    void baseUrl();
    void embeddedImages_data();
    void embeddedImages();

    void lineCount();
    void lineHeight();

    // ### these tests may be trivial
    void horizontalAlignment();
    void horizontalAlignment_RightToLeft();
    void verticalAlignment();
    void hAlignImplicitWidth();
    void font();
    void style();
    void color();
    void smooth();
    void renderType();
    void antialiasing();

    // QQuickFontValueType
    void weight();
    void underline();
    void overline();
    void strikeout();
    void capitalization();
    void letterSpacing();
    void wordSpacing();

    void linkInteraction_data();
    void linkInteraction();

    void implicitSize_data();
    void implicitSize();
    void dependentImplicitSizes();
    void contentSize();
    void implicitSizeBinding_data();
    void implicitSizeBinding();
    void geometryChanged();

    void boundingRect_data();
    void boundingRect();
    void clipRect();
    void lineLaidOut();
    void lineLaidOutRelayout();
    void lineLaidOutHAlign();

    void imgTagsBaseUrl_data();
    void imgTagsBaseUrl();
    void imgTagsAlign_data();
    void imgTagsAlign();
    void imgTagsMultipleImages();
    void imgTagsElide();
    void imgTagsUpdates();
    void imgTagsError();
    void fontSizeMode_data();
    void fontSizeMode();
    void fontSizeModeMultiline_data();
    void fontSizeModeMultiline();
    void multilengthStrings_data();
    void multilengthStrings();
    void fontFormatSizes_data();
    void fontFormatSizes();

    void baselineOffset_data();
    void baselineOffset();

    void htmlLists();
    void htmlLists_data();

    void elideBeforeMaximumLineCount();

    void hover();

    void growFromZeroWidth();

    void padding();

    void hintingPreference();

    void zeroWidthAndElidedDoesntRender();

    void hAlignWidthDependsOnImplicitWidth_data();
    void hAlignWidthDependsOnImplicitWidth();

private:
    QStringList standard;
    QStringList richText;

    QStringList horizontalAlignmentmentStrings;
    QStringList verticalAlignmentmentStrings;

    QList<Qt::Alignment> verticalAlignmentments;
    QList<Qt::Alignment> horizontalAlignmentments;

    QStringList styleStrings;
    QList<QQuickText::TextStyle> styles;

    QStringList colorStrings;

    QQmlEngine engine;

    QQuickView *createView(const QString &filename);
    int numberOfNonWhitePixels(int fromX, int toX, const QImage &image);
};

void tst_qquicktext::cleanup()
{
    QVERIFY(QGuiApplication::topLevelWindows().isEmpty());
}

tst_qquicktext::tst_qquicktext()
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

    styles << QQuickText::Normal
            << QQuickText::Outline
            << QQuickText::Raised
            << QQuickText::Sunken;

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
    qt_setQtEnableTestFont(true);
}

QQuickView *tst_qquicktext::createView(const QString &filename)
{
    QQuickView *window = new QQuickView(0);

    window->setSource(QUrl::fromLocalFile(filename));
    return window;
}

void tst_qquicktext::text()
{
    {
        QQmlComponent textComponent(&engine);
        textComponent.setData("import QtQuick 2.0\nText { text: \"\" }", QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->text(), QString(""));
        QCOMPARE(textObject->width(), qreal(0));

        delete textObject;
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"" + standard.at(i) + "\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));

        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->text(), standard.at(i));
        QVERIFY(textObject->width() > 0);

        delete textObject;
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"" + richText.at(i) + "\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QString expected = richText.at(i);
        QCOMPARE(textObject->text(), expected.replace("\\\"", "\""));
        QVERIFY(textObject->width() > 0);

        delete textObject;
    }
}

void tst_qquicktext::width()
{
    // uses Font metrics to find the width for standard and document to find the width for rich
    {
        QQmlComponent textComponent(&engine);
        textComponent.setData("import QtQuick 2.0\nText { text: \"\" }", QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->width(), 0.);

        delete textObject;
    }

    bool requiresUnhintedMetrics = !qmlDisableDistanceField();

    for (int i = 0; i < standard.size(); i++)
    {
        QVERIFY(!Qt::mightBeRichText(standard.at(i))); // self-test

        QFont f;
        qreal metricWidth = 0.0;

        if (requiresUnhintedMetrics) {
            QString s = standard.at(i);
            s.replace(QLatin1Char('\n'), QChar::LineSeparator);

            QTextLayout layout(s);
            layout.setFlags(Qt::TextExpandTabs | Qt::TextShowMnemonic);
            {
                QTextOption option;
                option.setUseDesignMetrics(true);
                layout.setTextOption(option);
            }

            layout.beginLayout();
            forever {
                QTextLine line = layout.createLine();
                if (!line.isValid())
                    break;
            }

            layout.endLayout();

            metricWidth = layout.boundingRect().width();
        } else {
            QFontMetricsF fm(f);
            metricWidth = fm.size(Qt::TextExpandTabs && Qt::TextShowMnemonic, standard.at(i)).width();
        }

        QString componentStr = "import QtQuick 2.0\nText { text: \"" + standard.at(i) + "\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QVERIFY(textObject->boundingRect().width() > 0);
        QCOMPARE(textObject->width(), qreal(metricWidth));
        QVERIFY(textObject->textFormat() == QQuickText::AutoText); // setting text doesn't change format

        delete textObject;
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QVERIFY(Qt::mightBeRichText(richText.at(i))); // self-test

        QString componentStr = "import QtQuick 2.0\nText { text: \"" + richText.at(i) + "\"; textFormat: Text.RichText }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());
        QVERIFY(textObject != 0);

        QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(textObject);
        QVERIFY(textPrivate != 0);
        QVERIFY(textPrivate->extra.isAllocated());

        QTextDocument *doc = textPrivate->extra->doc;
        QVERIFY(doc != 0);

        QCOMPARE(int(textObject->width()), int(doc->idealWidth()));
        QCOMPARE(textObject->textFormat(), QQuickText::RichText);

        delete textObject;
    }
}

void tst_qquicktext::wrap()
{
    int textHeight = 0;
    // for specified width and wrap set true
    {
        QQmlComponent textComponent(&engine);
        textComponent.setData("import QtQuick 2.0\nText { text: \"Hello\"; wrapMode: Text.WordWrap; width: 300 }", QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());
        textHeight = textObject->height();

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->wrapMode(), QQuickText::WordWrap);
        QCOMPARE(textObject->width(), 300.);

        delete textObject;
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nText { wrapMode: Text.WordWrap; width: 30; text: \"" + standard.at(i) + "\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->width(), 30.);
        QVERIFY(textObject->height() > textHeight);

        int oldHeight = textObject->height();
        textObject->setWidth(100);
        QVERIFY(textObject->height() < oldHeight);

        delete textObject;
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nText { wrapMode: Text.WordWrap; width: 30; text: \"" + richText.at(i) + "\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->width(), 30.);
        QVERIFY(textObject->height() > textHeight);

        qreal oldHeight = textObject->height();
        textObject->setWidth(100);
        QVERIFY(textObject->height() < oldHeight);

        delete textObject;
    }

    // Check that increasing width from idealWidth will cause a relayout
    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nText { wrapMode: Text.WordWrap; textFormat: Text.RichText; width: 30; text: \"" + richText.at(i) + "\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->width(), 30.);
        QVERIFY(textObject->height() > textHeight);

        QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(textObject);
        QVERIFY(textPrivate != 0);
        QVERIFY(textPrivate->extra.isAllocated());

        QTextDocument *doc = textPrivate->extra->doc;
        QVERIFY(doc != 0);
        textObject->setWidth(doc->idealWidth());
        QCOMPARE(textObject->width(), doc->idealWidth());
        QVERIFY(textObject->height() > textHeight);

        qreal oldHeight = textObject->height();
        textObject->setWidth(100);
        QVERIFY(textObject->height() < oldHeight);

        delete textObject;
    }

    // richtext again with a fixed height
    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nText { wrapMode: Text.WordWrap; width: 30; height: 50; text: \"" + richText.at(i) + "\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->width(), 30.);
        QVERIFY(textObject->implicitHeight() > textHeight);

        qreal oldHeight = textObject->implicitHeight();
        textObject->setWidth(100);
        QVERIFY(textObject->implicitHeight() < oldHeight);

        delete textObject;
    }

    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0\n Text {}", QUrl());
        QScopedPointer<QObject> object(component.create());
        QQuickText *textObject = qobject_cast<QQuickText *>(object.data());
        QVERIFY(textObject);

        QSignalSpy spy(textObject, SIGNAL(wrapModeChanged()));

        QCOMPARE(textObject->wrapMode(), QQuickText::NoWrap);

        textObject->setWrapMode(QQuickText::Wrap);
        QCOMPARE(textObject->wrapMode(), QQuickText::Wrap);
        QCOMPARE(spy.count(), 1);

        textObject->setWrapMode(QQuickText::Wrap);
        QCOMPARE(spy.count(), 1);

        textObject->setWrapMode(QQuickText::NoWrap);
        QCOMPARE(textObject->wrapMode(), QQuickText::NoWrap);
        QCOMPARE(spy.count(), 2);
    }
}

void tst_qquicktext::elide()
{
    for (QQuickText::TextElideMode m = QQuickText::ElideLeft; m<=QQuickText::ElideNone; m=QQuickText::TextElideMode(int(m)+1)) {
        const char* elidename[]={"ElideLeft", "ElideRight", "ElideMiddle", "ElideNone"};
        QString elide = "elide: Text." + QString(elidename[int(m)]) + ";";

        // XXX Poor coverage.

        {
            QQmlComponent textComponent(&engine);
            textComponent.setData(("import QtQuick 2.0\nText { text: \"\"; "+elide+" width: 100 }").toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

            QCOMPARE(textObject->elideMode(), m);
            QCOMPARE(textObject->width(), 100.);

            delete textObject;
        }

        for (int i = 0; i < standard.size(); i++)
        {
            QString componentStr = "import QtQuick 2.0\nText { "+elide+" width: 100; text: \"" + standard.at(i) + "\" }";
            QQmlComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

            QCOMPARE(textObject->elideMode(), m);
            QCOMPARE(textObject->width(), 100.);

            if (m != QQuickText::ElideNone && !standard.at(i).contains('\n'))
                QVERIFY(textObject->contentWidth() <= textObject->width());

            delete textObject;
        }

        for (int i = 0; i < richText.size(); i++)
        {
            QString componentStr = "import QtQuick 2.0\nText { "+elide+" width: 100; text: \"" + richText.at(i) + "\" }";
            QQmlComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

            QCOMPARE(textObject->elideMode(), m);
            QCOMPARE(textObject->width(), 100.);

            if (m != QQuickText::ElideNone && standard.at(i).contains("<br>"))
                QVERIFY(textObject->contentWidth() <= textObject->width());

            delete textObject;
        }
    }
}

void tst_qquicktext::multilineElide_data()
{
    QTest::addColumn<QQuickText::TextFormat>("format");
    QTest::newRow("plain") << QQuickText::PlainText;
    QTest::newRow("styled") << QQuickText::StyledText;
}

void tst_qquicktext::multilineElide()
{
    QFETCH(QQuickText::TextFormat, format);
    QScopedPointer<QQuickView> window(createView(testFile("multilineelide.qml")));

    QQuickText *myText = qobject_cast<QQuickText*>(window->rootObject());
    QVERIFY(myText != 0);
    myText->setTextFormat(format);

    QCOMPARE(myText->lineCount(), 3);
    QCOMPARE(myText->truncated(), true);

    qreal lineHeight = myText->contentHeight() / 3.;

    // Set a valid height greater than the truncated content height and ensure the line count is
    // unchanged.
    myText->setHeight(200);
    QCOMPARE(myText->lineCount(), 3);
    QCOMPARE(myText->truncated(), true);

    // reduce size and ensure fewer lines are drawn
    myText->setHeight(lineHeight * 2);
    QCOMPARE(myText->lineCount(), 2);

    myText->setHeight(lineHeight);
    QCOMPARE(myText->lineCount(), 1);

    myText->setHeight(5);
    QCOMPARE(myText->lineCount(), 1);

    myText->setHeight(lineHeight * 3);
    QCOMPARE(myText->lineCount(), 3);

    // remove max count and show all lines.
    myText->setHeight(1000);
    myText->resetMaximumLineCount();

    QCOMPARE(myText->truncated(), false);

    // reduce size again
    myText->setHeight(lineHeight * 2);
    QCOMPARE(myText->lineCount(), 2);
    QCOMPARE(myText->truncated(), true);

    // change line height
    myText->setLineHeight(1.1);
    QCOMPARE(myText->lineCount(), 1);
}

void tst_qquicktext::implicitElide_data()
{
    QTest::addColumn<QString>("width");
    QTest::addColumn<QString>("initialText");
    QTest::addColumn<QString>("text");

    QTest::newRow("maximum width, empty")
            << "Math.min(implicitWidth, 100)"
            << "";
    QTest::newRow("maximum width, short")
            << "Math.min(implicitWidth, 100)"
            << "the";
    QTest::newRow("maximum width, long")
            << "Math.min(implicitWidth, 100)"
            << "the quick brown fox jumped over the lazy dog";
    QTest::newRow("reset width, empty")
            << "implicitWidth > 100 ? 100 : undefined"
            << "";
    QTest::newRow("reset width, short")
            << "implicitWidth > 100 ? 100 : undefined"
            << "the";
    QTest::newRow("reset width, long")
            << "implicitWidth > 100 ? 100 : undefined"
            << "the quick brown fox jumped over the lazy dog";
}

void tst_qquicktext::implicitElide()
{
    QFETCH(QString, width);
    QFETCH(QString, initialText);

    QString componentStr =
            "import QtQuick 2.0\n"
            "Text {\n"
                "width: " + width + "\n"
                "text: \"" + initialText + "\"\n"
                "elide: Text.ElideRight\n"
            "}";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

    QVERIFY(textObject->contentWidth() <= textObject->width());

    textObject->setText("the quick brown fox jumped over");

    QVERIFY(textObject->contentWidth() > 0);
    QVERIFY(textObject->contentWidth() <= textObject->width());
}

void tst_qquicktext::textFormat()
{
    {
        QQmlComponent textComponent(&engine);
        textComponent.setData("import QtQuick 2.0\nText { text: \"Hello\"; textFormat: Text.RichText }", QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->textFormat(), QQuickText::RichText);

        QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(textObject);
        QVERIFY(textPrivate != 0);
        QVERIFY(textPrivate->richText);

        delete textObject;
    }
    {
        QQmlComponent textComponent(&engine);
        textComponent.setData("import QtQuick 2.0\nText { text: \"<b>Hello</b>\" }", QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->textFormat(), QQuickText::AutoText);

        QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(textObject);
        QVERIFY(textPrivate != 0);
        QVERIFY(textPrivate->styledText);

        delete textObject;
    }
    {
        QQmlComponent textComponent(&engine);
        textComponent.setData("import QtQuick 2.0\nText { text: \"<b>Hello</b>\"; textFormat: Text.PlainText }", QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->textFormat(), QQuickText::PlainText);

        delete textObject;
    }

    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0\n Text {}", QUrl());
        QScopedPointer<QObject> object(component.create());
        QQuickText *text = qobject_cast<QQuickText *>(object.data());
        QVERIFY(text);

        QSignalSpy spy(text, &QQuickText::textFormatChanged);

        QCOMPARE(text->textFormat(), QQuickText::AutoText);

        text->setTextFormat(QQuickText::StyledText);
        QCOMPARE(text->textFormat(), QQuickText::StyledText);
        QCOMPARE(spy.count(), 1);

        text->setTextFormat(QQuickText::StyledText);
        QCOMPARE(spy.count(), 1);

        text->setTextFormat(QQuickText::AutoText);
        QCOMPARE(text->textFormat(), QQuickText::AutoText);
        QCOMPARE(spy.count(), 2);
    }
}

//the alignment tests may be trivial o.oa
void tst_qquicktext::horizontalAlignment()
{
    //test one align each, and then test if two align fails.

    for (int i = 0; i < standard.size(); i++)
    {
        for (int j=0; j < horizontalAlignmentmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 2.0\nText { horizontalAlignment: \"" + horizontalAlignmentmentStrings.at(j) + "\"; text: \"" + standard.at(i) + "\" }";
            QQmlComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

            QCOMPARE((int)textObject->hAlign(), (int)horizontalAlignmentments.at(j));

            delete textObject;
        }
    }

    for (int i = 0; i < richText.size(); i++)
    {
        for (int j=0; j < horizontalAlignmentmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 2.0\nText { horizontalAlignment: \"" + horizontalAlignmentmentStrings.at(j) + "\"; text: \"" + richText.at(i) + "\" }";
            QQmlComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

            QCOMPARE((int)textObject->hAlign(), (int)horizontalAlignmentments.at(j));

            delete textObject;
        }
    }

}

void tst_qquicktext::horizontalAlignment_RightToLeft()
{
#if defined(Q_OS_BLACKBERRY)
    QQuickWindow dummy;      // On BlackBerry first window is always full screen,
    dummy.showFullScreen();  // so make test window a second window.
#endif

    QScopedPointer<QQuickView> window(createView(testFile("horizontalAlignment_RightToLeft.qml")));
    QQuickText *text = window->rootObject()->findChild<QQuickText*>("text");
    QVERIFY(text != 0);
    window->showNormal();

    QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(text);
    QVERIFY(textPrivate != 0);

    QTRY_VERIFY(textPrivate->layout.lineCount());

    // implicit alignment should follow the reading direction of RTL text
    QCOMPARE(text->hAlign(), QQuickText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() > window->width()/2);

    // explicitly left aligned text
    text->setHAlign(QQuickText::AlignLeft);
    QCOMPARE(text->hAlign(), QQuickText::AlignLeft);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() < window->width()/2);

    // explicitly right aligned text
    text->setHAlign(QQuickText::AlignRight);
    QCOMPARE(text->hAlign(), QQuickText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() > window->width()/2);

    // change to rich text
    QString textString = text->text();
    text->setText(QString("<i>") + textString + QString("</i>"));
    text->setTextFormat(QQuickText::RichText);
    text->resetHAlign();

    // implicitly aligned rich text should follow the reading direction of text
    QCOMPARE(text->hAlign(), QQuickText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->extra.isAllocated());
    QVERIFY(textPrivate->extra->doc->defaultTextOption().alignment() & Qt::AlignLeft);

    // explicitly left aligned rich text
    text->setHAlign(QQuickText::AlignLeft);
    QCOMPARE(text->hAlign(), QQuickText::AlignLeft);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->extra->doc->defaultTextOption().alignment() & Qt::AlignRight);

    // explicitly right aligned rich text
    text->setHAlign(QQuickText::AlignRight);
    QCOMPARE(text->hAlign(), QQuickText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->extra->doc->defaultTextOption().alignment() & Qt::AlignLeft);

    text->setText(textString);
    text->setTextFormat(QQuickText::PlainText);

    // explicitly center aligned
    text->setHAlign(QQuickText::AlignHCenter);
    QCOMPARE(text->hAlign(), QQuickText::AlignHCenter);
    QCOMPARE(text->effectiveHAlign(), text->hAlign());
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() < window->width()/2);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().right() > window->width()/2);

    // reseted alignment should go back to following the text reading direction
    text->resetHAlign();
    QCOMPARE(text->hAlign(), QQuickText::AlignRight);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() > window->width()/2);

    // mirror the text item
    QQuickItemPrivate::get(text)->setLayoutMirror(true);

    // mirrored implicit alignment should continue to follow the reading direction of the text
    QCOMPARE(text->hAlign(), QQuickText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), QQuickText::AlignRight);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() > window->width()/2);

    // mirrored explicitly right aligned behaves as left aligned
    text->setHAlign(QQuickText::AlignRight);
    QCOMPARE(text->hAlign(), QQuickText::AlignRight);
    QCOMPARE(text->effectiveHAlign(), QQuickText::AlignLeft);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() < window->width()/2);

    // mirrored explicitly left aligned behaves as right aligned
    text->setHAlign(QQuickText::AlignLeft);
    QCOMPARE(text->hAlign(), QQuickText::AlignLeft);
    QCOMPARE(text->effectiveHAlign(), QQuickText::AlignRight);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() > window->width()/2);

    // disable mirroring
    QQuickItemPrivate::get(text)->setLayoutMirror(false);
    text->resetHAlign();

    // English text should be implicitly left aligned
    text->setText("Hello world!");
    QCOMPARE(text->hAlign(), QQuickText::AlignLeft);
    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().left() < window->width()/2);

    // empty text with implicit alignment follows the system locale-based
    // keyboard input direction from QInputMethod::inputDirection()
    text->setText("");
    QCOMPARE(text->hAlign(), qApp->inputMethod()->inputDirection() == Qt::LeftToRight ?
                                  QQuickText::AlignLeft : QQuickText::AlignRight);
    text->setHAlign(QQuickText::AlignRight);
    QCOMPARE(text->hAlign(), QQuickText::AlignRight);

    window.reset();

    // alignment of Text with no text set to it
    QString componentStr = "import QtQuick 2.0\nText {}";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());
    QCOMPARE(textObject->hAlign(), qApp->inputMethod()->inputDirection() == Qt::LeftToRight ?
                                  QQuickText::AlignLeft : QQuickText::AlignRight);
    delete textObject;
}

int tst_qquicktext::numberOfNonWhitePixels(int fromX, int toX, const QImage &image)
{
    int pixels = 0;
    for (int x = fromX; x < toX; ++x) {
        for (int y = 0; y < image.height(); ++y) {
            if (image.pixel(x, y) != qRgb(255, 255, 255))
                pixels++;
        }
    }
    return pixels;
}

static inline QByteArray msgNotGreaterThan(int n1, int n2)
{
    return QByteArray::number(n1) + QByteArrayLiteral(" is not greater than ") + QByteArray::number(n2);
}

static inline QByteArray msgNotLessThan(int n1, int n2)
{
    return QByteArray::number(n1) + QByteArrayLiteral(" is not less than ") + QByteArray::number(n2);
}

void tst_qquicktext::hAlignImplicitWidth()
{
#if defined(QT_OPENGL_ES_2_ANGLE) && _MSC_VER==1600
    QSKIP("QTBUG-40658");
#endif
    QQuickView view(testFileUrl("hAlignImplicitWidth.qml"));
    view.setFlags(view.flags() | Qt::WindowStaysOnTopHint); // Prevent being obscured by other windows.
    view.show();
    view.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&view));

    QQuickText *text = view.rootObject()->findChild<QQuickText*>("textItem");
    QVERIFY(text != 0);

    // Try to check whether alignment works by checking the number of black
    // pixels in the thirds of the grabbed image.
    // QQuickWindow::grabWindow() scales the returned image by the devicePixelRatio of the screen.
    const qreal devicePixelRatio = view.screen()->devicePixelRatio();
    const int windowWidth = 220 * devicePixelRatio;
    const int textWidth = qCeil(text->implicitWidth()) * devicePixelRatio;
    QVERIFY2(textWidth < windowWidth, "System font too large.");
    const int sectionWidth = textWidth / 3;
    const int centeredSection1 = (windowWidth - textWidth) / 2;
    const int centeredSection2 = centeredSection1 + sectionWidth;
    const int centeredSection3 = centeredSection2 + sectionWidth;
    const int centeredSection3End = centeredSection3 + sectionWidth;

    {
        // Left Align
        QImage image = view.grabWindow();
        const int left = numberOfNonWhitePixels(centeredSection1, centeredSection2, image);
        const int mid = numberOfNonWhitePixels(centeredSection2, centeredSection3, image);
        const int right = numberOfNonWhitePixels(centeredSection3, centeredSection3End, image);
        QVERIFY2(left > mid, msgNotGreaterThan(left, mid).constData());
        QVERIFY2(mid > right, msgNotGreaterThan(mid, right).constData());
    }
    {
        // HCenter Align
        text->setHAlign(QQuickText::AlignHCenter);
        text->setText("Reset"); // set dummy string to force relayout once original text is set again
        text->setText("AA\nBBBBBBB\nCCCCCCCCCCCCCCCC");
        QImage image = view.grabWindow();
        const int left = numberOfNonWhitePixels(centeredSection1, centeredSection2, image);
        const int mid = numberOfNonWhitePixels(centeredSection2, centeredSection3, image);
        const int right = numberOfNonWhitePixels(centeredSection3, centeredSection3End, image);
        QVERIFY2(left < mid, msgNotLessThan(left, mid).constData());
        QVERIFY2(mid > right, msgNotGreaterThan(mid, right).constData());
    }
    {
        // Right Align
        text->setHAlign(QQuickText::AlignRight);
        text->setText("Reset"); // set dummy string to force relayout once original text is set again
        text->setText("AA\nBBBBBBB\nCCCCCCCCCCCCCCCC");
        QImage image = view.grabWindow();
        const int left = numberOfNonWhitePixels(centeredSection1, centeredSection2, image);
        const int mid = numberOfNonWhitePixels(centeredSection2, centeredSection3, image);
        const int right = numberOfNonWhitePixels(centeredSection3, centeredSection3End, image);
        QVERIFY2(left < mid, msgNotLessThan(left, mid).constData());
        QVERIFY2(mid < right, msgNotLessThan(mid, right).constData());
    }
}

void tst_qquicktext::verticalAlignment()
{
    //test one align each, and then test if two align fails.

    for (int i = 0; i < standard.size(); i++)
    {
        for (int j=0; j < verticalAlignmentmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 2.0\nText { verticalAlignment: \"" + verticalAlignmentmentStrings.at(j) + "\"; text: \"" + standard.at(i) + "\" }";
            QQmlComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

            QVERIFY(textObject != 0);
            QCOMPARE((int)textObject->vAlign(), (int)verticalAlignmentments.at(j));

            delete textObject;
        }
    }

    for (int i = 0; i < richText.size(); i++)
    {
        for (int j=0; j < verticalAlignmentmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 2.0\nText { verticalAlignment: \"" + verticalAlignmentmentStrings.at(j) + "\"; text: \"" + richText.at(i) + "\" }";
            QQmlComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

            QVERIFY(textObject != 0);
            QCOMPARE((int)textObject->vAlign(), (int)verticalAlignmentments.at(j));

            delete textObject;
        }
    }

}

void tst_qquicktext::font()
{
    //test size, then bold, then italic, then family
    {
        QString componentStr = "import QtQuick 2.0\nText { font.pointSize: 40; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QCOMPARE(textObject->font().pointSize(), 40);
        QCOMPARE(textObject->font().bold(), false);
        QCOMPARE(textObject->font().italic(), false);

        delete textObject;
    }

    {
        QString componentStr = "import QtQuick 2.0\nText { font.pixelSize: 40; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QCOMPARE(textObject->font().pixelSize(), 40);
        QCOMPARE(textObject->font().bold(), false);
        QCOMPARE(textObject->font().italic(), false);

        delete textObject;
    }

    {
        QString componentStr = "import QtQuick 2.0\nText { font.bold: true; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QCOMPARE(textObject->font().bold(), true);
        QCOMPARE(textObject->font().italic(), false);

        delete textObject;
    }

    {
        QString componentStr = "import QtQuick 2.0\nText { font.italic: true; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QCOMPARE(textObject->font().italic(), true);
        QCOMPARE(textObject->font().bold(), false);

        delete textObject;
    }

    {
        QString componentStr = "import QtQuick 2.0\nText { font.family: \"Helvetica\"; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QCOMPARE(textObject->font().family(), QString("Helvetica"));
        QCOMPARE(textObject->font().bold(), false);
        QCOMPARE(textObject->font().italic(), false);

        delete textObject;
    }

    {
        QString componentStr = "import QtQuick 2.0\nText { font.family: \"\"; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QCOMPARE(textObject->font().family(), QString(""));

        delete textObject;
    }
}

void tst_qquicktext::style()
{
    //test style
    for (int i = 0; i < styles.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nText { style: \"" + styleStrings.at(i) + "\"; styleColor: \"white\"; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QCOMPARE((int)textObject->style(), (int)styles.at(i));
        QCOMPARE(textObject->styleColor(), QColor("white"));

        delete textObject;
    }
    QString componentStr = "import QtQuick 2.0\nText { text: \"Hello World\" }";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

    QRectF brPre = textObject->boundingRect();
    textObject->setStyle(QQuickText::Outline);
    QRectF brPost = textObject->boundingRect();

    QVERIFY(brPre.width() < brPost.width());
    QVERIFY(brPre.height() < brPost.height());

    delete textObject;
}

void tst_qquicktext::color()
{
    //test style
    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nText { color: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QCOMPARE(textObject->color(), QColor(colorStrings.at(i)));
        QCOMPARE(textObject->styleColor(), QColor("black"));
        QCOMPARE(textObject->linkColor(), QColor("blue"));

        delete textObject;
    }

    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nText { styleColor: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QCOMPARE(textObject->styleColor(), QColor(colorStrings.at(i)));
        // default color to black?
        QCOMPARE(textObject->color(), QColor("black"));
        QCOMPARE(textObject->linkColor(), QColor("blue"));

        QSignalSpy colorSpy(textObject, SIGNAL(colorChanged()));
        QSignalSpy linkColorSpy(textObject, SIGNAL(linkColorChanged()));

        textObject->setColor(QColor("white"));
        QCOMPARE(textObject->color(), QColor("white"));
        QCOMPARE(colorSpy.count(), 1);

        textObject->setLinkColor(QColor("black"));
        QCOMPARE(textObject->linkColor(), QColor("black"));
        QCOMPARE(linkColorSpy.count(), 1);

        textObject->setColor(QColor("white"));
        QCOMPARE(colorSpy.count(), 1);

        textObject->setLinkColor(QColor("black"));
        QCOMPARE(linkColorSpy.count(), 1);

        textObject->setColor(QColor("black"));
        QCOMPARE(textObject->color(), QColor("black"));
        QCOMPARE(colorSpy.count(), 2);

        textObject->setLinkColor(QColor("blue"));
        QCOMPARE(textObject->linkColor(), QColor("blue"));
        QCOMPARE(linkColorSpy.count(), 2);

        delete textObject;
    }

    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nText { linkColor: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QCOMPARE(textObject->styleColor(), QColor("black"));
        QCOMPARE(textObject->color(), QColor("black"));
        QCOMPARE(textObject->linkColor(), QColor(colorStrings.at(i)));

        delete textObject;
    }

    for (int i = 0; i < colorStrings.size(); i++)
    {
        for (int j = 0; j < colorStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 2.0\nText { "
                    "color: \"" + colorStrings.at(i) + "\"; "
                    "styleColor: \"" + colorStrings.at(j) + "\"; "
                    "linkColor: \"" + colorStrings.at(j) + "\"; "
                    "text: \"Hello World\" }";
            QQmlComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

            QCOMPARE(textObject->color(), QColor(colorStrings.at(i)));
            QCOMPARE(textObject->styleColor(), QColor(colorStrings.at(j)));
            QCOMPARE(textObject->linkColor(), QColor(colorStrings.at(j)));

            delete textObject;
        }
    }
    {
        QString colorStr = "#AA001234";
        QColor testColor("#001234");
        testColor.setAlpha(170);

        QString componentStr = "import QtQuick 2.0\nText { color: \"" + colorStr + "\"; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QCOMPARE(textObject->color(), testColor);

        delete textObject;
    } {
        QString colorStr = "#001234";
        QColor testColor(colorStr);

        QString componentStr = "import QtQuick 2.0\nText { color: \"" + colorStr + "\"; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QScopedPointer<QObject> object(textComponent.create());
        QQuickText *textObject = qobject_cast<QQuickText*>(object.data());

        QSignalSpy spy(textObject, SIGNAL(colorChanged()));

        QCOMPARE(textObject->color(), testColor);
        textObject->setColor(testColor);
        QCOMPARE(textObject->color(), testColor);
        QCOMPARE(spy.count(), 0);

        testColor = QColor("black");
        textObject->setColor(testColor);
        QCOMPARE(textObject->color(), testColor);
        QCOMPARE(spy.count(), 1);
    } {
        QString colorStr = "#001234";
        QColor testColor(colorStr);

        QString componentStr = "import QtQuick 2.0\nText { styleColor: \"" + colorStr + "\"; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QScopedPointer<QObject> object(textComponent.create());
        QQuickText *textObject = qobject_cast<QQuickText*>(object.data());

        QSignalSpy spy(textObject, SIGNAL(styleColorChanged()));

        QCOMPARE(textObject->styleColor(), testColor);
        textObject->setStyleColor(testColor);
        QCOMPARE(textObject->styleColor(), testColor);
        QCOMPARE(spy.count(), 0);

        testColor = QColor("black");
        textObject->setStyleColor(testColor);
        QCOMPARE(textObject->styleColor(), testColor);
        QCOMPARE(spy.count(), 1);
    } {
        QString colorStr = "#001234";
        QColor testColor(colorStr);

        QString componentStr = "import QtQuick 2.0\nText { linkColor: \"" + colorStr + "\"; text: \"Hello World\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QScopedPointer<QObject> object(textComponent.create());
        QQuickText *textObject = qobject_cast<QQuickText*>(object.data());

        QSignalSpy spy(textObject, SIGNAL(linkColorChanged()));

        QCOMPARE(textObject->linkColor(), testColor);
        textObject->setLinkColor(testColor);
        QCOMPARE(textObject->linkColor(), testColor);
        QCOMPARE(spy.count(), 0);

        testColor = QColor("black");
        textObject->setLinkColor(testColor);
        QCOMPARE(textObject->linkColor(), testColor);
        QCOMPARE(spy.count(), 1);
    }
}

void tst_qquicktext::smooth()
{
    for (int i = 0; i < standard.size(); i++)
    {
        {
            QString componentStr = "import QtQuick 2.0\nText { smooth: false; text: \"" + standard.at(i) + "\" }";
            QQmlComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());
            QCOMPARE(textObject->smooth(), false);

            delete textObject;
        }
        {
            QString componentStr = "import QtQuick 2.0\nText { text: \"" + standard.at(i) + "\" }";
            QQmlComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());
            QCOMPARE(textObject->smooth(), true);

            delete textObject;
        }
    }
    for (int i = 0; i < richText.size(); i++)
    {
        {
            QString componentStr = "import QtQuick 2.0\nText { smooth: false; text: \"" + richText.at(i) + "\" }";
            QQmlComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());
            QCOMPARE(textObject->smooth(), false);

            delete textObject;
        }
        {
            QString componentStr = "import QtQuick 2.0\nText { text: \"" + richText.at(i) + "\" }";
            QQmlComponent textComponent(&engine);
            textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
            QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());
            QCOMPARE(textObject->smooth(), true);

            delete textObject;
        }
    }
}

void tst_qquicktext::renderType()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n Text {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickText *text = qobject_cast<QQuickText *>(object.data());
    QVERIFY(text);

    QSignalSpy spy(text, SIGNAL(renderTypeChanged()));

    QCOMPARE(text->renderType(), QQuickText::QtRendering);

    text->setRenderType(QQuickText::NativeRendering);
    QCOMPARE(text->renderType(), QQuickText::NativeRendering);
    QCOMPARE(spy.count(), 1);

    text->setRenderType(QQuickText::NativeRendering);
    QCOMPARE(spy.count(), 1);

    text->setRenderType(QQuickText::QtRendering);
    QCOMPARE(text->renderType(), QQuickText::QtRendering);
    QCOMPARE(spy.count(), 2);
}

void tst_qquicktext::antialiasing()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n Text {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickText *text = qobject_cast<QQuickText *>(object.data());
    QVERIFY(text);

    QSignalSpy spy(text, SIGNAL(antialiasingChanged(bool)));

    QCOMPARE(text->antialiasing(), true);

    text->setAntialiasing(false);
    QCOMPARE(text->antialiasing(), false);
    QCOMPARE(spy.count(), 1);

    text->setAntialiasing(false);
    QCOMPARE(spy.count(), 1);

    text->resetAntialiasing();
    QCOMPARE(text->antialiasing(), true);
    QCOMPARE(spy.count(), 2);

    // QTBUG-39047
    component.setData("import QtQuick 2.0\n Text { antialiasing: true }", QUrl());
    object.reset(component.create());
    text = qobject_cast<QQuickText *>(object.data());
    QVERIFY(text);
    QCOMPARE(text->antialiasing(), true);
}

void tst_qquicktext::weight()
{
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().weight(), (int)QQuickFontValueType::Normal);

        delete textObject;
    }
    {
        QString componentStr = "import QtQuick 2.0\nText { font.weight: \"Bold\"; text: \"Hello world!\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().weight(), (int)QQuickFontValueType::Bold);

        delete textObject;
    }
}

void tst_qquicktext::underline()
{
    QQuickView view(testFileUrl("underline.qml"));
    view.show();
    view.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QQuickText *textObject = view.rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(textObject != 0);
    QCOMPARE(textObject->font().overline(), false);
    QCOMPARE(textObject->font().underline(), true);
    QCOMPARE(textObject->font().strikeOut(), false);
}

void tst_qquicktext::overline()
{
    QQuickView view(testFileUrl("overline.qml"));
    view.show();
    view.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QQuickText *textObject = view.rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(textObject != 0);
    QCOMPARE(textObject->font().overline(), true);
    QCOMPARE(textObject->font().underline(), false);
    QCOMPARE(textObject->font().strikeOut(), false);
}

void tst_qquicktext::strikeout()
{
    QQuickView view(testFileUrl("strikeout.qml"));
    view.show();
    view.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QQuickText *textObject = view.rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(textObject != 0);
    QCOMPARE(textObject->font().overline(), false);
    QCOMPARE(textObject->font().underline(), false);
    QCOMPARE(textObject->font().strikeOut(), true);
}

void tst_qquicktext::capitalization()
{
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().capitalization(), (int)QQuickFontValueType::MixedCase);

        delete textObject;
    }
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\"; font.capitalization: \"AllUppercase\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().capitalization(), (int)QQuickFontValueType::AllUppercase);

        delete textObject;
    }
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\"; font.capitalization: \"AllLowercase\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().capitalization(), (int)QQuickFontValueType::AllLowercase);

        delete textObject;
    }
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\"; font.capitalization: \"SmallCaps\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().capitalization(), (int)QQuickFontValueType::SmallCaps);

        delete textObject;
    }
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\"; font.capitalization: \"Capitalize\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().capitalization(), (int)QQuickFontValueType::Capitalize);

        delete textObject;
    }
}

void tst_qquicktext::letterSpacing()
{
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().letterSpacing(), 0.0);

        delete textObject;
    }
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\"; font.letterSpacing: -2 }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().letterSpacing(), -2.);

        delete textObject;
    }
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\"; font.letterSpacing: 3 }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().letterSpacing(), 3.);

        delete textObject;
    }
}

void tst_qquicktext::wordSpacing()
{
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().wordSpacing(), 0.0);

        delete textObject;
    }
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\"; font.wordSpacing: -50 }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().wordSpacing(), -50.);

        delete textObject;
    }
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\"; font.wordSpacing: 200 }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->font().wordSpacing(), 200.);

        delete textObject;
    }
}

class EventSender : public QQuickItem
{
public:
    void sendEvent(QEvent *event) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            mousePressEvent(static_cast<QMouseEvent *>(event));
            break;
        case QEvent::MouseButtonRelease:
            mouseReleaseEvent(static_cast<QMouseEvent *>(event));
            break;
        case QEvent::MouseMove:
            mouseMoveEvent(static_cast<QMouseEvent *>(event));
            break;
        case QEvent::HoverEnter:
            hoverEnterEvent(static_cast<QHoverEvent *>(event));
            break;
        case QEvent::HoverLeave:
            hoverLeaveEvent(static_cast<QHoverEvent *>(event));
            break;
        case QEvent::HoverMove:
            hoverMoveEvent(static_cast<QHoverEvent *>(event));
            break;
        default:
            qWarning() << "Trying to send unsupported event type";
            break;
        }
    }
};

class LinkTest : public QObject
{
    Q_OBJECT
public:
    LinkTest() {}

    QString clickedLink;
    QString hoveredLink;

public slots:
    void linkClicked(QString l) { clickedLink = l; }
    void linkHovered(QString l) { hoveredLink = l; }
};

class TextMetrics
{
public:
    TextMetrics(const QString &text, Qt::TextElideMode elideMode = Qt::ElideNone)
    {
        QString adjustedText = text;
        adjustedText.replace(QLatin1Char('\n'), QChar(QChar::LineSeparator));
        if (elideMode == Qt::ElideLeft)
            adjustedText = QChar(0x2026) + adjustedText;
        else if (elideMode == Qt::ElideRight)
            adjustedText = adjustedText + QChar(0x2026);

        layout.setText(adjustedText);
        QTextOption option;
        option.setUseDesignMetrics(true);
        layout.setTextOption(option);

        layout.beginLayout();
        qreal height = 0;
        QTextLine line = layout.createLine();
        while (line.isValid()) {
            line.setLineWidth(FLT_MAX);
            line.setPosition(QPointF(0, height));
            height += line.height();
            line = layout.createLine();
        }
        layout.endLayout();
    }

    qreal width() const { return layout.maximumWidth(); }

    QRectF characterRectangle(
            int position,
            int hAlign = Qt::AlignLeft,
            int vAlign = Qt::AlignTop,
            const QSizeF &bounds = QSizeF(240, 320)) const
    {
        qreal dy = 0;
        switch (vAlign) {
        case Qt::AlignBottom:
            dy = bounds.height() - layout.boundingRect().height();
            break;
        case Qt::AlignVCenter:
            dy = (bounds.height() - layout.boundingRect().height()) / 2;
            break;
        default:
            break;
        }

        for (int i = 0; i < layout.lineCount(); ++i) {
            QTextLine line = layout.lineAt(i);
            if (position >= line.textStart() + line.textLength())
                continue;
            qreal dx = 0;
            switch (hAlign) {
            case Qt::AlignRight:
                dx = bounds.width() - line.naturalTextWidth();
                break;
            case Qt::AlignHCenter:
                dx = (bounds.width() - line.naturalTextWidth()) / 2;
                break;
            default:
                break;
            }

            QRectF rect;
            rect.setLeft(dx + line.cursorToX(position, QTextLine::Leading));
            rect.setRight(dx + line.cursorToX(position, QTextLine::Trailing));
            rect.setTop(dy + line.y());
            rect.setBottom(dy + line.y() + line.height());

            return rect;
        }
        return QRectF();
    }

    QTextLayout layout;
};


typedef QVector<QPointF> PointVector;
Q_DECLARE_METATYPE(PointVector);

void tst_qquicktext::linkInteraction_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<qreal>("width");
    QTest::addColumn<QString>("bindings");
    QTest::addColumn<PointVector>("mousePositions");
    QTest::addColumn<QString>("clickedLink");
    QTest::addColumn<QString>("hoverEnterLink");
    QTest::addColumn<QString>("hoverMoveLink");

    const QString singleLineText = "this text has a <a href=\\\"http://qt-project.org/single\\\">link</a> in it";
    const QString singleLineLink = "http://qt-project.org/single";
    const QString multipleLineText = "this text<br/>has <a href=\\\"http://qt-project.org/multiple\\\">multiple<br/>lines</a> in it";
    const QString multipleLineLink = "http://qt-project.org/multiple";
    const QString nestedText = "this text has a <a href=\\\"http://qt-project.org/outer\\\">nested <a href=\\\"http://qt-project.org/inner\\\">link</a> in it</a>";
    const QString outerLink = "http://qt-project.org/outer";
    const QString innerLink = "http://qt-project.org/inner";

    {
        const TextMetrics metrics("this text has a link in it");

        QTest::newRow("click on link")
                << singleLineText << 240.
                << ""
                << (PointVector() << metrics.characterRectangle(18).center())
                << singleLineLink
                << singleLineLink << singleLineLink;
        QTest::newRow("click on text")
                << singleLineText << 240.
                << ""
                << (PointVector() << metrics.characterRectangle(13).center())
                << QString()
                << QString() << QString();
        QTest::newRow("drag within link")
                << singleLineText << 240.
                << ""
                << (PointVector()
                    << metrics.characterRectangle(17).center()
                    << metrics.characterRectangle(19).center())
                << singleLineLink
                << singleLineLink << singleLineLink;
        QTest::newRow("drag away from link")
                << singleLineText << 240.
                << ""
                << (PointVector()
                    << metrics.characterRectangle(18).center()
                    << metrics.characterRectangle(13).center())
                << QString()
                << singleLineLink << QString();
        QTest::newRow("drag on to link")
                << singleLineText << 240.
                << ""
                << (PointVector()
                    << metrics.characterRectangle(13).center()
                    << metrics.characterRectangle(18).center())
                << QString()
                << QString() << singleLineLink;
        QTest::newRow("click on bottom right aligned link")
                << singleLineText << 240.
                << "horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignBottom"
                << (PointVector() << metrics.characterRectangle(18, Qt::AlignRight, Qt::AlignBottom).center())
                << singleLineLink
                << singleLineLink << singleLineLink;
        QTest::newRow("click on mirrored link")
                << singleLineText << 240.
                << "horizontalAlignment: Text.AlignLeft; LayoutMirroring.enabled: true"
                << (PointVector() << metrics.characterRectangle(18, Qt::AlignRight, Qt::AlignTop).center())
                << singleLineLink
                << singleLineLink << singleLineLink;
        QTest::newRow("click on center aligned link")
                << singleLineText << 240.
                << "horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter"
                << (PointVector() << metrics.characterRectangle(18, Qt::AlignHCenter, Qt::AlignVCenter).center())
                << singleLineLink
                << singleLineLink << singleLineLink;
        QTest::newRow("click on rich text link")
                << singleLineText << 240.
                << "textFormat: Text.RichText"
                << (PointVector() << metrics.characterRectangle(18).center())
                << singleLineLink
                << singleLineLink << singleLineLink;
        QTest::newRow("click on rich text")
                << singleLineText << 240.
                << "textFormat: Text.RichText"
                << (PointVector() << metrics.characterRectangle(13).center())
                << QString()
                << QString() << QString();
        QTest::newRow("click on bottom right aligned rich text link")
                << singleLineText << 240.
                << "textFormat: Text.RichText; horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignBottom"
                << (PointVector() << metrics.characterRectangle(18, Qt::AlignRight, Qt::AlignBottom).center())
                << singleLineLink
                << singleLineLink << singleLineLink;
        QTest::newRow("click on center aligned rich text link")
                << singleLineText << 240.
                << "textFormat: Text.RichText; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter"
                << (PointVector() << metrics.characterRectangle(18, Qt::AlignHCenter, Qt::AlignVCenter).center())
                << singleLineLink
                << singleLineLink << singleLineLink;
    } {
        const TextMetrics metrics("this text has a li", Qt::ElideRight);
        QTest::newRow("click on right elided link")
                << singleLineText << metrics.width() +  2
                << "elide: Text.ElideRight"
                << (PointVector() << metrics.characterRectangle(17).center())
                << singleLineLink
                << singleLineLink << singleLineLink;
    } {
        const TextMetrics metrics("ink in it", Qt::ElideLeft);
        QTest::newRow("click on left elided link")
                << singleLineText << metrics.width() +  2
                << "elide: Text.ElideLeft"
                << (PointVector() << metrics.characterRectangle(2).center())
                << singleLineLink
                << singleLineLink << singleLineLink;
    } {
        const TextMetrics metrics("this text\nhas multiple\nlines in it");
        QTest::newRow("click on second line")
                << multipleLineText << 240.
                << ""
                << (PointVector() << metrics.characterRectangle(18).center())
                << multipleLineLink
                << multipleLineLink << multipleLineLink;
        QTest::newRow("click on third line")
                << multipleLineText << 240.
                << ""
                << (PointVector() << metrics.characterRectangle(25).center())
                << multipleLineLink
                << multipleLineLink << multipleLineLink;
        QTest::newRow("drag from second line to third")
                << multipleLineText << 240.
                << ""
                << (PointVector()
                    << metrics.characterRectangle(18).center()
                    << metrics.characterRectangle(25).center())
                << multipleLineLink
                << multipleLineLink << multipleLineLink;
        QTest::newRow("click on rich text second line")
                << multipleLineText << 240.
                << "textFormat: Text.RichText"
                << (PointVector() << metrics.characterRectangle(18).center())
                << multipleLineLink
                << multipleLineLink << multipleLineLink;
        QTest::newRow("click on rich text third line")
                << multipleLineText << 240.
                << "textFormat: Text.RichText"
                << (PointVector() << metrics.characterRectangle(25).center())
                << multipleLineLink
                << multipleLineLink << multipleLineLink;
        QTest::newRow("drag rich text from second line to third")
                << multipleLineText << 240.
                << "textFormat: Text.RichText"
                << (PointVector()
                    << metrics.characterRectangle(18).center()
                    << metrics.characterRectangle(25).center())
                << multipleLineLink
                << multipleLineLink << multipleLineLink;
    } {
        const TextMetrics metrics("this text has a nested link in it");
        QTest::newRow("click on left outer link")
                << nestedText << 240.
                << ""
                << (PointVector() << metrics.characterRectangle(22).center())
                << outerLink
                << outerLink << outerLink;
        QTest::newRow("click on right outer link")
                << nestedText << 240.
                << ""
                << (PointVector() << metrics.characterRectangle(27).center())
                << outerLink
                << outerLink << outerLink;
        QTest::newRow("click on inner link left")
                << nestedText << 240.
                << ""
                << (PointVector() << metrics.characterRectangle(23).center())
                << innerLink
                << innerLink << innerLink;
        QTest::newRow("click on inner link right")
                << nestedText << 240.
                << ""
                << (PointVector() << metrics.characterRectangle(26).center())
                << innerLink
                << innerLink << innerLink;
        QTest::newRow("drag from inner to outer link")
                << nestedText << 240.
                << ""
                << (PointVector()
                    << metrics.characterRectangle(25).center()
                    << metrics.characterRectangle(30).center())
                << QString()
                << innerLink << outerLink;
        QTest::newRow("drag from outer to inner link")
                << nestedText << 240.
                << ""
                << (PointVector()
                    << metrics.characterRectangle(30).center()
                    << metrics.characterRectangle(25).center())
                << QString()
                << outerLink << innerLink;
        QTest::newRow("click on left outer rich text link")
                << nestedText << 240.
                << "textFormat: Text.RichText"
                << (PointVector() << metrics.characterRectangle(22).center())
                << outerLink
                << outerLink << outerLink;
        QTest::newRow("click on right outer rich text link")
                << nestedText << 240.
                << "textFormat: Text.RichText"
                << (PointVector() << metrics.characterRectangle(27).center())
                << outerLink
                << outerLink << outerLink;
        QTest::newRow("click on inner rich text link left")
                << nestedText << 240.
                << "textFormat: Text.RichText"
                << (PointVector() << metrics.characterRectangle(23).center())
                << innerLink
                << innerLink << innerLink;
        QTest::newRow("click on inner rich text link right")
                << nestedText << 240.
                << "textFormat: Text.RichText"
                << (PointVector() << metrics.characterRectangle(26).center())
                << innerLink
                << innerLink << innerLink;
        QTest::newRow("drag from inner to outer rich text link")
                << nestedText << 240.
                << "textFormat: Text.RichText"
                << (PointVector()
                    << metrics.characterRectangle(25).center()
                    << metrics.characterRectangle(30).center())
                << QString()
                << innerLink << outerLink;
        QTest::newRow("drag from outer to inner rich text link")
                << nestedText << 240.
                << "textFormat: Text.RichText"
                << (PointVector()
                    << metrics.characterRectangle(30).center()
                    << metrics.characterRectangle(25).center())
                << QString()
                << outerLink << innerLink;
    }
}

void tst_qquicktext::linkInteraction()
{
    QFETCH(QString, text);
    QFETCH(qreal, width);
    QFETCH(QString, bindings);
    QFETCH(PointVector, mousePositions);
    QFETCH(QString, clickedLink);
    QFETCH(QString, hoverEnterLink);
    QFETCH(QString, hoverMoveLink);

    QString componentStr =
            "import QtQuick 2.2\nText {\n"
                "width: " + QString::number(width) + "\n"
                "height: 320\n"
                "text: \"" + text + "\"\n"
                "" + bindings + "\n"
            "}";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

    QVERIFY(textObject != 0);

    LinkTest test;
    QObject::connect(textObject, SIGNAL(linkActivated(QString)), &test, SLOT(linkClicked(QString)));
    QObject::connect(textObject, SIGNAL(linkHovered(QString)), &test, SLOT(linkHovered(QString)));

    QVERIFY(mousePositions.count() > 0);

    QPointF mousePosition = mousePositions.first();
    {
        QHoverEvent he(QEvent::HoverEnter, mousePosition, QPointF());
        static_cast<EventSender*>(static_cast<QQuickItem*>(textObject))->sendEvent(&he);

        QMouseEvent me(QEvent::MouseButtonPress, mousePosition, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        static_cast<EventSender*>(static_cast<QQuickItem*>(textObject))->sendEvent(&me);
    }

    QCOMPARE(test.hoveredLink, hoverEnterLink);
    QCOMPARE(textObject->hoveredLink(), hoverEnterLink);
    QCOMPARE(textObject->linkAt(mousePosition.x(), mousePosition.y()), hoverEnterLink);

    for (int i = 1; i < mousePositions.count(); ++i) {
        mousePosition = mousePositions.at(i);

        QHoverEvent he(QEvent::HoverMove, mousePosition, QPointF());
        static_cast<EventSender*>(static_cast<QQuickItem*>(textObject))->sendEvent(&he);

        QMouseEvent me(QEvent::MouseMove, mousePosition, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        static_cast<EventSender*>(static_cast<QQuickItem*>(textObject))->sendEvent(&me);
    }

    QCOMPARE(test.hoveredLink, hoverMoveLink);
    QCOMPARE(textObject->hoveredLink(), hoverMoveLink);
    QCOMPARE(textObject->linkAt(mousePosition.x(), mousePosition.y()), hoverMoveLink);

    {
        QHoverEvent he(QEvent::HoverLeave, mousePosition, QPointF());
        static_cast<EventSender*>(static_cast<QQuickItem*>(textObject))->sendEvent(&he);

        QMouseEvent me(QEvent::MouseButtonRelease, mousePosition, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        static_cast<EventSender*>(static_cast<QQuickItem*>(textObject))->sendEvent(&me);
    }

    QCOMPARE(test.clickedLink, clickedLink);
    QCOMPARE(test.hoveredLink, QString());
    QCOMPARE(textObject->hoveredLink(), QString());
    QCOMPARE(textObject->linkAt(-1, -1), QString());

    delete textObject;
}

void tst_qquicktext::baseUrl()
{
    QUrl localUrl("file:///tests/text.qml");
    QUrl remoteUrl("http://www.qt-project.org/test.qml");

    QQmlComponent textComponent(&engine);
    textComponent.setData("import QtQuick 2.0\n Text {}", localUrl);
    QQuickText *textObject = qobject_cast<QQuickText *>(textComponent.create());

    QCOMPARE(textObject->baseUrl(), localUrl);

    QSignalSpy spy(textObject, SIGNAL(baseUrlChanged()));

    textObject->setBaseUrl(localUrl);
    QCOMPARE(textObject->baseUrl(), localUrl);
    QCOMPARE(spy.count(), 0);

    textObject->setBaseUrl(remoteUrl);
    QCOMPARE(textObject->baseUrl(), remoteUrl);
    QCOMPARE(spy.count(), 1);

    textObject->resetBaseUrl();
    QCOMPARE(textObject->baseUrl(), localUrl);
    QCOMPARE(spy.count(), 2);
}

void tst_qquicktext::embeddedImages_data()
{
    QTest::addColumn<QUrl>("qmlfile");
    QTest::addColumn<QString>("error");
    QTest::newRow("local") << testFileUrl("embeddedImagesLocal.qml") << "";
    QTest::newRow("local-error") << testFileUrl("embeddedImagesLocalError.qml")
        << testFileUrl("embeddedImagesLocalError.qml").toString()+":3:1: QML Text: Cannot open: " + testFileUrl("http/notexists.png").toString();
    QTest::newRow("local") << testFileUrl("embeddedImagesLocalRelative.qml") << "";
    QTest::newRow("remote") << testFileUrl("embeddedImagesRemote.qml") << "";
    QTest::newRow("remote-error") << testFileUrl("embeddedImagesRemoteError.qml")
                                  << testFileUrl("embeddedImagesRemoteError.qml").toString()+":3:1: QML Text: Error transferring {{ServerBaseUrl}}/notexists.png - server replied: Not found";
    QTest::newRow("remote-relative") << testFileUrl("embeddedImagesRemoteRelative.qml") << "";
}

void tst_qquicktext::embeddedImages()
{
    // Tests QTBUG-9900

    QFETCH(QUrl, qmlfile);
    QFETCH(QString, error);

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (qstrcmp(QTest::currentDataTag(), "remote") == 0
        || qstrcmp(QTest::currentDataTag(), "remote-error") == 0
        || qstrcmp(QTest::currentDataTag(), "remote-relative") == 0) {
        QSKIP("Remote tests cause occasional hangs in the CI system -- QTBUG-45655");
    }
#endif

    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(testFile("http"));
    error.replace(QStringLiteral("{{ServerBaseUrl}}"), server.baseUrl().toString());

    if (!error.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, error.toLatin1());

    QQuickView *view = new QQuickView;
    view->rootContext()->setContextProperty(QStringLiteral("serverBaseUrl"), server.baseUrl());
    view->setSource(qmlfile);
    view->show();
    view->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(view));
    QQuickText *textObject = qobject_cast<QQuickText*>(view->rootObject());

    QVERIFY(textObject != 0);
    QTRY_COMPARE(textObject->resourcesLoading(), 0);

    QPixmap pm(testFile("http/exists.png"));
    if (error.isEmpty()) {
        QCOMPARE(textObject->width(), double(pm.width()));
        QCOMPARE(textObject->height(), double(pm.height()));
    } else {
        QVERIFY(16 != pm.width()); // check test is effective
        QCOMPARE(textObject->width(), 16.0); // default size of QTextDocument broken image icon
        QCOMPARE(textObject->height(), 16.0);
    }

    delete view;
}

void tst_qquicktext::lineCount()
{
    QScopedPointer<QQuickView> window(createView(testFile("lineCount.qml")));

    QQuickText *myText = window->rootObject()->findChild<QQuickText*>("myText");
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

    myText->setElideMode(QQuickText::ElideRight);
    myText->setMaximumLineCount(2);
    QCOMPARE(myText->lineCount(), 2);
    QCOMPARE(myText->truncated(), true);
    QCOMPARE(myText->maximumLineCount(), 2);
}

void tst_qquicktext::lineHeight()
{
    QScopedPointer<QQuickView> window(createView(testFile("lineHeight.qml")));

    QQuickText *myText = window->rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(myText != 0);

    QCOMPARE(myText->lineHeight(), qreal(1));
    QCOMPARE(myText->lineHeightMode(), QQuickText::ProportionalHeight);

    qreal h = myText->height();
    myText->setLineHeight(1.5);
    QCOMPARE(myText->height(), qreal(qCeil(h)) * 1.5);

    myText->setLineHeightMode(QQuickText::FixedHeight);
    myText->setLineHeight(20);
    QCOMPARE(myText->height(), myText->lineCount() * 20.0);

    myText->setText("Lorem ipsum sit <b>amet</b>, consectetur adipiscing elit. Integer felis nisl, varius in pretium nec, venenatis non erat. Proin lobortis interdum dictum.");
    myText->setLineHeightMode(QQuickText::ProportionalHeight);
    myText->setLineHeight(1.0);

    qreal h2 = myText->height();
    myText->setLineHeight(2.0);
    QVERIFY(myText->height() == h2 * 2.0);

    myText->setLineHeightMode(QQuickText::FixedHeight);
    myText->setLineHeight(10);
    QCOMPARE(myText->height(), myText->lineCount() * 10.0);
}

void tst_qquicktext::implicitSize_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("width");
    QTest::addColumn<QString>("wrap");
    QTest::addColumn<QString>("elide");
    QTest::addColumn<QString>("format");
    QTest::newRow("plain") << "The quick red fox jumped over the lazy brown dog" << "50" << "Text.NoWrap" << "Text.ElideNone" << "Text.PlainText";
    QTest::newRow("richtext") << "<b>The quick red fox jumped over the lazy brown dog</b>" <<" 50" << "Text.NoWrap" << "Text.ElideNone" << "Text.RichText";
    QTest::newRow("styledtext") << "<b>The quick red fox jumped over the lazy brown dog</b>" <<" 50" << "Text.NoWrap" << "Text.ElideNone" << "Text.StyledText";
    QTest::newRow("plain, 0 width") << "The quick red fox jumped over the lazy brown dog" << "0" << "Text.NoWrap" << "Text.ElideNone" << "Text.PlainText";
    QTest::newRow("plain, elide") << "The quick red fox jumped over the lazy brown dog" << "50" << "Text.NoWrap" << "Text.ElideRight" << "Text.PlainText";
    QTest::newRow("plain, 0 width, elide") << "The quick red fox jumped over the lazy brown dog" << "0" << "Text.NoWrap" << "Text.ElideRight" << "Text.PlainText";
    QTest::newRow("richtext, 0 width") << "<b>The quick red fox jumped over the lazy brown dog</b>" <<" 0" << "Text.NoWrap" << "Text.ElideNone" << "Text.RichText";
    QTest::newRow("styledtext, 0 width") << "<b>The quick red fox jumped over the lazy brown dog</b>" <<" 0" << "Text.NoWrap" << "Text.ElideNone" << "Text.StyledText";
    QTest::newRow("plain_wrap") << "The quick red fox jumped over the lazy brown dog" << "50" << "Text.Wrap" << "Text.ElideNone" << "Text.PlainText";
    QTest::newRow("richtext_wrap") << "<b>The quick red fox jumped over the lazy brown dog</b>" << "50" << "Text.Wrap" << "Text.ElideNone" << "Text.RichText";
    QTest::newRow("styledtext_wrap") << "<b>The quick red fox jumped over the lazy brown dog</b>" << "50" << "Text.Wrap" << "Text.ElideNone" << "Text.StyledText";
    QTest::newRow("plain_wrap, 0 width") << "The quick red fox jumped over the lazy brown dog" << "0" << "Text.Wrap" << "Text.ElideNone" << "Text.PlainText";
    QTest::newRow("plain_wrap, elide") << "The quick red fox jumped over the lazy brown dog" << "50" << "Text.Wrap" << "Text.ElideRight" << "Text.PlainText";
    QTest::newRow("plain_wrap, 0 width, elide") << "The quick red fox jumped over the lazy brown dog" << "0" << "Text.Wrap" << "Text.ElideRight" << "Text.PlainText";
    QTest::newRow("richtext_wrap, 0 width") << "<b>The quick red fox jumped over the lazy brown dog</b>" << "0" << "Text.Wrap" << "Text.ElideNone" << "Text.RichText";
    QTest::newRow("styledtext_wrap, 0 width") << "<b>The quick red fox jumped over the lazy brown dog</b>" << "0" << "Text.Wrap" << "Text.ElideNone" << "Text.StyledText";
}

void tst_qquicktext::implicitSize()
{
    QFETCH(QString, text);
    QFETCH(QString, width);
    QFETCH(QString, format);
    QFETCH(QString, wrap);
    QFETCH(QString, elide);
    QString componentStr = "import QtQuick 2.0\nText { "
            "property real iWidth: implicitWidth; "
            "text: \"" + text + "\"; "
            "width: " + width + "; "
            "textFormat: " + format + "; "
            "wrapMode: " + wrap + "; "
            "elide: " + elide + "; "
            "maximumLineCount: 2 }";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

    QVERIFY(textObject->width() < textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());
    QCOMPARE(textObject->property("iWidth").toReal(), textObject->implicitWidth());

    textObject->resetWidth();
    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());

    delete textObject;
}

void tst_qquicktext::dependentImplicitSizes()
{
    QQmlComponent component(&engine, testFile("implicitSizes.qml"));
    QScopedPointer<QObject> object(component.create());
    QVERIFY(object.data());

    QQuickText *reference = object->findChild<QQuickText *>("reference");
    QQuickText *fixedWidthAndHeight = object->findChild<QQuickText *>("fixedWidthAndHeight");
    QQuickText *implicitWidthFixedHeight = object->findChild<QQuickText *>("implicitWidthFixedHeight");
    QQuickText *fixedWidthImplicitHeight = object->findChild<QQuickText *>("fixedWidthImplicitHeight");
    QQuickText *cappedWidthAndHeight = object->findChild<QQuickText *>("cappedWidthAndHeight");
    QQuickText *cappedWidthFixedHeight = object->findChild<QQuickText *>("cappedWidthFixedHeight");
    QQuickText *fixedWidthCappedHeight = object->findChild<QQuickText *>("fixedWidthCappedHeight");

    QVERIFY(reference);
    QVERIFY(fixedWidthAndHeight);
    QVERIFY(implicitWidthFixedHeight);
    QVERIFY(fixedWidthImplicitHeight);
    QVERIFY(cappedWidthAndHeight);
    QVERIFY(cappedWidthFixedHeight);
    QVERIFY(fixedWidthCappedHeight);

    QCOMPARE(reference->width(), reference->implicitWidth());
    QCOMPARE(reference->height(), reference->implicitHeight());

    QVERIFY(fixedWidthAndHeight->width() < fixedWidthAndHeight->implicitWidth());
    QVERIFY(fixedWidthAndHeight->height() < fixedWidthAndHeight->implicitHeight());
    QCOMPARE(fixedWidthAndHeight->implicitWidth(), reference->implicitWidth());
    QVERIFY(fixedWidthAndHeight->implicitHeight() > reference->implicitHeight());

    QCOMPARE(implicitWidthFixedHeight->width(), implicitWidthFixedHeight->implicitWidth());
    QVERIFY(implicitWidthFixedHeight->height() < implicitWidthFixedHeight->implicitHeight());
    QCOMPARE(implicitWidthFixedHeight->implicitWidth(), reference->implicitWidth());
    QCOMPARE(implicitWidthFixedHeight->implicitHeight(), reference->implicitHeight());

    QVERIFY(fixedWidthImplicitHeight->width() < fixedWidthImplicitHeight->implicitWidth());
    QCOMPARE(fixedWidthImplicitHeight->height(), fixedWidthImplicitHeight->implicitHeight());
    QCOMPARE(fixedWidthImplicitHeight->implicitWidth(), reference->implicitWidth());
    QCOMPARE(fixedWidthImplicitHeight->implicitHeight(), fixedWidthAndHeight->implicitHeight());

    QVERIFY(cappedWidthAndHeight->width() < cappedWidthAndHeight->implicitWidth());
    QVERIFY(cappedWidthAndHeight->height() < cappedWidthAndHeight->implicitHeight());
    QCOMPARE(cappedWidthAndHeight->implicitWidth(), reference->implicitWidth());
    QCOMPARE(cappedWidthAndHeight->implicitHeight(), fixedWidthAndHeight->implicitHeight());

    QVERIFY(cappedWidthFixedHeight->width() < cappedWidthAndHeight->implicitWidth());
    QVERIFY(cappedWidthFixedHeight->height() < cappedWidthFixedHeight->implicitHeight());
    QCOMPARE(cappedWidthFixedHeight->implicitWidth(), reference->implicitWidth());
    QCOMPARE(cappedWidthFixedHeight->implicitHeight(), fixedWidthAndHeight->implicitHeight());

    QVERIFY(fixedWidthCappedHeight->width() < fixedWidthCappedHeight->implicitWidth());
    QVERIFY(fixedWidthCappedHeight->height() < fixedWidthCappedHeight->implicitHeight());
    QCOMPARE(fixedWidthCappedHeight->implicitWidth(), reference->implicitWidth());
    QCOMPARE(fixedWidthCappedHeight->implicitHeight(), fixedWidthAndHeight->implicitHeight());
}

void tst_qquicktext::contentSize()
{
    QString componentStr = "import QtQuick 2.0\nText { width: 75; height: 16; font.pixelSize: 10 }";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QScopedPointer<QObject> object(textComponent.create());
    QQuickText *textObject = qobject_cast<QQuickText *>(object.data());

    QSignalSpy spy(textObject, SIGNAL(contentSizeChanged()));

    textObject->setText("The quick red fox jumped over the lazy brown dog");

    QVERIFY(textObject->contentWidth() > textObject->width());
    QVERIFY(textObject->contentHeight() < textObject->height());
    QCOMPARE(spy.count(), 1);

    textObject->setWrapMode(QQuickText::WordWrap);
    QVERIFY(textObject->contentWidth() <= textObject->width());
    QVERIFY(textObject->contentHeight() > textObject->height());
    QCOMPARE(spy.count(), 2);

    textObject->setElideMode(QQuickText::ElideRight);
    QVERIFY(textObject->contentWidth() <= textObject->width());
    QVERIFY(textObject->contentHeight() < textObject->height());
    QCOMPARE(spy.count(), 3);
    int spyCount = 3;
    qreal elidedWidth = textObject->contentWidth();

    textObject->setText("The quickredfoxjumpedoverthe lazy brown dog");
    QVERIFY(textObject->contentWidth() <= textObject->width());
    QVERIFY(textObject->contentHeight() < textObject->height());
    // this text probably won't have the same elided width, but it's not guaranteed.
    if (textObject->contentWidth() != elidedWidth)
        QCOMPARE(spy.count(), ++spyCount);
    else
        QCOMPARE(spy.count(), spyCount);

    textObject->setElideMode(QQuickText::ElideNone);
    QVERIFY(textObject->contentWidth() > textObject->width());
    QVERIFY(textObject->contentHeight() > textObject->height());
    QCOMPARE(spy.count(), ++spyCount);
}

void tst_qquicktext::geometryChanged()
{
    // Test that text is re-laid out when the geometry of the item by verifying changes in content
    // size.  Implicit width is also tested as that in combination with item geometry provides a
    // reference for expected content sizes.

    QString componentStr = "import QtQuick 2.0\nText { font.family: \"__Qt__Box__Engine__\"; font.pixelSize: 10 }";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QScopedPointer<QObject> object(textComponent.create());
    QQuickText *textObject = qobject_cast<QQuickText *>(object.data());

    const qreal implicitHeight = textObject->implicitHeight();

    const qreal widths[] = { 100, 2000, 3000, -100, 100 };
    const qreal heights[] = { implicitHeight, 2000, 3000, -implicitHeight, implicitHeight };

    QCOMPARE(textObject->implicitWidth(), 0.);
    QVERIFY(implicitHeight > 0.);
    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), implicitHeight);
    QCOMPARE(textObject->contentWidth(), textObject->implicitWidth());
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setText("The quick red fox jumped over the lazy brown dog");

    const qreal implicitWidth = textObject->implicitWidth();

    QVERIFY(implicitWidth > 0.);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());
    QCOMPARE(textObject->contentWidth(), textObject->implicitWidth());
    QCOMPARE(textObject->contentHeight(), textObject->implicitHeight());

    // Changing the geometry with no eliding, or wrapping doesn't change the content size.
    for (int i = 0; i < 5; ++i) {
        textObject->setWidth(widths[i]);
        QCOMPARE(textObject->implicitWidth(), implicitWidth);
        QCOMPARE(textObject->implicitHeight(), implicitHeight);
        QCOMPARE(textObject->width(), widths[i]);
        QCOMPARE(textObject->height(), implicitHeight);
        QCOMPARE(textObject->contentWidth(), implicitWidth);
        QCOMPARE(textObject->contentHeight(), implicitHeight);
    }

    // With eliding enabled the content width is bounded to the item width, but is never
    // larger than the implicit width.
    textObject->setElideMode(QQuickText::ElideRight);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), 100.);
    QCOMPARE(textObject->height(), implicitHeight);
    QVERIFY(textObject->contentWidth() <= 100.);
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setWidth(2000.);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), 2000.);
    QCOMPARE(textObject->height(), implicitHeight);
    QCOMPARE(textObject->contentWidth(), implicitWidth);
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setWidth(3000.);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), 3000.);
    QCOMPARE(textObject->height(), implicitHeight);
    QCOMPARE(textObject->contentWidth(), implicitWidth);
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setWidth(-100);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), -100.);
    QCOMPARE(textObject->height(), implicitHeight);
    QCOMPARE(textObject->contentWidth(), 0.);
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setWidth(100.);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), 100.);
    QCOMPARE(textObject->height(), implicitHeight);
    QVERIFY(textObject->contentWidth() <= 100.);
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    // With wrapping enabled the implicit height changes with the width.
    textObject->setElideMode(QQuickText::ElideNone);
    textObject->setWrapMode(QQuickText::Wrap);
    const qreal wrappedImplicitHeight = textObject->implicitHeight();

    QVERIFY(wrappedImplicitHeight > implicitHeight);

    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->width(), 100.);
    QCOMPARE(textObject->height(), wrappedImplicitHeight);
    QVERIFY(textObject->contentWidth() <= 100.);
    QCOMPARE(textObject->contentHeight(), wrappedImplicitHeight);

    textObject->setWidth(2000.);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), 2000.);
    QCOMPARE(textObject->height(), implicitHeight);
    QCOMPARE(textObject->contentWidth(), implicitWidth);
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setWidth(3000.);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), 3000.);
    QCOMPARE(textObject->height(), implicitHeight);
    QCOMPARE(textObject->contentWidth(), implicitWidth);
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setWidth(-100);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), -100.);
    QCOMPARE(textObject->height(), implicitHeight);
    QCOMPARE(textObject->contentWidth(), implicitWidth);    // 0 or negative width item won't wrap.
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setWidth(100.);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), wrappedImplicitHeight);
    QCOMPARE(textObject->width(), 100.);
    QCOMPARE(textObject->height(), wrappedImplicitHeight);
    QVERIFY(textObject->contentWidth() <= 100.);
    QCOMPARE(textObject->contentHeight(), wrappedImplicitHeight);

    // With no eliding or maximum line count the content height is the same as the implicit height.
    for (int i = 0; i < 5; ++i) {
        textObject->setHeight(heights[i]);
        QCOMPARE(textObject->implicitWidth(), implicitWidth);
        QCOMPARE(textObject->implicitHeight(), wrappedImplicitHeight);
        QCOMPARE(textObject->width(), 100.);
        QCOMPARE(textObject->height(), heights[i]);
        QVERIFY(textObject->contentWidth() <= 100.);
        QCOMPARE(textObject->contentHeight(), wrappedImplicitHeight);
    }

    // The implicit height is unaffected by eliding but the content height will change.
    textObject->setElideMode(QQuickText::ElideRight);

    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), wrappedImplicitHeight);
    QCOMPARE(textObject->width(), 100.);
    QCOMPARE(textObject->height(), implicitHeight);
    QVERIFY(textObject->contentWidth() <= 100.);
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setHeight(2000);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), wrappedImplicitHeight);
    QCOMPARE(textObject->width(), 100.);
    QCOMPARE(textObject->height(), 2000.);
    QVERIFY(textObject->contentWidth() <= 100.);
    QCOMPARE(textObject->contentHeight(), wrappedImplicitHeight);

    textObject->setHeight(3000);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), wrappedImplicitHeight);
    QCOMPARE(textObject->width(), 100.);
    QCOMPARE(textObject->height(), 3000.);
    QVERIFY(textObject->contentWidth() <= 100.);
    QCOMPARE(textObject->contentHeight(), wrappedImplicitHeight);

    textObject->setHeight(-implicitHeight);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), wrappedImplicitHeight);
    QCOMPARE(textObject->width(), 100.);
    QCOMPARE(textObject->height(), -implicitHeight);
    QVERIFY(textObject->contentWidth() <= 0.);
    QCOMPARE(textObject->contentHeight(), implicitHeight);  // content height is never less than font height. seems a little odd in this instance.

    textObject->setHeight(implicitHeight);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), wrappedImplicitHeight);
    QCOMPARE(textObject->width(), 100.);
    QCOMPARE(textObject->height(), implicitHeight);
    QVERIFY(textObject->contentWidth() <= 100.);
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    // Varying the height with a maximum line count but no eliding won't affect the content height.
    textObject->setElideMode(QQuickText::ElideNone);
    textObject->setMaximumLineCount(2);
    textObject->resetHeight();

    const qreal maxLineCountImplicitHeight = textObject->implicitHeight();
    QVERIFY(maxLineCountImplicitHeight > implicitHeight);
    QVERIFY(maxLineCountImplicitHeight < wrappedImplicitHeight);

    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->width(), 100.);
    QCOMPARE(textObject->height(), maxLineCountImplicitHeight);
    QVERIFY(textObject->contentWidth() <= 100.);
    QCOMPARE(textObject->contentHeight(), maxLineCountImplicitHeight);

    for (int i = 0; i < 5; ++i) {
        textObject->setHeight(heights[i]);
        QCOMPARE(textObject->implicitWidth(), implicitWidth);
        QCOMPARE(textObject->implicitHeight(), maxLineCountImplicitHeight);
        QCOMPARE(textObject->width(), 100.);
        QCOMPARE(textObject->height(), heights[i]);
        QVERIFY(textObject->contentWidth() <= 100.);
        QCOMPARE(textObject->contentHeight(), maxLineCountImplicitHeight);
    }

    // Varying the width with a maximum line count won't increase the implicit height beyond the
    // height of the maximum number of lines.
    textObject->setWidth(2000.);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), 2000.);
    QCOMPARE(textObject->height(), implicitHeight);
    QCOMPARE(textObject->contentWidth(), implicitWidth);
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setWidth(3000.);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), 3000.);
    QCOMPARE(textObject->height(), implicitHeight);
    QCOMPARE(textObject->contentWidth(), implicitWidth);
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setWidth(-100);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), implicitHeight);
    QCOMPARE(textObject->width(), -100.);
    QCOMPARE(textObject->height(), implicitHeight);
    QCOMPARE(textObject->contentWidth(), implicitWidth);    // 0 or negative width item won't wrap.
    QCOMPARE(textObject->contentHeight(), implicitHeight);

    textObject->setWidth(50.);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), maxLineCountImplicitHeight);
    QCOMPARE(textObject->width(), 50.);
    QCOMPARE(textObject->height(), implicitHeight);
    QVERIFY(textObject->contentWidth() <= 50.);
    QCOMPARE(textObject->contentHeight(), maxLineCountImplicitHeight);

    textObject->setWidth(100.);
    QCOMPARE(textObject->implicitWidth(), implicitWidth);
    QCOMPARE(textObject->implicitHeight(), maxLineCountImplicitHeight);
    QCOMPARE(textObject->width(), 100.);
    QCOMPARE(textObject->height(), implicitHeight);
    QVERIFY(textObject->contentWidth() <= 100.);
    QCOMPARE(textObject->contentHeight(), maxLineCountImplicitHeight);
}

void tst_qquicktext::implicitSizeBinding_data()
{
    implicitSize_data();
}

void tst_qquicktext::implicitSizeBinding()
{
    QFETCH(QString, text);
    QFETCH(QString, wrap);
    QFETCH(QString, format);
    QString componentStr = "import QtQuick 2.0\nText { text: \"" + text + "\"; width: implicitWidth; height: implicitHeight; wrapMode: " + wrap + "; textFormat: " + format + " }";

    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QScopedPointer<QObject> object(textComponent.create());
    QQuickText *textObject = qobject_cast<QQuickText *>(object.data());

    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());

    textObject->resetWidth();
    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());

    textObject->resetHeight();
    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());
}

void tst_qquicktext::boundingRect_data()
{
    QTest::addColumn<QString>("format");
    QTest::newRow("PlainText") << "Text.PlainText";
    QTest::newRow("StyledText") << "Text.StyledText";
    QTest::newRow("RichText") << "Text.RichText";
}

void tst_qquicktext::boundingRect()
{
    QFETCH(QString, format);

    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n Text { textFormat:" + format.toUtf8() + "}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickText *text = qobject_cast<QQuickText *>(object.data());
    QVERIFY(text);

    QCOMPARE(text->boundingRect().x(), qreal(0));
    QCOMPARE(text->boundingRect().y(), qreal(0));
    QCOMPARE(text->boundingRect().width(), qreal(0));
    QCOMPARE(text->boundingRect().height(), qreal(qCeil(QFontMetricsF(text->font()).height())));

    text->setText("Hello World");

    QTextLayout layout(text->text());
    layout.setFont(text->font());

    if (!qmlDisableDistanceField()) {
        QTextOption option;
        option.setUseDesignMetrics(true);
        layout.setTextOption(option);
    }
    layout.beginLayout();
    QTextLine line = layout.createLine();
    layout.endLayout();

    QCOMPARE(text->boundingRect().x(), qreal(0));
    QCOMPARE(text->boundingRect().y(), qreal(0));
    QCOMPARE(text->boundingRect().width(), line.naturalTextWidth());
    QCOMPARE(text->boundingRect().height(), line.height());

    // the size of the bounding rect shouldn't be bounded by the size of item.
    text->setWidth(text->width() / 2);
    QCOMPARE(text->boundingRect().x(), qreal(0));
    QCOMPARE(text->boundingRect().y(), qreal(0));
    QCOMPARE(text->boundingRect().width(), line.naturalTextWidth());
    QCOMPARE(text->boundingRect().height(), line.height());

    text->setHeight(text->height() * 2);
    QCOMPARE(text->boundingRect().x(), qreal(0));
    QCOMPARE(text->boundingRect().y(), qreal(0));
    QCOMPARE(text->boundingRect().width(), line.naturalTextWidth());
    QCOMPARE(text->boundingRect().height(), line.height());

    text->setHAlign(QQuickText::AlignRight);
    QCOMPARE(text->boundingRect().x(), text->width() - line.naturalTextWidth());
    QCOMPARE(text->boundingRect().y(), qreal(0));
    QCOMPARE(text->boundingRect().width(), line.naturalTextWidth());
    QCOMPARE(text->boundingRect().height(), line.height());

    QQuickItemPrivate::get(text)->setLayoutMirror(true);
    QCOMPARE(text->boundingRect().x(), qreal(0));
    QCOMPARE(text->boundingRect().y(), qreal(0));
    QCOMPARE(text->boundingRect().width(), line.naturalTextWidth());
    QCOMPARE(text->boundingRect().height(), line.height());

    text->setHAlign(QQuickText::AlignLeft);
    QCOMPARE(text->boundingRect().x(), text->width() - line.naturalTextWidth());
    QCOMPARE(text->boundingRect().y(), qreal(0));
    QCOMPARE(text->boundingRect().width(), line.naturalTextWidth());
    QCOMPARE(text->boundingRect().height(), line.height());

    text->setWrapMode(QQuickText::Wrap);
    QCOMPARE(text->boundingRect().right(), text->width());
    QCOMPARE(text->boundingRect().y(), qreal(0));
    QVERIFY(text->boundingRect().width() < line.naturalTextWidth());
    QVERIFY(text->boundingRect().height() > line.height());

    text->setVAlign(QQuickText::AlignBottom);
    QCOMPARE(text->boundingRect().right(), text->width());
    QCOMPARE(text->boundingRect().bottom(), text->height());
    QVERIFY(text->boundingRect().width() < line.naturalTextWidth());
    QVERIFY(text->boundingRect().height() > line.height());
}

void tst_qquicktext::clipRect()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n Text {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickText *text = qobject_cast<QQuickText *>(object.data());
    QVERIFY(text);

    QTextLayout layout;
    layout.setFont(text->font());

    QCOMPARE(text->clipRect().x(), qreal(0));
    QCOMPARE(text->clipRect().y(), qreal(0));
    QCOMPARE(text->clipRect().width(), text->width());
    QCOMPARE(text->clipRect().height(), text->height());

    text->setText("Hello World");

    QCOMPARE(text->clipRect().x(), qreal(0));
    QCOMPARE(text->clipRect().y(), qreal(0));
    QCOMPARE(text->clipRect().width(), text->width());
    QCOMPARE(text->clipRect().height(), text->height());

    // Clip rect follows the item not content dimensions.
    text->setWidth(text->width() / 2);
    QCOMPARE(text->clipRect().x(), qreal(0));
    QCOMPARE(text->clipRect().y(), qreal(0));
    QCOMPARE(text->clipRect().width(), text->width());
    QCOMPARE(text->clipRect().height(), text->height());

    text->setHeight(text->height() * 2);
    QCOMPARE(text->clipRect().x(), qreal(0));
    QCOMPARE(text->clipRect().y(), qreal(0));
    QCOMPARE(text->clipRect().width(), text->width());
    QCOMPARE(text->clipRect().height(), text->height());

    // Setting a style adds a small amount of padding to the clip rect.
    text->setStyle(QQuickText::Outline);
    QCOMPARE(text->clipRect().x(), qreal(-1));
    QCOMPARE(text->clipRect().y(), qreal(0));
    QCOMPARE(text->clipRect().width(), text->width() + 2);
    QCOMPARE(text->clipRect().height(), text->height() + 2);
}

void tst_qquicktext::lineLaidOut()
{
    QScopedPointer<QQuickView> window(createView(testFile("lineLayout.qml")));

    QQuickText *myText = window->rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(myText != 0);

    QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(myText);
    QVERIFY(textPrivate != 0);

    QVERIFY(!textPrivate->extra.isAllocated());

    for (int i = 0; i < textPrivate->layout.lineCount(); ++i) {
        QRectF r = textPrivate->layout.lineAt(i).rect();
        QVERIFY(r.width() == i * 15);
        if (i >= 30)
            QVERIFY(r.x() == r.width() + 30);
        if (i >= 60) {
            QVERIFY(r.x() == r.width() * 2 + 60);
            QCOMPARE(r.height(), qreal(20));
        }
    }
}

void tst_qquicktext::lineLaidOutRelayout()
{
    QScopedPointer<QQuickView> window(createView(testFile("lineLayoutRelayout.qml")));

    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));

    QQuickText *myText = window->rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(myText != 0);

    QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(myText);
    QVERIFY(textPrivate != 0);

    QVERIFY(!textPrivate->extra.isAllocated());

    qreal y = 0.0;
    for (int i = 0; i < textPrivate->layout.lineCount(); ++i) {
        QTextLine line = textPrivate->layout.lineAt(i);
        const QRectF r = line.rect();
        if (r.x() == 0) {
            QCOMPARE(r.y(), y);
        } else {
            if (qFuzzyIsNull(r.y()))
                y = 0.0;
            QCOMPARE(r.x(), myText->width() / 2);
            QCOMPARE(r.y(), y);
        }
        y += line.height();
    }
}

void tst_qquicktext::lineLaidOutHAlign()
{
    QScopedPointer<QQuickView> window(createView(testFile("lineLayoutHAlign.qml")));

    QQuickText *myText = window->rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(myText != 0);

    QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(myText);
    QVERIFY(textPrivate != 0);

    QCOMPARE(textPrivate->layout.lineCount(), 1);

    QVERIFY(textPrivate->layout.lineAt(0).naturalTextRect().x() < 0.0);
}

void tst_qquicktext::imgTagsBaseUrl_data()
{
    QTest::addColumn<QUrl>("src");
    QTest::addColumn<QUrl>("baseUrl");
    QTest::addColumn<QUrl>("contextUrl");
    QTest::addColumn<qreal>("imgHeight");

    QTest::newRow("absolute local")
            << testFileUrl("images/heart200.png")
            << QUrl()
            << QUrl()
            << 181.;
    QTest::newRow("relative local context 1")
            << QUrl("images/heart200.png")
            << QUrl()
            << testFileUrl("/app.qml")
            << 181.;
    QTest::newRow("relative local context 2")
            << QUrl("heart200.png")
            << QUrl()
            << testFileUrl("images/app.qml")
            << 181.;
    QTest::newRow("relative local base 1")
            << QUrl("images/heart200.png")
            << testFileUrl("")
            << testFileUrl("nonexistant/app.qml")
            << 181.;
    QTest::newRow("relative local base 2")
            << QUrl("heart200.png")
            << testFileUrl("images/")
            << testFileUrl("nonexistant/app.qml")
            << 181.;
    QTest::newRow("base relative to local context")
            << QUrl("heart200.png")
            << testFileUrl("images/")
            << testFileUrl("/app.qml")
            << 181.;

    QTest::newRow("absolute remote")
            << QUrl("http://testserver/images/heart200.png")
            << QUrl()
            << QUrl()
            << 181.;
    QTest::newRow("relative remote base 1")
            << QUrl("images/heart200.png")
            << QUrl("http://testserver/")
            << testFileUrl("nonexistant/app.qml")
            << 181.;
    QTest::newRow("relative remote base 2")
            << QUrl("heart200.png")
            << QUrl("http://testserver/images/")
            << testFileUrl("nonexistant/app.qml")
            << 181.;
}

static QUrl substituteTestServerUrl(const QUrl &serverUrl, const QUrl &testUrl)
{
    QUrl result = testUrl;
    if (result.host() == QStringLiteral("testserver")) {
        result.setScheme(serverUrl.scheme());
        result.setHost(serverUrl.host());
        result.setPort(serverUrl.port());
    }
    return result;
}

void tst_qquicktext::imgTagsBaseUrl()
{
    QFETCH(QUrl, src);
    QFETCH(QUrl, baseUrl);
    QFETCH(QUrl, contextUrl);
    QFETCH(qreal, imgHeight);

    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(testFile(""));

    src = substituteTestServerUrl(server.baseUrl(), src);
    baseUrl = substituteTestServerUrl(server.baseUrl(), baseUrl);
    contextUrl = substituteTestServerUrl(server.baseUrl(), contextUrl);

    QByteArray baseUrlFragment;
    if (!baseUrl.isEmpty())
        baseUrlFragment = "; baseUrl: \"" + baseUrl.toEncoded() + "\"";
    QByteArray componentStr = "import QtQuick 2.0\nText { text: \"This is a test <img src=\\\"" + src.toEncoded() + "\\\">\"" + baseUrlFragment + " }";

    QQmlComponent component(&engine);
    component.setData(componentStr, contextUrl);
    QScopedPointer<QObject> object(component.create());
    QQuickText *textObject = qobject_cast<QQuickText *>(object.data());
    QVERIFY(textObject);

    QCoreApplication::processEvents();

    QTRY_COMPARE(textObject->height(), imgHeight);
}

void tst_qquicktext::imgTagsAlign_data()
{
    QTest::addColumn<QString>("src");
    QTest::addColumn<int>("imgHeight");
    QTest::addColumn<QString>("align");
    QTest::newRow("heart-bottom") << "data/images/heart200.png" << 181 <<  "bottom";
    QTest::newRow("heart-middle") << "data/images/heart200.png" << 181 <<  "middle";
    QTest::newRow("heart-top") << "data/images/heart200.png" << 181 <<  "top";
    QTest::newRow("starfish-bottom") << "data/images/starfish_2.png" << 217 <<  "bottom";
    QTest::newRow("starfish-middle") << "data/images/starfish_2.png" << 217 <<  "middle";
    QTest::newRow("starfish-top") << "data/images/starfish_2.png" << 217 <<  "top";
}

void tst_qquicktext::imgTagsAlign()
{
    QFETCH(QString, src);
    QFETCH(int, imgHeight);
    QFETCH(QString, align);
    QString componentStr = "import QtQuick 2.0\nText { text: \"This is a test <img src=\\\"" + src + "\\\" align=\\\"" + align + "\\\"> of image.\" }";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile("."));
    QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

    QVERIFY(textObject != 0);
    QCOMPARE(textObject->height(), qreal(imgHeight));

    QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(textObject);
    QVERIFY(textPrivate != 0);

    QRectF br = textPrivate->layout.boundingRect();
    if (align == "bottom")
        QVERIFY(br.y() == imgHeight - br.height());
    else if (align == "middle")
        QVERIFY(br.y() == imgHeight / 2.0 - br.height() / 2.0);
    else if (align == "top")
        QCOMPARE(br.y(), qreal(0));

    delete textObject;
}

void tst_qquicktext::imgTagsMultipleImages()
{
    QString componentStr = "import QtQuick 2.0\nText { text: \"This is a starfish<img src=\\\"data/images/starfish_2.png\\\" width=\\\"60\\\" height=\\\"60\\\" > and another one<img src=\\\"data/images/heart200.png\\\" width=\\\"85\\\" height=\\\"85\\\">.\" }";

    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile("."));
    QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

    QVERIFY(textObject != 0);
    QCOMPARE(textObject->height(), qreal(85));

    QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(textObject);
    QVERIFY(textPrivate != 0);
    QCOMPARE(textPrivate->extra->visibleImgTags.count(), 2);

    delete textObject;
}

void tst_qquicktext::imgTagsElide()
{
    QScopedPointer<QQuickView> window(createView(testFile("imgTagsElide.qml")));
    QQuickText *myText = window->rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(myText != 0);

    QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(myText);
    QVERIFY(textPrivate != 0);
    QCOMPARE(textPrivate->extra->visibleImgTags.count(), 0);
    myText->setMaximumLineCount(20);
    QTRY_COMPARE(textPrivate->extra->visibleImgTags.count(), 1);

    delete myText;
}

void tst_qquicktext::imgTagsUpdates()
{
    QScopedPointer<QQuickView> window(createView(testFile("imgTagsUpdates.qml")));
    QQuickText *myText = window->rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(myText != 0);

    QSignalSpy spy(myText, SIGNAL(contentSizeChanged()));

    QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(myText);
    QVERIFY(textPrivate != 0);

    myText->setText("This is a heart<img src=\"images/heart200.png\">.");
    QCOMPARE(textPrivate->extra->visibleImgTags.count(), 1);
    QCOMPARE(spy.count(), 1);

    myText->setMaximumLineCount(2);
    myText->setText("This is another heart<img src=\"images/heart200.png\">.");
    QTRY_COMPARE(textPrivate->extra->visibleImgTags.count(), 1);

    // if maximumLineCount is set and the img tag doesn't have an explicit size
    // we relayout twice.
    QCOMPARE(spy.count(), 3);

    delete myText;
}

void tst_qquicktext::imgTagsError()
{
    QString componentStr = "import QtQuick 2.0\nText { text: \"This is a starfish<img src=\\\"data/images/starfish_2.pn\\\" width=\\\"60\\\" height=\\\"60\\\">.\" }";

    QQmlComponent textComponent(&engine);
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>:2:1: QML Text: Cannot open: file:data/images/starfish_2.pn");
    textComponent.setData(componentStr.toLatin1(), QUrl("file:"));
    QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

    QVERIFY(textObject != 0);
    delete textObject;
}

void tst_qquicktext::fontSizeMode_data()
{
    QTest::addColumn<QString>("text");
    QTest::newRow("plain") << "The quick red fox jumped over the lazy brown dog";
    QTest::newRow("styled") << "<b>The quick red fox jumped over the lazy brown dog</b>";
}

void tst_qquicktext::fontSizeMode()
{
    QFETCH(QString, text);

    QScopedPointer<QQuickView> window(createView(testFile("fontSizeMode.qml")));
    window->show();

    QQuickText *myText = window->rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(myText != 0);

    myText->setText(text);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    qreal originalWidth = myText->contentWidth();
    qreal originalHeight = myText->contentHeight();

    // The original text unwrapped should exceed the width of the item.
    QVERIFY(originalWidth > myText->width());
    QVERIFY(originalHeight < myText->height());

    QFont font = myText->font();
    font.setPixelSize(64);

    myText->setFont(font);
    myText->setFontSizeMode(QQuickText::HorizontalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // Font size reduced to fit within the width of the item.
    qreal horizontalFitWidth = myText->contentWidth();
    qreal horizontalFitHeight = myText->contentHeight();
    QVERIFY(horizontalFitWidth <= myText->width() + 2); // rounding
    QVERIFY(horizontalFitHeight <= myText->height() + 2);

    // Elide won't affect the size with HorizontalFit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    myText->setElideMode(QQuickText::ElideLeft);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    myText->setElideMode(QQuickText::ElideMiddle);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::VerticalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // Font size increased to fill the height of the item.
    qreal verticalFitHeight = myText->contentHeight();
    QVERIFY(myText->contentWidth() > myText->width());
    QVERIFY(verticalFitHeight <= myText->height() + 2);
    QVERIFY(verticalFitHeight > originalHeight);

    // Elide won't affect the height of a single line with VerticalFit but will crop the width.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(myText->truncated());
    QVERIFY(myText->contentWidth() <= myText->width() + 2);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideLeft);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(myText->truncated());
    QVERIFY(myText->contentWidth() <= myText->width() + 2);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideMiddle);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(myText->truncated());
    QVERIFY(myText->contentWidth() <= myText->width() + 2);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::Fit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // Should be the same as HorizontalFit with no wrapping.
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    // Elide won't affect the size with Fit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    myText->setElideMode(QQuickText::ElideLeft);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    myText->setElideMode(QQuickText::ElideMiddle);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::FixedSize);
    myText->setWrapMode(QQuickText::Wrap);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    originalWidth = myText->contentWidth();
    originalHeight = myText->contentHeight();

    // The original text wrapped should exceed the height of the item.
    QVERIFY(originalWidth <= myText->width() + 2);
    QVERIFY(originalHeight > myText->height());

    myText->setFontSizeMode(QQuickText::HorizontalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // HorizontalFit should reduce the font size to minimize wrapping, which brings it back to the
    // same size as without text wrapping.
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    // Elide won't affect the size with HorizontalFit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::VerticalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // VerticalFit should reduce the size to the wrapped text within the vertical height.
    verticalFitHeight = myText->contentHeight();
    qreal verticalFitWidth = myText->contentWidth();
    QVERIFY(myText->contentWidth() <= myText->width() + 2);
    QVERIFY(verticalFitHeight <= myText->height() + 2);
    QVERIFY(verticalFitHeight < originalHeight);

    // Elide won't affect the height or width of a wrapped text with VerticalFit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::Fit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // Should be the same as VerticalFit with wrapping.
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    // Elide won't affect the size with Fit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::FixedSize);
    myText->setMaximumLineCount(2);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    // The original text wrapped should exceed the height of the item.
    QVERIFY(originalWidth <= myText->width() + 2);
    QVERIFY(originalHeight > myText->height());

    myText->setFontSizeMode(QQuickText::HorizontalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // HorizontalFit should reduce the font size to minimize wrapping, which brings it back to the
    // same size as without text wrapping.
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    // Elide won't affect the size with HorizontalFit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::VerticalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // VerticalFit should reduce the size to the wrapped text within the vertical height.
    verticalFitHeight = myText->contentHeight();
    verticalFitWidth = myText->contentWidth();
    QVERIFY(myText->contentWidth() <= myText->width() + 2);
    QVERIFY(verticalFitHeight <= myText->height() + 2);
    QVERIFY(verticalFitHeight < originalHeight);

    // Elide won't affect the height or width of a wrapped text with VerticalFit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::Fit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // Should be the same as VerticalFit with wrapping.
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    // Elide won't affect the size with Fit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
}

void tst_qquicktext::fontSizeModeMultiline_data()
{
    QTest::addColumn<QString>("text");
    QTest::newRow("plain") << "The quick red fox jumped\n over the lazy brown dog";
    QTest::newRow("styledtext") << "<b>The quick red fox jumped<br/> over the lazy brown dog</b>";
}

void tst_qquicktext::fontSizeModeMultiline()
{
    QFETCH(QString, text);

    QScopedPointer<QQuickView> window(createView(testFile("fontSizeMode.qml")));
    window->show();

    QQuickText *myText = window->rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(myText != 0);

    myText->setText(text);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    qreal originalWidth = myText->contentWidth();
    qreal originalHeight = myText->contentHeight();
    QCOMPARE(myText->lineCount(), 2);

    // The original text unwrapped should exceed the width and height of the item.
    QVERIFY(originalWidth > myText->width());
    QVERIFY(originalHeight > myText->height());

    QFont font = myText->font();
    font.setPixelSize(64);

    myText->setFont(font);
    myText->setFontSizeMode(QQuickText::HorizontalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // Font size reduced to fit within the width of the item.
    QCOMPARE(myText->lineCount(), 2);
    qreal horizontalFitWidth = myText->contentWidth();
    qreal horizontalFitHeight = myText->contentHeight();
    QVERIFY(horizontalFitWidth <= myText->width() + 2); // rounding
    QVERIFY(horizontalFitHeight > myText->height());

    // Right eliding will remove the last line
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(myText->truncated());
    QCOMPARE(myText->lineCount(), 1);
    QVERIFY(myText->contentWidth() <= myText->width() + 2);
    QVERIFY(myText->contentHeight() <= myText->height() + 2);

    // Left or middle eliding wont have any effect.
    myText->setElideMode(QQuickText::ElideLeft);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    myText->setElideMode(QQuickText::ElideMiddle);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::VerticalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // Font size reduced to fit within the height of the item.
    qreal verticalFitWidth = myText->contentWidth();
    qreal verticalFitHeight = myText->contentHeight();
    QVERIFY(verticalFitWidth <= myText->width() + 2);
    QVERIFY(verticalFitHeight <= myText->height() + 2);

    // Elide will have no effect.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QVERIFY(myText->contentWidth() <= myText->width() + 2);
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideLeft);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideMiddle);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::Fit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // Should be the same as VerticalFit with no wrapping.
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    // Elide won't affect the size with Fit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideLeft);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideMiddle);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::FixedSize);
    myText->setWrapMode(QQuickText::Wrap);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    originalWidth = myText->contentWidth();
    originalHeight = myText->contentHeight();

    // The original text wrapped should exceed the height of the item.
    QVERIFY(originalWidth <= myText->width() + 2);
    QVERIFY(originalHeight > myText->height());

    myText->setFontSizeMode(QQuickText::HorizontalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // HorizontalFit should reduce the font size to minimize wrapping, which brings it back to the
    // same size as without text wrapping.
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    // Text will be elided vertically with HorizontalFit
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(myText->truncated());
    QVERIFY(myText->contentWidth() <= myText->width() + 2);
    QVERIFY(myText->contentHeight() <= myText->height() + 2);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::VerticalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // VerticalFit should reduce the size to the wrapped text within the vertical height.
    verticalFitHeight = myText->contentHeight();
    verticalFitWidth = myText->contentWidth();
    QVERIFY(myText->contentWidth() <= myText->width() + 2);
    QVERIFY(verticalFitHeight <= myText->height() + 2);
    QVERIFY(verticalFitHeight < originalHeight);

    // Elide won't affect the height or width of a wrapped text with VerticalFit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::Fit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // Should be the same as VerticalFit with wrapping.
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    // Elide won't affect the size with Fit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::FixedSize);
    myText->setMaximumLineCount(2);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    // The original text wrapped should exceed the height of the item.
    QVERIFY(originalWidth <= myText->width() + 2);
    QVERIFY(originalHeight > myText->height());

    myText->setFontSizeMode(QQuickText::HorizontalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // HorizontalFit should reduce the font size to minimize wrapping, which brings it back to the
    // same size as without text wrapping.
    QCOMPARE(myText->contentWidth(), horizontalFitWidth);
    QCOMPARE(myText->contentHeight(), horizontalFitHeight);

    // Elide won't affect the size with HorizontalFit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(myText->truncated());
    QVERIFY(myText->contentWidth() <= myText->width() + 2);
    QVERIFY(myText->contentHeight() <= myText->height() + 2);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::VerticalFit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // VerticalFit should reduce the size to the wrapped text within the vertical height.
    verticalFitHeight = myText->contentHeight();
    verticalFitWidth = myText->contentWidth();
    QVERIFY(myText->contentWidth() <= myText->width() + 2);
    QVERIFY(verticalFitHeight <= myText->height() + 2);
    QVERIFY(verticalFitHeight < originalHeight);

    // Elide won't affect the height or width of a wrapped text with VerticalFit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    myText->setFontSizeMode(QQuickText::Fit);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    // Should be the same as VerticalFit with wrapping.
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    // Elide won't affect the size with Fit.
    myText->setElideMode(QQuickText::ElideRight);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    QVERIFY(!myText->truncated());
    QCOMPARE(myText->contentWidth(), verticalFitWidth);
    QCOMPARE(myText->contentHeight(), verticalFitHeight);

    myText->setElideMode(QQuickText::ElideNone);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
}

void tst_qquicktext::multilengthStrings_data()
{
    QTest::addColumn<QString>("source");
    QTest::newRow("No Wrap") << testFile("multilengthStrings.qml");
    QTest::newRow("Wrap") << testFile("multilengthStringsWrapped.qml");
}

void tst_qquicktext::multilengthStrings()
{
    QFETCH(QString, source);

    QScopedPointer<QQuickView> window(createView(source));
    window->show();

    QQuickText *myText = window->rootObject()->findChild<QQuickText*>("myText");
    QVERIFY(myText != 0);

    const QString longText = "the quick brown fox jumped over the lazy dog";
    const QString mediumText = "the brown fox jumped over the dog";
    const QString shortText = "fox jumped dog";

    myText->setText(longText);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    const qreal longWidth = myText->contentWidth();
    const qreal longHeight = myText->contentHeight();

    myText->setText(mediumText);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    const qreal mediumWidth = myText->contentWidth();
    const qreal mediumHeight = myText->contentHeight();

    myText->setText(shortText);
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);
    const qreal shortWidth = myText->contentWidth();
    const qreal shortHeight = myText->contentHeight();

    myText->setElideMode(QQuickText::ElideRight);
    myText->setText(longText + QLatin1Char('\x9c') + mediumText + QLatin1Char('\x9c') + shortText);

    myText->setSize(QSizeF(longWidth, longHeight));
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    QCOMPARE(myText->contentWidth(), longWidth);
    QCOMPARE(myText->contentHeight(), longHeight);
    QCOMPARE(myText->truncated(), false);

    myText->setSize(QSizeF(mediumWidth, mediumHeight));
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    QCOMPARE(myText->contentWidth(), mediumWidth);
    QCOMPARE(myText->contentHeight(), mediumHeight);
    QCOMPARE(myText->truncated(), true);

    myText->setSize(QSizeF(shortWidth, shortHeight));
    QTRY_COMPARE(QQuickItemPrivate::get(myText)->polishScheduled, false);

    QCOMPARE(myText->contentWidth(), shortWidth);
    QCOMPARE(myText->contentHeight(), shortHeight);
    QCOMPARE(myText->truncated(), true);
}

void tst_qquicktext::fontFormatSizes_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("textWithTag");
    QTest::addColumn<bool>("fontIsBigger");

    QTest::newRow("fs1") << "Hello world!" << "Hello <font size=\"1\">world</font>!" << false;
    QTest::newRow("fs2") << "Hello world!" << "Hello <font size=\"2\">world</font>!" << false;
    QTest::newRow("fs3") << "Hello world!" << "Hello <font size=\"3\">world</font>!" << false;
    QTest::newRow("fs4") << "Hello world!" << "Hello <font size=\"4\">world</font>!" << true;
    QTest::newRow("fs5") << "Hello world!" << "Hello <font size=\"5\">world</font>!" << true;
    QTest::newRow("fs6") << "Hello world!" << "Hello <font size=\"6\">world</font>!" << true;
    QTest::newRow("fs7") << "Hello world!" << "Hello <font size=\"7\">world</font>!" << true;
    QTest::newRow("h1") << "This is<br/>a font<br/> size test." << "This is <h1>a font</h1> size test." << true;
    QTest::newRow("h2") << "This is<br/>a font<br/> size test." << "This is <h2>a font</h2> size test." << true;
    QTest::newRow("h3") << "This is<br/>a font<br/> size test." << "This is <h3>a font</h3> size test." << true;
    QTest::newRow("h4") << "This is<br/>a font<br/> size test." << "This is <h4>a font</h4> size test." << true;
    QTest::newRow("h5") << "This is<br/>a font<br/> size test." << "This is <h5>a font</h5> size test." << false;
    QTest::newRow("h6") << "This is<br/>a font<br/> size test." << "This is <h6>a font</h6> size test." << false;
}

void tst_qquicktext::fontFormatSizes()
{
    QFETCH(QString, text);
    QFETCH(QString, textWithTag);
    QFETCH(bool, fontIsBigger);

    QQuickView *view = new QQuickView;
    {
        view->setSource(testFileUrl("pointFontSizes.qml"));
        view->show();

        QQuickText *qtext = view->rootObject()->findChild<QQuickText*>("text");
        QQuickText *qtextWithTag = view->rootObject()->findChild<QQuickText*>("textWithTag");
        QVERIFY(qtext != 0);
        QVERIFY(qtextWithTag != 0);

        qtext->setText(text);
        qtextWithTag->setText(textWithTag);

        for (int size = 6; size < 100; size += 4) {
            view->rootObject()->setProperty("pointSize", size);
            if (fontIsBigger)
                QVERIFY(qtext->height() <= qtextWithTag->height());
            else
                QVERIFY(qtext->height() >= qtextWithTag->height());
        }
    }

    {
        view->setSource(testFileUrl("pixelFontSizes.qml"));
        QQuickText *qtext = view->rootObject()->findChild<QQuickText*>("text");
        QQuickText *qtextWithTag = view->rootObject()->findChild<QQuickText*>("textWithTag");
        QVERIFY(qtext != 0);
        QVERIFY(qtextWithTag != 0);

        qtext->setText(text);
        qtextWithTag->setText(textWithTag);

        for (int size = 6; size < 100; size += 4) {
            view->rootObject()->setProperty("pixelSize", size);
            if (fontIsBigger)
                QVERIFY(qtext->height() <= qtextWithTag->height());
            else
                QVERIFY(qtext->height() >= qtextWithTag->height());
        }
    }
    delete view;
}

typedef qreal (*ExpectedBaseline)(QQuickText *item);
Q_DECLARE_METATYPE(ExpectedBaseline)

static qreal expectedBaselineTop(QQuickText *item)
{
    QFontMetricsF fm(item->font());
    return fm.ascent() + item->topPadding();
}

static qreal expectedBaselineBottom(QQuickText *item)
{
    QFontMetricsF fm(item->font());
    return item->height() - item->contentHeight() - item->bottomPadding() + fm.ascent();
}

static qreal expectedBaselineCenter(QQuickText *item)
{
    QFontMetricsF fm(item->font());
    return ((item->height() - item->contentHeight() - item->topPadding() - item->bottomPadding()) / 2) + fm.ascent() + item->topPadding();
}

static qreal expectedBaselineBold(QQuickText *item)
{
    QFont font = item->font();
    font.setBold(true);
    QFontMetricsF fm(font);
    return fm.ascent() + item->topPadding();
}

static qreal expectedBaselineImage(QQuickText *item)
{
    QFontMetricsF fm(item->font());
    // The line is positioned so the bottom of the line is aligned with the bottom of the image,
    // or image height - line height and the baseline is line position + ascent.  Because
    // QTextLine's height is rounded up this can give slightly different results to image height
    // - descent.
    return 181 - qCeil(fm.height()) + fm.ascent() + item->topPadding();
}

static qreal expectedBaselineCustom(QQuickText *item)
{
    QFontMetricsF fm(item->font());
    return 16 + fm.ascent() + item->topPadding();
}

static qreal expectedBaselineScaled(QQuickText *item)
{
    QFont font = item->font();
    QTextLayout layout(item->text().replace(QLatin1Char('\n'), QChar::LineSeparator));
    do {
        layout.setFont(font);
        qreal width = 0;
        layout.beginLayout();
        for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
            line.setLineWidth(FLT_MAX);
            width = qMax(line.naturalTextWidth(), width);
        }
        layout.endLayout();

        if (width < item->width()) {
            QFontMetricsF fm(layout.font());
            return fm.ascent() + item->topPadding();
        }
        font.setPointSize(font.pointSize() - 1);
    } while (font.pointSize() > 0);
    return item->topPadding();
}

static qreal expectedBaselineFixedBottom(QQuickText *item)
{
    QFontMetricsF fm(item->font());
    qreal dy = item->text().contains(QLatin1Char('\n'))
            ? 160
            : 180;
    return dy + fm.ascent() - item->bottomPadding();
}

static qreal expectedBaselineProportionalBottom(QQuickText *item)
{
    QFontMetricsF fm(item->font());
    qreal dy = item->text().contains(QLatin1Char('\n'))
            ? 200 - (qCeil(fm.height()) * 3)
            : 200 - (qCeil(fm.height()) * 1.5);
    return dy + fm.ascent() - item->bottomPadding();
}

void tst_qquicktext::baselineOffset_data()
{
    qRegisterMetaType<ExpectedBaseline>();
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("wrappedText");
    QTest::addColumn<QByteArray>("bindings");
    QTest::addColumn<ExpectedBaseline>("expectedBaseline");
    QTest::addColumn<ExpectedBaseline>("expectedBaselineEmpty");

    QTest::newRow("top align")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; verticalAlignment: Text.AlignTop")
            << &expectedBaselineTop
            << &expectedBaselineTop;
    QTest::newRow("bottom align")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; verticalAlignment: Text.AlignBottom")
            << &expectedBaselineBottom
            << &expectedBaselineBottom;
    QTest::newRow("center align")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; verticalAlignment: Text.AlignVCenter")
            << &expectedBaselineCenter
            << &expectedBaselineCenter;

    QTest::newRow("bold")
            << "<b>hello world</b>"
            << "<b>hello<br/>world</b>"
            << QByteArray("height: 200")
            << &expectedBaselineBold
            << &expectedBaselineTop;

    QTest::newRow("richText")
            << "<b>hello world</b>"
            << "<b>hello<br/>world</b>"
            << QByteArray("height: 200; textFormat: Text.RichText")
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("elided")
            << "hello world"
            << "hello\nworld"
            << QByteArray("width: 20; height: 8; elide: Text.ElideRight")
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("elided bottom align")
            << "hello world"
            << "hello\nworld!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
            << QByteArray("width: 200; height: 200; elide: Text.ElideRight; verticalAlignment: Text.AlignBottom")
            << &expectedBaselineBottom
            << &expectedBaselineBottom;

    QTest::newRow("image")
            << "hello <img src=\"images/heart200.png\" /> world"
            << "hello <img src=\"images/heart200.png\" /><br/>world"
            << QByteArray("height: 200\n; baseUrl: \"") + testFileUrl("reference").toEncoded() + QByteArray("\"")
            << &expectedBaselineImage
            << &expectedBaselineTop;

    QTest::newRow("customLine")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; onLineLaidOut: line.y += 16")
            << &expectedBaselineCustom
            << &expectedBaselineCustom;

    QTest::newRow("scaled font")
            << "hello world"
            << "hello\nworld"
            << QByteArray("width: 200; minimumPointSize: 1; font.pointSize: 64; fontSizeMode: Text.HorizontalFit")
            << &expectedBaselineScaled
            << &expectedBaselineTop;

    QTest::newRow("fixed line height top align")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; lineHeightMode: Text.FixedHeight; lineHeight: 20; verticalAlignment: Text.AlignTop")
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("fixed line height bottom align")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; lineHeightMode: Text.FixedHeight; lineHeight: 20; verticalAlignment: Text.AlignBottom")
            << &expectedBaselineFixedBottom
            << &expectedBaselineFixedBottom;

    QTest::newRow("proportional line height top align")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; lineHeightMode: Text.ProportionalHeight; lineHeight: 1.5; verticalAlignment: Text.AlignTop")
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("proportional line height bottom align")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; lineHeightMode: Text.ProportionalHeight; lineHeight: 1.5; verticalAlignment: Text.AlignBottom")
            << &expectedBaselineProportionalBottom
            << &expectedBaselineProportionalBottom;

    QTest::newRow("top align with padding")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; topPadding: 10; bottomPadding: 20; verticalAlignment: Text.AlignTop")
            << &expectedBaselineTop
            << &expectedBaselineTop;
    QTest::newRow("bottom align with padding")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; topPadding: 10; bottomPadding: 20; verticalAlignment: Text.AlignBottom")
            << &expectedBaselineBottom
            << &expectedBaselineBottom;
    QTest::newRow("center align with padding")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; topPadding: 10; bottomPadding: 20; verticalAlignment: Text.AlignVCenter")
            << &expectedBaselineCenter
            << &expectedBaselineCenter;

    QTest::newRow("bold width padding")
            << "<b>hello world</b>"
            << "<b>hello<br/>world</b>"
            << QByteArray("height: 200; topPadding: 10; bottomPadding: 20")
            << &expectedBaselineBold
            << &expectedBaselineTop;

    QTest::newRow("richText with padding")
            << "<b>hello world</b>"
            << "<b>hello<br/>world</b>"
            << QByteArray("height: 200; topPadding: 10; bottomPadding: 20; textFormat: Text.RichText")
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("elided with padding")
            << "hello world"
            << "hello\nworld"
            << QByteArray("width: 20; height: 8; topPadding: 10; bottomPadding: 20; elide: Text.ElideRight")
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("elided bottom align with padding")
            << "hello world"
            << "hello\nworld!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
            << QByteArray("width: 200; height: 200; topPadding: 10; bottomPadding: 20; elide: Text.ElideRight; verticalAlignment: Text.AlignBottom")
            << &expectedBaselineBottom
            << &expectedBaselineBottom;

    QTest::newRow("image with padding")
            << "hello <img src=\"images/heart200.png\" /> world"
            << "hello <img src=\"images/heart200.png\" /><br/>world"
            << QByteArray("height: 200\n; topPadding: 10; bottomPadding: 20; baseUrl: \"") + testFileUrl("reference").toEncoded() + QByteArray("\"")
            << &expectedBaselineImage
            << &expectedBaselineTop;

    QTest::newRow("customLine with padding")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; topPadding: 10; bottomPadding: 20; onLineLaidOut: line.y += 16")
            << &expectedBaselineCustom
            << &expectedBaselineCustom;

    QTest::newRow("scaled font with padding")
            << "hello world"
            << "hello\nworld"
            << QByteArray("width: 200; topPadding: 10; bottomPadding: 20; minimumPointSize: 1; font.pointSize: 64; fontSizeMode: Text.HorizontalFit")
            << &expectedBaselineScaled
            << &expectedBaselineTop;

    QTest::newRow("fixed line height top align with padding")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; topPadding: 10; bottomPadding: 20; lineHeightMode: Text.FixedHeight; lineHeight: 20; verticalAlignment: Text.AlignTop")
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("fixed line height bottom align with padding")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; topPadding: 10; bottomPadding: 20; lineHeightMode: Text.FixedHeight; lineHeight: 20; verticalAlignment: Text.AlignBottom")
            << &expectedBaselineFixedBottom
            << &expectedBaselineFixedBottom;

    QTest::newRow("proportional line height top align with padding")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; topPadding: 10; bottomPadding: 20; lineHeightMode: Text.ProportionalHeight; lineHeight: 1.5; verticalAlignment: Text.AlignTop")
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("proportional line height bottom align with padding")
            << "hello world"
            << "hello\nworld"
            << QByteArray("height: 200; topPadding: 10; bottomPadding: 20; lineHeightMode: Text.ProportionalHeight; lineHeight: 1.5; verticalAlignment: Text.AlignBottom")
            << &expectedBaselineProportionalBottom
            << &expectedBaselineProportionalBottom;
}

void tst_qquicktext::baselineOffset()
{
    QFETCH(QString, text);
    QFETCH(QString, wrappedText);
    QFETCH(QByteArray, bindings);
    QFETCH(ExpectedBaseline, expectedBaseline);
    QFETCH(ExpectedBaseline, expectedBaselineEmpty);

    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.6\n"
            "Text {\n"
                + bindings + "\n"
            "}", QUrl());

    QScopedPointer<QObject> object(component.create());

    QQuickText *item = qobject_cast<QQuickText *>(object.data());
    QVERIFY(item);

    {
        qreal baseline = expectedBaselineEmpty(item);

        QCOMPARE(item->baselineOffset(), baseline);

        item->setText(text);
        if (expectedBaseline != expectedBaselineEmpty)
            baseline = expectedBaseline(item);

        QCOMPARE(item->baselineOffset(), baseline);

        item->setText(wrappedText);
        QCOMPARE(item->baselineOffset(), expectedBaseline(item));
    }

    QFont font = item->font();
    font.setPointSize(font.pointSize() + 8);

    {
        QCOMPARE(item->baselineOffset(), expectedBaseline(item));

        item->setText(text);
        qreal baseline = expectedBaseline(item);
        QCOMPARE(item->baselineOffset(), baseline);

        item->setText(QString());
        if (expectedBaselineEmpty != expectedBaseline)
            baseline = expectedBaselineEmpty(item);

        QCOMPARE(item->baselineOffset(), baseline);
    }
}

void tst_qquicktext::htmlLists()
{
    QFETCH(QString, text);
    QFETCH(int, nbLines);

    QQuickView *view = createView(testFile("htmlLists.qml"));
    QQuickText *textObject = view->rootObject()->findChild<QQuickText*>("myText");

    QQuickTextPrivate *textPrivate = QQuickTextPrivate::get(textObject);
    QVERIFY(textPrivate != 0);
    QVERIFY(textPrivate->extra.isAllocated());

    QVERIFY(textObject != 0);
    textObject->setText(text);

    view->show();
    view->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(view));

    QCOMPARE(textPrivate->extra->doc->lineCount(), nbLines);

    delete view;
}

void tst_qquicktext::htmlLists_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("nbLines");

    QTest::newRow("ordered list") << "<ol><li>one<li>two<li>three" << 3;
    QTest::newRow("ordered list closed") << "<ol><li>one</li></ol>" << 1;
    QTest::newRow("ordered list alpha") << "<ol type=\"a\"><li>one</li><li>two</li></ol>" << 2;
    QTest::newRow("ordered list upper alpha") << "<ol type=\"A\"><li>one</li><li>two</li></ol>" << 2;
    QTest::newRow("ordered list roman") << "<ol type=\"i\"><li>one</li><li>two</li></ol>" << 2;
    QTest::newRow("ordered list upper roman") << "<ol type=\"I\"><li>one</li><li>two</li></ol>" << 2;
    QTest::newRow("ordered list bad") << "<ol type=\"z\"><li>one</li><li>two</li></ol>" << 2;
    QTest::newRow("unordered list") << "<ul><li>one<li>two" << 2;
    QTest::newRow("unordered list closed") << "<ul><li>one</li><li>two</li></ul>" << 2;
    QTest::newRow("unordered list disc") << "<ul type=\"disc\"><li>one</li><li>two</li></ul>" << 2;
    QTest::newRow("unordered list square") << "<ul type=\"square\"><li>one</li><li>two</li></ul>" << 2;
    QTest::newRow("unordered list bad") << "<ul type=\"bad\"><li>one</li><li>two</li></ul>" << 2;
}

void tst_qquicktext::elideBeforeMaximumLineCount()
{   // QTBUG-31471
    QQmlComponent component(&engine, testFile("elideBeforeMaximumLineCount.qml"));

    QScopedPointer<QObject> object(component.create());

    QQuickText *item = qobject_cast<QQuickText *>(object.data());
    QVERIFY(item);

    QCOMPARE(item->lineCount(), 2);
}

void tst_qquicktext::hover()
{   // QTBUG-33842
    QQmlComponent component(&engine, testFile("hover.qml"));

    QScopedPointer<QObject> object(component.create());

    QQuickWindow *window = qobject_cast<QQuickWindow *>(object.data());
    QVERIFY(window);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickMouseArea *mouseArea = window->property("mouseArea").value<QQuickMouseArea *>();
    QVERIFY(mouseArea);
    QQuickText *textItem = window->property("textItem").value<QQuickText *>();
    QVERIFY(textItem);

    QVERIFY(!mouseArea->property("wasHovered").toBool());

    QPoint center(window->width() / 2, window->height() / 2);
    QPoint delta(window->width() / 10, window->height() / 10);

    QTest::mouseMove(window, center - delta);
    QTest::mouseMove(window, center + delta);

    QVERIFY(mouseArea->property("wasHovered").toBool());
}

void tst_qquicktext::growFromZeroWidth()
{
    QQmlComponent component(&engine, testFile("growFromZeroWidth.qml"));

    QScopedPointer<QObject> object(component.create());

    QQuickText *text = qobject_cast<QQuickText *>(object.data());
    QVERIFY(text);

    QCOMPARE(text->lineCount(), 3);

    text->setWidth(80);

    // the new width should force our contents to wrap
    QVERIFY(text->lineCount() > 3);
}

void tst_qquicktext::padding()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("padding.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QQuickItem *root = window->rootObject();
    QVERIFY(root);
    QQuickText *obj = qobject_cast<QQuickText*>(root);
    QVERIFY(obj != 0);

    qreal cw = obj->contentWidth();
    qreal ch = obj->contentHeight();

    QVERIFY(cw > 0);
    QVERIFY(ch > 0);

    QCOMPARE(obj->topPadding(), 20.0);
    QCOMPARE(obj->leftPadding(), 30.0);
    QCOMPARE(obj->rightPadding(), 40.0);
    QCOMPARE(obj->bottomPadding(), 50.0);

    QCOMPARE(obj->implicitWidth(), cw + obj->leftPadding() + obj->rightPadding());
    QCOMPARE(obj->implicitHeight(), ch + obj->topPadding() + obj->bottomPadding());

    obj->setTopPadding(2.25);
    QCOMPARE(obj->topPadding(), 2.25);
    QCOMPARE(obj->implicitHeight(), ch + obj->topPadding() + obj->bottomPadding());

    obj->setLeftPadding(3.75);
    QCOMPARE(obj->leftPadding(), 3.75);
    QCOMPARE(obj->implicitWidth(), cw + obj->leftPadding() + obj->rightPadding());

    obj->setRightPadding(4.4);
    QCOMPARE(obj->rightPadding(), 4.4);
    QCOMPARE(obj->implicitWidth(), cw + obj->leftPadding() + obj->rightPadding());

    obj->setBottomPadding(1.11);
    QCOMPARE(obj->bottomPadding(), 1.11);
    QCOMPARE(obj->implicitHeight(), ch + obj->topPadding() + obj->bottomPadding());

    obj->setWidth(cw / 2);
    obj->setElideMode(QQuickText::ElideRight);
    QCOMPARE(obj->implicitWidth(), cw + obj->leftPadding() + obj->rightPadding());
    QCOMPARE(obj->implicitHeight(), ch + obj->topPadding() + obj->bottomPadding());
    obj->setElideMode(QQuickText::ElideNone);
    obj->resetWidth();

    obj->setWrapMode(QQuickText::WordWrap);
    QCOMPARE(obj->implicitWidth(), cw + obj->leftPadding() + obj->rightPadding());
    QCOMPARE(obj->implicitHeight(), ch + obj->topPadding() + obj->bottomPadding());
    obj->setWrapMode(QQuickText::NoWrap);

    obj->setText("Qt");
    QVERIFY(obj->contentWidth() < cw);
    QCOMPARE(obj->contentHeight(), ch);
    cw = obj->contentWidth();

    QCOMPARE(obj->implicitWidth(), cw + obj->leftPadding() + obj->rightPadding());
    QCOMPARE(obj->implicitHeight(), ch + obj->topPadding() + obj->bottomPadding());

    obj->setFont(QFont("Courier", 96));
    QVERIFY(obj->contentWidth() > cw);
    QVERIFY(obj->contentHeight() > ch);
    cw = obj->contentWidth();
    ch = obj->contentHeight();

    QCOMPARE(obj->implicitWidth(), cw + obj->leftPadding() + obj->rightPadding());
    QCOMPARE(obj->implicitHeight(), ch + obj->topPadding() + obj->bottomPadding());

    obj->resetTopPadding();
    QCOMPARE(obj->topPadding(), 10.0);
    obj->resetLeftPadding();
    QCOMPARE(obj->leftPadding(), 10.0);
    obj->resetRightPadding();
    QCOMPARE(obj->rightPadding(), 10.0);
    obj->resetBottomPadding();
    QCOMPARE(obj->bottomPadding(), 10.0);

    obj->resetPadding();
    QCOMPARE(obj->padding(), 0.0);
    QCOMPARE(obj->topPadding(), 0.0);
    QCOMPARE(obj->leftPadding(), 0.0);
    QCOMPARE(obj->rightPadding(), 0.0);
    QCOMPARE(obj->bottomPadding(), 0.0);

    delete root;
}

void tst_qquicktext::hintingPreference()
{
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().hintingPreference(), (int)QFont::PreferDefaultHinting);

        delete textObject;
    }
    {
        QString componentStr = "import QtQuick 2.0\nText { text: \"Hello world!\"; font.hintingPreference: Font.PreferNoHinting }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickText *textObject = qobject_cast<QQuickText*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE((int)textObject->font().hintingPreference(), (int)QFont::PreferNoHinting);

        delete textObject;
    }
}


void tst_qquicktext::zeroWidthAndElidedDoesntRender()
{
    // Tests QTBUG-34990

    QQmlComponent component(&engine, testFile("ellipsisText.qml"));

    QScopedPointer<QObject> object(component.create());

    QQuickText *text = qobject_cast<QQuickText *>(object.data());
    QVERIFY(text);

    QCOMPARE(text->contentWidth(), 0.0);

    QQuickText *reference = text->findChild<QQuickText *>("elidedRef");
    QVERIFY(reference);

    text->setWidth(10);
    QCOMPARE(text->contentWidth(), reference->contentWidth());
}

void tst_qquicktext::hAlignWidthDependsOnImplicitWidth_data()
{
    QTest::addColumn<QQuickText::HAlignment>("horizontalAlignment");
    QTest::addColumn<QQuickText::TextElideMode>("elide");
    QTest::addColumn<int>("extraWidth");

    QTest::newRow("AlignHCenter, ElideNone, 0 extraWidth") << QQuickText::AlignHCenter << QQuickText::ElideNone << 0;
    QTest::newRow("AlignRight, ElideNone, 0 extraWidth") << QQuickText::AlignRight << QQuickText::ElideNone << 0;
    QTest::newRow("AlignHCenter, ElideRight, 0 extraWidth") << QQuickText::AlignHCenter << QQuickText::ElideRight << 0;
    QTest::newRow("AlignRight, ElideRight, 0 extraWidth") << QQuickText::AlignRight << QQuickText::ElideRight << 0;
    QTest::newRow("AlignHCenter, ElideNone, 20 extraWidth") << QQuickText::AlignHCenter << QQuickText::ElideNone << 20;
    QTest::newRow("AlignRight, ElideNone, 20 extraWidth") << QQuickText::AlignRight << QQuickText::ElideNone << 20;
    QTest::newRow("AlignHCenter, ElideRight, 20 extraWidth") << QQuickText::AlignHCenter << QQuickText::ElideRight << 20;
    QTest::newRow("AlignRight, ElideRight, 20 extraWidth") << QQuickText::AlignRight << QQuickText::ElideRight << 20;
}

void tst_qquicktext::hAlignWidthDependsOnImplicitWidth()
{
    QFETCH(QQuickText::HAlignment, horizontalAlignment);
    QFETCH(QQuickText::TextElideMode, elide);
    QFETCH(int, extraWidth);

    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("hAlignWidthDependsOnImplicitWidth.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickItem *rect = window->rootObject();
    QVERIFY(rect);

    QVERIFY(rect->setProperty("horizontalAlignment", horizontalAlignment));
    QVERIFY(rect->setProperty("elide", elide));
    QVERIFY(rect->setProperty("extraWidth", extraWidth));

    QImage image = window->grabWindow();
    const int rectX = 100 * window->screen()->devicePixelRatio();
    QCOMPARE(numberOfNonWhitePixels(0, rectX - 1, image), 0);

    QVERIFY(rect->setProperty("text", "this is mis-aligned"));
    image = window->grabWindow();
    QCOMPARE(numberOfNonWhitePixels(0, rectX - 1, image), 0);
}

QTEST_MAIN(tst_qquicktext)

#include "tst_qquicktext.moc"
