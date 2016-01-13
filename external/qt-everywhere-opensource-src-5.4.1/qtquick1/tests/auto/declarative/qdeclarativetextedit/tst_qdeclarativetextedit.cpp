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
#include <QtTest/QSignalSpy>
#include "../shared/testhttpserver.h"
#include <qdeclarativedatatest.h>
#include <math.h>
#include <QFile>
#include <QTextDocument>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecontext.h>
#include <QtDeclarative/qdeclarativeexpression.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <private/qdeclarativetextedit_p.h>
#include <private/qdeclarativetextedit_p_p.h>
#include <QFontMetrics>
#include <QtDeclarative/QDeclarativeView>
#include <QDir>
#include <QStyle>
#include <QClipboard>
#include <QMimeData>
#include <private/qapplication_p.h>
#include <private/qinputmethod_p.h>
#include <private/qwidgettextcontrol_p.h>
#include "../shared/platforminputcontext.h"

Q_DECLARE_METATYPE(QDeclarativeTextEdit::SelectionMode)

// Make a widget frameless to prevent size constraints of title bars
// from interfering (Windows).
static inline void setFrameless(QWidget *w)
{
    Qt::WindowFlags flags = w->windowFlags();
    flags |= Qt::FramelessWindowHint;
    flags &= ~(Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    w->setWindowFlags(flags);
}

// Helper message for comparisons
static inline QByteArray msgComparison(int v1, int v2)
{
    return QByteArray::number(v1) + ' ' + QByteArray::number(v2);
}

void sendPreeditText(const QString &text, int cursor)
{
    QList<QInputMethodEvent::Attribute> attributes;
    attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, cursor,
                                                   text.length(), QVariant()));
    QInputMethodEvent event(text, attributes);
    if (qApp->focusObject())
        QApplication::sendEvent(qApp->focusObject(), &event);
}

// A QScopedPointer which provides operator QDeclarativeView *, removing the
// need to write canvas.data() where a QDeclarativeView * is required.
class DeclarativeViewScopedPointer : public QScopedPointer<QDeclarativeView> {
public:
    explicit inline DeclarativeViewScopedPointer(QDeclarativeView *v = 0) : QScopedPointer<QDeclarativeView>(v) {}
    inline operator QDeclarativeView *() const { return data(); }
};

class tst_qdeclarativetextedit : public QDeclarativeDataTest

{
    Q_OBJECT
public:
    tst_qdeclarativetextedit();

private slots:
    void cleanup();

    void text();
    void width();
    void wrap();
    void textFormat();
    void alignments();
    void alignments_data();

    // ### these tests may be trivial
    void hAlign();
    void hAlign_RightToLeft();
    void vAlign();
    void font();
    void color();
    void textMargin();
    void persistentSelection();
    void focusOnPress();
    void selection();
    void isRightToLeft_data();
    void isRightToLeft();
    void keySelection();
    void moveCursorSelection_data();
    void moveCursorSelection();
    void moveCursorSelectionSequence_data();
    void moveCursorSelectionSequence();
    void mouseSelection_data();
    void mouseSelection();
    void multilineMouseSelection();
    void deferEnableSelectByMouse_data();
    void deferEnableSelectByMouse();
    void deferDisableSelectByMouse_data();
    void deferDisableSelectByMouse();
    void mouseSelectionMode_data();
    void mouseSelectionMode();
    void dragMouseSelection();
    void inputMethodHints();

    void positionAt();

    void cursorDelegate();
    void cursorVisible();
    void delegateLoading_data();
    void delegateLoading();
    void navigation();
    void readOnly();
    void copyAndPaste();
    void canPaste();
    void canPasteEmpty();
    void textInput();
    void openInputPanelOnClick();
    void openInputPanelOnFocus();
    void geometrySignals();
    void pastingRichText_QTBUG_14003();
    void implicitSize_data();
    void implicitSize();
    void implicitSizePreedit_data();
    void implicitSizePreedit();
    void testQtQuick11Attributes();
    void testQtQuick11Attributes_data();

    void preeditMicroFocus();
    void inputContextMouseHandler();
    void inputMethodComposing();
    void cursorRectangleSize();
    void deselect();

private:
    void simulateKey(QDeclarativeView *, int key, Qt::KeyboardModifiers modifiers = 0);
    QDeclarativeView *createView(const QString &filename);
    QString alignmentReferenceImage(const QString& filebasename) const;

    QStringList standard;
    QStringList richText;

    QStringList hAlignmentStrings;
    QStringList vAlignmentStrings;

    QList<Qt::Alignment> vAlignments;
    QList<Qt::Alignment> hAlignments;

    QStringList colorStrings;

    QDeclarativeEngine engine;
};

tst_qdeclarativetextedit::tst_qdeclarativetextedit()
{
    standard << "the quick brown fox jumped over the lazy dog"
             << "the quick brown fox\n jumped over the lazy dog"
             << "Hello, world!"
             << "!dlrow ,olleH";

    richText << "<i>the <b>quick</b> brown <a href=\\\"http://www.google.com\\\">fox</a> jumped over the <b>lazy</b> dog</i>"
             << "<i>the <b>quick</b> brown <a href=\\\"http://www.google.com\\\">fox</a><br>jumped over the <b>lazy</b> dog</i>";

    hAlignmentStrings << "AlignLeft"
                      << "AlignRight"
                      << "AlignHCenter";

    vAlignmentStrings << "AlignTop"
                      << "AlignBottom"
                      << "AlignVCenter";

    hAlignments << Qt::AlignLeft
                << Qt::AlignRight
                << Qt::AlignHCenter;

    vAlignments << Qt::AlignTop
                << Qt::AlignBottom
                << Qt::AlignVCenter;

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

void tst_qdeclarativetextedit::cleanup()
{
    // ensure not even skipped tests with custom input context leave it dangling
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = 0;
}

void tst_qdeclarativetextedit::text()
{
    {
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData("import QtQuick 1.0\nTextEdit {  text: \"\"  }", QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->text(), QString(""));
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nTextEdit { text: \"" + standard.at(i) + "\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->text(), standard.at(i));
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nTextEdit { text: \"" + richText.at(i) + "\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QString actual = textEditObject->text();
        QString expected = richText.at(i);
        actual.replace(QRegExp(".*<body[^>]*>"),"");
        actual.replace(QRegExp("(<[^>]*>)+"),"<>");
        expected.replace(QRegExp("(<[^>]*>)+"),"<>");
        QCOMPARE(actual.simplified(),expected.simplified());
    }
}

void tst_qdeclarativetextedit::width()
{
    // uses Font metrics to find the width for standard and document to find the width for rich
    {
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData("import QtQuick 1.0\nTextEdit {  text: \"\" }", QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), 0.0);
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QFont f;
        QFontMetricsF fm(f);
        qreal metricWidth = fm.size(Qt::TextExpandTabs && Qt::TextShowMnemonic, standard.at(i)).width();
        metricWidth = ceil(metricWidth);

        QString componentStr = "import QtQuick 1.0\nTextEdit { text: \"" + standard.at(i) + "\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), qreal(metricWidth));
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QTextDocument document;
        document.setHtml(richText.at(i));
        document.setDocumentMargin(0);

        int documentWidth = ceil(document.idealWidth());

        QString componentStr = "import QtQuick 1.0\nTextEdit { text: \"" + richText.at(i) + "\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), qreal(documentWidth));
    }
}

void tst_qdeclarativetextedit::wrap()
{
    // for specified width and wrap set true
    {
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData("import QtQuick 1.0\nTextEdit {  text: \"\"; wrapMode: TextEdit.WordWrap; width: 300 }", QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), 300.);
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  wrapMode: TextEdit.WordWrap; width: 300; text: \"" + standard.at(i) + "\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), 300.);
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  wrapMode: TextEdit.WordWrap; width: 300; text: \"" + richText.at(i) + "\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), 300.);
    }

}

void tst_qdeclarativetextedit::textFormat()
{
    {
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData("import QtQuick 1.0\nTextEdit { text: \"Hello\"; textFormat: Text.RichText }", QUrl::fromLocalFile(""));
        QDeclarativeTextEdit *textObject = qobject_cast<QDeclarativeTextEdit*>(textComponent.create());

        QVERIFY(textObject != 0);
        QVERIFY(textObject->textFormat() == QDeclarativeTextEdit::RichText);
    }
    {
        QDeclarativeComponent textComponent(&engine);
        textComponent.setData("import QtQuick 1.0\nTextEdit { text: \"<b>Hello</b>\"; textFormat: Text.PlainText }", QUrl::fromLocalFile(""));
        QDeclarativeTextEdit *textObject = qobject_cast<QDeclarativeTextEdit*>(textComponent.create());

        QVERIFY(textObject != 0);
        QVERIFY(textObject->textFormat() == QDeclarativeTextEdit::PlainText);
    }
}

void tst_qdeclarativetextedit::alignments_data()
{
    QTest::addColumn<int>("hAlign");
    QTest::addColumn<int>("vAlign");
    QTest::addColumn<QString>("expectfile");

    QTest::newRow("LT") << int(Qt::AlignLeft) << int(Qt::AlignTop) << "alignments_lt";
    QTest::newRow("RT") << int(Qt::AlignRight) << int(Qt::AlignTop) << "alignments_rt";
    QTest::newRow("CT") << int(Qt::AlignHCenter) << int(Qt::AlignTop) << "alignments_ct";

    QTest::newRow("LB") << int(Qt::AlignLeft) << int(Qt::AlignBottom) << "alignments_lb";
    QTest::newRow("RB") << int(Qt::AlignRight) << int(Qt::AlignBottom) << "alignments_rb";
    QTest::newRow("CB") << int(Qt::AlignHCenter) << int(Qt::AlignBottom) << "alignments_cb";

    QTest::newRow("LC") << int(Qt::AlignLeft) << int(Qt::AlignVCenter) << "alignments_lc";
    QTest::newRow("RC") << int(Qt::AlignRight) << int(Qt::AlignVCenter) << "alignments_rc";
    QTest::newRow("CC") << int(Qt::AlignHCenter) << int(Qt::AlignVCenter) << "alignments_cc";
}

QString tst_qdeclarativetextedit::alignmentReferenceImage(const QString& filebasename) const
{
    // XXX This will be replaced by some clever persistent platform image store.
    static const QString arch = QGuiApplication::platformName() + QLatin1Char('-') + qApp->style()->objectName();
    return testFile(filebasename + QLatin1Char('-') + arch + QStringLiteral(".png"));
}

void tst_qdeclarativetextedit::alignments()
{
    QFETCH(int, hAlign);
    QFETCH(int, vAlign);
    QFETCH(QString, expectfile);

    const QString referenceImage = alignmentReferenceImage(expectfile);
    if (!QFile(referenceImage).exists())
        QSKIP(qPrintable(QStringLiteral("Reference image '")
                         + QDir::toNativeSeparators(referenceImage)
                         + QStringLiteral("' does not exist.")));

    DeclarativeViewScopedPointer canvas(createView(testFile("alignments.qml")));
    setFrameless(canvas);

    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
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

    QImage expect(referenceImage);
    QVERIFY(!expect.isNull());

    QCOMPARE(actual,expect);
}


//the alignment tests may be trivial o.oa
void tst_qdeclarativetextedit::hAlign()
{
    //test one align each, and then test if two align fails.

    for (int i = 0; i < standard.size(); i++)
    {
        for (int j=0; j < hAlignmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 1.0\nTextEdit {  horizontalAlignment: \"" + hAlignmentStrings.at(j) + "\"; text: \"" + standard.at(i) + "\" }";
            QDeclarativeComponent texteditComponent(&engine);
            texteditComponent.setData(componentStr.toLatin1(), QUrl());
            QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

            QVERIFY(textEditObject != 0);
            QCOMPARE((int)textEditObject->hAlign(), (int)hAlignments.at(j));
        }
    }

    for (int i = 0; i < richText.size(); i++)
    {
        for (int j=0; j < hAlignmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 1.0\nTextEdit {  horizontalAlignment: \"" + hAlignmentStrings.at(j) + "\"; text: \"" + richText.at(i) + "\" }";
            QDeclarativeComponent texteditComponent(&engine);
            texteditComponent.setData(componentStr.toLatin1(), QUrl());
            QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

            QVERIFY(textEditObject != 0);
            QCOMPARE((int)textEditObject->hAlign(), (int)hAlignments.at(j));
        }
    }

}

void tst_qdeclarativetextedit::hAlign_RightToLeft()
{
    DeclarativeViewScopedPointer canvas(createView(testFile("horizontalAlignment_RightToLeft.qml")));
    QDeclarativeTextEdit *textEdit = canvas->rootObject()->findChild<QDeclarativeTextEdit*>("text");
    QVERIFY(textEdit != 0);
    canvas->show();

    const QString rtlText = textEdit->text();

    // implicit alignment should follow the reading direction of text
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > canvas->width()/2);

    // explicitly left aligned
    textEdit->setHAlign(QDeclarativeTextEdit::AlignLeft);
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignLeft);
    QVERIFY(textEdit->positionToRectangle(0).x() < canvas->width()/2);

    // explicitly right aligned
    textEdit->setHAlign(QDeclarativeTextEdit::AlignRight);
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > canvas->width()/2);

    QString textString = textEdit->text();
    textEdit->setText(QString("<i>") + textString + QString("</i>"));
    textEdit->resetHAlign();

    // implicitly aligned rich text should follow the reading direction of RTL text
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignRight);
    QCOMPARE(textEdit->effectiveHAlign(), textEdit->hAlign());
    QVERIFY(textEdit->positionToRectangle(0).x() > canvas->width()/2);

    // explicitly left aligned rich text
    textEdit->setHAlign(QDeclarativeTextEdit::AlignLeft);
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignLeft);
    QCOMPARE(textEdit->effectiveHAlign(), textEdit->hAlign());
    QVERIFY(textEdit->positionToRectangle(0).x() < canvas->width()/2);

    // explicitly right aligned rich text
    textEdit->setHAlign(QDeclarativeTextEdit::AlignRight);
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignRight);
    QCOMPARE(textEdit->effectiveHAlign(), textEdit->hAlign());
    QVERIFY(textEdit->positionToRectangle(0).x() > canvas->width()/2);

    textEdit->setText(textString);

    // explicitly center aligned
    textEdit->setHAlign(QDeclarativeTextEdit::AlignHCenter);
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignHCenter);
    QVERIFY(textEdit->positionToRectangle(0).x() > canvas->width()/2);

    // reseted alignment should go back to following the text reading direction
    textEdit->resetHAlign();
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > canvas->width()/2);

    // mirror the text item
    QDeclarativeItemPrivate::get(textEdit)->setLayoutMirror(true);

    // mirrored implicit alignment should continue to follow the reading direction of the text
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignRight);
    QCOMPARE(textEdit->effectiveHAlign(), QDeclarativeTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > canvas->width()/2);

    // mirrored explicitly right aligned behaves as left aligned
    textEdit->setHAlign(QDeclarativeTextEdit::AlignRight);
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignRight);
    QCOMPARE(textEdit->effectiveHAlign(), QDeclarativeTextEdit::AlignLeft);
    QVERIFY(textEdit->positionToRectangle(0).x() < canvas->width()/2);

    // mirrored explicitly left aligned behaves as right aligned
    textEdit->setHAlign(QDeclarativeTextEdit::AlignLeft);
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignLeft);
    QCOMPARE(textEdit->effectiveHAlign(), QDeclarativeTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > canvas->width()/2);

    // disable mirroring
    QDeclarativeItemPrivate::get(textEdit)->setLayoutMirror(false);
    textEdit->resetHAlign();

    // English text should be implicitly left aligned
    textEdit->setText("Hello world!");
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignLeft);
    QVERIFY(textEdit->positionToRectangle(0).x() < canvas->width()/2);

    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    textEdit->setText(QString());
    { QInputMethodEvent ev(rtlText, QList<QInputMethodEvent::Attribute>()); QApplication::sendEvent(canvas, &ev); }
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignRight);
    { QInputMethodEvent ev("Hello world!", QList<QInputMethodEvent::Attribute>()); QApplication::sendEvent(canvas, &ev); }
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignLeft);

#ifndef Q_OS_MAC    // QTBUG-18040
    // empty text with implicit alignment follows the system locale-based
    // keyboard input direction from QInputMethod::inputDirection
    textEdit->setText("");
    QCOMPARE(textEdit->hAlign(), qApp->inputMethod()->inputDirection() == Qt::LeftToRight ?
                                  QDeclarativeTextEdit::AlignLeft : QDeclarativeTextEdit::AlignRight);
    if (qApp->inputMethod()->inputDirection() == Qt::LeftToRight)
        QVERIFY(textEdit->positionToRectangle(0).x() < canvas->width()/2);
    else
        QVERIFY(textEdit->positionToRectangle(0).x() > canvas->width()/2);
    textEdit->setHAlign(QDeclarativeTextEdit::AlignRight);
    QCOMPARE(textEdit->hAlign(), QDeclarativeTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > canvas->width()/2);
#endif

    canvas.reset();

#ifndef Q_OS_MAC    // QTBUG-18040
    // alignment of TextEdit with no text set to it
    QString componentStr = "import QtQuick 1.0\nTextEdit {}";
    QDeclarativeComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeTextEdit *textObject = qobject_cast<QDeclarativeTextEdit*>(textComponent.create());
    QCOMPARE(textObject->hAlign(), qApp->inputMethod()->inputDirection() == Qt::LeftToRight ?
                                  QDeclarativeTextEdit::AlignLeft : QDeclarativeTextEdit::AlignRight);
    delete textObject;
#endif
}

void tst_qdeclarativetextedit::vAlign()
{
    //test one align each, and then test if two align fails.

    for (int i = 0; i < standard.size(); i++)
    {
        for (int j=0; j < vAlignmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 1.0\nTextEdit {  verticalAlignment: \"" + vAlignmentStrings.at(j) + "\"; text: \"" + standard.at(i) + "\" }";
            QDeclarativeComponent texteditComponent(&engine);
            texteditComponent.setData(componentStr.toLatin1(), QUrl());
            QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

            QVERIFY(textEditObject != 0);
            QCOMPARE((int)textEditObject->vAlign(), (int)vAlignments.at(j));
        }
    }

    for (int i = 0; i < richText.size(); i++)
    {
        for (int j=0; j < vAlignmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 1.0\nTextEdit {  verticalAlignment: \"" + vAlignmentStrings.at(j) + "\"; text: \"" + richText.at(i) + "\" }";
            QDeclarativeComponent texteditComponent(&engine);
            texteditComponent.setData(componentStr.toLatin1(), QUrl());
            QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

            QVERIFY(textEditObject != 0);
            QCOMPARE((int)textEditObject->vAlign(), (int)vAlignments.at(j));
        }
    }

    QDeclarativeComponent texteditComponent(&engine);
    texteditComponent.setData("import QtQuick 1.0\n TextEdit { width: 200; height: 200; text: \"Hello World\" }", QUrl());
    QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

    QVERIFY(textEditObject != 0);

    QCOMPARE(textEditObject->vAlign(), QDeclarativeTextEdit::AlignTop);
    QVERIFY(textEditObject->cursorRectangle().bottom() < 50);
    QVERIFY(textEditObject->positionToRectangle(0).bottom() < 50);

    // bottom aligned
    textEditObject->setVAlign(QDeclarativeTextEdit::AlignBottom);

    QCOMPARE(textEditObject->vAlign(), QDeclarativeTextEdit::AlignBottom);
    QVERIFY(textEditObject->cursorRectangle().top() > 100);
    QVERIFY(textEditObject->positionToRectangle(0).top() > 100);

    // explicitly center aligned
    textEditObject->setVAlign(QDeclarativeTextEdit::AlignVCenter);
    QCOMPARE(textEditObject->vAlign(), QDeclarativeTextEdit::AlignVCenter);
    QVERIFY(textEditObject->cursorRectangle().top() < 100);
    QVERIFY(textEditObject->cursorRectangle().bottom() > 100);
    QVERIFY(textEditObject->positionToRectangle(0).top() < 100);
    QVERIFY(textEditObject->positionToRectangle(0).bottom() > 100);

    // Test vertical alignment after resizing the height.
    textEditObject->setHeight(textEditObject->height() - 20);
    QVERIFY(textEditObject->cursorRectangle().top() < 90);
    QVERIFY(textEditObject->cursorRectangle().bottom() > 90);
    QVERIFY(textEditObject->positionToRectangle(0).top() < 90);
    QVERIFY(textEditObject->positionToRectangle(0).bottom() > 90);

    textEditObject->setHeight(textEditObject->height() + 40);
    QVERIFY(textEditObject->cursorRectangle().top() < 110);
    QVERIFY(textEditObject->cursorRectangle().bottom() > 110);
    QVERIFY(textEditObject->positionToRectangle(0).top() < 110);
    QVERIFY(textEditObject->positionToRectangle(0).bottom() > 110);
}

void tst_qdeclarativetextedit::font()
{
    //test size, then bold, then italic, then family
    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  font.pointSize: 40; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->font().pointSize(), 40);
        QCOMPARE(textEditObject->font().bold(), false);
        QCOMPARE(textEditObject->font().italic(), false);
    }

    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  font.bold: true; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->font().bold(), true);
        QCOMPARE(textEditObject->font().italic(), false);
    }

    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  font.italic: true; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->font().italic(), true);
        QCOMPARE(textEditObject->font().bold(), false);
    }

    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  font.family: \"Helvetica\"; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->font().family(), QString("Helvetica"));
        QCOMPARE(textEditObject->font().bold(), false);
        QCOMPARE(textEditObject->font().italic(), false);
    }

    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  font.family: \"\"; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->font().family(), QString(""));
    }
}

void tst_qdeclarativetextedit::color()
{
    //test initial color
    {
        QString componentStr = "import QtQuick 1.0\nTextEdit { text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QDeclarativeTextEditPrivate *textEditPrivate = static_cast<QDeclarativeTextEditPrivate*>(QDeclarativeItemPrivate::get(textEditObject));

        QVERIFY(textEditObject);
        QVERIFY(textEditPrivate);
        QVERIFY(textEditPrivate->control);

        QPalette pal = textEditPrivate->control->palette();
        QCOMPARE(textEditPrivate->color, QColor("black"));
        QCOMPARE(textEditPrivate->color, pal.color(QPalette::Text));
    }
    //test normal
    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  color: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());
        //qDebug() << "textEditObject: " << textEditObject->color() << "vs. " << QColor(colorStrings.at(i));
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->color(), QColor(colorStrings.at(i)));
    }

    //test selection
    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  selectionColor: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->selectionColor(), QColor(colorStrings.at(i)));
    }

    //test selected text
    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  selectedTextColor: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->selectedTextColor(), QColor(colorStrings.at(i)));
    }

    {
        QString colorStr = "#AA001234";
        QColor testColor("#001234");
        testColor.setAlpha(170);

        QString componentStr = "import QtQuick 1.0\nTextEdit {  color: \"" + colorStr + "\"; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->color(), testColor);
    }
}

void tst_qdeclarativetextedit::textMargin()
{
    for(qreal i=0; i<=10; i+=0.3){
        QString componentStr = "import QtQuick 1.0\nTextEdit {  textMargin: " + QString::number(i) + "; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->textMargin(), i);
    }
}

void tst_qdeclarativetextedit::persistentSelection()
{
    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  persistentSelection: true; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->persistentSelection(), true);
    }

    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  persistentSelection: false; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->persistentSelection(), false);
    }
}

void tst_qdeclarativetextedit::focusOnPress()
{
    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  activeFocusOnPress: true; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->focusOnPress(), true);
    }

    {
        QString componentStr = "import QtQuick 1.0\nTextEdit {  activeFocusOnPress: false; text: \"Hello World\" }";
        QDeclarativeComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->focusOnPress(), false);
    }
}

void tst_qdeclarativetextedit::selection()
{
    QString testStr = standard[0];//TODO: What should happen for multiline/rich text?
    QString componentStr = "import QtQuick 1.0\nTextEdit {  text: \""+ testStr +"\"; }";
    QDeclarativeComponent texteditComponent(&engine);
    texteditComponent.setData(componentStr.toLatin1(), QUrl());
    QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());
    QVERIFY(textEditObject != 0);


    //Test selection follows cursor
    for(int i=0; i<= testStr.size(); i++) {
        textEditObject->setCursorPosition(i);
        QCOMPARE(textEditObject->cursorPosition(), i);
        QCOMPARE(textEditObject->selectionStart(), i);
        QCOMPARE(textEditObject->selectionEnd(), i);
        QVERIFY(textEditObject->selectedText().isNull());
    }
    //Test cursor follows selection
    for(int i=0; i<= testStr.size(); i++) {
        textEditObject->select(i,i);
        QCOMPARE(textEditObject->cursorPosition(), i);
        QCOMPARE(textEditObject->selectionStart(), i);
        QCOMPARE(textEditObject->selectionEnd(), i);
    }


    textEditObject->setCursorPosition(0);
    QVERIFY(textEditObject->cursorPosition() == 0);
    QVERIFY(textEditObject->selectionStart() == 0);
    QVERIFY(textEditObject->selectionEnd() == 0);
    QVERIFY(textEditObject->selectedText().isNull());

    // Verify invalid positions are ignored.
    textEditObject->setCursorPosition(-1);
    QVERIFY(textEditObject->cursorPosition() == 0);
    QVERIFY(textEditObject->selectionStart() == 0);
    QVERIFY(textEditObject->selectionEnd() == 0);
    QVERIFY(textEditObject->selectedText().isNull());

    textEditObject->setCursorPosition(textEditObject->text().count()+1);
    QVERIFY(textEditObject->cursorPosition() == 0);
    QVERIFY(textEditObject->selectionStart() == 0);
    QVERIFY(textEditObject->selectionEnd() == 0);
    QVERIFY(textEditObject->selectedText().isNull());

    //Test selection
    for(int i=0; i<= testStr.size(); i++) {
        textEditObject->select(0,i);
        QCOMPARE(testStr.mid(0,i), textEditObject->selectedText());
        QCOMPARE(textEditObject->cursorPosition(), i);
    }
    for(int i=0; i<= testStr.size(); i++) {
        textEditObject->select(i,testStr.size());
        QCOMPARE(testStr.mid(i,testStr.size()-i), textEditObject->selectedText());
        QCOMPARE(textEditObject->cursorPosition(), testStr.size());
    }

    textEditObject->setCursorPosition(0);
    QVERIFY(textEditObject->cursorPosition() == 0);
    QVERIFY(textEditObject->selectionStart() == 0);
    QVERIFY(textEditObject->selectionEnd() == 0);
    QVERIFY(textEditObject->selectedText().isNull());

    //Test Error Ignoring behaviour
    textEditObject->setCursorPosition(0);
    QVERIFY(textEditObject->selectedText().isNull());
    textEditObject->select(-10,0);
    QVERIFY(textEditObject->selectedText().isNull());
    textEditObject->select(100,101);
    QVERIFY(textEditObject->selectedText().isNull());
    textEditObject->select(0,-10);
    QVERIFY(textEditObject->selectedText().isNull());
    textEditObject->select(0,100);
    QVERIFY(textEditObject->selectedText().isNull());
    textEditObject->select(0,10);
    QVERIFY(textEditObject->selectedText().size() == 10);
    textEditObject->select(-10,0);
    QVERIFY(textEditObject->selectedText().size() == 10);
    textEditObject->select(100,101);
    QVERIFY(textEditObject->selectedText().size() == 10);
    textEditObject->select(0,-10);
    QVERIFY(textEditObject->selectedText().size() == 10);
    textEditObject->select(0,100);
    QVERIFY(textEditObject->selectedText().size() == 10);

    textEditObject->deselect();
    QVERIFY(textEditObject->selectedText().isNull());
    textEditObject->select(0,10);
    QVERIFY(textEditObject->selectedText().size() == 10);
    textEditObject->deselect();
    QVERIFY(textEditObject->selectedText().isNull());
}

void tst_qdeclarativetextedit::isRightToLeft_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("emptyString");
    QTest::addColumn<bool>("firstCharacter");
    QTest::addColumn<bool>("lastCharacter");
    QTest::addColumn<bool>("middleCharacter");
    QTest::addColumn<bool>("startString");
    QTest::addColumn<bool>("midString");
    QTest::addColumn<bool>("endString");

    const quint16 arabic_str[] = { 0x0638, 0x0643, 0x00646, 0x0647, 0x0633, 0x0638, 0x0643, 0x00646, 0x0647, 0x0633, 0x0647};
    QTest::newRow("Empty") << "" << false << false << false << false << false << false << false;
    QTest::newRow("Neutral") << "23244242" << false << false << false << false << false << false << false;
    QTest::newRow("LTR") << "Hello world" << false << false << false << false << false << false << false;
    QTest::newRow("RTL") << QString::fromUtf16(arabic_str, 11) << false << true << true << true << true << true << true;
    QTest::newRow("Bidi RTL + LTR + RTL") << QString::fromUtf16(arabic_str, 11) + QString("Hello world") + QString::fromUtf16(arabic_str, 11) << false << true << true << false << true << true << true;
    QTest::newRow("Bidi LTR + RTL + LTR") << QString("Hello world") + QString::fromUtf16(arabic_str, 11) + QString("Hello world") << false << false << false << true << false << false << false;
}

void tst_qdeclarativetextedit::isRightToLeft()
{
    QFETCH(QString, text);
    QFETCH(bool, emptyString);
    QFETCH(bool, firstCharacter);
    QFETCH(bool, lastCharacter);
    QFETCH(bool, middleCharacter);
    QFETCH(bool, startString);
    QFETCH(bool, midString);
    QFETCH(bool, endString);

    QDeclarativeTextEdit textEdit;
    textEdit.setText(text);

    // first test that the right string is delivered to the QString::isRightToLeft()
    QCOMPARE(textEdit.isRightToLeft(0,0), text.mid(0,0).isRightToLeft());
    QCOMPARE(textEdit.isRightToLeft(0,1), text.mid(0,1).isRightToLeft());
    QCOMPARE(textEdit.isRightToLeft(text.count()-2, text.count()-1), text.mid(text.count()-2, text.count()-1).isRightToLeft());
    QCOMPARE(textEdit.isRightToLeft(text.count()/2, text.count()/2 + 1), text.mid(text.count()/2, text.count()/2 + 1).isRightToLeft());
    QCOMPARE(textEdit.isRightToLeft(0,text.count()/4), text.mid(0,text.count()/4).isRightToLeft());
    QCOMPARE(textEdit.isRightToLeft(text.count()/4,3*text.count()/4), text.mid(text.count()/4,3*text.count()/4).isRightToLeft());
    if (text.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML TextEdit: isRightToLeft(start, end) called with the end property being smaller than the start.");
    QCOMPARE(textEdit.isRightToLeft(3*text.count()/4,text.count()-1), text.mid(3*text.count()/4,text.count()-1).isRightToLeft());

    // then test that the feature actually works
    QCOMPARE(textEdit.isRightToLeft(0,0), emptyString);
    QCOMPARE(textEdit.isRightToLeft(0,1), firstCharacter);
    QCOMPARE(textEdit.isRightToLeft(text.count()-2, text.count()-1), lastCharacter);
    QCOMPARE(textEdit.isRightToLeft(text.count()/2, text.count()/2 + 1), middleCharacter);
    QCOMPARE(textEdit.isRightToLeft(0,text.count()/4), startString);
    QCOMPARE(textEdit.isRightToLeft(text.count()/4,3*text.count()/4), midString);
    if (text.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML TextEdit: isRightToLeft(start, end) called with the end property being smaller than the start.");
    QCOMPARE(textEdit.isRightToLeft(3*text.count()/4,text.count()-1), endString);
}

void tst_qdeclarativetextedit::keySelection()
{
    DeclarativeViewScopedPointer canvas(createView(testFile("navigation.qml")));
    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));
    canvas->setFocus();

    QVERIFY(canvas->rootObject() != 0);

    QDeclarativeTextEdit *input = qobject_cast<QDeclarativeTextEdit *>(qvariant_cast<QObject *>(canvas->rootObject()->property("myInput")));

    QVERIFY(input != 0);
    QTRY_VERIFY(input->hasActiveFocus() == true);

    QSignalSpy spy(input, SIGNAL(selectionChanged()));

    simulateKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
    QVERIFY(input->hasActiveFocus() == true);
    QCOMPARE(input->selectedText(), QString("a"));
    QCOMPARE(spy.count(), 1);
    simulateKey(canvas, Qt::Key_Right);
    QVERIFY(input->hasActiveFocus() == true);
    QCOMPARE(input->selectedText(), QString());
    QCOMPARE(spy.count(), 2);
    simulateKey(canvas, Qt::Key_Right);
    QVERIFY(input->hasActiveFocus() == false);
    QCOMPARE(input->selectedText(), QString());
    QCOMPARE(spy.count(), 2);

    simulateKey(canvas, Qt::Key_Left);
    QVERIFY(input->hasActiveFocus() == true);
    QCOMPARE(spy.count(), 2);
    simulateKey(canvas, Qt::Key_Left, Qt::ShiftModifier);
    QVERIFY(input->hasActiveFocus() == true);
    QCOMPARE(input->selectedText(), QString("a"));
    QCOMPARE(spy.count(), 3);
    simulateKey(canvas, Qt::Key_Left);
    QVERIFY(input->hasActiveFocus() == true);
    QCOMPARE(input->selectedText(), QString());
    QCOMPARE(spy.count(), 4);
    simulateKey(canvas, Qt::Key_Left);
    QVERIFY(input->hasActiveFocus() == false);
    QCOMPARE(input->selectedText(), QString());
    QCOMPARE(spy.count(), 4);
}

void tst_qdeclarativetextedit::moveCursorSelection_data()
{
    QTest::addColumn<QString>("testStr");
    QTest::addColumn<int>("cursorPosition");
    QTest::addColumn<int>("movePosition");
    QTest::addColumn<QDeclarativeTextEdit::SelectionMode>("mode");
    QTest::addColumn<int>("selectionStart");
    QTest::addColumn<int>("selectionEnd");
    QTest::addColumn<bool>("reversible");

    QTest::newRow("(t)he|characters")
            << standard[0] << 0 << 1 << QDeclarativeTextEdit::SelectCharacters << 0 << 1 << true;
    QTest::newRow("do(g)|characters")
            << standard[0] << 43 << 44 << QDeclarativeTextEdit::SelectCharacters << 43 << 44 << true;
    QTest::newRow("jum(p)ed|characters")
            << standard[0] << 23 << 24 << QDeclarativeTextEdit::SelectCharacters << 23 << 24 << true;
    QTest::newRow("jumped( )over|characters")
            << standard[0] << 26 << 27 << QDeclarativeTextEdit::SelectCharacters << 26 << 27 << true;
    QTest::newRow("(the )|characters")
            << standard[0] << 0 << 4 << QDeclarativeTextEdit::SelectCharacters << 0 << 4 << true;
    QTest::newRow("( dog)|characters")
            << standard[0] << 40 << 44 << QDeclarativeTextEdit::SelectCharacters << 40 << 44 << true;
    QTest::newRow("( jumped )|characters")
            << standard[0] << 19 << 27 << QDeclarativeTextEdit::SelectCharacters << 19 << 27 << true;
    QTest::newRow("th(e qu)ick|characters")
            << standard[0] << 2 << 6 << QDeclarativeTextEdit::SelectCharacters << 2 << 6 << true;
    QTest::newRow("la(zy d)og|characters")
            << standard[0] << 38 << 42 << QDeclarativeTextEdit::SelectCharacters << 38 << 42 << true;
    QTest::newRow("jum(ped ov)er|characters")
            << standard[0] << 23 << 29 << QDeclarativeTextEdit::SelectCharacters << 23 << 29 << true;
    QTest::newRow("()the|characters")
            << standard[0] << 0 << 0 << QDeclarativeTextEdit::SelectCharacters << 0 << 0 << true;
    QTest::newRow("dog()|characters")
            << standard[0] << 44 << 44 << QDeclarativeTextEdit::SelectCharacters << 44 << 44 << true;
    QTest::newRow("jum()ped|characters")
            << standard[0] << 23 << 23 << QDeclarativeTextEdit::SelectCharacters << 23 << 23 << true;

    QTest::newRow("<(t)he>|words")
            << standard[0] << 0 << 1 << QDeclarativeTextEdit::SelectWords << 0 << 3 << true;
    QTest::newRow("<do(g)>|words")
            << standard[0] << 43 << 44 << QDeclarativeTextEdit::SelectWords << 41 << 44 << true;
    QTest::newRow("<jum(p)ed>|words")
            << standard[0] << 23 << 24 << QDeclarativeTextEdit::SelectWords << 20 << 26 << true;
    QTest::newRow("<jumped( )>over|words")
            << standard[0] << 26 << 27 << QDeclarativeTextEdit::SelectWords << 20 << 27 << false;
    QTest::newRow("jumped<( )over>|words,reversed")
            << standard[0] << 27 << 26 << QDeclarativeTextEdit::SelectWords << 26 << 31 << false;
    QTest::newRow("<(the )>quick|words")
            << standard[0] << 0 << 4 << QDeclarativeTextEdit::SelectWords << 0 << 4 << false;
    QTest::newRow("<(the )quick>|words,reversed")
            << standard[0] << 4 << 0 << QDeclarativeTextEdit::SelectWords << 0 << 9 << false;
    QTest::newRow("<lazy( dog)>|words")
            << standard[0] << 40 << 44 << QDeclarativeTextEdit::SelectWords << 36 << 44 << false;
    QTest::newRow("lazy<( dog)>|words,reversed")
            << standard[0] << 44 << 40 << QDeclarativeTextEdit::SelectWords << 40 << 44 << false;
    QTest::newRow("<fox( jumped )>over|words")
            << standard[0] << 19 << 27 << QDeclarativeTextEdit::SelectWords << 16 << 27 << false;
    QTest::newRow("fox<( jumped )over>|words,reversed")
            << standard[0] << 27 << 19 << QDeclarativeTextEdit::SelectWords << 19 << 31 << false;
    QTest::newRow("<th(e qu)ick>|words")
            << standard[0] << 2 << 6 << QDeclarativeTextEdit::SelectWords << 0 << 9 << true;
    QTest::newRow("<la(zy d)og|words>")
            << standard[0] << 38 << 42 << QDeclarativeTextEdit::SelectWords << 36 << 44 << true;
    QTest::newRow("<jum(ped ov)er>|words")
            << standard[0] << 23 << 29 << QDeclarativeTextEdit::SelectWords << 20 << 31 << true;
    QTest::newRow("<()>the|words")
            << standard[0] << 0 << 0 << QDeclarativeTextEdit::SelectWords << 0 << 0 << true;
    QTest::newRow("dog<()>|words")
            << standard[0] << 44 << 44 << QDeclarativeTextEdit::SelectWords << 44 << 44 << true;
    QTest::newRow("jum<()>ped|words")
            << standard[0] << 23 << 23 << QDeclarativeTextEdit::SelectWords << 23 << 23 << true;

    QTest::newRow("Hello<(,)> |words")
            << standard[2] << 5 << 6 << QDeclarativeTextEdit::SelectWords << 5 << 6 << true;
    QTest::newRow("Hello<(, )>world|words")
            << standard[2] << 5 << 7 << QDeclarativeTextEdit::SelectWords << 5 << 7 << false;
    QTest::newRow("Hello<(, )world>|words,reversed")
            << standard[2] << 7 << 5 << QDeclarativeTextEdit::SelectWords << 5 << 12 << false;
    QTest::newRow("<Hel(lo, )>world|words")
            << standard[2] << 3 << 7 << QDeclarativeTextEdit::SelectWords << 0 << 7 << false;
    QTest::newRow("<Hel(lo, )world>|words,reversed")
            << standard[2] << 7 << 3 << QDeclarativeTextEdit::SelectWords << 0 << 12 << false;
    QTest::newRow("<Hel(lo)>,|words")
            << standard[2] << 3 << 5 << QDeclarativeTextEdit::SelectWords << 0 << 5 << true;
    QTest::newRow("Hello<()>,|words")
            << standard[2] << 5 << 5 << QDeclarativeTextEdit::SelectWords << 5 << 5 << true;
    QTest::newRow("Hello,<()>|words")
            << standard[2] << 6 << 6 << QDeclarativeTextEdit::SelectWords << 6 << 6 << true;
    QTest::newRow("Hello<,( )>world|words")
            << standard[2] << 6 << 7 << QDeclarativeTextEdit::SelectWords << 5 << 7 << false;
    QTest::newRow("Hello,<( )world>|words,reversed")
            << standard[2] << 7 << 6 << QDeclarativeTextEdit::SelectWords << 6 << 12 << false;
    QTest::newRow("Hello<,( world)>|words")
            << standard[2] << 6 << 12 << QDeclarativeTextEdit::SelectWords << 5 << 12 << false;
    QTest::newRow("Hello,<( world)>|words,reversed")
            << standard[2] << 12 << 6 << QDeclarativeTextEdit::SelectWords << 6 << 12 << false;
    QTest::newRow("Hello<,( world!)>|words")
            << standard[2] << 6 << 13 << QDeclarativeTextEdit::SelectWords << 5 << 13 << false;
    QTest::newRow("Hello,<( world!)>|words,reversed")
            << standard[2] << 13 << 6 << QDeclarativeTextEdit::SelectWords << 6 << 13 << false;
    QTest::newRow("Hello<(, world!)>|words")
            << standard[2] << 5 << 13 << QDeclarativeTextEdit::SelectWords << 5 << 13 << true;
    QTest::newRow("world<(!)>|words")
            << standard[2] << 12 << 13 << QDeclarativeTextEdit::SelectWords << 12 << 13 << true;
    QTest::newRow("world!<()>)|words")
            << standard[2] << 13 << 13 << QDeclarativeTextEdit::SelectWords << 13 << 13 << true;
    QTest::newRow("world<()>!)|words")
            << standard[2] << 12 << 12 << QDeclarativeTextEdit::SelectWords << 12 << 12 << true;

    QTest::newRow("<(,)>olleH |words")
            << standard[3] << 7 << 8 << QDeclarativeTextEdit::SelectWords << 7 << 8 << true;
    QTest::newRow("<dlrow( ,)>olleH|words")
            << standard[3] << 6 << 8 << QDeclarativeTextEdit::SelectWords << 1 << 8 << false;
    QTest::newRow("dlrow<( ,)>olleH|words,reversed")
            << standard[3] << 8 << 6 << QDeclarativeTextEdit::SelectWords << 6 << 8 << false;
    QTest::newRow("<dlrow( ,ol)leH>|words")
            << standard[3] << 6 << 10 << QDeclarativeTextEdit::SelectWords << 1 << 13 << false;
    QTest::newRow("dlrow<( ,ol)leH>|words,reversed")
            << standard[3] << 10 << 6 << QDeclarativeTextEdit::SelectWords << 6 << 13 << false;
    QTest::newRow(",<(ol)leH>,|words")
            << standard[3] << 8 << 10 << QDeclarativeTextEdit::SelectWords << 8 << 13 << true;
    QTest::newRow(",<()>olleH|words")
            << standard[3] << 8 << 8 << QDeclarativeTextEdit::SelectWords << 8 << 8 << true;
    QTest::newRow("<()>,olleH|words")
            << standard[3] << 7 << 7 << QDeclarativeTextEdit::SelectWords << 7 << 7 << true;
    QTest::newRow("<dlrow( )>,olleH|words")
            << standard[3] << 6 << 7 << QDeclarativeTextEdit::SelectWords << 1 << 7 << false;
    QTest::newRow("dlrow<( ),>olleH|words,reversed")
            << standard[3] << 7 << 6 << QDeclarativeTextEdit::SelectWords << 6 << 8 << false;
    QTest::newRow("<(dlrow )>,olleH|words")
            << standard[3] << 1 << 7 << QDeclarativeTextEdit::SelectWords << 1 << 7 << false;
    QTest::newRow("<(dlrow ),>olleH|words,reversed")
            << standard[3] << 7 << 1 << QDeclarativeTextEdit::SelectWords << 1 << 8 << false;
    QTest::newRow("<(!dlrow )>,olleH|words")
            << standard[3] << 0 << 7 << QDeclarativeTextEdit::SelectWords << 0 << 7 << false;
    QTest::newRow("<(!dlrow ),>olleH|words,reversed")
            << standard[3] << 7 << 0 << QDeclarativeTextEdit::SelectWords << 0 << 8 << false;
    QTest::newRow("(!dlrow ,)olleH|words")
            << standard[3] << 0 << 8 << QDeclarativeTextEdit::SelectWords << 0 << 8 << true;
    QTest::newRow("<(!)>dlrow|words")
            << standard[3] << 0 << 1 << QDeclarativeTextEdit::SelectWords << 0 << 1 << true;
    QTest::newRow("<()>!dlrow|words")
            << standard[3] << 0 << 0 << QDeclarativeTextEdit::SelectWords << 0 << 0 << true;
    QTest::newRow("!<()>dlrow|words")
            << standard[3] << 1 << 1 << QDeclarativeTextEdit::SelectWords << 1 << 1 << true;
}

void tst_qdeclarativetextedit::moveCursorSelection()
{
    QFETCH(QString, testStr);
    QFETCH(int, cursorPosition);
    QFETCH(int, movePosition);
    QFETCH(QDeclarativeTextEdit::SelectionMode, mode);
    QFETCH(int, selectionStart);
    QFETCH(int, selectionEnd);
    QFETCH(bool, reversible);

    QString componentStr = "import QtQuick 1.1\nTextEdit {  text: \""+ testStr +"\"; }";
    QDeclarativeComponent textinputComponent(&engine);
    textinputComponent.setData(componentStr.toLatin1(), QUrl());
    QDeclarativeTextEdit *texteditObject = qobject_cast<QDeclarativeTextEdit*>(textinputComponent.create());
    QVERIFY(texteditObject != 0);

    texteditObject->setCursorPosition(cursorPosition);
    texteditObject->moveCursorSelection(movePosition, mode);

    QCOMPARE(texteditObject->selectedText(), testStr.mid(selectionStart, selectionEnd - selectionStart));
    QCOMPARE(texteditObject->selectionStart(), selectionStart);
    QCOMPARE(texteditObject->selectionEnd(), selectionEnd);

    if (reversible) {
        texteditObject->setCursorPosition(movePosition);
        texteditObject->moveCursorSelection(cursorPosition, mode);

        QCOMPARE(texteditObject->selectedText(), testStr.mid(selectionStart, selectionEnd - selectionStart));
        QCOMPARE(texteditObject->selectionStart(), selectionStart);
        QCOMPARE(texteditObject->selectionEnd(), selectionEnd);
    }
}

void tst_qdeclarativetextedit::moveCursorSelectionSequence_data()
{
    QTest::addColumn<QString>("testStr");
    QTest::addColumn<int>("cursorPosition");
    QTest::addColumn<int>("movePosition1");
    QTest::addColumn<int>("movePosition2");
    QTest::addColumn<int>("selection1Start");
    QTest::addColumn<int>("selection1End");
    QTest::addColumn<int>("selection2Start");
    QTest::addColumn<int>("selection2End");

    QTest::newRow("the {<quick( bro)wn> f^ox} jumped|ltr")
            << standard[0]
            << 9 << 13 << 17
            << 4 << 15
            << 4 << 19;
    QTest::newRow("the quick<( {bro)wn> f^ox} jumped|rtl")
            << standard[0]
            << 13 << 9 << 17
            << 9 << 15
            << 10 << 19;
    QTest::newRow("the {<quick( bro)wn> ^}fox jumped|ltr")
            << standard[0]
            << 9 << 13 << 16
            << 4 << 15
            << 4 << 16;
    QTest::newRow("the quick<( {bro)wn> ^}fox jumped|rtl")
            << standard[0]
            << 13 << 9 << 16
            << 9 << 15
            << 10 << 16;
    QTest::newRow("the {<quick( bro)wn^>} fox jumped|ltr")
            << standard[0]
            << 9 << 13 << 15
            << 4 << 15
            << 4 << 15;
    QTest::newRow("the quick<( {bro)wn^>} f^ox jumped|rtl")
            << standard[0]
            << 13 << 9 << 15
            << 9 << 15
            << 10 << 15;
    QTest::newRow("the {<quick() ^}bro)wn> fox|ltr")
            << standard[0]
            << 9 << 13 << 10
            << 4 << 15
            << 4 << 10;
    QTest::newRow("the quick<(^ {^bro)wn>} fox|rtl")
            << standard[0]
            << 13 << 9 << 10
            << 9 << 15
            << 10 << 15;
    QTest::newRow("the {<quick^}( bro)wn> fox|ltr")
            << standard[0]
            << 9 << 13 << 9
            << 4 << 15
            << 4 << 9;
    QTest::newRow("the quick{<(^ bro)wn>} fox|rtl")
            << standard[0]
            << 13 << 9 << 9
            << 9 << 15
            << 9 << 15;
    QTest::newRow("the {<qui^ck}( bro)wn> fox|ltr")
            << standard[0]
            << 9 << 13 << 7
            << 4 << 15
            << 4 << 9;
    QTest::newRow("the {<qui^ck}( bro)wn> fox|rtl")
            << standard[0]
            << 13 << 9 << 7
            << 9 << 15
            << 4 << 15;
    QTest::newRow("the {<^quick}( bro)wn> fox|ltr")
            << standard[0]
            << 9 << 13 << 4
            << 4 << 15
            << 4 << 9;
    QTest::newRow("the {<^quick}( bro)wn> fox|rtl")
            << standard[0]
            << 13 << 9 << 4
            << 9 << 15
            << 4 << 15;
    QTest::newRow("the{^ <quick}( bro)wn> fox|ltr")
            << standard[0]
            << 9 << 13 << 3
            << 4 << 15
            << 3 << 9;
    QTest::newRow("the{^ <quick}( bro)wn> fox|rtl")
            << standard[0]
            << 13 << 9 << 3
            << 9 << 15
            << 3 << 15;
    QTest::newRow("{t^he <quick}( bro)wn> fox|ltr")
            << standard[0]
            << 9 << 13 << 1
            << 4 << 15
            << 0 << 9;
    QTest::newRow("{t^he <quick}( bro)wn> fox|rtl")
            << standard[0]
            << 13 << 9 << 1
            << 9 << 15
            << 0 << 15;

    QTest::newRow("{<He(ll)o>, w^orld}!|ltr")
            << standard[2]
            << 2 << 4 << 8
            << 0 << 5
            << 0 << 12;
    QTest::newRow("{<He(ll)o>, w^orld}!|rtl")
            << standard[2]
            << 4 << 2 << 8
            << 0 << 5
            << 0 << 12;

    QTest::newRow("!{dlro^w ,<o(ll)eH>}|ltr")
            << standard[3]
            << 9 << 11 << 5
            << 8 << 13
            << 1 << 13;
    QTest::newRow("!{dlro^w ,<o(ll)eH>}|rtl")
            << standard[3]
            << 11 << 9 << 5
            << 8 << 13
            << 1 << 13;
}

void tst_qdeclarativetextedit::moveCursorSelectionSequence()
{
    QFETCH(QString, testStr);
    QFETCH(int, cursorPosition);
    QFETCH(int, movePosition1);
    QFETCH(int, movePosition2);
    QFETCH(int, selection1Start);
    QFETCH(int, selection1End);
    QFETCH(int, selection2Start);
    QFETCH(int, selection2End);

    QString componentStr = "import QtQuick 1.1\nTextEdit {  text: \""+ testStr +"\"; }";
    QDeclarativeComponent texteditComponent(&engine);
    texteditComponent.setData(componentStr.toLatin1(), QUrl());
    QDeclarativeTextEdit *texteditObject = qobject_cast<QDeclarativeTextEdit*>(texteditComponent.create());
    QVERIFY(texteditObject != 0);

    texteditObject->setCursorPosition(cursorPosition);

    texteditObject->moveCursorSelection(movePosition1, QDeclarativeTextEdit::SelectWords);
    QCOMPARE(texteditObject->selectedText(), testStr.mid(selection1Start, selection1End - selection1Start));
    QCOMPARE(texteditObject->selectionStart(), selection1Start);
    QCOMPARE(texteditObject->selectionEnd(), selection1End);

    texteditObject->moveCursorSelection(movePosition2, QDeclarativeTextEdit::SelectWords);
    QCOMPARE(texteditObject->selectedText(), testStr.mid(selection2Start, selection2End - selection2Start));
    QCOMPARE(texteditObject->selectionStart(), selection2Start);
    QCOMPARE(texteditObject->selectionEnd(), selection2End);
}


void tst_qdeclarativetextedit::mouseSelection_data()
{
    QTest::addColumn<QString>("qmlfile");
    QTest::addColumn<int>("from");
    QTest::addColumn<int>("to");
    QTest::addColumn<QString>("selectedText");

    // import installed
    QTest::newRow("on") << testFile("mouseselection_true.qml") << 4 << 9 << "45678";
    QTest::newRow("off") << testFile("mouseselection_false.qml") << 4 << 9 << QString();
    QTest::newRow("default") << testFile("mouseselection_default.qml") << 4 << 9 << QString();
    QTest::newRow("off word selection") << testFile("mouseselection_false_words.qml") << 4 << 9 << QString();
    QTest::newRow("on word selection (4,9)") << testFile("mouseselection_true_words.qml") << 4 << 9 << "0123456789";
    QTest::newRow("on word selection (2,13)") << testFile("mouseselection_true_words.qml") << 2 << 13 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QTest::newRow("on word selection (2,30)") << testFile("mouseselection_true_words.qml") << 2 << 30 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QTest::newRow("on word selection (9,13)") << testFile("mouseselection_true_words.qml") << 9 << 13 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QTest::newRow("on word selection (9,30)") << testFile("mouseselection_true_words.qml") << 9 << 30 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QTest::newRow("on word selection (13,2)") << testFile("mouseselection_true_words.qml") << 13 << 2 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QTest::newRow("on word selection (20,2)") << testFile("mouseselection_true_words.qml") << 20 << 2 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QTest::newRow("on word selection (12,9)") << testFile("mouseselection_true_words.qml") << 12 << 9 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QTest::newRow("on word selection (30,9)") << testFile("mouseselection_true_words.qml") << 30 << 9 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ";
}

void tst_qdeclarativetextedit::mouseSelection()
{
    QFETCH(QString, qmlfile);
    QFETCH(int, from);
    QFETCH(int, to);
    QFETCH(QString, selectedText);

    DeclarativeViewScopedPointer canvas(createView(qmlfile));

    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    QVERIFY(canvas->rootObject() != 0);
    QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit *>(canvas->rootObject());
    QVERIFY(textEditObject != 0);

    // press-and-drag-and-release from x1 to x2
    QPoint p1 = canvas->mapFromScene(textEditObject->positionToRectangle(from).center());
    QPoint p2 = canvas->mapFromScene(textEditObject->positionToRectangle(to).center());
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, 0, p1);
    //QTest::mouseMove(canvas->viewport(), canvas->mapFromScene(QPoint(x2,y))); // doesn't work
    QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(p2), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &mv);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, 0, p2);
    QCOMPARE(textEditObject->selectedText(), selectedText);

    // Clicking and shift to clicking between the same points should select the same text.
    textEditObject->setCursorPosition(0);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, p1);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::ShiftModifier, p2);
    QCOMPARE(textEditObject->selectedText(), selectedText);
}

void tst_qdeclarativetextedit::multilineMouseSelection()
{
    DeclarativeViewScopedPointer canvas(createView(testFile("mouseselection_multiline.qml")));
    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    QVERIFY(canvas->rootObject() != 0);
    QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit *>(canvas->rootObject());
    QVERIFY(textEditObject != 0);

    // press-and-drag from x1,y1 to x2,y1
    int x1 = 10;
    int x2 = textEditObject->width() - 10;
    int y1 = textEditObject->height() / 4;
    int y2 = textEditObject->height() * 3 / 4;
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x1,y1)));
    QMouseEvent mv1(QEvent::MouseMove, canvas->mapFromScene(QPoint(x2,y1)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &mv1);
    QString str1 = textEditObject->selectedText();
    QVERIFY(str1.length() > 3); // don't reallly care *what* was selected (and it's too sensitive to platform)

    // drag-and-release from x2,y1 to x2,y2
    QMouseEvent mv2(QEvent::MouseMove, canvas->mapFromScene(QPoint(x2,y2)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &mv2);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x2,y2)));
    QString str2 = textEditObject->selectedText();
    QVERIFY(str1 != str2);
    QVERIFY(str2.length() > 3);
}

void tst_qdeclarativetextedit::deferEnableSelectByMouse_data()
{
    QTest::addColumn<QString>("qmlfile");

    QTest::newRow("writable") << testFile("mouseselection_false.qml");
    QTest::newRow("read only") << testFile("mouseselection_false_readonly.qml");
}

void tst_qdeclarativetextedit::deferEnableSelectByMouse()
{
    // Verify text isn't selected if selectByMouse is enabled after the mouse button has been pressed.
    QFETCH(QString, qmlfile);

    DeclarativeViewScopedPointer canvas(createView(qmlfile));

    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    QVERIFY(canvas->rootObject() != 0);
    QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit *>(canvas->rootObject());
    QVERIFY(textEditObject != 0);

    // press-and-drag-and-release from x1 to x2
    int x1 = 10;
    int x2 = 70;
    int y = textEditObject->height()/2;

    QTest::mousePress(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x1,y)));
    textEditObject->setSelectByMouse(true);
    //QTest::mouseMove(canvas->viewport(), canvas->mapFromScene(QPoint(x2,y))); // doesn't work
    QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(QPoint(x2,y)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &mv);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x2,y)));
    QVERIFY(textEditObject->selectedText().isEmpty());
}

void tst_qdeclarativetextedit::deferDisableSelectByMouse_data()
{
    QTest::addColumn<QString>("qmlfile");

    QTest::newRow("writable") << testFile("mouseselection_true.qml");
    QTest::newRow("read only") << testFile("mouseselection_true_readonly.qml");
}

void tst_qdeclarativetextedit::deferDisableSelectByMouse()
{
    // Verify text isn't selected if selectByMouse is enabled after the mouse button has been pressed.
    QFETCH(QString, qmlfile);

    DeclarativeViewScopedPointer canvas(createView(qmlfile));

    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    QVERIFY(canvas->rootObject() != 0);
    QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit *>(canvas->rootObject());
    QVERIFY(textEditObject != 0);

    // press-and-drag-and-release from x1 to x2
    int x1 = 10;
    int x2 = 70;
    int y = textEditObject->height()/2;

    QTest::mousePress(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x1,y)));
    textEditObject->setSelectByMouse(false);
    //QTest::mouseMove(canvas->viewport(), canvas->mapFromScene(QPoint(x2,y))); // doesn't work
    QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(QPoint(x2,y)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &mv);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x2,y)));
    QVERIFY(textEditObject->selectedText().length() > 3);
}

void tst_qdeclarativetextedit::dragMouseSelection()
{
    QString qmlfile = testFile("mouseselection_true.qml");

    DeclarativeViewScopedPointer canvas(createView(qmlfile));

    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    QVERIFY(canvas->rootObject() != 0);
    QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit *>(canvas->rootObject());
    QVERIFY(textEditObject != 0);

    textEditObject->setAcceptDrops(true);

    // press-and-drag-and-release from x1 to x2
    int x1 = 10;
    int x2 = 70;
    int y = textEditObject->height()/2;
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x1,y)));
    {
        QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(QPoint(x2,y)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &mv);
    }
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x2,y)));
    QString str1 = textEditObject->selectedText();
    QVERIFY(str1.length() > 3);

    // press and drag the current selection.
    x1 = 40;
    x2 = 100;
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x1,y)));
    {
        QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(QPoint(x2,y)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &mv);
    }
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x2,y)));
    QString str2 = textEditObject->selectedText();
    QVERIFY(str2.length() > 3);

    QVERIFY(str1 != str2); // Verify the second press and drag is a new selection and doesn't not the first moved.
}

void tst_qdeclarativetextedit::mouseSelectionMode_data()
{
    QTest::addColumn<QString>("qmlfile");
    QTest::addColumn<bool>("selectWords");

    // import installed
    QTest::newRow("SelectWords") << testFile("mouseselectionmode_words.qml") << true;
    QTest::newRow("SelectCharacters") << testFile("mouseselectionmode_characters.qml") << false;
    QTest::newRow("default") << testFile("mouseselectionmode_default.qml") << false;
}

void tst_qdeclarativetextedit::mouseSelectionMode()
{
    QFETCH(QString, qmlfile);
    QFETCH(bool, selectWords);

    QString text = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    DeclarativeViewScopedPointer canvas(createView(qmlfile));

    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    QVERIFY(canvas->rootObject() != 0);
    QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit *>(canvas->rootObject());
    QVERIFY(textEditObject != 0);

    // press-and-drag-and-release from x1 to x2
    int x1 = 10;
    int x2 = 70;
    int y = textEditObject->height()/2;
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x1,y)));
    //QTest::mouseMove(canvas->viewport(), canvas->mapFromScene(QPoint(x2,y))); // doesn't work
    QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(QPoint(x2,y)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &mv);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(x2,y)));
    QString str = textEditObject->selectedText();
    if (selectWords) {
        QCOMPARE(str, text);
    } else {
        QVERIFY(str.length() > 3);
        QVERIFY(str != text);
    }

    // Clicking and shift to clicking between the same points should select the same text.
    textEditObject->setCursorPosition(0);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, canvas->mapFromScene(QPoint(x1,y)));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::ShiftModifier, canvas->mapFromScene(QPoint(x2,y)));
    QCOMPARE(textEditObject->selectedText(), str);
}

void tst_qdeclarativetextedit::inputMethodHints()
{
    DeclarativeViewScopedPointer canvas(createView(testFile("inputmethodhints.qml")));
    setFrameless(canvas);
    canvas->show();
    canvas->setFocus();

    QVERIFY(canvas->rootObject() != 0);
    QDeclarativeTextEdit *textEditObject = qobject_cast<QDeclarativeTextEdit *>(canvas->rootObject());
    QVERIFY(textEditObject != 0);
    QVERIFY(textEditObject->inputMethodHints() & Qt::ImhNoPredictiveText);
    textEditObject->setInputMethodHints(Qt::ImhUppercaseOnly);
    QVERIFY(textEditObject->inputMethodHints() & Qt::ImhUppercaseOnly);
}

static QByteArray msgFont(const QFont &f)
{
    QString s;
    QDebug(&s) << f;
    return s.toLocal8Bit();
}

void tst_qdeclarativetextedit::positionAt()
{
    DeclarativeViewScopedPointer canvas(createView(testFile("positionAt.qml")));
    QVERIFY(canvas->rootObject() != 0);
    setFrameless(canvas);
    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    canvas->setFocus();

    QDeclarativeTextEdit *texteditObject = qobject_cast<QDeclarativeTextEdit *>(canvas->rootObject());
    QVERIFY(texteditObject != 0);

    /*
    QFontMetrics fm(texteditObject->font(), canvas);
    const int y0 = fm.height() / 2;
    const int y1 = fm.height() * 3 / 2;

    int pos = texteditObject->positionAt(texteditObject->width()/2, y0);
    int diff = abs(int(fm.width(texteditObject->text().left(pos))-texteditObject->width()/2));

    // some tollerance for different fonts.
#ifdef Q_OS_LINUX
    QVERIFY(diff < 2);
#else
    QVERIFY(diff < 5);
#endif
    */
    const QFont font = texteditObject->font();
    const QString text = texteditObject->text();
    QTextLayout layout(text.left(text.indexOf('\n')), font, canvas);

    layout.beginLayout();
    QTextLine line = layout.createLine();
    QVERIFY(line.isValid());
    layout.endLayout();

    const int y0 = line.height() / 2;
    const int y1 = line.height() * 3 / 2;

    int pos = texteditObject->positionAt(texteditObject->width()/2, y0);

    int widthBegin = floor(line.cursorToX(pos - 1));
    int widthEnd = ceil(line.cursorToX(pos + 1));

    const int halfObjectWidth = texteditObject->width() / 2;
    QVERIFY2(widthBegin <= halfObjectWidth,
             (msgComparison(widthBegin, halfObjectWidth) + ' ' + msgFont(font)).constData());
    QVERIFY2(widthEnd >= halfObjectWidth,
             (msgComparison(widthEnd, halfObjectWidth) + ' ' + msgFont(font)).constData());

    const qreal x0 = texteditObject->positionToRectangle(pos).x();
    const qreal x1 = texteditObject->positionToRectangle(pos + 1).x();

    QString preeditText = texteditObject->text().mid(0, pos);
    texteditObject->setText(texteditObject->text().mid(pos));
    texteditObject->setCursorPosition(0);

    QInputMethodEvent inputEvent(preeditText, QList<QInputMethodEvent::Attribute>());
    QApplication::sendEvent(canvas, &inputEvent);

    // Check all points within the preedit text return the same position.
    QCOMPARE(texteditObject->positionAt(0, y0), 0);
    QCOMPARE(texteditObject->positionAt(x0 / 2, y0), 0);
    QCOMPARE(texteditObject->positionAt(x0, y0), 0);

    // Verify positioning returns to normal after the preedit text.
    QCOMPARE(texteditObject->positionAt(x1, y0), 1);
    QCOMPARE(texteditObject->positionToRectangle(1).x(), x1);

    QVERIFY(texteditObject->positionAt(x0 / 2, y1) > 0);
}

void tst_qdeclarativetextedit::cursorDelegate()
{
    QDeclarativeView* view = createView(testFile("cursorTest.qml"));
    view->show();
    view->setFocus();
    QDeclarativeTextEdit *textEditObject = view->rootObject()->findChild<QDeclarativeTextEdit*>("textEditObject");
    QVERIFY(textEditObject != 0);
    QVERIFY(textEditObject->findChild<QDeclarativeItem*>("cursorInstance"));
    //Test Delegate gets created
    textEditObject->setFocus(true);
    QDeclarativeItem* delegateObject = textEditObject->findChild<QDeclarativeItem*>("cursorInstance");
    QVERIFY(delegateObject);
    //Test Delegate gets moved
    for(int i=0; i<= textEditObject->text().length(); i++){
        textEditObject->setCursorPosition(i);
        QCOMPARE(textEditObject->cursorRectangle().x(), qRound(delegateObject->x()));
        QCOMPARE(textEditObject->cursorRectangle().y(), qRound(delegateObject->y()));
    }
    const QString preedit = "preedit";
    for (int i = 0; i <= preedit.length(); i++) {
        QInputMethodEvent event(preedit, QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, i, 1, QVariant()));
        QApplication::sendEvent(view, &event);

        QCOMPARE(textEditObject->cursorRectangle().x(), qRound(delegateObject->x()));
        QCOMPARE(textEditObject->cursorRectangle().y(), qRound(delegateObject->y()));
    }
    // Clear preedit text;
    QInputMethodEvent event;
    QApplication::sendEvent(view, &event);


    // Test delegate gets moved on mouse press.
    textEditObject->setSelectByMouse(true);
    textEditObject->setCursorPosition(0);
    const QPoint point1 = view->mapFromScene(textEditObject->positionToRectangle(5).center());
    QTest::mouseClick(view->viewport(), Qt::LeftButton, 0, point1);
    QVERIFY(textEditObject->cursorPosition() != 0);
    QCOMPARE(textEditObject->cursorRectangle().x(), qRound(delegateObject->x()));
    QCOMPARE(textEditObject->cursorRectangle().y(), qRound(delegateObject->y()));

    // Test delegate gets moved on mouse drag
    textEditObject->setCursorPosition(0);
    const QPoint point2 = view->mapFromScene(textEditObject->positionToRectangle(10).center());
    QTest::mousePress(view->viewport(), Qt::LeftButton, 0, point1);
    QMouseEvent mv(QEvent::MouseMove, point2, Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &mv);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, 0, point2);
    QCOMPARE(textEditObject->cursorRectangle().x(), qRound(delegateObject->x()));
    QCOMPARE(textEditObject->cursorRectangle().y(), qRound(delegateObject->y()));

    textEditObject->setReadOnly(true);
    textEditObject->setCursorPosition(0);
    QTest::mouseClick(view->viewport(), Qt::LeftButton, 0, view->mapFromScene(textEditObject->positionToRectangle(5).center()));
    QVERIFY(textEditObject->cursorPosition() != 0);
    QCOMPARE(textEditObject->cursorRectangle().x(), qRound(delegateObject->x()));
    QCOMPARE(textEditObject->cursorRectangle().y(), qRound(delegateObject->y()));

    textEditObject->setCursorPosition(0);
    QTest::mouseClick(view->viewport(), Qt::LeftButton, 0, view->mapFromScene(textEditObject->positionToRectangle(5).center()));
    QVERIFY(textEditObject->cursorPosition() != 0);
    QCOMPARE(textEditObject->cursorRectangle().x(), qRound(delegateObject->x()));
    QCOMPARE(textEditObject->cursorRectangle().y(), qRound(delegateObject->y()));

    textEditObject->setCursorPosition(0);
    QCOMPARE(textEditObject->cursorRectangle().x(), qRound(delegateObject->x()));
    QCOMPARE(textEditObject->cursorRectangle().y(), qRound(delegateObject->y()));
    QVERIFY(textEditObject->cursorRectangle().y() >= 0);
    QVERIFY(textEditObject->cursorRectangle().y() < textEditObject->cursorRectangle().height());
    textEditObject->setVAlign(QDeclarativeTextEdit::AlignVCenter);
    QCOMPARE(textEditObject->cursorRectangle().x(), qRound(delegateObject->x()));
    QCOMPARE(textEditObject->cursorRectangle().y(), qRound(delegateObject->y()));
    QVERIFY(textEditObject->cursorRectangle().y() > (textEditObject->height() / 2) - textEditObject->cursorRectangle().height());
    QVERIFY(textEditObject->cursorRectangle().y() < (textEditObject->height() / 2) + textEditObject->cursorRectangle().height());
    textEditObject->setVAlign(QDeclarativeTextEdit::AlignBottom);
    QCOMPARE(textEditObject->cursorRectangle().x(), qRound(delegateObject->x()));
    QCOMPARE(textEditObject->cursorRectangle().y(), qRound(delegateObject->y()));
    QVERIFY(textEditObject->cursorRectangle().y() > textEditObject->height() - (textEditObject->cursorRectangle().height() * 2));
    QVERIFY(textEditObject->cursorRectangle().y() < textEditObject->height());

    //Test Delegate gets deleted
    textEditObject->setCursorDelegate(0);
    QVERIFY(!textEditObject->findChild<QDeclarativeItem*>("cursorInstance"));

    delete view;
}

void tst_qdeclarativetextedit::cursorVisible()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.show();
    QApplication::setActiveWindow(&view);
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(&view));
    view.setFocus();

    QDeclarativeTextEdit edit;
    QSignalSpy spy(&edit, SIGNAL(cursorVisibleChanged(bool)));

    QCOMPARE(edit.isCursorVisible(), false);

    edit.setCursorVisible(true);
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 1);

    edit.setCursorVisible(false);
    QCOMPARE(edit.isCursorVisible(), false);
    QCOMPARE(spy.count(), 2);

    edit.setFocus(true);
    QCOMPARE(edit.isCursorVisible(), false);
    QCOMPARE(spy.count(), 2);

    scene.addItem(&edit);
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 3);

    edit.setFocus(false);
    QCOMPARE(edit.isCursorVisible(), false);
    QCOMPARE(spy.count(), 4);

    edit.setFocus(true);
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 5);

    scene.clearFocus();
    QCOMPARE(edit.isCursorVisible(), false);
    QCOMPARE(spy.count(), 6);

    scene.setFocus();
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 7);

    view.clearFocus();
    QCOMPARE(edit.isCursorVisible(), false);
    QCOMPARE(spy.count(), 8);

    view.setFocus();
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 9);

    // on mac, setActiveWindow(0) on mac does not deactivate the current application
    // (you have to switch to a different app or hide the current app to trigger this)
#if !defined(Q_WS_MAC)
    QApplication::setActiveWindow(0);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget *>(0));
    QCOMPARE(edit.isCursorVisible(), false);
    QCOMPARE(spy.count(), 10);

    QApplication::setActiveWindow(&view);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget *>(&view));
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 11);
#endif
}

void tst_qdeclarativetextedit::delegateLoading_data()
{
    QTest::addColumn<QString>("qmlfile");
    QTest::addColumn<QString>("error");

    // import installed
    QTest::newRow("pass") << "cursorHttpTestPass.qml" << "";
    QTest::newRow("fail1") << "cursorHttpTestFail1.qml" << "http://localhost:42332/FailItem.qml: Remote host closed the connection";
    QTest::newRow("fail2") << "cursorHttpTestFail2.qml" << "http://localhost:42332/ErrItem.qml:4:5: Fungus is not a type";
}

void tst_qdeclarativetextedit::delegateLoading()
{
    QFETCH(QString, qmlfile);
    QFETCH(QString, error);

    TestHTTPServer server(42332);
    server.serveDirectory(testFile("httpfail"), TestHTTPServer::Disconnect);
    server.serveDirectory(testFile("httpslow"), TestHTTPServer::Delay);
    server.serveDirectory(testFile("http"));

    QScopedPointer<QDeclarativeView> view(new QDeclarativeView(0));

    view->setSource(QUrl(QLatin1String("http://localhost:42332/") + qmlfile));
    view->show();
    view->setFocus();

    if (!error.isEmpty()) {
        QTest::ignoreMessage(QtWarningMsg, error.toUtf8());
        QTRY_VERIFY(view->status()==QDeclarativeView::Error);
        QTRY_VERIFY(!view->rootObject()); // there is fail item inside this test
    } else {
        QTRY_VERIFY_WITH_TIMEOUT(view->rootObject(), 7000);//Wait for loading to finish.
        QDeclarativeTextEdit *textEditObject = view->rootObject()->findChild<QDeclarativeTextEdit*>("textEditObject");
        //    view->rootObject()->dumpObjectTree();
        QVERIFY(textEditObject != 0);
        textEditObject->setFocus(true);
        QDeclarativeItem *delegate;
        delegate = view->rootObject()->findChild<QDeclarativeItem*>("delegateOkay");
        QVERIFY(delegate);
        delegate = view->rootObject()->findChild<QDeclarativeItem*>("delegateSlow");
        QVERIFY(delegate);

        delete delegate;
    }


    //A test should be added here with a component which is ready but component.create() returns null
    //Not sure how to accomplish this with QDeclarativeTextEdits cursor delegate
    //###This was only needed for code coverage, and could be a case of overzealous defensive programming
    //delegate = view->rootObject()->findChild<QDeclarativeItem*>("delegateErrorB");
    //QVERIFY(!delegate);
}

/*
TextEdit element should only handle left/right keys until the cursor reaches
the extent of the text, then they should ignore the keys.
*/
void tst_qdeclarativetextedit::navigation()
{
    DeclarativeViewScopedPointer canvas(createView(testFile("navigation.qml")));
    canvas->show();
    canvas->setFocus();

    QVERIFY(canvas->rootObject() != 0);

    QDeclarativeTextEdit *input = qobject_cast<QDeclarativeTextEdit *>(qvariant_cast<QObject *>(canvas->rootObject()->property("myInput")));

    QVERIFY(input != 0);
    QTRY_VERIFY(input->hasActiveFocus() == true);
    simulateKey(canvas, Qt::Key_Left);
    QVERIFY(input->hasActiveFocus() == false);
    simulateKey(canvas, Qt::Key_Right);
    QVERIFY(input->hasActiveFocus() == true);
    simulateKey(canvas, Qt::Key_Right);
    QVERIFY(input->hasActiveFocus() == true);
    simulateKey(canvas, Qt::Key_Right);
    QVERIFY(input->hasActiveFocus() == false);
    simulateKey(canvas, Qt::Key_Left);
    QVERIFY(input->hasActiveFocus() == true);

    // Test left and right navigation works if the TextEdit is empty (QTBUG-25447).
    input->setText(QString());
    QCOMPARE(input->cursorPosition(), 0);
    simulateKey(canvas, Qt::Key_Right);
    QCOMPARE(input->hasActiveFocus(), false);
    simulateKey(canvas, Qt::Key_Left);
    QCOMPARE(input->hasActiveFocus(), true);
    simulateKey(canvas, Qt::Key_Left);
    QCOMPARE(input->hasActiveFocus(), false);
}

void tst_qdeclarativetextedit::copyAndPaste() {
#ifndef QT_NO_CLIPBOARD

#ifdef Q_WS_MAC
    {
        PasteboardRef pasteboard;
        OSStatus status = PasteboardCreate(0, &pasteboard);
        if (status == noErr)
            CFRelease(pasteboard);
        else
            QSKIP("This machine doesn't support the clipboard");
    }
#endif

    QString componentStr = "import QtQuick 1.0\nTextEdit { text: \"Hello world!\" }";
    QDeclarativeComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QDeclarativeTextEdit *textEdit = qobject_cast<QDeclarativeTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    // copy and paste
    QCOMPARE(textEdit->text().length(), 12);
    textEdit->select(0, textEdit->text().length());;
    textEdit->copy();
    QCOMPARE(textEdit->selectedText(), QString("Hello world!"));
    QCOMPARE(textEdit->selectedText().length(), 12);
    textEdit->setCursorPosition(0);
    QVERIFY(textEdit->canPaste());
    textEdit->paste();
    QCOMPARE(textEdit->text(), QString("Hello world!Hello world!"));
    QCOMPARE(textEdit->text().length(), 24);

    // canPaste
    QVERIFY(textEdit->canPaste());
    textEdit->setReadOnly(true);
    QVERIFY(!textEdit->canPaste());
    textEdit->setReadOnly(false);
    QVERIFY(textEdit->canPaste());

    // QTBUG-12339
    // test that document and internal text attribute are in sync
    QDeclarativeItemPrivate* pri = QDeclarativeItemPrivate::get(textEdit);
    QDeclarativeTextEditPrivate *editPrivate = static_cast<QDeclarativeTextEditPrivate*>(pri);
    QCOMPARE(textEdit->text(), editPrivate->text);

    // select word
    textEdit->setCursorPosition(0);
    textEdit->selectWord();
    QCOMPARE(textEdit->selectedText(), QString("Hello"));

    // select all and cut
    textEdit->selectAll();
    textEdit->cut();
    QCOMPARE(textEdit->text().length(), 0);
    textEdit->paste();
    QCOMPARE(textEdit->text(), QString("Hello world!Hello world!"));
    QCOMPARE(textEdit->text().length(), 24);
#endif
}

void tst_qdeclarativetextedit::canPaste() {
#ifndef QT_NO_CLIPBOARD

    QApplication::clipboard()->setText("Some text");

    QString componentStr = "import QtQuick 1.0\nTextEdit { text: \"Hello world!\" }";
    QDeclarativeComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QDeclarativeTextEdit *textEdit = qobject_cast<QDeclarativeTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    // check initial value - QTBUG-17765
    QWidgetTextControl tc;
    QCOMPARE(textEdit->canPaste(), tc.canPaste());

#endif
}

void tst_qdeclarativetextedit::canPasteEmpty() {
#ifndef QT_NO_CLIPBOARD

    QApplication::clipboard()->clear();

    QString componentStr = "import QtQuick 1.0\nTextEdit { text: \"Hello world!\" }";
    QDeclarativeComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QDeclarativeTextEdit *textEdit = qobject_cast<QDeclarativeTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    // check initial value - QTBUG-17765
    QWidgetTextControl tc;
    QCOMPARE(textEdit->canPaste(), tc.canPaste());

#endif
}

void tst_qdeclarativetextedit::readOnly()
{
    DeclarativeViewScopedPointer canvas(createView(testFile("readOnly.qml")));
    canvas->show();
    canvas->setFocus();

    QVERIFY(canvas->rootObject() != 0);

    QDeclarativeTextEdit *edit = qobject_cast<QDeclarativeTextEdit *>(qvariant_cast<QObject *>(canvas->rootObject()->property("myInput")));

    QVERIFY(edit != 0);
    QTRY_VERIFY(edit->hasActiveFocus() == true);
    QVERIFY(edit->isReadOnly() == true);
    QString initial = edit->text();
    for(int k=Qt::Key_0; k<=Qt::Key_Z; k++)
        simulateKey(canvas, k);
    simulateKey(canvas, Qt::Key_Return);
    simulateKey(canvas, Qt::Key_Space);
    simulateKey(canvas, Qt::Key_Escape);
    QCOMPARE(edit->text(), initial);
}

void tst_qdeclarativetextedit::simulateKey(QDeclarativeView *view, int key, Qt::KeyboardModifiers modifiers)
{
    QKeyEvent press(QKeyEvent::KeyPress, key, modifiers);
    QKeyEvent release(QKeyEvent::KeyRelease, key, modifiers);

    QApplication::sendEvent(view, &press);
    QApplication::sendEvent(view, &release);
}

QDeclarativeView *tst_qdeclarativetextedit::createView(const QString &filename)
{
    QDeclarativeView *canvas = new QDeclarativeView(0);

    canvas->setSource(QUrl::fromLocalFile(filename));
    return canvas;
}

void tst_qdeclarativetextedit::textInput()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    QDeclarativeTextEdit edit;
    QDeclarativeItemPrivate* pri = QDeclarativeItemPrivate::get(&edit);
    QDeclarativeTextEditPrivate *editPrivate = static_cast<QDeclarativeTextEditPrivate*>(pri);
    edit.setPos(0, 0);
    scene.addItem(&edit);
    view.show();
    QApplication::setActiveWindow(&view);
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(&view));
    edit.setFocus(true);
    QVERIFY(edit.hasActiveFocus() == true);

    // test that input method event is committed
    QInputMethodEvent event;
    event.setCommitString( "Hello world!", 0, 0);
    QApplication::sendEvent(&view, &event);
    QCOMPARE(edit.text(), QString("Hello world!"));

    // QTBUG-12339
    // test that document and internal text attribute are in sync
    QCOMPARE(editPrivate->text, QString("Hello world!"));
}

void tst_qdeclarativetextedit::openInputPanelOnClick()
{
    PlatformInputContext ic;

    QGraphicsScene scene;
    QGraphicsView view(&scene);
    QDeclarativeTextEdit edit;
    QSignalSpy focusOnPressSpy(&edit, SIGNAL(activeFocusOnPressChanged(bool)));
    edit.setText("Hello world");
    edit.setPos(0, 0);
    scene.addItem(&edit);
    view.show();
    qApp->setAutoSipEnabled(true);
    QApplication::setActiveWindow(&view);
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(&view));

    QDeclarativeItemPrivate* pri = QDeclarativeItemPrivate::get(&edit);
    QDeclarativeTextEditPrivate *editPrivate = static_cast<QDeclarativeTextEditPrivate*>(pri);

    // input panel on click
    editPrivate->showInputPanelOnFocus = false;

    QStyle::RequestSoftwareInputPanel behavior = QStyle::RequestSoftwareInputPanel(
            view.style()->styleHint(QStyle::SH_RequestSoftwareInputPanel));
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, view.mapFromScene(edit.scenePos()));
    QApplication::processEvents();
    if (behavior == QStyle::RSIP_OnMouseClickAndAlreadyFocused) {
        QCOMPARE(ic.isInputPanelVisible(), false);
        QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, view.mapFromScene(edit.scenePos()));
        QApplication::processEvents();
        QCOMPARE(ic.isInputPanelVisible(), true);
    } else if (behavior == QStyle::RSIP_OnMouseClick) {
        QCOMPARE(ic.isInputPanelVisible(), true);
    }
    ic.clear();

    // focus should not cause input panels to open or close
    edit.setFocus(false);
    edit.setFocus(true);
    edit.setFocus(false);
    edit.setFocus(true);
    edit.setFocus(false);
    QApplication::processEvents();
    QCOMPARE(ic.m_showInputPanelCallCount, 0);
    QCOMPARE(ic.m_hideInputPanelCallCount, 0);
}

void tst_qdeclarativetextedit::openInputPanelOnFocus()
{
    PlatformInputContext ic;

    QGraphicsScene scene;
    QGraphicsView view(&scene);
    QDeclarativeTextEdit edit;
    QSignalSpy focusOnPressSpy(&edit, SIGNAL(activeFocusOnPressChanged(bool)));
    edit.setText("Hello world");
    edit.setPos(0, 0);
    scene.addItem(&edit);
    view.show();
    qApp->setAutoSipEnabled(true);
    QApplication::setActiveWindow(&view);
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(&view));

    QDeclarativeItemPrivate* pri = QDeclarativeItemPrivate::get(&edit);
    QDeclarativeTextEditPrivate *editPrivate = static_cast<QDeclarativeTextEditPrivate*>(pri);
    editPrivate->showInputPanelOnFocus = true;

    // test default values
    QVERIFY(edit.focusOnPress());
    QCOMPARE(ic.m_showInputPanelCallCount, 0);
    QCOMPARE(ic.m_hideInputPanelCallCount, 0);

    // focus on press, input panel on focus
    QTest::mousePress(view.viewport(), Qt::LeftButton, 0, view.mapFromScene(edit.scenePos()));
    QApplication::processEvents();
    QVERIFY(edit.hasActiveFocus());
    QCOMPARE(ic.isInputPanelVisible(), true);
    ic.clear();

    // no events on release
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, 0, view.mapFromScene(edit.scenePos()));
    QCOMPARE(ic.isInputPanelVisible(), false);
    ic.clear();

    // if already focused, input panel can be opened on press
    QVERIFY(edit.hasActiveFocus());
    QTest::mousePress(view.viewport(), Qt::LeftButton, 0, view.mapFromScene(edit.scenePos()));
    QApplication::processEvents();
    QCOMPARE(ic.isInputPanelVisible(), true);
    ic.clear();

    // input method should stay enabled if focus
    // is lost to an item that also accepts inputs
    QDeclarativeTextEdit anotherEdit;
    scene.addItem(&anotherEdit);
    anotherEdit.setFocus(true);
    QApplication::processEvents();
    QCOMPARE(ic.isInputPanelVisible(), true);
    ic.clear();
    QVERIFY(view.testAttribute(Qt::WA_InputMethodEnabled));

    // input method should be disabled if focus
    // is lost to an item that doesn't accept inputs
    QDeclarativeItem item;
    scene.addItem(&item);
    item.setFocus(true);
    QApplication::processEvents();
    QCOMPARE(ic.isInputPanelVisible(), false);
    QVERIFY(!view.testAttribute(Qt::WA_InputMethodEnabled));

    // no automatic input panel events should
    // be sent if activeFocusOnPress is false
    edit.setFocusOnPress(false);
    QCOMPARE(focusOnPressSpy.count(),1);
    edit.setFocusOnPress(false);
    QCOMPARE(focusOnPressSpy.count(),1);
    edit.setFocus(false);
    edit.setFocus(true);
    QTest::mousePress(view.viewport(), Qt::LeftButton, 0, view.mapFromScene(edit.scenePos()));
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, 0, view.mapFromScene(edit.scenePos()));
    QApplication::processEvents();
    QCOMPARE(ic.m_showInputPanelCallCount, 0);
    QCOMPARE(ic.m_hideInputPanelCallCount, 0);

    // one show input panel event should
    // be set when openSoftwareInputPanel is called
    edit.openSoftwareInputPanel();
    QCOMPARE(ic.isInputPanelVisible(), true);
    QCOMPARE(ic.m_hideInputPanelCallCount, 0);
    ic.clear();

    // one close input panel event should
    // be sent when closeSoftwareInputPanel is called
    edit.closeSoftwareInputPanel();
    QCOMPARE(ic.m_showInputPanelCallCount, 0);
    QVERIFY(ic.m_hideInputPanelCallCount > 0);
    ic.clear();

    // set activeFocusOnPress back to true
    edit.setFocusOnPress(true);
    QCOMPARE(focusOnPressSpy.count(),2);
    edit.setFocusOnPress(true);
    QCOMPARE(focusOnPressSpy.count(),2);
    edit.setFocus(false);
    QApplication::processEvents();
    QCOMPARE(ic.m_showInputPanelCallCount, 0);
    QCOMPARE(ic.m_hideInputPanelCallCount, 0);
    ic.clear();

    // input panel should not re-open
    // if focus has already been set
    edit.setFocus(true);
    QCOMPARE(ic.isInputPanelVisible(), true);
    ic.clear();
    edit.setFocus(true);
    QCOMPARE(ic.isInputPanelVisible(), false);

    // input method should be disabled
    // if TextEdit loses focus
    edit.setFocus(false);
    QApplication::processEvents();
    QVERIFY(!view.testAttribute(Qt::WA_InputMethodEnabled));

    // input method should not be enabled
    // if TextEdit is read only.
    edit.setReadOnly(true);
    ic.clear();
    edit.setFocus(true);
    QApplication::processEvents();
    QCOMPARE(ic.isInputPanelVisible(), false);
    QVERIFY(!view.testAttribute(Qt::WA_InputMethodEnabled));
}

void tst_qdeclarativetextedit::geometrySignals()
{
    QDeclarativeComponent component(&engine, testFile("geometrySignals.qml"));
    QObject *o = component.create();
    QVERIFY(o);
    QCOMPARE(o->property("bindingWidth").toInt(), 400);
    QCOMPARE(o->property("bindingHeight").toInt(), 500);
    delete o;
}

void tst_qdeclarativetextedit::pastingRichText_QTBUG_14003()
{
#ifndef QT_NO_CLIPBOARD
    QString componentStr = "import QtQuick 1.0\nTextEdit { textFormat: TextEdit.PlainText }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeTextEdit *obj = qobject_cast<QDeclarativeTextEdit*>(component.create());

    QTRY_VERIFY(obj != 0);
    QTRY_VERIFY(obj->textFormat() == QDeclarativeTextEdit::PlainText);

    QMimeData *mData = new QMimeData;
    mData->setHtml("<font color=\"red\">Hello</font>");
    QApplication::clipboard()->setMimeData(mData);

    obj->paste();
    QTRY_VERIFY(obj->text() == "");
    QTRY_VERIFY(obj->textFormat() == QDeclarativeTextEdit::PlainText);
#endif
}

void tst_qdeclarativetextedit::implicitSize_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("wrap");
    QTest::newRow("plain") << "The quick red fox jumped over the lazy brown dog" << "TextEdit.NoWrap";
    QTest::newRow("richtext") << "<b>The quick red fox jumped over the lazy brown dog</b>" << "TextEdit.NoWrap";
    QTest::newRow("plain_wrap") << "The quick red fox jumped over the lazy brown dog" << "TextEdit.Wrap";
    QTest::newRow("richtext_wrap") << "<b>The quick red fox jumped over the lazy brown dog</b>" << "TextEdit.Wrap";
}

void tst_qdeclarativetextedit::implicitSize()
{
    QFETCH(QString, text);
    QFETCH(QString, wrap);
    QString componentStr = "import QtQuick 1.1\nTextEdit { text: \"" + text + "\"; width: 50; wrapMode: " + wrap + " }";
    QDeclarativeComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeTextEdit *textObject = qobject_cast<QDeclarativeTextEdit*>(textComponent.create());

    QVERIFY(textObject->width() < textObject->implicitWidth());
    QVERIFY(textObject->height() == textObject->implicitHeight());

    textObject->resetWidth();
    QVERIFY(textObject->width() == textObject->implicitWidth());
    QVERIFY(textObject->height() == textObject->implicitHeight());
}

void tst_qdeclarativetextedit::implicitSizePreedit_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("wrap");
    QTest::addColumn<bool>("wrapped");
    QTest::newRow("plain") << "The quick red fox jumped over the lazy brown dog" << "TextEdit.NoWrap" << false;
    QTest::newRow("plain_wrap") << "The quick red fox jumped over the lazy brown dog" << "TextEdit.Wrap" << true;

}

void tst_qdeclarativetextedit::implicitSizePreedit()
{
    QFETCH(QString, text);
    QFETCH(QString, wrap);
    QFETCH(bool, wrapped);

    QString componentStr = "import QtQuick 1.1\nTextEdit { focus: true; width: 50; wrapMode: " + wrap + " }";
    QDeclarativeComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeTextEdit *textObject = qobject_cast<QDeclarativeTextEdit*>(textComponent.create());

    QGraphicsScene scene;
    QGraphicsView view(&scene);
    scene.addItem(textObject);
    view.show();
    QApplication::setActiveWindow(&view);
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(&view));

    QInputMethodEvent event(text, QList<QInputMethodEvent::Attribute>());
    QCoreApplication::sendEvent(&view, &event);

    QVERIFY2(textObject->width() < textObject->implicitWidth(),
             msgComparison(textObject->width(), textObject->implicitWidth()).constData());
    QVERIFY2(textObject->height() == textObject->implicitHeight(),
             msgComparison(textObject->height(), textObject->implicitHeight()).constData());
    qreal wrappedHeight = textObject->height();

    textObject->resetWidth();
    QVERIFY(textObject->width() == textObject->implicitWidth());
    QVERIFY(textObject->height() == textObject->implicitHeight());
    QCOMPARE(textObject->height() < wrappedHeight, wrapped);
}

void tst_qdeclarativetextedit::testQtQuick11Attributes()
{
    QFETCH(QString, code);
    QFETCH(QString, warning);
    QFETCH(QString, error);

    QDeclarativeEngine engine;
    QObject *obj;

    QDeclarativeComponent valid(&engine);
    valid.setData("import QtQuick 1.1; TextEdit { " + code.toUtf8() + " }", QUrl(""));
    obj = valid.create();
    QVERIFY(obj);
    QVERIFY(valid.errorString().isEmpty());
    delete obj;

    QDeclarativeComponent invalid(&engine);
    invalid.setData("import QtQuick 1.0; TextEdit { " + code.toUtf8() + " }", QUrl(""));
    QTest::ignoreMessage(QtWarningMsg, warning.toUtf8());
    obj = invalid.create();
    QCOMPARE(invalid.errorString(), error);
    delete obj;
}

void tst_qdeclarativetextedit::testQtQuick11Attributes_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("warning");
    QTest::addColumn<QString>("error");

    QTest::newRow("canPaste") << "property bool foo: canPaste"
        << "<Unknown File>:1: ReferenceError: Can't find variable: canPaste"
        << "";

    QTest::newRow("lineCount") << "property int foo: lineCount"
        << "<Unknown File>:1: ReferenceError: Can't find variable: lineCount"
        << "";

    QTest::newRow("moveCursorSelection") << "Component.onCompleted: moveCursorSelection(0, TextEdit.SelectCharacters)"
        << "<Unknown File>:1: ReferenceError: Can't find variable: moveCursorSelection"
        << "";

    QTest::newRow("deselect") << "Component.onCompleted: deselect()"
        << "<Unknown File>:1: ReferenceError: Can't find variable: deselect"
        << "";

    QTest::newRow("onLinkActivated") << "onLinkActivated: {}"
        << "QDeclarativeComponent: Component is not ready"
        << ":1 \"TextEdit.onLinkActivated\" is not available in QtQuick 1.0.\n";
}

void tst_qdeclarativetextedit::preeditMicroFocus()
{
    PlatformInputContext ic;

    QGraphicsScene scene;
    QGraphicsView view(&scene);
    QDeclarativeTextEdit edit;
    edit.setFocus(true);
    scene.addItem(&edit);
    view.show();
    QApplication::setActiveWindow(&view);
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(&view));

    QSignalSpy cursorRectangleSpy(&edit, SIGNAL(cursorRectangleChanged()));

    QRect currentRect;
    QRect previousRect = edit.inputMethodQuery(Qt::ImMicroFocus).toRect();

    QString preeditText = "super";

    // Verify that the micro focus rect is positioned the same for position 0 as
    // it would be if there was no preedit text.
    ic.clear();
    sendPreeditText(preeditText, 0);
    currentRect = edit.inputMethodQuery(Qt::ImMicroFocus).toRect();
    QCOMPARE(currentRect, previousRect);
#if defined(Q_WS_X11)
    QCOMPARE(ic.updateCallCount, 0); // The cursor position hasn't changed.
#endif
    QCOMPARE(cursorRectangleSpy.count(), 0);

    // Verify that the micro focus rect moves to the left as the cursor position
    // is incremented.
    for (int i = 1; i <= 5; ++i) {
        ic.clear();
        sendPreeditText(preeditText, i);
        currentRect = edit.inputMethodQuery(Qt::ImMicroFocus).toRect();
        QVERIFY2(previousRect.left() < currentRect.left(),
                 msgComparison(previousRect.left(), currentRect.left()).constData());
#if defined(Q_WS_X11)
        QVERIFY(ic.updateCallCount > 0);
#endif
        QVERIFY(cursorRectangleSpy.count() > 0);
        cursorRectangleSpy.clear();
        previousRect = currentRect;
    }

    // Verify that if there is no preedit cursor then the micro focus rect is the
    // same as it would be if it were positioned at the end of the preedit text.
    sendPreeditText(preeditText, 0);
    ic.clear();
    QInputMethodEvent imEvent(preeditText, QList<QInputMethodEvent::Attribute>());
    if (qApp->focusObject())
        QApplication::sendEvent(qApp->focusObject(), &imEvent);
    currentRect = edit.inputMethodQuery(Qt::ImMicroFocus).toRect();
    QCOMPARE(currentRect, previousRect);
#if defined(Q_WS_X11)
    QVERIFY(ic.updateCallCount > 0);
#endif
    QVERIFY(cursorRectangleSpy.count() > 0);
}

void tst_qdeclarativetextedit::inputContextMouseHandler()
{
    PlatformInputContext ic;

    QString text = "supercalifragisiticexpialidocious!";

    QGraphicsScene scene;
    QGraphicsView view(&scene);
    QDeclarativeTextEdit edit;
    edit.setPos(0, 0);
    edit.setWidth(200);
    edit.setText(text.mid(0, 12));
    edit.setCursorPosition(0);
    edit.setFocus(true);
    scene.addItem(&edit);
    view.show();
    QApplication::setActiveWindow(&view);
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(&view));
    view.setFocus();

    QFontMetricsF fm(edit.font());
    const qreal y = fm.height() / 2;

    QPoint position2 = view.mapFromScene(edit.mapToScene(QPointF(fm.width(text.mid(0, 2)), y)));

    QInputMethodEvent inputEvent(text.mid(0, 12), QList<QInputMethodEvent::Attribute>());
    QApplication::sendEvent(&view, &inputEvent);

    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, position2);
    QTest::mouseRelease(view.viewport(), Qt::RightButton, Qt::ControlModifier, position2);
    QApplication::processEvents();

    QCOMPARE(ic.m_action, QInputMethod::Click);
    QCOMPARE(ic.m_invokeActionCallCount, 1);
    QCOMPARE(ic.m_cursorPosition, 2);
}

void tst_qdeclarativetextedit::inputMethodComposing()
{
    QString text = "supercalifragisiticexpialidocious!";

    QGraphicsScene scene;
    QGraphicsView view(&scene);
    QDeclarativeTextEdit edit;
    edit.setWidth(200);
    edit.setText(text.mid(0, 12));
    edit.setCursorPosition(12);
    edit.setPos(0, 0);
    edit.setFocus(true);
    scene.addItem(&edit);
    view.show();
    QApplication::setActiveWindow(&view);
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(&view));

    QSignalSpy spy(&edit, SIGNAL(inputMethodComposingChanged()));

    QCOMPARE(edit.isInputMethodComposing(), false);

    {
        QInputMethodEvent imEvent(text.mid(3), QList<QInputMethodEvent::Attribute>());
        QApplication::sendEvent(&view, &imEvent);
    }
    QCOMPARE(edit.isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 1);

    {
        QInputMethodEvent imEvent(text.mid(12), QList<QInputMethodEvent::Attribute>());
        QApplication::sendEvent(&view, &imEvent);
    }
    QCOMPARE(edit.isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 1);

    {
        QInputMethodEvent imEvent;
        QApplication::sendEvent(&view, &imEvent);
    }
    QCOMPARE(edit.isInputMethodComposing(), false);
    QCOMPARE(spy.count(), 2);
}

void tst_qdeclarativetextedit::cursorRectangleSize()
{
    DeclarativeViewScopedPointer canvas(createView(testFile("CursorRect.qml")));
    setFrameless(canvas);
    QVERIFY(canvas->rootObject() != 0);
    canvas->show();
    canvas->setFocus();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));

    QDeclarativeTextEdit *textEdit = qobject_cast<QDeclarativeTextEdit *>(canvas->rootObject());
    QVERIFY(textEdit != 0);
    textEdit->setFocus(true);
    QRectF cursorRect = textEdit->positionToRectangle(textEdit->cursorPosition());
    QRectF microFocusFromScene = canvas->scene()->inputMethodQuery(Qt::ImMicroFocus).toRectF();
    QRectF microFocusFromApp= QApplication::focusWidget()->inputMethodQuery(Qt::ImMicroFocus).toRectF();

    QCOMPARE(microFocusFromScene.size(), cursorRect.size());
    QCOMPARE(microFocusFromApp.size(), cursorRect.size());
}

void tst_qdeclarativetextedit::deselect()
{
    DeclarativeViewScopedPointer canvas(createView(testFile("CursorRect.qml")));
    setFrameless(canvas);
    QVERIFY(canvas->rootObject() != 0);
    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    canvas->setFocus();

    QDeclarativeTextEdit *textEdit = qobject_cast<QDeclarativeTextEdit *>(canvas->rootObject());
    QVERIFY(textEdit != 0);

    textEdit->setText("Select");

    QSignalSpy selectionStartSpy(textEdit, SIGNAL(selectionStartChanged()));
    QSignalSpy selectionEndSpy(textEdit, SIGNAL(selectionEndChanged()));
    QSignalSpy selectionSpy(textEdit, SIGNAL(selectionChanged()));

    textEdit->select(5, 6);

    QCOMPARE(selectionStartSpy.count(), 1);
    QCOMPARE(selectionEndSpy.count(), 1);
    QCOMPARE(selectionSpy.count(), 1);
    QCOMPARE(textEdit->selectionStart(), 5);
    QCOMPARE(textEdit->selectionEnd(), 6);
    QCOMPARE(textEdit->selectedText(), QLatin1String("t"));
    QCOMPARE(textEdit->cursorPosition(), 6);

    textEdit->deselect();

    QCOMPARE(selectionStartSpy.count(), 2);
    QCOMPARE(selectionEndSpy.count(), 1);
    QCOMPARE(selectionSpy.count(), 2);
    QCOMPARE(textEdit->selectionStart(), textEdit->cursorPosition());
    QCOMPARE(textEdit->selectionEnd(), textEdit->cursorPosition());
    QCOMPARE(textEdit->selectedText(), QLatin1String(""));
    QCOMPARE(textEdit->cursorPosition(), 6);

    textEdit->select(5, 6);

    QCOMPARE(selectionStartSpy.count(), 3);
    QCOMPARE(selectionEndSpy.count(), 1);
    QCOMPARE(selectionSpy.count(), 3);
    QCOMPARE(textEdit->selectionStart(), 5);
    QCOMPARE(textEdit->selectionEnd(), 6);
    QCOMPARE(textEdit->selectedText(), QLatin1String("t"));
    QCOMPARE(textEdit->cursorPosition(), 6);

    QKeyEvent leftArrowPress(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
    QKeyEvent leftArrowRelese(QEvent::KeyRelease, Qt::Key_Left, Qt::NoModifier);
    QApplication::sendEvent(canvas, &leftArrowPress);
    QApplication::sendEvent(canvas, &leftArrowRelese);

    QCOMPARE(selectionStartSpy.count(), 3);
    QCOMPARE(selectionEndSpy.count(), 2);
    QCOMPARE(selectionSpy.count(), 4);
    QCOMPARE(textEdit->selectionStart(), textEdit->cursorPosition());
    QCOMPARE(textEdit->selectionEnd(), textEdit->cursorPosition());
    QCOMPARE(textEdit->selectedText(), QLatin1String(""));
    QCOMPARE(textEdit->cursorPosition(), 5);

    textEdit->select(5, 6);

    QCOMPARE(selectionStartSpy.count(), 3);
    QCOMPARE(selectionEndSpy.count(), 3);
    QCOMPARE(selectionSpy.count(), 5);
    QCOMPARE(textEdit->selectionStart(), 5);
    QCOMPARE(textEdit->selectionEnd(), 6);
    QCOMPARE(textEdit->selectedText(), QLatin1String("t"));
    QCOMPARE(textEdit->cursorPosition(), 6);

    QList<QInputMethodEvent::Attribute> attributes;
    attributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 0, 0, QVariant());
    QInputMethodEvent event(QLatin1String(""), attributes);
    QApplication::sendEvent(canvas, &event);

    QCOMPARE(selectionStartSpy.count(), 4);
    QCOMPARE(selectionEndSpy.count(), 4);
    QCOMPARE(selectionSpy.count(), 6);
    QCOMPARE(textEdit->selectionStart(), textEdit->cursorPosition());
    QCOMPARE(textEdit->selectionEnd(), textEdit->cursorPosition());
    QCOMPARE(textEdit->selectedText(), QLatin1String(""));
    QCOMPARE(textEdit->cursorPosition(), 0);

    textEdit->setCursorPosition(1);

    QCOMPARE(selectionStartSpy.count(), 5);
    QCOMPARE(selectionEndSpy.count(), 5);
    QCOMPARE(selectionSpy.count(), 6);

    QKeyEvent leftArrowShiftPress(QEvent::KeyPress, Qt::Key_Left, Qt::ShiftModifier);
    QKeyEvent leftArrowShiftRelese(QEvent::KeyRelease, Qt::Key_Left, Qt::ShiftModifier);
    QApplication::sendEvent(canvas, &leftArrowShiftPress);
    QApplication::sendEvent(canvas, &leftArrowShiftRelese);

    QCOMPARE(selectionStartSpy.count(), 6);
    QCOMPARE(selectionEndSpy.count(), 5);
    QCOMPARE(selectionSpy.count(), 7);
    QCOMPARE(textEdit->selectionStart(), 0);
    QCOMPARE(textEdit->selectionEnd(), 1);
    QCOMPARE(textEdit->selectedText(), QLatin1String("S"));
    QCOMPARE(textEdit->cursorPosition(), 0);

    QApplication::sendEvent(canvas, &event);

    QCOMPARE(selectionStartSpy.count(), 6);
    QCOMPARE(selectionEndSpy.count(), 6);
    QCOMPARE(selectionSpy.count(), 8);
    QCOMPARE(textEdit->selectionStart(), textEdit->cursorPosition());
    QCOMPARE(textEdit->selectionEnd(), textEdit->cursorPosition());
    QCOMPARE(textEdit->selectedText(), QLatin1String(""));
    QCOMPARE(textEdit->cursorPosition(), 0);
}
QTEST_MAIN(tst_qdeclarativetextedit)

#include "tst_qdeclarativetextedit.moc"
