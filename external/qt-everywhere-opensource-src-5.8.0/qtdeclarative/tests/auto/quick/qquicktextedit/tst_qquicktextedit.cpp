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
#include "../../shared/testhttpserver.h"
#include <math.h>
#include <QFile>
#include <QTextDocument>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlexpression.h>
#include <QtQml/qqmlcomponent.h>
#include <QtGui/qguiapplication.h>
#include <private/qquicktextedit_p.h>
#include <private/qquicktextedit_p_p.h>
#include <private/qquicktext_p.h>
#include <private/qquicktextdocument_p.h>
#include <QFontMetrics>
#include <QtQuick/QQuickView>
#include <QDir>
#include <QInputMethod>
#include <QClipboard>
#include <QMimeData>
#include <private/qquicktextcontrol_p.h>
#include "../../shared/util.h"
#include "../../shared/platformquirks.h"
#include "../../shared/platforminputcontext.h"
#include <private/qinputmethod_p.h>
#include <QtGui/qstylehints.h>
#include <qmath.h>

#ifdef Q_OS_MAC
#include <Carbon/Carbon.h>
#endif

Q_DECLARE_METATYPE(QQuickTextEdit::SelectionMode)
Q_DECLARE_METATYPE(Qt::Key)
DEFINE_BOOL_CONFIG_OPTION(qmlDisableDistanceField, QML_DISABLE_DISTANCEFIELD)

QString createExpectedFileIfNotFound(const QString& filebasename, const QImage& actual)
{
    // XXX This will be replaced by some clever persistent platform image store.
    QString persistent_dir = QQmlDataTest::instance()->dataDirectory();
    QString arch = "unknown-architecture"; // QTest needs to help with this.

    QString expectfile = persistent_dir + QDir::separator() + filebasename + QLatin1Char('-') + arch + ".png";

    if (!QFile::exists(expectfile)) {
        actual.save(expectfile);
        qWarning() << "created" << expectfile;
    }

    return expectfile;
}

typedef QPair<int, QChar> Key;

class tst_qquicktextedit : public QQmlDataTest

{
    Q_OBJECT
public:
    tst_qquicktextedit();

private slots:
    void cleanup();
    void text();
    void width();
    void wrap();
    void textFormat();
    void lineCount_data();
    void lineCount();

    // ### these tests may be trivial
    void hAlign();
    void hAlign_RightToLeft();
    void hAlignVisual();
    void vAlign();
    void font();
    void color();
    void textMargin();
    void persistentSelection();
    void selectionOnFocusOut();
    void focusOnPress();
    void selection();
    void overwriteMode();
    void isRightToLeft_data();
    void isRightToLeft();
    void keySelection();
    void moveCursorSelection_data();
    void moveCursorSelection();
    void moveCursorSelectionSequence_data();
    void moveCursorSelectionSequence();
    void mouseSelection_data();
    void mouseSelection();
    void mouseSelectionMode_data();
    void mouseSelectionMode();
    void dragMouseSelection();
    void mouseSelectionMode_accessors();
    void selectByMouse();
    void selectByKeyboard();
    void keyboardSelection_data();
    void keyboardSelection();
    void renderType();
    void inputMethodHints();

    void positionAt_data();
    void positionAt();

    void linkInteraction();

    void cursorDelegate_data();
    void cursorDelegate();
    void remoteCursorDelegate();
    void cursorVisible();
    void delegateLoading_data();
    void delegateLoading();
    void cursorDelegateHeight();
    void navigation();
    void readOnly();
#ifndef QT_NO_CLIPBOARD
    void copyAndPaste();
    void canPaste();
    void canPasteEmpty();
    void middleClickPaste();
#endif
    void textInput();
    void inputMethodUpdate();
    void openInputPanel();
    void geometrySignals();
#ifndef QT_NO_CLIPBOARD
    void pastingRichText_QTBUG_14003();
#endif
    void implicitSize_data();
    void implicitSize();
    void contentSize();
    void boundingRect();
    void clipRect();
    void implicitSizeBinding_data();
    void implicitSizeBinding();

    void signal_editingfinished();

    void preeditCursorRectangle();
    void inputMethodComposing();
    void cursorRectangleSize_data();
    void cursorRectangleSize();

    void getText_data();
    void getText();
    void getFormattedText_data();
    void getFormattedText();
    void append_data();
    void append();
    void insert_data();
    void insert();
    void remove_data();
    void remove();

    void keySequence_data();
    void keySequence();

    void undo_data();
    void undo();
    void redo_data();
    void redo();
    void undo_keypressevents_data();
    void undo_keypressevents();
    void clear();

    void baseUrl();
    void embeddedImages();
    void embeddedImages_data();

    void emptytags_QTBUG_22058();
    void cursorRectangle_QTBUG_38947();
    void textCached_QTBUG_41583();
    void doubleSelect_QTBUG_38704();

    void padding();
    void QTBUG_51115_readOnlyResetsSelection();

private:
    void simulateKeys(QWindow *window, const QList<Key> &keys);
    void simulateKeys(QWindow *window, const QKeySequence &sequence);

    void simulateKey(QWindow *, int key, Qt::KeyboardModifiers modifiers = 0);

    QStringList standard;
    QStringList richText;

    QStringList hAlignmentStrings;
    QStringList vAlignmentStrings;

    QList<Qt::Alignment> vAlignments;
    QList<Qt::Alignment> hAlignments;

    QStringList colorStrings;

    QQmlEngine engine;
};

typedef QList<int> IntList;
Q_DECLARE_METATYPE(IntList)

typedef QList<Key> KeyList;
Q_DECLARE_METATYPE(KeyList)

Q_DECLARE_METATYPE(QQuickTextEdit::HAlignment)
Q_DECLARE_METATYPE(QQuickTextEdit::VAlignment)
Q_DECLARE_METATYPE(QQuickTextEdit::TextFormat)

void tst_qquicktextedit::simulateKeys(QWindow *window, const QList<Key> &keys)
{
    for (int i = 0; i < keys.count(); ++i) {
        const int key = keys.at(i).first;
        const int modifiers = key & Qt::KeyboardModifierMask;
        const QString text = !keys.at(i).second.isNull() ? QString(keys.at(i).second) : QString();

        QKeyEvent press(QEvent::KeyPress, Qt::Key(key), Qt::KeyboardModifiers(modifiers), text);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key(key), Qt::KeyboardModifiers(modifiers), text);

        QGuiApplication::sendEvent(window, &press);
        QGuiApplication::sendEvent(window, &release);
    }
}

void tst_qquicktextedit::simulateKeys(QWindow *window, const QKeySequence &sequence)
{
    for (int i = 0; i < sequence.count(); ++i) {
        const int key = sequence[i];
        const int modifiers = key & Qt::KeyboardModifierMask;

        QTest::keyClick(window, Qt::Key(key & ~modifiers), Qt::KeyboardModifiers(modifiers));
    }
}

QList<Key> &operator <<(QList<Key> &keys, const QKeySequence &sequence)
{
    for (int i = 0; i < sequence.count(); ++i)
        keys << Key(sequence[i], QChar());
    return keys;
}

template <int N> QList<Key> &operator <<(QList<Key> &keys, const char (&characters)[N])
{
    for (int i = 0; i < N - 1; ++i) {
        int key = QTest::asciiToKey(characters[i]);
        QChar character = QLatin1Char(characters[i]);
        keys << Key(key, character);
    }
    return keys;
}

QList<Key> &operator <<(QList<Key> &keys, Qt::Key key)
{
    keys << Key(key, QChar());
    return keys;
}

tst_qquicktextedit::tst_qquicktextedit()
{
    qRegisterMetaType<QQuickTextEdit::TextFormat>();
    qRegisterMetaType<QQuickTextEdit::SelectionMode>();

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

void tst_qquicktextedit::cleanup()
{
    // ensure not even skipped tests with custom input context leave it dangling
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = 0;
}

void tst_qquicktextedit::text()
{
    {
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData("import QtQuick 2.0\nTextEdit {  text: \"\"  }", QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->text(), QString(""));
        QCOMPARE(textEditObject->length(), 0);
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit { text: \"" + standard.at(i) + "\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->text(), standard.at(i));
        QCOMPARE(textEditObject->length(), standard.at(i).length());
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit { text: \"" + richText.at(i) + "\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());

        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);

        QString expected = richText.at(i);
        expected.replace(QRegExp("\\\\(.)"),"\\1");
        QCOMPARE(textEditObject->text(), expected);
        QCOMPARE(textEditObject->length(), expected.length());
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit { textFormat: TextEdit.RichText; text: \"" + standard.at(i) + "\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);

        QString actual = textEditObject->text();
        QString expected = standard.at(i);
        actual.remove(QRegExp(".*<body[^>]*>"));
        actual.remove(QRegExp("(<[^>]*>)+"));
        expected.remove("\n");
        QCOMPARE(actual.simplified(), expected);
        QCOMPARE(textEditObject->length(), expected.length());
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit { textFormat: TextEdit.RichText; text: \"" + richText.at(i) + "\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QString actual = textEditObject->text();
        QString expected = richText.at(i);
        actual.replace(QRegExp(".*<body[^>]*>"),"");
        actual.replace(QRegExp("(<[^>]*>)+"),"<>");
        expected.replace(QRegExp("(<[^>]*>)+"),"<>");
        QCOMPARE(actual.simplified(),expected.simplified());

        expected.replace("<>", " ");
        QCOMPARE(textEditObject->length(), expected.simplified().length());
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit { textFormat: TextEdit.AutoText; text: \"" + standard.at(i) + "\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->text(), standard.at(i));
        QCOMPARE(textEditObject->length(), standard.at(i).length());
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit { textFormat: TextEdit.AutoText; text: \"" + richText.at(i) + "\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QString actual = textEditObject->text();
        QString expected = richText.at(i);
        actual.replace(QRegExp(".*<body[^>]*>"),"");
        actual.replace(QRegExp("(<[^>]*>)+"),"<>");
        expected.replace(QRegExp("(<[^>]*>)+"),"<>");
        QCOMPARE(actual.simplified(),expected.simplified());

        expected.replace("<>", " ");
        QCOMPARE(textEditObject->length(), expected.simplified().length());
    }
}

void tst_qquicktextedit::width()
{
    // uses Font metrics to find the width for standard and document to find the width for rich
    {
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData("import QtQuick 2.0\nTextEdit {  text: \"\" }", QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), 0.0);
    }

    bool requiresUnhintedMetrics = !qmlDisableDistanceField();

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit { text: \"" + standard.at(i) + "\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QString s = standard.at(i);
        s.replace(QLatin1Char('\n'), QChar::LineSeparator);

        QTextLayout layout(s);
        layout.setFont(textEditObject->font());
        layout.setFlags(Qt::TextExpandTabs | Qt::TextShowMnemonic);
        if (requiresUnhintedMetrics) {
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

        qreal metricWidth = layout.boundingRect().width();

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), metricWidth);
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QTextDocument document;
        document.setHtml(richText.at(i));
        document.setDocumentMargin(0);
        if (requiresUnhintedMetrics)
            document.setUseDesignMetrics(true);

        qreal documentWidth = document.idealWidth();

        QString componentStr = "import QtQuick 2.0\nTextEdit { textFormat: TextEdit.RichText; text: \"" + richText.at(i) + "\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), documentWidth);
    }
}

void tst_qquicktextedit::wrap()
{
    // for specified width and wrap set true
    {
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData("import QtQuick 2.0\nTextEdit {  text: \"\"; wrapMode: TextEdit.WordWrap; width: 300 }", QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), 300.);
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit {  wrapMode: TextEdit.WordWrap; width: 300; text: \"" + standard.at(i) + "\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), 300.);
    }

    for (int i = 0; i < richText.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit {  wrapMode: TextEdit.WordWrap; width: 300; text: \"" + richText.at(i) + "\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->width(), 300.);
    }
    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0\n TextEdit {}", QUrl());
        QScopedPointer<QObject> object(component.create());
        QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(object.data());
        QVERIFY(edit);

        QSignalSpy spy(edit, SIGNAL(wrapModeChanged()));

        QCOMPARE(edit->wrapMode(), QQuickTextEdit::NoWrap);

        edit->setWrapMode(QQuickTextEdit::Wrap);
        QCOMPARE(edit->wrapMode(), QQuickTextEdit::Wrap);
        QCOMPARE(spy.count(), 1);

        edit->setWrapMode(QQuickTextEdit::Wrap);
        QCOMPARE(spy.count(), 1);

        edit->setWrapMode(QQuickTextEdit::NoWrap);
        QCOMPARE(edit->wrapMode(), QQuickTextEdit::NoWrap);
        QCOMPARE(spy.count(), 2);
    }

}

void tst_qquicktextedit::textFormat()
{
    {
        QQmlComponent textComponent(&engine);
        textComponent.setData("import QtQuick 2.0\nTextEdit { text: \"Hello\"; textFormat: Text.RichText }", QUrl::fromLocalFile(""));
        QQuickTextEdit *textObject = qobject_cast<QQuickTextEdit*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->textFormat(), QQuickTextEdit::RichText);
    }
    {
        QQmlComponent textComponent(&engine);
        textComponent.setData("import QtQuick 2.0\nTextEdit { text: \"<b>Hello</b>\"; textFormat: Text.PlainText }", QUrl::fromLocalFile(""));
        QQuickTextEdit *textObject = qobject_cast<QQuickTextEdit*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->textFormat(), QQuickTextEdit::PlainText);
    }
    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0\n TextEdit {}", QUrl());
        QScopedPointer<QObject> object(component.create());
        QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(object.data());
        QVERIFY(edit);

        QSignalSpy spy(edit, &QQuickTextEdit::textFormatChanged);

        QCOMPARE(edit->textFormat(), QQuickTextEdit::PlainText);

        edit->setTextFormat(QQuickTextEdit::RichText);
        QCOMPARE(edit->textFormat(), QQuickTextEdit::RichText);
        QCOMPARE(spy.count(), 1);

        edit->setTextFormat(QQuickTextEdit::RichText);
        QCOMPARE(spy.count(), 1);

        edit->setTextFormat(QQuickTextEdit::PlainText);
        QCOMPARE(edit->textFormat(), QQuickTextEdit::PlainText);
        QCOMPARE(spy.count(), 2);
    }
}

static int calcLineCount(QTextDocument* doc)
{
    int subLines = 0;
    for (QTextBlock it = doc->begin(); it != doc->end(); it = it.next()) {
        QTextLayout *layout = it.layout();
        if (!layout)
            continue;
        subLines += layout->lineCount()-1;
    }
    return doc->lineCount() + subLines;
}

void tst_qquicktextedit::lineCount_data()
{
    QTest::addColumn<QStringList>("texts");
    QTest::newRow("plaintext") << standard;
    QTest::newRow("richtext") << richText;
}

void tst_qquicktextedit::lineCount()
{
    QFETCH(QStringList, texts);

    foreach (const QString& text, texts) {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0\nTextEdit { }", QUrl());

        QQuickTextEdit *textedit = qobject_cast<QQuickTextEdit*>(component.create());
        QVERIFY(textedit);

        QTextDocument *doc = QQuickTextEditPrivate::get(textedit)->document;
        QVERIFY(doc);

        textedit->setText(text);

        textedit->setWidth(100.0);
        QCOMPARE(textedit->lineCount(), calcLineCount(doc));

        textedit->setWrapMode(QQuickTextEdit::Wrap);
        QCOMPARE(textedit->lineCount(), calcLineCount(doc));

        textedit->setWidth(50.0);
        QCOMPARE(textedit->lineCount(), calcLineCount(doc));

        textedit->setWrapMode(QQuickTextEdit::NoWrap);
        QCOMPARE(textedit->lineCount(), calcLineCount(doc));
    }
}

//the alignment tests may be trivial o.oa
void tst_qquicktextedit::hAlign()
{
    //test one align each, and then test if two align fails.

    for (int i = 0; i < standard.size(); i++)
    {
        for (int j=0; j < hAlignmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 2.0\nTextEdit {  horizontalAlignment: \"" + hAlignmentStrings.at(j) + "\"; text: \"" + standard.at(i) + "\" }";
            QQmlComponent texteditComponent(&engine);
            texteditComponent.setData(componentStr.toLatin1(), QUrl());
            QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

            QVERIFY(textEditObject != 0);
            QCOMPARE((int)textEditObject->hAlign(), (int)hAlignments.at(j));
        }
    }

    for (int i = 0; i < richText.size(); i++)
    {
        for (int j=0; j < hAlignmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 2.0\nTextEdit {  horizontalAlignment: \"" + hAlignmentStrings.at(j) + "\"; text: \"" + richText.at(i) + "\" }";
            QQmlComponent texteditComponent(&engine);
            texteditComponent.setData(componentStr.toLatin1(), QUrl());
            QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

            QVERIFY(textEditObject != 0);
            QCOMPARE((int)textEditObject->hAlign(), (int)hAlignments.at(j));
        }
    }

}

void tst_qquicktextedit::hAlign_RightToLeft()
{
    PlatformInputContext platformInputContext;
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = &platformInputContext;

    QQuickView window(testFileUrl("horizontalAlignment_RightToLeft.qml"));
    QQuickTextEdit *textEdit = window.rootObject()->findChild<QQuickTextEdit*>("text");
    QVERIFY(textEdit != 0);
    window.showNormal();

    const QString rtlText = textEdit->text();

    // implicit alignment should follow the reading direction of text
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > window.width()/2);

    // explicitly left aligned
    textEdit->setHAlign(QQuickTextEdit::AlignLeft);
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignLeft);
    QVERIFY(textEdit->positionToRectangle(0).x() < window.width()/2);

    // explicitly right aligned
    textEdit->setHAlign(QQuickTextEdit::AlignRight);
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > window.width()/2);

    QString textString = textEdit->text();
    textEdit->setText(QString("<i>") + textString + QString("</i>"));
    textEdit->resetHAlign();

    // implicitly aligned rich text should follow the reading direction of RTL text
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    QCOMPARE(textEdit->effectiveHAlign(), textEdit->hAlign());
    QVERIFY(textEdit->positionToRectangle(0).x() > window.width()/2);

    // explicitly left aligned rich text
    textEdit->setHAlign(QQuickTextEdit::AlignLeft);
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignLeft);
    QCOMPARE(textEdit->effectiveHAlign(), textEdit->hAlign());
    QVERIFY(textEdit->positionToRectangle(0).x() < window.width()/2);

    // explicitly right aligned rich text
    textEdit->setHAlign(QQuickTextEdit::AlignRight);
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    QCOMPARE(textEdit->effectiveHAlign(), textEdit->hAlign());
    QVERIFY(textEdit->positionToRectangle(0).x() > window.width()/2);

    textEdit->setText(textString);

    // explicitly center aligned
    textEdit->setHAlign(QQuickTextEdit::AlignHCenter);
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignHCenter);
    QVERIFY(textEdit->positionToRectangle(0).x() > window.width()/2);

    // reseted alignment should go back to following the text reading direction
    textEdit->resetHAlign();
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > window.width()/2);

    // mirror the text item
    QQuickItemPrivate::get(textEdit)->setLayoutMirror(true);

    // mirrored implicit alignment should continue to follow the reading direction of the text
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    QCOMPARE(textEdit->effectiveHAlign(), QQuickTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > window.width()/2);

    // mirrored explicitly right aligned behaves as left aligned
    textEdit->setHAlign(QQuickTextEdit::AlignRight);
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    QCOMPARE(textEdit->effectiveHAlign(), QQuickTextEdit::AlignLeft);
    QVERIFY(textEdit->positionToRectangle(0).x() < window.width()/2);

    // mirrored explicitly left aligned behaves as right aligned
    textEdit->setHAlign(QQuickTextEdit::AlignLeft);
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignLeft);
    QCOMPARE(textEdit->effectiveHAlign(), QQuickTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > window.width()/2);

    // disable mirroring
    QQuickItemPrivate::get(textEdit)->setLayoutMirror(false);
    textEdit->resetHAlign();

    // English text should be implicitly left aligned
    textEdit->setText("Hello world!");
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignLeft);
    QVERIFY(textEdit->positionToRectangle(0).x() < window.width()/2);

    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(textEdit->hasActiveFocus());

    textEdit->setText(QString());
    { QInputMethodEvent ev(rtlText, QList<QInputMethodEvent::Attribute>()); QGuiApplication::sendEvent(textEdit, &ev); }
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    { QInputMethodEvent ev("Hello world!", QList<QInputMethodEvent::Attribute>()); QGuiApplication::sendEvent(textEdit, &ev); }
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignLeft);

    // Clear pre-edit text.  TextEdit should maybe do this itself on setText, but that may be
    // redundant as an actual input method may take care of it.
    { QInputMethodEvent ev; QGuiApplication::sendEvent(textEdit, &ev); }

    // empty text with implicit alignment follows the system locale-based
    // keyboard input direction from qApp->inputMethod()->inputDirection
    textEdit->setText("");
    platformInputContext.setInputDirection(Qt::LeftToRight);
    QCOMPARE(qApp->inputMethod()->inputDirection(), Qt::LeftToRight);
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignLeft);
    QVERIFY(textEdit->positionToRectangle(0).x() < window.width()/2);

    QSignalSpy cursorRectangleSpy(textEdit, SIGNAL(cursorRectangleChanged()));

    platformInputContext.setInputDirection(Qt::RightToLeft);
    QCOMPARE(cursorRectangleSpy.count(), 1);
    QCOMPARE(qApp->inputMethod()->inputDirection(), Qt::RightToLeft);
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > window.width()/2);

    // neutral text follows also input method direction
    textEdit->resetHAlign();
    textEdit->setText(" ()((=<>");
    platformInputContext.setInputDirection(Qt::LeftToRight);
    QCOMPARE(textEdit->effectiveHAlign(), QQuickTextEdit::AlignLeft);
    QVERIFY(textEdit->cursorRectangle().left() < window.width()/2);
    platformInputContext.setInputDirection(Qt::RightToLeft);
    QCOMPARE(textEdit->effectiveHAlign(), QQuickTextEdit::AlignRight);
    QVERIFY(textEdit->cursorRectangle().left() > window.width()/2);

    // set input direction while having content
    platformInputContext.setInputDirection(Qt::LeftToRight);
    textEdit->setText("a");
    textEdit->setCursorPosition(1);
    platformInputContext.setInputDirection(Qt::RightToLeft);
    QTest::keyClick(&window, Qt::Key_Backspace);
    QVERIFY(textEdit->text().isEmpty());
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    QVERIFY(textEdit->cursorRectangle().left() > window.width()/2);

    // input direction changed while not having focus
    platformInputContext.setInputDirection(Qt::LeftToRight);
    textEdit->setFocus(false);
    platformInputContext.setInputDirection(Qt::RightToLeft);
    textEdit->setFocus(true);
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    QVERIFY(textEdit->cursorRectangle().left() > window.width()/2);

    textEdit->setHAlign(QQuickTextEdit::AlignRight);
    QCOMPARE(textEdit->hAlign(), QQuickTextEdit::AlignRight);
    QVERIFY(textEdit->positionToRectangle(0).x() > window.width()/2);

    // make sure editor doesn't rely on input for updating size
    QQuickTextEdit *emptyEdit = window.rootObject()->findChild<QQuickTextEdit*>("emptyTextEdit");
    QVERIFY(emptyEdit != 0);
    platformInputContext.setInputDirection(Qt::RightToLeft);
    emptyEdit->setFocus(true);
    QCOMPARE(emptyEdit->hAlign(), QQuickTextEdit::AlignRight);
    QVERIFY(emptyEdit->cursorRectangle().left() > window.width()/2);
}


static int numberOfNonWhitePixels(int fromX, int toX, const QImage &image)
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

void tst_qquicktextedit::hAlignVisual()
{
    QQuickView view(testFileUrl("hAlignVisual.qml"));
    view.setFlags(view.flags() | Qt::WindowStaysOnTopHint); // Prevent being obscured by other windows.
    view.showNormal();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QQuickText *text = view.rootObject()->findChild<QQuickText*>("textItem");
    QVERIFY(text != 0);

    // Try to check whether alignment works by checking the number of black
    // pixels in the thirds of the grabbed image.
    const int windowWidth = 200;
    const int textWidth = qCeil(text->implicitWidth());
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
        QImage image = view.grabWindow();
        const int left = numberOfNonWhitePixels(centeredSection1, centeredSection2, image);
        const int mid = numberOfNonWhitePixels(centeredSection2, centeredSection3, image);
        const int right = numberOfNonWhitePixels(centeredSection3, centeredSection3End, image);
        image.save("test3.png");
        QVERIFY2(left < mid, msgNotLessThan(left, mid).constData());
        QVERIFY2(mid < right, msgNotLessThan(mid, right).constData());
    }

    text->setWidth(200);

    {
        // Left Align
        QImage image = view.grabWindow();
        int x = qCeil(text->implicitWidth());
        int left = numberOfNonWhitePixels(0, x, image);
        int right = numberOfNonWhitePixels(x, image.width() - x, image);
        QVERIFY2(left > 0, msgNotGreaterThan(left, 0).constData());
        QCOMPARE(right, 0);
    }
    {
        // HCenter Align
        text->setHAlign(QQuickText::AlignHCenter);
        QImage image = view.grabWindow();
        int x1 = qFloor(image.width() - text->implicitWidth()) / 2;
        int x2 = image.width() - x1;
        int left = numberOfNonWhitePixels(0, x1, image);
        int mid = numberOfNonWhitePixels(x1, x2 - x1, image);
        int right = numberOfNonWhitePixels(x2, image.width() - x2, image);
        QCOMPARE(left, 0);
        QVERIFY2(mid > 0, msgNotGreaterThan(left, 0).constData());
        QCOMPARE(right, 0);
    }
    {
        // Right Align
        text->setHAlign(QQuickText::AlignRight);
        QImage image = view.grabWindow();
        int x = image.width() - qCeil(text->implicitWidth());
        int left = numberOfNonWhitePixels(0, x, image);
        int right = numberOfNonWhitePixels(x, image.width() - x, image);
        QCOMPARE(left, 0);
        QVERIFY2(right > 0, msgNotGreaterThan(left, 0).constData());
    }
}

void tst_qquicktextedit::vAlign()
{
    //test one align each, and then test if two align fails.

    for (int i = 0; i < standard.size(); i++)
    {
        for (int j=0; j < vAlignmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 2.0\nTextEdit {  verticalAlignment: \"" + vAlignmentStrings.at(j) + "\"; text: \"" + standard.at(i) + "\" }";
            QQmlComponent texteditComponent(&engine);
            texteditComponent.setData(componentStr.toLatin1(), QUrl());
            QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

            QVERIFY(textEditObject != 0);
            QCOMPARE((int)textEditObject->vAlign(), (int)vAlignments.at(j));
        }
    }

    for (int i = 0; i < richText.size(); i++)
    {
        for (int j=0; j < vAlignmentStrings.size(); j++)
        {
            QString componentStr = "import QtQuick 2.0\nTextEdit {  verticalAlignment: \"" + vAlignmentStrings.at(j) + "\"; text: \"" + richText.at(i) + "\" }";
            QQmlComponent texteditComponent(&engine);
            texteditComponent.setData(componentStr.toLatin1(), QUrl());
            QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

            QVERIFY(textEditObject != 0);
            QCOMPARE((int)textEditObject->vAlign(), (int)vAlignments.at(j));
        }
    }

    QQmlComponent texteditComponent(&engine);
    texteditComponent.setData(
                "import QtQuick 2.0\n"
                "TextEdit { width: 100; height: 100; text: \"Hello World\" }", QUrl());
    QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

    QVERIFY(textEditObject != 0);

    QCOMPARE(textEditObject->vAlign(), QQuickTextEdit::AlignTop);
    QVERIFY(textEditObject->cursorRectangle().bottom() < 50);
    QVERIFY(textEditObject->positionToRectangle(0).bottom() < 50);

    // bottom aligned
    textEditObject->setVAlign(QQuickTextEdit::AlignBottom);
    QCOMPARE(textEditObject->vAlign(), QQuickTextEdit::AlignBottom);
    QVERIFY(textEditObject->cursorRectangle().top() > 50);
    QVERIFY(textEditObject->positionToRectangle(0).top() > 50);

    // explicitly center aligned
    textEditObject->setVAlign(QQuickTextEdit::AlignVCenter);
    QCOMPARE(textEditObject->vAlign(), QQuickTextEdit::AlignVCenter);
    QVERIFY(textEditObject->cursorRectangle().top() < 50);
    QVERIFY(textEditObject->cursorRectangle().bottom() > 50);
    QVERIFY(textEditObject->positionToRectangle(0).top() < 50);
    QVERIFY(textEditObject->positionToRectangle(0).bottom() > 50);

    // Test vertical alignment after resizing the height.
    textEditObject->setHeight(textEditObject->height() - 20);
    QVERIFY(textEditObject->cursorRectangle().top() < 40);
    QVERIFY(textEditObject->cursorRectangle().bottom() > 40);
    QVERIFY(textEditObject->positionToRectangle(0).top() < 40);
    QVERIFY(textEditObject->positionToRectangle(0).bottom() > 40);

    textEditObject->setHeight(textEditObject->height() + 40);
    QVERIFY(textEditObject->cursorRectangle().top() < 60);
    QVERIFY(textEditObject->cursorRectangle().bottom() > 60);
    QVERIFY(textEditObject->positionToRectangle(0).top() < 60);
    QVERIFY(textEditObject->positionToRectangle(0).bottom() > 60);
}

void tst_qquicktextedit::font()
{
    //test size, then bold, then italic, then family
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit {  font.pointSize: 40; text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->font().pointSize(), 40);
        QCOMPARE(textEditObject->font().bold(), false);
        QCOMPARE(textEditObject->font().italic(), false);
    }

    {
        QString componentStr = "import QtQuick 2.0\nTextEdit {  font.bold: true; text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->font().bold(), true);
        QCOMPARE(textEditObject->font().italic(), false);
    }

    {
        QString componentStr = "import QtQuick 2.0\nTextEdit {  font.italic: true; text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->font().italic(), true);
        QCOMPARE(textEditObject->font().bold(), false);
    }

    {
        QString componentStr = "import QtQuick 2.0\nTextEdit {  font.family: \"Helvetica\"; text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->font().family(), QString("Helvetica"));
        QCOMPARE(textEditObject->font().bold(), false);
        QCOMPARE(textEditObject->font().italic(), false);
    }

    {
        QString componentStr = "import QtQuick 2.0\nTextEdit {  font.family: \"\"; text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->font().family(), QString(""));
    }
}

void tst_qquicktextedit::color()
{
    //test initial color
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit { text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject);
        QCOMPARE(textEditObject->color(), QColor("black"));
        QCOMPARE(textEditObject->selectionColor(), QColor::fromRgba(0xFF000080));
        QCOMPARE(textEditObject->selectedTextColor(), QColor("white"));

        QSignalSpy colorSpy(textEditObject, SIGNAL(colorChanged(QColor)));
        QSignalSpy selectionColorSpy(textEditObject, SIGNAL(selectionColorChanged(QColor)));
        QSignalSpy selectedTextColorSpy(textEditObject, SIGNAL(selectedTextColorChanged(QColor)));

        textEditObject->setColor(QColor("white"));
        QCOMPARE(textEditObject->color(), QColor("white"));
        QCOMPARE(colorSpy.count(), 1);

        textEditObject->setSelectionColor(QColor("black"));
        QCOMPARE(textEditObject->selectionColor(), QColor("black"));
        QCOMPARE(selectionColorSpy.count(), 1);

        textEditObject->setSelectedTextColor(QColor("blue"));
        QCOMPARE(textEditObject->selectedTextColor(), QColor("blue"));
        QCOMPARE(selectedTextColorSpy.count(), 1);

        textEditObject->setColor(QColor("white"));
        QCOMPARE(colorSpy.count(), 1);

        textEditObject->setSelectionColor(QColor("black"));
        QCOMPARE(selectionColorSpy.count(), 1);

        textEditObject->setSelectedTextColor(QColor("blue"));
        QCOMPARE(selectedTextColorSpy.count(), 1);

        textEditObject->setColor(QColor("black"));
        QCOMPARE(textEditObject->color(), QColor("black"));
        QCOMPARE(colorSpy.count(), 2);

        textEditObject->setSelectionColor(QColor("blue"));
        QCOMPARE(textEditObject->selectionColor(), QColor("blue"));
        QCOMPARE(selectionColorSpy.count(), 2);

        textEditObject->setSelectedTextColor(QColor("white"));
        QCOMPARE(textEditObject->selectedTextColor(), QColor("white"));
        QCOMPARE(selectedTextColorSpy.count(), 2);
    }

    //test normal
    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit {  color: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());
        //qDebug() << "textEditObject: " << textEditObject->color() << "vs. " << QColor(colorStrings.at(i));
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->color(), QColor(colorStrings.at(i)));
    }

    //test selection
    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit {  selectionColor: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->selectionColor(), QColor(colorStrings.at(i)));
    }

    //test selected text
    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextEdit {  selectedTextColor: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->selectedTextColor(), QColor(colorStrings.at(i)));
    }

    {
        QString colorStr = "#AA001234";
        QColor testColor("#001234");
        testColor.setAlpha(170);

        QString componentStr = "import QtQuick 2.0\nTextEdit {  color: \"" + colorStr + "\"; text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());

        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->color(), testColor);
    }
}

void tst_qquicktextedit::textMargin()
{
    for (qreal i=0; i<=10; i+=0.3) {
        QString componentStr = "import QtQuick 2.0\nTextEdit {  textMargin: " + QString::number(i) + "; text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());
        QVERIFY(textEditObject != 0);
        QCOMPARE(textEditObject->textMargin(), i);
    }
}

void tst_qquicktextedit::persistentSelection()
{
    QQuickView window(testFileUrl("persistentSelection.qml"));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(window.rootObject());
    QVERIFY(edit);
    QVERIFY(edit->hasActiveFocus());

    QSignalSpy spy(edit, SIGNAL(persistentSelectionChanged(bool)));

    QCOMPARE(edit->persistentSelection(), false);

    edit->setPersistentSelection(false);
    QCOMPARE(edit->persistentSelection(), false);
    QCOMPARE(spy.count(), 0);

    edit->select(1, 4);
    QCOMPARE(edit->property("selected").toString(), QLatin1String("ell"));

    edit->setFocus(false);
    QCOMPARE(edit->property("selected").toString(), QString());

    edit->setFocus(true);
    QCOMPARE(edit->property("selected").toString(), QString());

    edit->setPersistentSelection(true);
    QCOMPARE(edit->persistentSelection(), true);
    QCOMPARE(spy.count(), 1);

    edit->select(1, 4);
    QCOMPARE(edit->property("selected").toString(), QLatin1String("ell"));

    edit->setFocus(false);
    QCOMPARE(edit->property("selected").toString(), QLatin1String("ell"));

    edit->setFocus(true);
    QCOMPARE(edit->property("selected").toString(), QLatin1String("ell"));

}

void tst_qquicktextedit::selectionOnFocusOut()
{
    QQuickView window(testFileUrl("focusOutSelection.qml"));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QPoint p1(25, 35);
    QPoint p2(25, 85);

    QQuickTextEdit *edit1 = window.rootObject()->findChild<QQuickTextEdit*>("text1");
    QQuickTextEdit *edit2 = window.rootObject()->findChild<QQuickTextEdit*>("text2");

    QTest::mouseClick(&window, Qt::LeftButton, 0, p1);
    QVERIFY(edit1->hasActiveFocus());
    QVERIFY(!edit2->hasActiveFocus());

    edit1->selectAll();
    QCOMPARE(edit1->selectedText(), QLatin1String("text 1"));

    QTest::mouseClick(&window, Qt::LeftButton, 0, p2);

    QCOMPARE(edit1->selectedText(), QLatin1String(""));
    QVERIFY(!edit1->hasActiveFocus());
    QVERIFY(edit2->hasActiveFocus());

    edit2->selectAll();
    QCOMPARE(edit2->selectedText(), QLatin1String("text 2"));


    edit2->setFocus(false, Qt::ActiveWindowFocusReason);
    QVERIFY(!edit2->hasActiveFocus());
    QCOMPARE(edit2->selectedText(), QLatin1String("text 2"));

    edit2->setFocus(true);
    QVERIFY(edit2->hasActiveFocus());

    edit2->setFocus(false, Qt::PopupFocusReason);
    QVERIFY(edit2->hasActiveFocus());
    QCOMPARE(edit2->selectedText(), QLatin1String("text 2"));
}

void tst_qquicktextedit::focusOnPress()
{
    QString componentStr =
            "import QtQuick 2.0\n"
            "TextEdit {\n"
                "property bool selectOnFocus: false\n"
                "width: 100; height: 50\n"
                "activeFocusOnPress: true\n"
                "text: \"Hello World\"\n"
                "onFocusChanged: { if (focus && selectOnFocus) selectAll() }"
            " }";
    QQmlComponent texteditComponent(&engine);
    texteditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());
    QVERIFY(textEditObject != 0);
    QCOMPARE(textEditObject->focusOnPress(), true);
    QCOMPARE(textEditObject->hasFocus(), false);

    QSignalSpy focusSpy(textEditObject, SIGNAL(focusChanged(bool)));
    QSignalSpy activeFocusSpy(textEditObject, SIGNAL(focusChanged(bool)));
    QSignalSpy activeFocusOnPressSpy(textEditObject, SIGNAL(activeFocusOnPressChanged(bool)));

    textEditObject->setFocusOnPress(true);
    QCOMPARE(textEditObject->focusOnPress(), true);
    QCOMPARE(activeFocusOnPressSpy.count(), 0);

    QQuickWindow window;
    window.resize(100, 50);
    textEditObject->setParentItem(window.contentItem());
    window.showNormal();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QCOMPARE(textEditObject->hasFocus(), false);
    QCOMPARE(textEditObject->hasActiveFocus(), false);

    QPoint centerPoint(window.width()/2, window.height()/2);
    Qt::KeyboardModifiers noModifiers = 0;
    QTest::mousePress(&window, Qt::LeftButton, noModifiers, centerPoint);
    QGuiApplication::processEvents();
    QCOMPARE(textEditObject->hasFocus(), true);
    QCOMPARE(textEditObject->hasActiveFocus(), true);
    QCOMPARE(focusSpy.count(), 1);
    QCOMPARE(activeFocusSpy.count(), 1);
    QCOMPARE(textEditObject->selectedText(), QString());
    QTest::mouseRelease(&window, Qt::LeftButton, noModifiers, centerPoint);

    textEditObject->setFocusOnPress(false);
    QCOMPARE(textEditObject->focusOnPress(), false);
    QCOMPARE(activeFocusOnPressSpy.count(), 1);

    textEditObject->setFocus(false);
    QCOMPARE(textEditObject->hasFocus(), false);
    QCOMPARE(textEditObject->hasActiveFocus(), false);
    QCOMPARE(focusSpy.count(), 2);
    QCOMPARE(activeFocusSpy.count(), 2);

    // Wait for double click timeout to expire before clicking again.
    QTest::qWait(400);
    QTest::mousePress(&window, Qt::LeftButton, noModifiers, centerPoint);
    QGuiApplication::processEvents();
    QCOMPARE(textEditObject->hasFocus(), false);
    QCOMPARE(textEditObject->hasActiveFocus(), false);
    QCOMPARE(focusSpy.count(), 2);
    QCOMPARE(activeFocusSpy.count(), 2);
    QTest::mouseRelease(&window, Qt::LeftButton, noModifiers, centerPoint);

    textEditObject->setFocusOnPress(true);
    QCOMPARE(textEditObject->focusOnPress(), true);
    QCOMPARE(activeFocusOnPressSpy.count(), 2);

    // Test a selection made in the on(Active)FocusChanged handler isn't overwritten.
    textEditObject->setProperty("selectOnFocus", true);

    QTest::qWait(400);
    QTest::mousePress(&window, Qt::LeftButton, noModifiers, centerPoint);
    QGuiApplication::processEvents();
    QCOMPARE(textEditObject->hasFocus(), true);
    QCOMPARE(textEditObject->hasActiveFocus(), true);
    QCOMPARE(focusSpy.count(), 3);
    QCOMPARE(activeFocusSpy.count(), 3);
    QCOMPARE(textEditObject->selectedText(), textEditObject->text());
    QTest::mouseRelease(&window, Qt::LeftButton, noModifiers, centerPoint);
}

void tst_qquicktextedit::selection()
{
    QString testStr = standard[0];//TODO: What should happen for multiline/rich text?
    QString componentStr = "import QtQuick 2.0\nTextEdit {  text: \""+ testStr +"\"; }";
    QQmlComponent texteditComponent(&engine);
    texteditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());
    QVERIFY(textEditObject != 0);


    //Test selection follows cursor
    for (int i=0; i<= testStr.size(); i++) {
        textEditObject->setCursorPosition(i);
        QCOMPARE(textEditObject->cursorPosition(), i);
        QCOMPARE(textEditObject->selectionStart(), i);
        QCOMPARE(textEditObject->selectionEnd(), i);
        QVERIFY(textEditObject->selectedText().isNull());
    }

    textEditObject->setCursorPosition(0);
    QCOMPARE(textEditObject->cursorPosition(), 0);
    QCOMPARE(textEditObject->selectionStart(), 0);
    QCOMPARE(textEditObject->selectionEnd(), 0);
    QVERIFY(textEditObject->selectedText().isNull());

    // Verify invalid positions are ignored.
    textEditObject->setCursorPosition(-1);
    QCOMPARE(textEditObject->cursorPosition(), 0);
    QCOMPARE(textEditObject->selectionStart(), 0);
    QCOMPARE(textEditObject->selectionEnd(), 0);
    QVERIFY(textEditObject->selectedText().isNull());

    textEditObject->setCursorPosition(textEditObject->text().count()+1);
    QCOMPARE(textEditObject->cursorPosition(), 0);
    QCOMPARE(textEditObject->selectionStart(), 0);
    QCOMPARE(textEditObject->selectionEnd(), 0);
    QVERIFY(textEditObject->selectedText().isNull());

    //Test selection
    for (int i=0; i<= testStr.size(); i++) {
        textEditObject->select(0,i);
        QCOMPARE(testStr.mid(0,i), textEditObject->selectedText());
    }
    for (int i=0; i<= testStr.size(); i++) {
        textEditObject->select(i,testStr.size());
        QCOMPARE(testStr.mid(i,testStr.size()-i), textEditObject->selectedText());
    }

    textEditObject->setCursorPosition(0);
    QCOMPARE(textEditObject->cursorPosition(), 0);
    QCOMPARE(textEditObject->selectionStart(), 0);
    QCOMPARE(textEditObject->selectionEnd(), 0);
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
    QCOMPARE(textEditObject->selectedText().size(), 10);
    textEditObject->select(-10,0);
    QCOMPARE(textEditObject->selectedText().size(), 10);
    textEditObject->select(100,101);
    QCOMPARE(textEditObject->selectedText().size(), 10);
    textEditObject->select(0,-10);
    QCOMPARE(textEditObject->selectedText().size(), 10);
    textEditObject->select(0,100);
    QCOMPARE(textEditObject->selectedText().size(), 10);

    textEditObject->deselect();
    QVERIFY(textEditObject->selectedText().isNull());
    textEditObject->select(0,10);
    QCOMPARE(textEditObject->selectedText().size(), 10);
    textEditObject->deselect();
    QVERIFY(textEditObject->selectedText().isNull());
}

void tst_qquicktextedit::overwriteMode()
{
    QString componentStr = "import QtQuick 2.0\nTextEdit { focus: true; }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    QSignalSpy spy(textEdit, SIGNAL(overwriteModeChanged(bool)));

    QQuickWindow window;
    textEdit->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(textEdit->hasActiveFocus());

    textEdit->setOverwriteMode(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(true, textEdit->overwriteMode());
    textEdit->setOverwriteMode(false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(false, textEdit->overwriteMode());

    QVERIFY(!textEdit->overwriteMode());
    QString insertString = "Some first text";
    for (int j = 0; j < insertString.length(); j++)
        QTest::keyClick(&window, insertString.at(j).toLatin1());

    QCOMPARE(textEdit->text(), QString("Some first text"));

    textEdit->setOverwriteMode(true);
    QCOMPARE(spy.count(), 3);
    textEdit->setCursorPosition(5);

    insertString = "shiny";
    for (int j = 0; j < insertString.length(); j++)
        QTest::keyClick(&window, insertString.at(j).toLatin1());
    QCOMPARE(textEdit->text(), QString("Some shiny text"));

    textEdit->setCursorPosition(textEdit->text().length());
    QTest::keyClick(&window, Qt::Key_Enter);

    textEdit->setOverwriteMode(false);
    QCOMPARE(spy.count(), 4);

    insertString = "Second paragraph";

    for (int j = 0; j < insertString.length(); j++)
        QTest::keyClick(&window, insertString.at(j).toLatin1());
    QCOMPARE(textEdit->lineCount(), 2);

    textEdit->setCursorPosition(15);

    QCOMPARE(textEdit->cursorPosition(), 15);

    textEdit->setOverwriteMode(true);
    QCOMPARE(spy.count(), 5);

    insertString = " blah";
    for (int j = 0; j < insertString.length(); j++)
        QTest::keyClick(&window, insertString.at(j).toLatin1());
    QCOMPARE(textEdit->lineCount(), 2);

    QCOMPARE(textEdit->text(), QString("Some shiny text blah\nSecond paragraph"));
}

void tst_qquicktextedit::isRightToLeft_data()
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

void tst_qquicktextedit::isRightToLeft()
{
    QFETCH(QString, text);
    QFETCH(bool, emptyString);
    QFETCH(bool, firstCharacter);
    QFETCH(bool, lastCharacter);
    QFETCH(bool, middleCharacter);
    QFETCH(bool, startString);
    QFETCH(bool, midString);
    QFETCH(bool, endString);

    QQuickTextEdit textEdit;
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

void tst_qquicktextedit::keySelection()
{
    QQuickView window(testFileUrl("navigation.qml"));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);

    QQuickTextEdit *input = qobject_cast<QQuickTextEdit *>(qvariant_cast<QObject *>(window.rootObject()->property("myInput")));

    QVERIFY(input != 0);
    QVERIFY(input->hasActiveFocus());

    QSignalSpy spy(input, SIGNAL(selectedTextChanged()));

    simulateKey(&window, Qt::Key_Right, Qt::ShiftModifier);
    QVERIFY(input->hasActiveFocus());
    QCOMPARE(input->selectedText(), QString("a"));
    QCOMPARE(spy.count(), 1);
    simulateKey(&window, Qt::Key_Right);
    QVERIFY(input->hasActiveFocus());
    QCOMPARE(input->selectedText(), QString());
    QCOMPARE(spy.count(), 2);
    simulateKey(&window, Qt::Key_Right);
    QVERIFY(!input->hasActiveFocus());
    QCOMPARE(input->selectedText(), QString());
    QCOMPARE(spy.count(), 2);

    simulateKey(&window, Qt::Key_Left);
    QVERIFY(input->hasActiveFocus());
    QCOMPARE(spy.count(), 2);
    simulateKey(&window, Qt::Key_Left, Qt::ShiftModifier);
    QVERIFY(input->hasActiveFocus());
    QCOMPARE(input->selectedText(), QString("a"));
    QCOMPARE(spy.count(), 3);
    simulateKey(&window, Qt::Key_Left);
    QVERIFY(input->hasActiveFocus());
    QCOMPARE(input->selectedText(), QString());
    QCOMPARE(spy.count(), 4);
    simulateKey(&window, Qt::Key_Left);
    QVERIFY(!input->hasActiveFocus());
    QCOMPARE(input->selectedText(), QString());
    QCOMPARE(spy.count(), 4);
}

void tst_qquicktextedit::moveCursorSelection_data()
{
    QTest::addColumn<QString>("testStr");
    QTest::addColumn<int>("cursorPosition");
    QTest::addColumn<int>("movePosition");
    QTest::addColumn<QQuickTextEdit::SelectionMode>("mode");
    QTest::addColumn<int>("selectionStart");
    QTest::addColumn<int>("selectionEnd");
    QTest::addColumn<bool>("reversible");

    QTest::newRow("(t)he|characters")
            << standard[0] << 0 << 1 << QQuickTextEdit::SelectCharacters << 0 << 1 << true;
    QTest::newRow("do(g)|characters")
            << standard[0] << 43 << 44 << QQuickTextEdit::SelectCharacters << 43 << 44 << true;
    QTest::newRow("jum(p)ed|characters")
            << standard[0] << 23 << 24 << QQuickTextEdit::SelectCharacters << 23 << 24 << true;
    QTest::newRow("jumped( )over|characters")
            << standard[0] << 26 << 27 << QQuickTextEdit::SelectCharacters << 26 << 27 << true;
    QTest::newRow("(the )|characters")
            << standard[0] << 0 << 4 << QQuickTextEdit::SelectCharacters << 0 << 4 << true;
    QTest::newRow("( dog)|characters")
            << standard[0] << 40 << 44 << QQuickTextEdit::SelectCharacters << 40 << 44 << true;
    QTest::newRow("( jumped )|characters")
            << standard[0] << 19 << 27 << QQuickTextEdit::SelectCharacters << 19 << 27 << true;
    QTest::newRow("th(e qu)ick|characters")
            << standard[0] << 2 << 6 << QQuickTextEdit::SelectCharacters << 2 << 6 << true;
    QTest::newRow("la(zy d)og|characters")
            << standard[0] << 38 << 42 << QQuickTextEdit::SelectCharacters << 38 << 42 << true;
    QTest::newRow("jum(ped ov)er|characters")
            << standard[0] << 23 << 29 << QQuickTextEdit::SelectCharacters << 23 << 29 << true;
    QTest::newRow("()the|characters")
            << standard[0] << 0 << 0 << QQuickTextEdit::SelectCharacters << 0 << 0 << true;
    QTest::newRow("dog()|characters")
            << standard[0] << 44 << 44 << QQuickTextEdit::SelectCharacters << 44 << 44 << true;
    QTest::newRow("jum()ped|characters")
            << standard[0] << 23 << 23 << QQuickTextEdit::SelectCharacters << 23 << 23 << true;

    QTest::newRow("<(t)he>|words")
            << standard[0] << 0 << 1 << QQuickTextEdit::SelectWords << 0 << 3 << true;
    QTest::newRow("<do(g)>|words")
            << standard[0] << 43 << 44 << QQuickTextEdit::SelectWords << 41 << 44 << true;
    QTest::newRow("<jum(p)ed>|words")
            << standard[0] << 23 << 24 << QQuickTextEdit::SelectWords << 20 << 26 << true;
    QTest::newRow("<jumped( )>over|words")
            << standard[0] << 26 << 27 << QQuickTextEdit::SelectWords << 20 << 27 << false;
    QTest::newRow("jumped<( )over>|words,reversed")
            << standard[0] << 27 << 26 << QQuickTextEdit::SelectWords << 26 << 31 << false;
    QTest::newRow("<(the )>quick|words")
            << standard[0] << 0 << 4 << QQuickTextEdit::SelectWords << 0 << 4 << false;
    QTest::newRow("<(the )quick>|words,reversed")
            << standard[0] << 4 << 0 << QQuickTextEdit::SelectWords << 0 << 9 << false;
    QTest::newRow("<lazy( dog)>|words")
            << standard[0] << 40 << 44 << QQuickTextEdit::SelectWords << 36 << 44 << false;
    QTest::newRow("lazy<( dog)>|words,reversed")
            << standard[0] << 44 << 40 << QQuickTextEdit::SelectWords << 40 << 44 << false;
    QTest::newRow("<fox( jumped )>over|words")
            << standard[0] << 19 << 27 << QQuickTextEdit::SelectWords << 16 << 27 << false;
    QTest::newRow("fox<( jumped )over>|words,reversed")
            << standard[0] << 27 << 19 << QQuickTextEdit::SelectWords << 19 << 31 << false;
    QTest::newRow("<th(e qu)ick>|words")
            << standard[0] << 2 << 6 << QQuickTextEdit::SelectWords << 0 << 9 << true;
    QTest::newRow("<la(zy d)og|words>")
            << standard[0] << 38 << 42 << QQuickTextEdit::SelectWords << 36 << 44 << true;
    QTest::newRow("<jum(ped ov)er>|words")
            << standard[0] << 23 << 29 << QQuickTextEdit::SelectWords << 20 << 31 << true;
    QTest::newRow("<()>the|words")
            << standard[0] << 0 << 0 << QQuickTextEdit::SelectWords << 0 << 0 << true;
    QTest::newRow("dog<()>|words")
            << standard[0] << 44 << 44 << QQuickTextEdit::SelectWords << 44 << 44 << true;
    QTest::newRow("jum<()>ped|words")
            << standard[0] << 23 << 23 << QQuickTextEdit::SelectWords << 23 << 23 << true;

    QTest::newRow("Hello<(,)> |words")
            << standard[2] << 5 << 6 << QQuickTextEdit::SelectWords << 5 << 6 << true;
    QTest::newRow("Hello<(, )>world|words")
            << standard[2] << 5 << 7 << QQuickTextEdit::SelectWords << 5 << 7 << false;
    QTest::newRow("Hello<(, )world>|words,reversed")
            << standard[2] << 7 << 5 << QQuickTextEdit::SelectWords << 5 << 12 << false;
    QTest::newRow("<Hel(lo, )>world|words")
            << standard[2] << 3 << 7 << QQuickTextEdit::SelectWords << 0 << 7 << false;
    QTest::newRow("<Hel(lo, )world>|words,reversed")
            << standard[2] << 7 << 3 << QQuickTextEdit::SelectWords << 0 << 12 << false;
    QTest::newRow("<Hel(lo)>,|words")
            << standard[2] << 3 << 5 << QQuickTextEdit::SelectWords << 0 << 5 << true;
    QTest::newRow("Hello<()>,|words")
            << standard[2] << 5 << 5 << QQuickTextEdit::SelectWords << 5 << 5 << true;
    QTest::newRow("Hello,<()>|words")
            << standard[2] << 6 << 6 << QQuickTextEdit::SelectWords << 6 << 6 << true;
    QTest::newRow("Hello<,( )>world|words")
            << standard[2] << 6 << 7 << QQuickTextEdit::SelectWords << 5 << 7 << false;
    QTest::newRow("Hello,<( )world>|words,reversed")
            << standard[2] << 7 << 6 << QQuickTextEdit::SelectWords << 6 << 12 << false;
    QTest::newRow("Hello<,( world)>|words")
            << standard[2] << 6 << 12 << QQuickTextEdit::SelectWords << 5 << 12 << false;
    QTest::newRow("Hello,<( world)>|words,reversed")
            << standard[2] << 12 << 6 << QQuickTextEdit::SelectWords << 6 << 12 << false;
    QTest::newRow("Hello<,( world!)>|words")
            << standard[2] << 6 << 13 << QQuickTextEdit::SelectWords << 5 << 13 << false;
    QTest::newRow("Hello,<( world!)>|words,reversed")
            << standard[2] << 13 << 6 << QQuickTextEdit::SelectWords << 6 << 13 << false;
    QTest::newRow("Hello<(, world!)>|words")
            << standard[2] << 5 << 13 << QQuickTextEdit::SelectWords << 5 << 13 << true;
    QTest::newRow("world<(!)>|words")
            << standard[2] << 12 << 13 << QQuickTextEdit::SelectWords << 12 << 13 << true;
    QTest::newRow("world!<()>)|words")
            << standard[2] << 13 << 13 << QQuickTextEdit::SelectWords << 13 << 13 << true;
    QTest::newRow("world<()>!)|words")
            << standard[2] << 12 << 12 << QQuickTextEdit::SelectWords << 12 << 12 << true;

    QTest::newRow("<(,)>olleH |words")
            << standard[3] << 7 << 8 << QQuickTextEdit::SelectWords << 7 << 8 << true;
    QTest::newRow("<dlrow( ,)>olleH|words")
            << standard[3] << 6 << 8 << QQuickTextEdit::SelectWords << 1 << 8 << false;
    QTest::newRow("dlrow<( ,)>olleH|words,reversed")
            << standard[3] << 8 << 6 << QQuickTextEdit::SelectWords << 6 << 8 << false;
    QTest::newRow("<dlrow( ,ol)leH>|words")
            << standard[3] << 6 << 10 << QQuickTextEdit::SelectWords << 1 << 13 << false;
    QTest::newRow("dlrow<( ,ol)leH>|words,reversed")
            << standard[3] << 10 << 6 << QQuickTextEdit::SelectWords << 6 << 13 << false;
    QTest::newRow(",<(ol)leH>,|words")
            << standard[3] << 8 << 10 << QQuickTextEdit::SelectWords << 8 << 13 << true;
    QTest::newRow(",<()>olleH|words")
            << standard[3] << 8 << 8 << QQuickTextEdit::SelectWords << 8 << 8 << true;
    QTest::newRow("<()>,olleH|words")
            << standard[3] << 7 << 7 << QQuickTextEdit::SelectWords << 7 << 7 << true;
    QTest::newRow("<dlrow( )>,olleH|words")
            << standard[3] << 6 << 7 << QQuickTextEdit::SelectWords << 1 << 7 << false;
    QTest::newRow("dlrow<( ),>olleH|words,reversed")
            << standard[3] << 7 << 6 << QQuickTextEdit::SelectWords << 6 << 8 << false;
    QTest::newRow("<(dlrow )>,olleH|words")
            << standard[3] << 1 << 7 << QQuickTextEdit::SelectWords << 1 << 7 << false;
    QTest::newRow("<(dlrow ),>olleH|words,reversed")
            << standard[3] << 7 << 1 << QQuickTextEdit::SelectWords << 1 << 8 << false;
    QTest::newRow("<(!dlrow )>,olleH|words")
            << standard[3] << 0 << 7 << QQuickTextEdit::SelectWords << 0 << 7 << false;
    QTest::newRow("<(!dlrow ),>olleH|words,reversed")
            << standard[3] << 7 << 0 << QQuickTextEdit::SelectWords << 0 << 8 << false;
    QTest::newRow("(!dlrow ,)olleH|words")
            << standard[3] << 0 << 8 << QQuickTextEdit::SelectWords << 0 << 8 << true;
    QTest::newRow("<(!)>dlrow|words")
            << standard[3] << 0 << 1 << QQuickTextEdit::SelectWords << 0 << 1 << true;
    QTest::newRow("<()>!dlrow|words")
            << standard[3] << 0 << 0 << QQuickTextEdit::SelectWords << 0 << 0 << true;
    QTest::newRow("!<()>dlrow|words")
            << standard[3] << 1 << 1 << QQuickTextEdit::SelectWords << 1 << 1 << true;
}

void tst_qquicktextedit::moveCursorSelection()
{
    QFETCH(QString, testStr);
    QFETCH(int, cursorPosition);
    QFETCH(int, movePosition);
    QFETCH(QQuickTextEdit::SelectionMode, mode);
    QFETCH(int, selectionStart);
    QFETCH(int, selectionEnd);
    QFETCH(bool, reversible);

    QString componentStr = "import QtQuick 2.0\nTextEdit {  text: \""+ testStr +"\"; }";
    QQmlComponent textinputComponent(&engine);
    textinputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *texteditObject = qobject_cast<QQuickTextEdit*>(textinputComponent.create());
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

void tst_qquicktextedit::moveCursorSelectionSequence_data()
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

void tst_qquicktextedit::moveCursorSelectionSequence()
{
    QFETCH(QString, testStr);
    QFETCH(int, cursorPosition);
    QFETCH(int, movePosition1);
    QFETCH(int, movePosition2);
    QFETCH(int, selection1Start);
    QFETCH(int, selection1End);
    QFETCH(int, selection2Start);
    QFETCH(int, selection2End);

    QString componentStr = "import QtQuick 2.0\nTextEdit {  text: \""+ testStr +"\"; }";
    QQmlComponent texteditComponent(&engine);
    texteditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *texteditObject = qobject_cast<QQuickTextEdit*>(texteditComponent.create());
    QVERIFY(texteditObject != 0);

    texteditObject->setCursorPosition(cursorPosition);

    texteditObject->moveCursorSelection(movePosition1, QQuickTextEdit::SelectWords);
    QCOMPARE(texteditObject->selectedText(), testStr.mid(selection1Start, selection1End - selection1Start));
    QCOMPARE(texteditObject->selectionStart(), selection1Start);
    QCOMPARE(texteditObject->selectionEnd(), selection1End);

    texteditObject->moveCursorSelection(movePosition2, QQuickTextEdit::SelectWords);
    QCOMPARE(texteditObject->selectedText(), testStr.mid(selection2Start, selection2End - selection2Start));
    QCOMPARE(texteditObject->selectionStart(), selection2Start);
    QCOMPARE(texteditObject->selectionEnd(), selection2End);
}


void tst_qquicktextedit::mouseSelection_data()
{
    QTest::addColumn<QString>("qmlfile");
    QTest::addColumn<int>("from");
    QTest::addColumn<int>("to");
    QTest::addColumn<QString>("selectedText");
    QTest::addColumn<bool>("focus");
    QTest::addColumn<bool>("focusOnPress");
    QTest::addColumn<int>("clicks");

    // import installed
    QTest::newRow("on") << testFile("mouseselection_true.qml") << 4 << 9 << "45678" << true << true << 1;
    QTest::newRow("off") << testFile("mouseselection_false.qml") << 4 << 9 << QString() << true << true << 1;
    QTest::newRow("default") << testFile("mouseselection_default.qml") << 4 << 9 << QString() << true << true << 1;
    QTest::newRow("off word selection") << testFile("mouseselection_false_words.qml") << 4 << 9 << QString() << true << true << 1;
    QTest::newRow("on word selection (4,9)") << testFile("mouseselection_true_words.qml") << 4 << 9 << "0123456789" << true << true << 1;

    QTest::newRow("on unfocused") << testFile("mouseselection_true.qml") << 4 << 9 << "45678" << false << false << 1;
    QTest::newRow("on word selection (4,9) unfocused") << testFile("mouseselection_true_words.qml") << 4 << 9 << "0123456789" << false << false << 1;

    QTest::newRow("on focus on press") << testFile("mouseselection_true.qml") << 4 << 9 << "45678" << false << true << 1;
    QTest::newRow("on word selection (4,9) focus on press") << testFile("mouseselection_true_words.qml") << 4 << 9 << "0123456789" << false << true << 1;

    QTest::newRow("on word selection (2,13)") << testFile("mouseselection_true_words.qml") << 2 << 13 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 1;
    QTest::newRow("on word selection (2,30)") << testFile("mouseselection_true_words.qml") << 2 << 30 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 1;
    QTest::newRow("on word selection (9,13)") << testFile("mouseselection_true_words.qml") << 9 << 13 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 1;
    QTest::newRow("on word selection (9,30)") << testFile("mouseselection_true_words.qml") << 9 << 30 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 1;
    QTest::newRow("on word selection (13,2)") << testFile("mouseselection_true_words.qml") << 13 << 2 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 1;
    QTest::newRow("on word selection (20,2)") << testFile("mouseselection_true_words.qml") << 20 << 2 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 1;
    QTest::newRow("on word selection (12,9)") << testFile("mouseselection_true_words.qml") << 12 << 9 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 1;
    QTest::newRow("on word selection (30,9)") << testFile("mouseselection_true_words.qml") << 30 << 9 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 1;

    QTest::newRow("on double click (4,9)") << testFile("mouseselection_true.qml") << 4 << 9 << "0123456789" << true << true << 2;
    QTest::newRow("on double click (2,13)") << testFile("mouseselection_true.qml") << 2 << 13 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 2;
    QTest::newRow("on double click (2,30)") << testFile("mouseselection_true.qml") << 2 << 30 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 2;
    QTest::newRow("on double click (9,13)") << testFile("mouseselection_true.qml") << 9 << 13 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 2;
    QTest::newRow("on double click (9,30)") << testFile("mouseselection_true.qml") << 9 << 30 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 2;
    QTest::newRow("on double click (13,2)") << testFile("mouseselection_true.qml") << 13 << 2 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 2;
    QTest::newRow("on double click (20,2)") << testFile("mouseselection_true.qml") << 20 << 2 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 2;
    QTest::newRow("on double click (12,9)") << testFile("mouseselection_true.qml") << 12 << 9 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 2;
    QTest::newRow("on double click (30,9)") << testFile("mouseselection_true.qml") << 30 << 9 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ" << true << true << 2;

    QTest::newRow("on triple click (4,9)") << testFile("mouseselection_true.qml") << 4 << 9 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n" << true << true << 3;
    QTest::newRow("on triple click (2,13)") << testFile("mouseselection_true.qml") << 2 << 13 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n" << true << true << 3;
    QTest::newRow("on triple click (2,30)") << testFile("mouseselection_true.qml") << 2 << 30 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n" << true << true << 3;
    QTest::newRow("on triple click (9,13)") << testFile("mouseselection_true.qml") << 9 << 13 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n" << true << true << 3;
    QTest::newRow("on triple click (9,30)") << testFile("mouseselection_true.qml") << 9 << 30 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n" << true << true << 3;
    QTest::newRow("on triple click (13,2)") << testFile("mouseselection_true.qml") << 13 << 2 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n" << true << true << 3;
    QTest::newRow("on triple click (20,2)") << testFile("mouseselection_true.qml") << 20 << 2 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n" << true << true << 3;
    QTest::newRow("on triple click (12,9)") << testFile("mouseselection_true.qml") << 12 << 9 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n" << true << true << 3;
    QTest::newRow("on triple click (30,9)") << testFile("mouseselection_true.qml") << 30 << 9 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n" << true << true << 3;

    QTest::newRow("on triple click (2,40)") << testFile("mouseselection_true.qml") << 2 << 40 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n9876543210\n" << true << true << 3;
    QTest::newRow("on triple click (2,50)") << testFile("mouseselection_true.qml") << 2 << 50 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n9876543210\n\nZXYWVUTSRQPON MLKJIHGFEDCBA" << true << true << 3;
    QTest::newRow("on triple click (25,40)") << testFile("mouseselection_true.qml") << 25 << 40 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n9876543210\n" << true << true << 3;
    QTest::newRow("on triple click (25,50)") << testFile("mouseselection_true.qml") << 25 << 50 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n9876543210\n\nZXYWVUTSRQPON MLKJIHGFEDCBA" << true << true << 3;
    QTest::newRow("on triple click (40,25)") << testFile("mouseselection_true.qml") << 40 << 25 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n9876543210\n" << true << true << 3;
    QTest::newRow("on triple click (40,50)") << testFile("mouseselection_true.qml") << 40 << 50 << "9876543210\n\nZXYWVUTSRQPON MLKJIHGFEDCBA" << true << true << 3;
    QTest::newRow("on triple click (50,25)") << testFile("mouseselection_true.qml") << 50 << 25 << "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n9876543210\n\nZXYWVUTSRQPON MLKJIHGFEDCBA" << true << true << 3;
    QTest::newRow("on triple click (50,40)") << testFile("mouseselection_true.qml") << 50 << 40 << "9876543210\n\nZXYWVUTSRQPON MLKJIHGFEDCBA" << true << true << 3;

    QTest::newRow("on tr align") << testFile("mouseselection_align_tr.qml") << 4 << 9 << "45678" << true << true << 1;
    QTest::newRow("on center align") << testFile("mouseselection_align_center.qml") << 4 << 9 << "45678" << true << true << 1;
    QTest::newRow("on bl align") << testFile("mouseselection_align_bl.qml") << 4 << 9 << "45678" << true << true << 1;
}

void tst_qquicktextedit::mouseSelection()
{
    QFETCH(QString, qmlfile);
    QFETCH(int, from);
    QFETCH(int, to);
    QFETCH(QString, selectedText);
    QFETCH(bool, focus);
    QFETCH(bool, focusOnPress);
    QFETCH(int, clicks);

    QQuickView window(QUrl::fromLocalFile(qmlfile));

    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);
    QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit *>(window.rootObject());
    QVERIFY(textEditObject != 0);

    textEditObject->setFocus(focus);
    textEditObject->setFocusOnPress(focusOnPress);

    // press-and-drag-and-release from x1 to x2
    QPoint p1 = textEditObject->positionToRectangle(from).center().toPoint();
    QPoint p2 = textEditObject->positionToRectangle(to).center().toPoint();
    if (clicks == 2)
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, p1);
    else if (clicks == 3)
        QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, p1);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, p1);
    if (clicks == 2) {
        // QTBUG-50022: Since qtbase commit beef975, QTestLib avoids generating
        // double click events by adding 500ms delta to release event timestamps.
        // Send a double click event by hand to ensure the correct sequence:
        // press, release, press, _dbl click_, move, release.
        QMouseEvent dblClickEvent(QEvent::MouseButtonDblClick, p1, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QGuiApplication::sendEvent(textEditObject, &dblClickEvent);
    }
    QTest::mouseMove(&window, p2);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, p2);
    QTRY_COMPARE(textEditObject->selectedText(), selectedText);

    // Clicking and shift to clicking between the same points should select the same text.
    textEditObject->setCursorPosition(0);
    if (clicks > 1)
        QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, p1);
    if (clicks != 2)
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, p1);
    QTest::mouseClick(&window, Qt::LeftButton, Qt::ShiftModifier, p2);
    QTRY_COMPARE(textEditObject->selectedText(), selectedText);
}

void tst_qquicktextedit::dragMouseSelection()
{
    QString qmlfile = testFile("mouseselection_true.qml");

    QQuickView window(QUrl::fromLocalFile(qmlfile));

    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);
    QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit *>(window.rootObject());
    QVERIFY(textEditObject != 0);

    // press-and-drag-and-release from x1 to x2
    int x1 = 10;
    int x2 = 70;
    int y = QFontMetrics(textEditObject->font()).height() / 2;
    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(x1,y));
    QTest::mouseMove(&window, QPoint(x2, y));
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(x2,y));
    QTest::qWait(300);
    QString str1;
    QTRY_VERIFY((str1 = textEditObject->selectedText()).length() > 3);

    // press and drag the current selection.
    x1 = 40;
    x2 = 100;
    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(x1,y));
    QTest::mouseMove(&window, QPoint(x2, y));
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(x2,y));
    QTest::qWait(300);
    QString str2;
    QTRY_VERIFY((str2 = textEditObject->selectedText()).length() > 3);

    QVERIFY(str1 != str2); // Verify the second press and drag is a new selection and not the first moved.

}

void tst_qquicktextedit::mouseSelectionMode_data()
{
    QTest::addColumn<QString>("qmlfile");
    QTest::addColumn<bool>("selectWords");

    // import installed
    QTest::newRow("SelectWords") << testFile("mouseselectionmode_words.qml") << true;
    QTest::newRow("SelectCharacters") << testFile("mouseselectionmode_characters.qml") << false;
    QTest::newRow("default") << testFile("mouseselectionmode_default.qml") << false;
}

void tst_qquicktextedit::mouseSelectionMode()
{
    QFETCH(QString, qmlfile);
    QFETCH(bool, selectWords);

    QString text = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    QQuickView window(QUrl::fromLocalFile(qmlfile));

    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);
    QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit *>(window.rootObject());
    QVERIFY(textEditObject != 0);

    // press-and-drag-and-release from x1 to x2
    int x1 = 10;
    int x2 = 70;
    int y = textEditObject->height()/2;
    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(x1,y));
    QTest::mouseMove(&window, QPoint(x2, y));
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(x2,y));
    QString str = textEditObject->selectedText();
    if (selectWords) {
        QTRY_COMPARE(textEditObject->selectedText(), text);
    } else {
        QTRY_VERIFY(textEditObject->selectedText().length() > 3);
        QVERIFY(str != text);
    }
}

void tst_qquicktextedit::mouseSelectionMode_accessors()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n TextEdit {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(object.data());
    QVERIFY(edit);

    QSignalSpy spy(edit, &QQuickTextEdit::mouseSelectionModeChanged);

    QCOMPARE(edit->mouseSelectionMode(), QQuickTextEdit::SelectCharacters);

    edit->setMouseSelectionMode(QQuickTextEdit::SelectWords);
    QCOMPARE(edit->mouseSelectionMode(), QQuickTextEdit::SelectWords);
    QCOMPARE(spy.count(), 1);

    edit->setMouseSelectionMode(QQuickTextEdit::SelectWords);
    QCOMPARE(spy.count(), 1);

    edit->setMouseSelectionMode(QQuickTextEdit::SelectCharacters);
    QCOMPARE(edit->mouseSelectionMode(), QQuickTextEdit::SelectCharacters);
    QCOMPARE(spy.count(), 2);
}

void tst_qquicktextedit::selectByMouse()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n TextEdit {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(object.data());
    QVERIFY(edit);

    QSignalSpy spy(edit, SIGNAL(selectByMouseChanged(bool)));

    QCOMPARE(edit->selectByMouse(), false);

    edit->setSelectByMouse(true);
    QCOMPARE(edit->selectByMouse(), true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);

    edit->setSelectByMouse(true);
    QCOMPARE(spy.count(), 1);

    edit->setSelectByMouse(false);
    QCOMPARE(edit->selectByMouse(), false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

void tst_qquicktextedit::selectByKeyboard()
{
    QQmlComponent oldComponent(&engine);
    oldComponent.setData("import QtQuick 2.0\n TextEdit { selectByKeyboard: true }", QUrl());
    QVERIFY(!oldComponent.create());

    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.1\n TextEdit { }", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(object.data());
    QVERIFY(edit);

    QSignalSpy spy(edit, SIGNAL(selectByKeyboardChanged(bool)));

    QCOMPARE(edit->isReadOnly(), false);
    QCOMPARE(edit->selectByKeyboard(), true);

    edit->setReadOnly(true);
    QCOMPARE(edit->selectByKeyboard(), false);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);

    edit->setSelectByKeyboard(true);
    QCOMPARE(edit->selectByKeyboard(), true);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), true);

    edit->setReadOnly(false);
    QCOMPARE(edit->selectByKeyboard(), true);
    QCOMPARE(spy.count(), 2);

    edit->setSelectByKeyboard(false);
    QCOMPARE(edit->selectByKeyboard(), false);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(2).at(0).toBool(), false);
}

Q_DECLARE_METATYPE(QKeySequence::StandardKey)

void tst_qquicktextedit::keyboardSelection_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("readOnly");
    QTest::addColumn<bool>("selectByKeyboard");
    QTest::addColumn<int>("cursorPosition");
    QTest::addColumn<QKeySequence::StandardKey>("standardKey");
    QTest::addColumn<QString>("selectedText");

    QTest::newRow("editable - select first char")
            << QStringLiteral("editable - select first char") << false << true << 0 << QKeySequence::SelectNextChar << QStringLiteral("e");
    QTest::newRow("editable - select first word")
            << QStringLiteral("editable - select first char") << false << true << 0 << QKeySequence::SelectNextWord << QStringLiteral("editable ");

    QTest::newRow("editable - cannot select first char")
            << QStringLiteral("editable - cannot select first char") << false << false << 0 << QKeySequence::SelectNextChar << QStringLiteral("");
    QTest::newRow("editable - cannot select first word")
            << QStringLiteral("editable - cannot select first word") << false << false << 0 << QKeySequence::SelectNextWord << QStringLiteral("");

    QTest::newRow("editable - select last char")
            << QStringLiteral("editable - select last char") << false << true << 27 << QKeySequence::SelectPreviousChar << QStringLiteral("r");
    QTest::newRow("editable - select last word")
            << QStringLiteral("editable - select last word") << false << true << 27 << QKeySequence::SelectPreviousWord << QStringLiteral("word");

    QTest::newRow("editable - cannot select last char")
            << QStringLiteral("editable - cannot select last char") << false << false << 35 << QKeySequence::SelectPreviousChar << QStringLiteral("");
    QTest::newRow("editable - cannot select last word")
            << QStringLiteral("editable - cannot select last word") << false << false << 35 << QKeySequence::SelectPreviousWord << QStringLiteral("");

    QTest::newRow("read-only - cannot select first char")
            << QStringLiteral("read-only - cannot select first char") << true << false << 0 << QKeySequence::SelectNextChar << QStringLiteral("");
    QTest::newRow("read-only - cannot select first word")
            << QStringLiteral("read-only - cannot select first word") << true << false << 0 << QKeySequence::SelectNextWord << QStringLiteral("");

    QTest::newRow("read-only - cannot select last char")
            << QStringLiteral("read-only - cannot select last char") << true << false << 35 << QKeySequence::SelectPreviousChar << QStringLiteral("");
    QTest::newRow("read-only - cannot select last word")
            << QStringLiteral("read-only - cannot select last word") << true << false << 35 << QKeySequence::SelectPreviousWord << QStringLiteral("");

    QTest::newRow("read-only - select first char")
            << QStringLiteral("read-only - select first char") << true << true << 0 << QKeySequence::SelectNextChar << QStringLiteral("r");
    QTest::newRow("read-only - select first word")
            << QStringLiteral("read-only - select first word") << true << true << 0 << QKeySequence::SelectNextWord << QStringLiteral("read");

    QTest::newRow("read-only - select last char")
            << QStringLiteral("read-only - select last char") << true << true << 28 << QKeySequence::SelectPreviousChar << QStringLiteral("r");
    QTest::newRow("read-only - select last word")
            << QStringLiteral("read-only - select last word") << true << true << 28 << QKeySequence::SelectPreviousWord << QStringLiteral("word");
}

void tst_qquicktextedit::keyboardSelection()
{
    QFETCH(QString, text);
    QFETCH(bool, readOnly);
    QFETCH(bool, selectByKeyboard);
    QFETCH(int, cursorPosition);
    QFETCH(QKeySequence::StandardKey, standardKey);
    QFETCH(QString, selectedText);

    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.1\n TextEdit { focus: true }", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(object.data());
    QVERIFY(edit);

    edit->setText(text);
    edit->setSelectByKeyboard(selectByKeyboard);
    edit->setReadOnly(readOnly);
    edit->setCursorPosition(cursorPosition);

    QQuickWindow window;
    edit->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(edit->hasActiveFocus());

    simulateKeys(&window, standardKey);

    QCOMPARE(edit->selectedText(), selectedText);
}

void tst_qquicktextedit::renderType()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n TextEdit {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(object.data());
    QVERIFY(edit);

    QSignalSpy spy(edit, SIGNAL(renderTypeChanged()));

    QCOMPARE(edit->renderType(), QQuickTextEdit::QtRendering);

    edit->setRenderType(QQuickTextEdit::NativeRendering);
    QCOMPARE(edit->renderType(), QQuickTextEdit::NativeRendering);
    QCOMPARE(spy.count(), 1);

    edit->setRenderType(QQuickTextEdit::NativeRendering);
    QCOMPARE(spy.count(), 1);

    edit->setRenderType(QQuickTextEdit::QtRendering);
    QCOMPARE(edit->renderType(), QQuickTextEdit::QtRendering);
    QCOMPARE(spy.count(), 2);
}

void tst_qquicktextedit::inputMethodHints()
{
    QQuickView window(testFileUrl("inputmethodhints.qml"));
    window.show();
    window.requestActivate();

    QVERIFY(window.rootObject() != 0);
    QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit *>(window.rootObject());
    QVERIFY(textEditObject != 0);
    QVERIFY(textEditObject->inputMethodHints() & Qt::ImhNoPredictiveText);
    QSignalSpy inputMethodHintSpy(textEditObject, SIGNAL(inputMethodHintsChanged()));
    textEditObject->setInputMethodHints(Qt::ImhUppercaseOnly);
    QVERIFY(textEditObject->inputMethodHints() & Qt::ImhUppercaseOnly);
    QCOMPARE(inputMethodHintSpy.count(), 1);
    textEditObject->setInputMethodHints(Qt::ImhUppercaseOnly);
    QCOMPARE(inputMethodHintSpy.count(), 1);

    QQuickTextEdit plainTextEdit;
    QCOMPARE(plainTextEdit.inputMethodHints(), Qt::ImhNone);
}

void tst_qquicktextedit::positionAt_data()
{
    QTest::addColumn<QQuickTextEdit::HAlignment>("horizontalAlignment");
    QTest::addColumn<QQuickTextEdit::VAlignment>("verticalAlignment");

    QTest::newRow("top-left") << QQuickTextEdit::AlignLeft << QQuickTextEdit::AlignTop;
    QTest::newRow("bottom-left") << QQuickTextEdit::AlignLeft << QQuickTextEdit::AlignBottom;
    QTest::newRow("center-left") << QQuickTextEdit::AlignLeft << QQuickTextEdit::AlignVCenter;

    QTest::newRow("top-right") << QQuickTextEdit::AlignRight << QQuickTextEdit::AlignTop;
    QTest::newRow("top-center") << QQuickTextEdit::AlignHCenter << QQuickTextEdit::AlignTop;

    QTest::newRow("center") << QQuickTextEdit::AlignHCenter << QQuickTextEdit::AlignVCenter;
}

void tst_qquicktextedit::positionAt()
{
    QFETCH(QQuickTextEdit::HAlignment, horizontalAlignment);
    QFETCH(QQuickTextEdit::VAlignment, verticalAlignment);

    QQuickView window(testFileUrl("positionAt.qml"));
    QVERIFY(window.rootObject() != 0);
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QQuickTextEdit *texteditObject = qobject_cast<QQuickTextEdit *>(window.rootObject());
    QVERIFY(texteditObject != 0);
    texteditObject->setHAlign(horizontalAlignment);
    texteditObject->setVAlign(verticalAlignment);

    QTextLayout layout(texteditObject->text().replace(QLatin1Char('\n'), QChar::LineSeparator));
    layout.setFont(texteditObject->font());

    if (!qmlDisableDistanceField()) {
        QTextOption option;
        option.setUseDesignMetrics(true);
        layout.setTextOption(option);
    }

    layout.beginLayout();
    QTextLine line = layout.createLine();
    line.setLineWidth(texteditObject->width());
    QTextLine secondLine = layout.createLine();
    secondLine.setLineWidth(texteditObject->width());
    layout.endLayout();

    qreal y0 = 0;
    qreal y1 = 0;

    switch (verticalAlignment) {
    case QQuickTextEdit::AlignTop:
        y0 = line.height() / 2;
        y1 = line.height() * 3 / 2;
        break;
    case QQuickTextEdit::AlignVCenter:
        y0 = (texteditObject->height() - line.height()) / 2;
        y1 = (texteditObject->height() + line.height()) / 2;
        break;
    case QQuickTextEdit::AlignBottom:
        y0 = texteditObject->height() - line.height() * 3 / 2;
        y1 = texteditObject->height() - line.height() / 2;
        break;
    }

    qreal xoff = 0;
    switch (horizontalAlignment) {
    case QQuickTextEdit::AlignLeft:
        break;
    case QQuickTextEdit::AlignHCenter:
        xoff = (texteditObject->width() - secondLine.naturalTextWidth()) / 2;
        break;
    case QQuickTextEdit::AlignRight:
        xoff = texteditObject->width() - secondLine.naturalTextWidth();
        break;
    default:
        break;
    }
    int pos = texteditObject->positionAt(texteditObject->width()/2, y0);

    int widthBegin = floor(xoff + line.cursorToX(pos - 1));
    int widthEnd = ceil(xoff + line.cursorToX(pos + 1));

    QVERIFY(widthBegin <= texteditObject->width() / 2);
    QVERIFY(widthEnd >= texteditObject->width() / 2);

    const qreal x0 = texteditObject->positionToRectangle(pos).x();
    const qreal x1 = texteditObject->positionToRectangle(pos + 1).x();

    QString preeditText = texteditObject->text().mid(0, pos);
    texteditObject->setText(texteditObject->text().mid(pos));
    texteditObject->setCursorPosition(0);

    QInputMethodEvent inputEvent(preeditText, QList<QInputMethodEvent::Attribute>());
    QGuiApplication::sendEvent(texteditObject, &inputEvent);

    // Check all points within the preedit text return the same position.
    QCOMPARE(texteditObject->positionAt(0, y0), 0);
    QCOMPARE(texteditObject->positionAt(x0 / 2, y0), 0);
    QCOMPARE(texteditObject->positionAt(x0, y0), 0);

    // Verify positioning returns to normal after the preedit text.
    QCOMPARE(texteditObject->positionAt(x1, y0), 1);
    QCOMPARE(texteditObject->positionToRectangle(1).x(), x1);

    QVERIFY(texteditObject->positionAt(x0 / 2, y1) > 0);
}

void tst_qquicktextedit::linkInteraction()
{
    QQuickView window(testFileUrl("linkInteraction.qml"));
    QVERIFY(window.rootObject() != 0);
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QQuickTextEdit *texteditObject = qobject_cast<QQuickTextEdit *>(window.rootObject());
    QVERIFY(texteditObject != 0);

    QSignalSpy spy(texteditObject, SIGNAL(linkActivated(QString)));
    QSignalSpy hover(texteditObject, SIGNAL(linkHovered(QString)));

    const QString link("http://example.com/");

    const QPointF linkPos = texteditObject->positionToRectangle(7).center();
    const QPointF textPos = texteditObject->positionToRectangle(2).center();

    QTest::mouseClick(&window, Qt::LeftButton, 0, linkPos.toPoint());
    QTRY_COMPARE(spy.count(), 1);
    QTRY_COMPARE(hover.count(), 1);
    QCOMPARE(spy.last()[0].toString(), link);
    QCOMPARE(hover.last()[0].toString(), link);
    QCOMPARE(texteditObject->hoveredLink(), link);
    QCOMPARE(texteditObject->linkAt(linkPos.x(), linkPos.y()), link);

    QTest::mouseClick(&window, Qt::LeftButton, 0, textPos.toPoint());
    QTRY_COMPARE(spy.count(), 1);
    QTRY_COMPARE(hover.count(), 2);
    QCOMPARE(hover.last()[0].toString(), QString());
    QCOMPARE(texteditObject->hoveredLink(), QString());
    QCOMPARE(texteditObject->linkAt(textPos.x(), textPos.y()), QString());

    texteditObject->setReadOnly(true);

    QTest::mouseClick(&window, Qt::LeftButton, 0, linkPos.toPoint());
    QTRY_COMPARE(spy.count(), 2);
    QTRY_COMPARE(hover.count(), 3);
    QCOMPARE(spy.last()[0].toString(), link);
    QCOMPARE(hover.last()[0].toString(), link);
    QCOMPARE(texteditObject->hoveredLink(), link);
    QCOMPARE(texteditObject->linkAt(linkPos.x(), linkPos.y()), link);

    QTest::mouseClick(&window, Qt::LeftButton, 0, textPos.toPoint());
    QTRY_COMPARE(spy.count(), 2);
    QTRY_COMPARE(hover.count(), 4);
    QCOMPARE(hover.last()[0].toString(), QString());
    QCOMPARE(texteditObject->hoveredLink(), QString());
    QCOMPARE(texteditObject->linkAt(textPos.x(), textPos.y()), QString());
}

void tst_qquicktextedit::cursorDelegate_data()
{
    QTest::addColumn<QUrl>("source");
    QTest::newRow("out of line") << testFileUrl("cursorTest.qml");
    QTest::newRow("in line") << testFileUrl("cursorTestInline.qml");
    QTest::newRow("external") << testFileUrl("cursorTestExternal.qml");
}

void tst_qquicktextedit::cursorDelegate()
{
    QFETCH(QUrl, source);
    QQuickView view(source);
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QQuickTextEdit *textEditObject = view.rootObject()->findChild<QQuickTextEdit*>("textEditObject");
    QVERIFY(textEditObject != 0);
    // Delegate creation is deferred until focus in or cursor visibility is forced.
    QVERIFY(!textEditObject->findChild<QQuickItem*>("cursorInstance"));
    QVERIFY(!textEditObject->isCursorVisible());
    //Test Delegate gets created
    textEditObject->setFocus(true);
    QVERIFY(textEditObject->isCursorVisible());
    QQuickItem* delegateObject = textEditObject->findChild<QQuickItem*>("cursorInstance");
    QVERIFY(delegateObject);
    QCOMPARE(delegateObject->property("localProperty").toString(), QString("Hello"));
    //Test Delegate gets moved
    for (int i=0; i<= textEditObject->text().length(); i++) {
        textEditObject->setCursorPosition(i);
        QCOMPARE(textEditObject->cursorRectangle().x(), delegateObject->x());
        QCOMPARE(textEditObject->cursorRectangle().y(), delegateObject->y());
    }

    // Test delegate gets moved on mouse press.
    textEditObject->setSelectByMouse(true);
    textEditObject->setCursorPosition(0);
    const QPoint point1 = textEditObject->positionToRectangle(5).center().toPoint();
    QTest::qWait(400);  //ensure this isn't treated as a double-click
    QTest::mouseClick(&view, Qt::LeftButton, 0, point1);
    QTest::qWait(50);
    QTRY_VERIFY(textEditObject->cursorPosition() != 0);
    QCOMPARE(textEditObject->cursorRectangle().x(), delegateObject->x());
    QCOMPARE(textEditObject->cursorRectangle().y(), delegateObject->y());

    // Test delegate gets moved on mouse drag
    textEditObject->setCursorPosition(0);
    const QPoint point2 = textEditObject->positionToRectangle(10).center().toPoint();
    QTest::qWait(400);  //ensure this isn't treated as a double-click
    QTest::mousePress(&view, Qt::LeftButton, 0, point1);
    QMouseEvent mv(QEvent::MouseMove, point2, Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
    QGuiApplication::sendEvent(&view, &mv);
    QTest::mouseRelease(&view, Qt::LeftButton, 0, point2);
    QTest::qWait(50);
    QTRY_COMPARE(textEditObject->cursorRectangle().x(), delegateObject->x());
    QCOMPARE(textEditObject->cursorRectangle().y(), delegateObject->y());

    textEditObject->setReadOnly(true);
    textEditObject->setCursorPosition(0);
    QTest::qWait(400);  //ensure this isn't treated as a double-click
    QTest::mouseClick(&view, Qt::LeftButton, 0, textEditObject->positionToRectangle(5).center().toPoint());
    QTest::qWait(50);
    QTRY_VERIFY(textEditObject->cursorPosition() != 0);
    QCOMPARE(textEditObject->cursorRectangle().x(), delegateObject->x());
    QCOMPARE(textEditObject->cursorRectangle().y(), delegateObject->y());

    textEditObject->setCursorPosition(0);
    QTest::qWait(400);  //ensure this isn't treated as a double-click
    QTest::mouseClick(&view, Qt::LeftButton, 0, textEditObject->positionToRectangle(5).center().toPoint());
    QTest::qWait(50);
    QTRY_VERIFY(textEditObject->cursorPosition() != 0);
    QCOMPARE(textEditObject->cursorRectangle().x(), delegateObject->x());
    QCOMPARE(textEditObject->cursorRectangle().y(), delegateObject->y());

    textEditObject->setCursorPosition(0);
    QCOMPARE(textEditObject->cursorRectangle().x(), delegateObject->x());
    QCOMPARE(textEditObject->cursorRectangle().y(), delegateObject->y());

    textEditObject->setReadOnly(false);

    // Delegate moved when text is entered
    textEditObject->setText(QString());
    for (int i = 0; i < 20; ++i) {
        QTest::keyClick(&view, Qt::Key_A);
        QCOMPARE(textEditObject->cursorRectangle().x(), delegateObject->x());
        QCOMPARE(textEditObject->cursorRectangle().y(), delegateObject->y());
    }

    // Delegate moved when text is entered by im.
    textEditObject->setText(QString());
    for (int i = 0; i < 20; ++i) {
        QInputMethodEvent event;
        event.setCommitString("a");
        QGuiApplication::sendEvent(textEditObject, &event);
        QCOMPARE(textEditObject->cursorRectangle().x(), delegateObject->x());
        QCOMPARE(textEditObject->cursorRectangle().y(), delegateObject->y());
    }
    // Delegate moved when text is removed by im.
    for (int i = 19; i > 1; --i) {
        QInputMethodEvent event;
        event.setCommitString(QString(), -1, 1);
        QGuiApplication::sendEvent(textEditObject, &event);
        QCOMPARE(textEditObject->cursorRectangle().x(), delegateObject->x());
        QCOMPARE(textEditObject->cursorRectangle().y(), delegateObject->y());
    }
    {   // Delegate moved the text is changed in place by im.
        QInputMethodEvent event;
        event.setCommitString("i", -1, 1);
        QGuiApplication::sendEvent(textEditObject, &event);
        QCOMPARE(textEditObject->cursorRectangle().x(), delegateObject->x());
        QCOMPARE(textEditObject->cursorRectangle().y(), delegateObject->y());
    }

    //Test Delegate gets deleted
    textEditObject->setCursorDelegate(0);
    QVERIFY(!textEditObject->findChild<QQuickItem*>("cursorInstance"));
}

void tst_qquicktextedit::remoteCursorDelegate()
{
    ThreadedTestHTTPServer server(dataDirectory(), TestHTTPServer::Delay);

    QQuickView view;

    QQmlComponent component(view.engine(), server.url("/RemoteCursor.qml"));

    view.rootContext()->setContextProperty("contextDelegate", &component);
    view.setSource(testFileUrl("cursorTestRemote.qml"));
    view.showNormal();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QQuickTextEdit *textEditObject = view.rootObject()->findChild<QQuickTextEdit*>("textEditObject");
    QVERIFY(textEditObject != 0);

    // Delegate is created on demand, and so won't be available immediately.  Focus in or
    // setCursorVisible(true) will trigger creation.
    QTRY_VERIFY(!textEditObject->findChild<QQuickItem*>("cursorInstance"));
    QVERIFY(!textEditObject->isCursorVisible());

    textEditObject->setFocus(true);
    QVERIFY(textEditObject->isCursorVisible());

    // Wait for component to load.
    QTRY_COMPARE(component.status(), QQmlComponent::Ready);
    QVERIFY(textEditObject->findChild<QQuickItem*>("cursorInstance"));
}

void tst_qquicktextedit::cursorVisible()
{
    QQuickTextEdit edit;
    edit.componentComplete();
    QSignalSpy spy(&edit, SIGNAL(cursorVisibleChanged(bool)));

    QQuickView view(testFileUrl("cursorVisible.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QCOMPARE(&view, qGuiApp->focusWindow());

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

    edit.setParentItem(view.rootObject());
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 3);

    edit.setFocus(false);
    QCOMPARE(edit.isCursorVisible(), false);
    QCOMPARE(spy.count(), 4);

    edit.setFocus(true);
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 5);

    QWindow alternateView;
    alternateView.show();
    alternateView.requestActivate();
    QTest::qWaitForWindowActive(&alternateView);

    QCOMPARE(edit.isCursorVisible(), false);
    QCOMPARE(spy.count(), 6);

    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 7);

    {   // Cursor attribute with 0 length hides cursor.
        QInputMethodEvent ev(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 0, QVariant()));
        QCoreApplication::sendEvent(&edit, &ev);
    }
    QCOMPARE(edit.isCursorVisible(), false);
    QCOMPARE(spy.count(), 8);

    {   // Cursor attribute with non zero length shows cursor.
        QInputMethodEvent ev(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 1, QVariant()));
        QCoreApplication::sendEvent(&edit, &ev);
    }
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 9);


    {   // If the cursor is hidden by the input method and the text is changed it should be visible again.
        QInputMethodEvent ev(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 0, QVariant()));
        QCoreApplication::sendEvent(&edit, &ev);
    }
    QCOMPARE(edit.isCursorVisible(), false);
    QCOMPARE(spy.count(), 10);

    edit.setText("something");
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 11);

    {   // If the cursor is hidden by the input method and the cursor position is changed it should be visible again.
        QInputMethodEvent ev(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 0, QVariant()));
        QCoreApplication::sendEvent(&edit, &ev);
    }
    QCOMPARE(edit.isCursorVisible(), false);
    QCOMPARE(spy.count(), 12);

    edit.setCursorPosition(5);
    QCOMPARE(edit.isCursorVisible(), true);
    QCOMPARE(spy.count(), 13);
}

void tst_qquicktextedit::delegateLoading_data()
{
    QTest::addColumn<QString>("qmlfile");
    QTest::addColumn<QString>("error");

    // import installed
    QTest::newRow("pass") << "cursorHttpTestPass.qml" << "";
    QTest::newRow("fail1") << "cursorHttpTestFail1.qml" << "{{ServerBaseUrl}}/FailItem.qml: Remote host closed the connection";
    QTest::newRow("fail2") << "cursorHttpTestFail2.qml" << "{{ServerBaseUrl}}/ErrItem.qml:4:5: Fungus is not a type";
}

void tst_qquicktextedit::delegateLoading()
{
    QFETCH(QString, qmlfile);
    QFETCH(QString, error);

    QHash<QString, TestHTTPServer::Mode> dirs;
    dirs[testFile("httpfail")] = TestHTTPServer::Disconnect;
    dirs[testFile("httpslow")] = TestHTTPServer::Delay;
    dirs[testFile("http")] = TestHTTPServer::Normal;
    ThreadedTestHTTPServer server(dirs);

    error.replace(QStringLiteral("{{ServerBaseUrl}}"), server.baseUrl().toString());

    if (!error.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, error.toUtf8());
    QQuickView view(server.url(qmlfile));
    view.show();
    view.requestActivate();

    if (!error.isEmpty()) {
        QTRY_VERIFY(view.status()==QQuickView::Error);
        QTRY_VERIFY(!view.rootObject()); // there is fail item inside this test
    } else {
        QTRY_VERIFY(view.rootObject());//Wait for loading to finish.
        QQuickTextEdit *textEditObject = view.rootObject()->findChild<QQuickTextEdit*>("textEditObject");
        //    view.rootObject()->dumpObjectTree();
        QVERIFY(textEditObject != 0);
        textEditObject->setFocus(true);
        QQuickItem *delegate;
        delegate = view.rootObject()->findChild<QQuickItem*>("delegateOkay");
        QVERIFY(delegate);
        delegate = view.rootObject()->findChild<QQuickItem*>("delegateSlow");
        QVERIFY(delegate);

        delete delegate;
    }


    //A test should be added here with a component which is ready but component.create() returns null
    //Not sure how to accomplish this with QQuickTextEdits cursor delegate
    //###This was only needed for code coverage, and could be a case of overzealous defensive programming
    //delegate = view.rootObject()->findChild<QQuickItem*>("delegateErrorB");
    //QVERIFY(!delegate);
}

void tst_qquicktextedit::cursorDelegateHeight()
{
    QQuickView view(testFileUrl("cursorHeight.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QQuickTextEdit *textEditObject = view.rootObject()->findChild<QQuickTextEdit*>("textEditObject");
    QVERIFY(textEditObject);
    // Delegate creation is deferred until focus in or cursor visibility is forced.
    QVERIFY(!textEditObject->findChild<QQuickItem*>("cursorInstance"));
    QVERIFY(!textEditObject->isCursorVisible());

    // Test that the delegate gets created.
    textEditObject->setFocus(true);
    QVERIFY(textEditObject->isCursorVisible());
    QQuickItem* delegateObject = textEditObject->findChild<QQuickItem*>("cursorInstance");
    QVERIFY(delegateObject);

    const int largerHeight = textEditObject->cursorRectangle().height();

    textEditObject->setCursorPosition(0);
    QCOMPARE(delegateObject->x(), textEditObject->cursorRectangle().x());
    QCOMPARE(delegateObject->y(), textEditObject->cursorRectangle().y());
    QCOMPARE(delegateObject->height(), textEditObject->cursorRectangle().height());

    // Move the cursor to the next line, which has a smaller font.
    textEditObject->setCursorPosition(5);
    QCOMPARE(delegateObject->x(), textEditObject->cursorRectangle().x());
    QCOMPARE(delegateObject->y(), textEditObject->cursorRectangle().y());
    QVERIFY(textEditObject->cursorRectangle().height() < largerHeight);
    QCOMPARE(delegateObject->height(), textEditObject->cursorRectangle().height());

    // Test that the delegate gets deleted
    textEditObject->setCursorDelegate(0);
    QVERIFY(!textEditObject->findChild<QQuickItem*>("cursorInstance"));
}

/*
TextEdit element should only handle left/right keys until the cursor reaches
the extent of the text, then they should ignore the keys.
*/
void tst_qquicktextedit::navigation()
{
    QQuickView window(testFileUrl("navigation.qml"));
    window.show();
    window.requestActivate();

    QVERIFY(window.rootObject() != 0);

    QQuickTextEdit *input = qobject_cast<QQuickTextEdit *>(qvariant_cast<QObject *>(window.rootObject()->property("myInput")));

    QVERIFY(input != 0);
    QTRY_VERIFY(input->hasActiveFocus());
    simulateKey(&window, Qt::Key_Left);
    QVERIFY(!input->hasActiveFocus());
    simulateKey(&window, Qt::Key_Right);
    QVERIFY(input->hasActiveFocus());
    simulateKey(&window, Qt::Key_Right);
    QVERIFY(input->hasActiveFocus());
    simulateKey(&window, Qt::Key_Right);
    QVERIFY(!input->hasActiveFocus());
    simulateKey(&window, Qt::Key_Left);
    QVERIFY(input->hasActiveFocus());

    // Test left and right navigation works if the TextEdit is empty (QTBUG-25447).
    input->setText(QString());
    QCOMPARE(input->cursorPosition(), 0);
    simulateKey(&window, Qt::Key_Right);
    QCOMPARE(input->hasActiveFocus(), false);
    simulateKey(&window, Qt::Key_Left);
    QCOMPARE(input->hasActiveFocus(), true);
    simulateKey(&window, Qt::Key_Left);
    QCOMPARE(input->hasActiveFocus(), false);
}

#ifndef QT_NO_CLIPBOARD
void tst_qquicktextedit::copyAndPaste()
{
    if (!PlatformQuirks::isClipboardAvailable())
        QSKIP("This machine doesn't support the clipboard");

    QString componentStr = "import QtQuick 2.0\nTextEdit { text: \"Hello world!\" }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
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
    textEdit->paste();
    QCOMPARE(textEdit->text(), QString("Hello world!Hello world!"));
    QCOMPARE(textEdit->text().length(), 24);
    textEdit->setReadOnly(false);
    QVERIFY(textEdit->canPaste());

    // QTBUG-12339
    // test that document and internal text attribute are in sync
    QQuickItemPrivate* pri = QQuickItemPrivate::get(textEdit);
    QQuickTextEditPrivate *editPrivate = static_cast<QQuickTextEditPrivate*>(pri);
    QCOMPARE(textEdit->text(), editPrivate->text);

    // cut: no selection
    textEdit->cut();
    QCOMPARE(textEdit->text(), QString("Hello world!Hello world!"));

    // select word
    textEdit->setCursorPosition(0);
    textEdit->selectWord();
    QCOMPARE(textEdit->selectedText(), QString("Hello"));

    // cut: read only.
    textEdit->setReadOnly(true);
    textEdit->cut();
    QCOMPARE(textEdit->text(), QString("Hello world!Hello world!"));
    textEdit->setReadOnly(false);

    // select all and cut
    textEdit->selectAll();
    textEdit->cut();
    QCOMPARE(textEdit->text().length(), 0);
    textEdit->paste();
    QCOMPARE(textEdit->text(), QString("Hello world!Hello world!"));
    QCOMPARE(textEdit->text().length(), 24);

    // Copy first word.
    textEdit->setCursorPosition(0);
    textEdit->selectWord();
    textEdit->copy();
    // copy: no selection, previous copy retained;
    textEdit->setCursorPosition(0);
    QCOMPARE(textEdit->selectedText(), QString());
    textEdit->copy();
    textEdit->setText(QString());
    textEdit->paste();
    QCOMPARE(textEdit->text(), QString("Hello"));
}
#endif

#ifndef QT_NO_CLIPBOARD
void tst_qquicktextedit::canPaste()
{
    QGuiApplication::clipboard()->setText("Some text");

    QString componentStr = "import QtQuick 2.0\nTextEdit { text: \"Hello world!\" }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    // check initial value - QTBUG-17765
    QTextDocument document;
    QQuickTextControl tc(&document);
    QCOMPARE(textEdit->canPaste(), tc.canPaste());
}
#endif

#ifndef QT_NO_CLIPBOARD
void tst_qquicktextedit::canPasteEmpty()
{
    QGuiApplication::clipboard()->clear();

    QString componentStr = "import QtQuick 2.0\nTextEdit { text: \"Hello world!\" }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    // check initial value - QTBUG-17765
    QTextDocument document;
    QQuickTextControl tc(&document);
    QCOMPARE(textEdit->canPaste(), tc.canPaste());
}
#endif

#ifndef QT_NO_CLIPBOARD
void tst_qquicktextedit::middleClickPaste()
{
    if (!PlatformQuirks::isClipboardAvailable())
        QSKIP("This machine doesn't support the clipboard");

    QQuickView window(testFileUrl("mouseselection_true.qml"));

    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);
    QQuickTextEdit *textEditObject = qobject_cast<QQuickTextEdit *>(window.rootObject());
    QVERIFY(textEditObject != 0);

    textEditObject->setFocus(true);

    QString originalText = textEditObject->text();
    QString selectedText = "234567";

    // press-and-drag-and-release from x1 to x2
    const QPoint p1 = textEditObject->positionToRectangle(2).center().toPoint();
    const QPoint p2 = textEditObject->positionToRectangle(8).center().toPoint();
    const QPoint p3 = textEditObject->positionToRectangle(1).center().toPoint();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, p1);
    QTest::mouseMove(&window, p2);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, p2);
    QTRY_COMPARE(textEditObject->selectedText(), selectedText);

    // Middle click pastes the selected text, assuming the platform supports it.
    QTest::mouseClick(&window, Qt::MiddleButton, Qt::NoModifier, p3);

    if (QGuiApplication::clipboard()->supportsSelection())
        QCOMPARE(textEditObject->text().mid(1, selectedText.length()), selectedText);
    else
        QCOMPARE(textEditObject->text(), originalText);
}
#endif

void tst_qquicktextedit::readOnly()
{
    QQuickView window(testFileUrl("readOnly.qml"));
    window.show();
    window.requestActivate();

    QVERIFY(window.rootObject() != 0);

    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(qvariant_cast<QObject *>(window.rootObject()->property("myInput")));

    QVERIFY(edit != 0);
    QTRY_VERIFY(edit->hasActiveFocus());
    QVERIFY(edit->isReadOnly());
    QString initial = edit->text();
    for (int k=Qt::Key_0; k<=Qt::Key_Z; k++)
        simulateKey(&window, k);
    simulateKey(&window, Qt::Key_Return);
    simulateKey(&window, Qt::Key_Space);
    simulateKey(&window, Qt::Key_Escape);
    QCOMPARE(edit->text(), initial);

    edit->setCursorPosition(3);
    edit->setReadOnly(false);
    QCOMPARE(edit->isReadOnly(), false);
    QCOMPARE(edit->cursorPosition(), edit->text().length());
}

void tst_qquicktextedit::simulateKey(QWindow *view, int key, Qt::KeyboardModifiers modifiers)
{
    QKeyEvent press(QKeyEvent::KeyPress, key, modifiers);
    QKeyEvent release(QKeyEvent::KeyRelease, key, modifiers);

    QGuiApplication::sendEvent(view, &press);
    QGuiApplication::sendEvent(view, &release);
}

void tst_qquicktextedit::textInput()
{
    QQuickView view(testFileUrl("inputMethodEvent.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(view.rootObject());
    QVERIFY(edit);
    QVERIFY(edit->hasActiveFocus());

    // test that input method event is committed and change signal is emitted
    QSignalSpy spy(edit, SIGNAL(textChanged()));
    QInputMethodEvent event;
    event.setCommitString( "Hello world!", 0, 0);
    QGuiApplication::sendEvent(edit, &event);
    QCOMPARE(edit->text(), QString("Hello world!"));
    QCOMPARE(spy.count(), 1);

    // QTBUG-12339
    // test that document and internal text attribute are in sync
    QQuickTextEditPrivate *editPrivate = static_cast<QQuickTextEditPrivate*>(QQuickItemPrivate::get(edit));
    QCOMPARE(editPrivate->text, QString("Hello world!"));

    QInputMethodQueryEvent queryEvent(Qt::ImEnabled);
    QGuiApplication::sendEvent(edit, &queryEvent);
    QCOMPARE(queryEvent.value(Qt::ImEnabled).toBool(), true);

    edit->setReadOnly(true);
    QGuiApplication::sendEvent(edit, &queryEvent);
    QCOMPARE(queryEvent.value(Qt::ImEnabled).toBool(), false);

    edit->setReadOnly(false);

    QInputMethodEvent preeditEvent("PREEDIT", QList<QInputMethodEvent::Attribute>());
    QGuiApplication::sendEvent(edit, &preeditEvent);
    QCOMPARE(edit->text(), QString("Hello world!"));
    QCOMPARE(edit->preeditText(), QString("PREEDIT"));

    QInputMethodEvent preeditEvent2("PREEDIT2", QList<QInputMethodEvent::Attribute>());
    QGuiApplication::sendEvent(edit, &preeditEvent2);
    QCOMPARE(edit->text(), QString("Hello world!"));
    QCOMPARE(edit->preeditText(), QString("PREEDIT2"));

    QInputMethodEvent preeditEvent3("", QList<QInputMethodEvent::Attribute>());
    QGuiApplication::sendEvent(edit, &preeditEvent3);
    QCOMPARE(edit->text(), QString("Hello world!"));
    QCOMPARE(edit->preeditText(), QString(""));
}

void tst_qquicktextedit::inputMethodUpdate()
{
    PlatformInputContext platformInputContext;
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = &platformInputContext;

    QQuickView view(testFileUrl("inputMethodEvent.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(view.rootObject());
    QVERIFY(edit);
    QVERIFY(edit->hasActiveFocus());

    // text change even without cursor position change needs to trigger update
    edit->setText("test");
    platformInputContext.clear();
    edit->setText("xxxx");
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // input method event replacing text
    platformInputContext.clear();
    {
        QInputMethodEvent inputMethodEvent;
        inputMethodEvent.setCommitString("y", -1, 1);
        QGuiApplication::sendEvent(edit, &inputMethodEvent);
    }
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // input method changing selection
    platformInputContext.clear();
    {
        QList<QInputMethodEvent::Attribute> attributes;
        attributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 0, 2, QVariant());
        QInputMethodEvent inputMethodEvent("", attributes);
        QGuiApplication::sendEvent(edit, &inputMethodEvent);
    }
    QVERIFY(edit->selectionStart() != edit->selectionEnd());
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // programmatical selections trigger update
    platformInputContext.clear();
    edit->selectAll();
    QCOMPARE(platformInputContext.m_updateCallCount, 1);

    // font changes
    platformInputContext.clear();
    QFont font = edit->font();
    font.setBold(!font.bold());
    edit->setFont(font);
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // normal input
    platformInputContext.clear();
    {
        QInputMethodEvent inputMethodEvent;
        inputMethodEvent.setCommitString("y");
        QGuiApplication::sendEvent(edit, &inputMethodEvent);
    }
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // changing cursor position
    platformInputContext.clear();
    edit->setCursorPosition(0);
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // continuing with selection
    platformInputContext.clear();
    edit->moveCursorSelection(1);
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // read only disabled input method
    platformInputContext.clear();
    edit->setReadOnly(true);
    QVERIFY(platformInputContext.m_updateCallCount > 0);
    edit->setReadOnly(false);

    // no updates while no focus
    edit->setFocus(false);
    platformInputContext.clear();
    edit->setText("Foo");
    QCOMPARE(platformInputContext.m_updateCallCount, 0);
    edit->setCursorPosition(1);
    QCOMPARE(platformInputContext.m_updateCallCount, 0);
    edit->selectAll();
    QCOMPARE(platformInputContext.m_updateCallCount, 0);
    edit->setReadOnly(true);
    QCOMPARE(platformInputContext.m_updateCallCount, 0);
}

void tst_qquicktextedit::openInputPanel()
{
    PlatformInputContext platformInputContext;
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = &platformInputContext;

    QQuickView view(testFileUrl("openInputPanel.qml"));
    view.showNormal();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);

    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(view.rootObject());
    QVERIFY(edit);

    // check default values
    QVERIFY(edit->focusOnPress());
    QVERIFY(!edit->hasActiveFocus());
    QVERIFY(qApp->focusObject() != edit);

    QCOMPARE(qApp->inputMethod()->isVisible(), false);

    // input panel should open on focus
    QPoint centerPoint(view.width()/2, view.height()/2);
    Qt::KeyboardModifiers noModifiers = 0;
    QTest::mousePress(&view, Qt::LeftButton, noModifiers, centerPoint);
    QGuiApplication::processEvents();
    QVERIFY(edit->hasActiveFocus());
    QCOMPARE(qApp->focusObject(), edit);
    QCOMPARE(qApp->inputMethod()->isVisible(), true);
    QTest::mouseRelease(&view, Qt::LeftButton, noModifiers, centerPoint);

    // input panel should be re-opened when pressing already focused TextEdit
    qApp->inputMethod()->hide();
    QCOMPARE(qApp->inputMethod()->isVisible(), false);
    QVERIFY(edit->hasActiveFocus());
    QTest::mousePress(&view, Qt::LeftButton, noModifiers, centerPoint);
    QGuiApplication::processEvents();
    QCOMPARE(qApp->inputMethod()->isVisible(), true);
    QTest::mouseRelease(&view, Qt::LeftButton, noModifiers, centerPoint);

    // input panel should stay visible if focus is lost to another text editor
    QSignalSpy inputPanelVisibilitySpy(qApp->inputMethod(), SIGNAL(visibleChanged()));
    QQuickTextEdit anotherEdit;
    anotherEdit.setParentItem(view.rootObject());
    anotherEdit.setFocus(true);
    QCOMPARE(qApp->inputMethod()->isVisible(), true);
    QCOMPARE(qApp->focusObject(), qobject_cast<QObject*>(&anotherEdit));
    QCOMPARE(inputPanelVisibilitySpy.count(), 0);

    anotherEdit.setFocus(false);
    QVERIFY(qApp->focusObject() != &anotherEdit);
    QCOMPARE(view.activeFocusItem(), view.contentItem());
    anotherEdit.setFocus(true);

    qApp->inputMethod()->hide();

    // input panel should not be opened if TextEdit is read only
    edit->setReadOnly(true);
    edit->setFocus(true);
    QCOMPARE(qApp->inputMethod()->isVisible(), false);
    QTest::mousePress(&view, Qt::LeftButton, noModifiers, centerPoint);
    QTest::mouseRelease(&view, Qt::LeftButton, noModifiers, centerPoint);
    QGuiApplication::processEvents();
    QCOMPARE(qApp->inputMethod()->isVisible(), false);

    // input panel should not be opened if focusOnPress is set to false
    edit->setFocusOnPress(false);
    edit->setFocus(false);
    edit->setFocus(true);
    QCOMPARE(qApp->inputMethod()->isVisible(), false);
    QTest::mousePress(&view, Qt::LeftButton, noModifiers, centerPoint);
    QTest::mouseRelease(&view, Qt::LeftButton, noModifiers, centerPoint);
    QCOMPARE(qApp->inputMethod()->isVisible(), false);

    inputMethodPrivate->testContext = 0;
}

void tst_qquicktextedit::geometrySignals()
{
    QQmlComponent component(&engine, testFileUrl("geometrySignals.qml"));
    QObject *o = component.create();
    QVERIFY(o);
    QCOMPARE(o->property("bindingWidth").toInt(), 400);
    QCOMPARE(o->property("bindingHeight").toInt(), 500);
    delete o;
}

#ifndef QT_NO_CLIPBOARD
void tst_qquicktextedit::pastingRichText_QTBUG_14003()
{
    QString componentStr = "import QtQuick 2.0\nTextEdit { textFormat: TextEdit.PlainText }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickTextEdit *obj = qobject_cast<QQuickTextEdit*>(component.create());

    QTRY_VERIFY(obj != 0);
    QTRY_COMPARE(obj->textFormat(), QQuickTextEdit::PlainText);

    QMimeData *mData = new QMimeData;
    mData->setHtml("<font color=\"red\">Hello</font>");
    QGuiApplication::clipboard()->setMimeData(mData);

    obj->paste();
    QTRY_COMPARE(obj->text(), QString());
    QTRY_COMPARE(obj->textFormat(), QQuickTextEdit::PlainText);
}
#endif

void tst_qquicktextedit::implicitSize_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("wrap");
    QTest::addColumn<QString>("format");
    QTest::newRow("plain") << "The quick red fox jumped over the lazy brown dog" << "TextEdit.NoWrap" << "TextEdit.PlainText";
    QTest::newRow("richtext") << "<b>The quick red fox jumped over the lazy brown dog</b>" << "TextEdit.NoWrap" << "TextEdit.RichText";
    QTest::newRow("plain_wrap") << "The quick red fox jumped over the lazy brown dog" << "TextEdit.Wrap" << "TextEdit.PlainText";
    QTest::newRow("richtext_wrap") << "<b>The quick red fox jumped over the lazy brown dog</b>" << "TextEdit.Wrap" << "TextEdit.RichText";
}

void tst_qquicktextedit::implicitSize()
{
    QFETCH(QString, text);
    QFETCH(QString, wrap);
    QFETCH(QString, format);
    QString componentStr = "import QtQuick 2.0\nTextEdit { text: \"" + text + "\"; width: 50; wrapMode: " + wrap + "; textFormat: " + format + " }";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickTextEdit *textObject = qobject_cast<QQuickTextEdit*>(textComponent.create());

    QVERIFY(textObject->width() < textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());

    textObject->resetWidth();
    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());
}

void tst_qquicktextedit::contentSize()
{
    QString componentStr = "import QtQuick 2.0\nTextEdit { width: 75; height: 16; font.pixelSize: 10 }";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QScopedPointer<QObject> object(textComponent.create());
    QQuickTextEdit *textObject = qobject_cast<QQuickTextEdit *>(object.data());

    QSignalSpy spy(textObject, SIGNAL(contentSizeChanged()));

    textObject->setText("The quick red fox jumped over the lazy brown dog");

    QVERIFY(textObject->contentWidth() > textObject->width());
    QVERIFY(textObject->contentHeight() < textObject->height());
    QCOMPARE(spy.count(), 1);

    textObject->setWrapMode(QQuickTextEdit::WordWrap);
    QVERIFY(textObject->contentWidth() <= textObject->width());
    QVERIFY(textObject->contentHeight() > textObject->height());
    QCOMPARE(spy.count(), 2);

    textObject->setText("The quickredfoxjumpedoverthe lazy brown dog");

    QVERIFY(textObject->contentWidth() > textObject->width());
    QVERIFY(textObject->contentHeight() > textObject->height());
    QCOMPARE(spy.count(), 3);
}

void tst_qquicktextedit::implicitSizeBinding_data()
{
    implicitSize_data();
}

void tst_qquicktextedit::implicitSizeBinding()
{
    QFETCH(QString, text);
    QFETCH(QString, wrap);
    QFETCH(QString, format);
    QString componentStr = "import QtQuick 2.0\nTextEdit { text: \"" + text + "\"; width: implicitWidth; height: implicitHeight; wrapMode: " + wrap + "; textFormat: " + format + " }";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QScopedPointer<QObject> object(textComponent.create());
    QQuickTextEdit *textObject = qobject_cast<QQuickTextEdit *>(object.data());

    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());

    textObject->resetWidth();
    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());

    textObject->resetHeight();
    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());
}

void tst_qquicktextedit::signal_editingfinished()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("signal_editingfinished.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QVERIFY(window->rootObject() != 0);

    QQuickTextEdit *input1 = qobject_cast<QQuickTextEdit *>(qvariant_cast<QObject *>(window->rootObject()->property("input1")));
    QVERIFY(input1);
    QQuickTextEdit *input2 = qobject_cast<QQuickTextEdit *>(qvariant_cast<QObject *>(window->rootObject()->property("input2")));
    QVERIFY(input2);

    QSignalSpy editingFinished1Spy(input1, SIGNAL(editingFinished()));

    input1->setFocus(true);
    QTRY_VERIFY(input1->hasActiveFocus());
    QTRY_VERIFY(!input2->hasActiveFocus());

    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());
    QTRY_COMPARE(editingFinished1Spy.count(), 1);

    QTRY_VERIFY(!input1->hasActiveFocus());
    QTRY_VERIFY(input2->hasActiveFocus());

    QSignalSpy editingFinished2Spy(input2, SIGNAL(editingFinished()));

    input2->setFocus(true);
    QTRY_VERIFY(!input1->hasActiveFocus());
    QTRY_VERIFY(input2->hasActiveFocus());

    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());
    QTRY_COMPARE(editingFinished2Spy.count(), 1);

    QTRY_VERIFY(input1->hasActiveFocus());
    QTRY_VERIFY(!input2->hasActiveFocus());
}

void tst_qquicktextedit::clipRect()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n TextEdit {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(object.data());
    QVERIFY(edit);

    QCOMPARE(edit->clipRect().x(), qreal(0));
    QCOMPARE(edit->clipRect().y(), qreal(0));

    QCOMPARE(edit->clipRect().width(), edit->width() + edit->cursorRectangle().width());
    QCOMPARE(edit->clipRect().height(), edit->height());

    edit->setText("Hello World");
    QCOMPARE(edit->clipRect().x(), qreal(0));
    QCOMPARE(edit->clipRect().y(), qreal(0));
    // XXX: TextEdit allows an extra 3 pixels boundary for the cursor beyond it's width for non
    // empty text. TextInput doesn't.
    QCOMPARE(edit->clipRect().width(), edit->width() + edit->cursorRectangle().width() + 3);
    QCOMPARE(edit->clipRect().height(), edit->height());

    // clip rect shouldn't exceed the size of the item, expect for the cursor width;
    edit->setWidth(edit->width() / 2);
    QCOMPARE(edit->clipRect().x(), qreal(0));
    QCOMPARE(edit->clipRect().y(), qreal(0));
    QCOMPARE(edit->clipRect().width(), edit->width() + edit->cursorRectangle().width() + 3);
    QCOMPARE(edit->clipRect().height(), edit->height());

    edit->setHeight(edit->height() * 2);
    QCOMPARE(edit->clipRect().x(), qreal(0));
    QCOMPARE(edit->clipRect().y(), qreal(0));
    QCOMPARE(edit->clipRect().width(), edit->width() + edit->cursorRectangle().width() + 3);
    QCOMPARE(edit->clipRect().height(), edit->height());

    QQmlComponent cursorComponent(&engine);
    cursorComponent.setData("import QtQuick 2.0\nRectangle { height: 20; width: 8 }", QUrl());

    edit->setCursorDelegate(&cursorComponent);
    edit->setCursorVisible(true);

    // If a cursor delegate is used it's size should determine the excess width.
    QCOMPARE(edit->clipRect().x(), qreal(0));
    QCOMPARE(edit->clipRect().y(), qreal(0));
    QCOMPARE(edit->clipRect().width(), edit->width() + 8 + 3);
    QCOMPARE(edit->clipRect().height(), edit->height());

    // Alignment and wrapping don't affect the clip rect.
    edit->setHAlign(QQuickTextEdit::AlignRight);
    QCOMPARE(edit->clipRect().x(), qreal(0));
    QCOMPARE(edit->clipRect().y(), qreal(0));
    QCOMPARE(edit->clipRect().width(), edit->width() + 8 + 3);
    QCOMPARE(edit->clipRect().height(), edit->height());

    edit->setWrapMode(QQuickTextEdit::Wrap);
    QCOMPARE(edit->clipRect().x(), qreal(0));
    QCOMPARE(edit->clipRect().y(), qreal(0));
    QCOMPARE(edit->clipRect().width(), edit->width() + 8 + 3);
    QCOMPARE(edit->clipRect().height(), edit->height());

    edit->setVAlign(QQuickTextEdit::AlignBottom);
    QCOMPARE(edit->clipRect().x(), qreal(0));
    QCOMPARE(edit->clipRect().y(), qreal(0));
    QCOMPARE(edit->clipRect().width(), edit->width() + 8 + 3);
    QCOMPARE(edit->clipRect().height(), edit->height());
}

void tst_qquicktextedit::boundingRect()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n TextEdit {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(object.data());
    QVERIFY(edit);

    QTextLayout layout;
    layout.setFont(edit->font());

    if (!qmlDisableDistanceField()) {
        QTextOption option;
        option.setUseDesignMetrics(true);
        layout.setTextOption(option);
    }
    layout.beginLayout();
    QTextLine line = layout.createLine();
    layout.endLayout();

    QCOMPARE(edit->boundingRect().x(), qreal(0));
    QCOMPARE(edit->boundingRect().y(), qreal(0));
    QCOMPARE(edit->boundingRect().width(), edit->cursorRectangle().width());
    QCOMPARE(edit->boundingRect().height(), line.height());

    edit->setText("Hello World");

    layout.setText(edit->text());
    layout.beginLayout();
    line = layout.createLine();
    layout.endLayout();

    QCOMPARE(edit->boundingRect().x(), qreal(0));
    QCOMPARE(edit->boundingRect().y(), qreal(0));
    QCOMPARE(edit->boundingRect().width(), line.naturalTextWidth() + edit->cursorRectangle().width() + 3);
    QCOMPARE(edit->boundingRect().height(), line.height());

    // the size of the bounding rect shouldn't be bounded by the size of item.
    edit->setWidth(edit->width() / 2);
    QCOMPARE(edit->boundingRect().x(), qreal(0));
    QCOMPARE(edit->boundingRect().y(), qreal(0));
    QCOMPARE(edit->boundingRect().width(), line.naturalTextWidth() + edit->cursorRectangle().width() + 3);
    QCOMPARE(edit->boundingRect().height(), line.height());

    edit->setHeight(edit->height() * 2);
    QCOMPARE(edit->boundingRect().x(), qreal(0));
    QCOMPARE(edit->boundingRect().y(), qreal(0));
    QCOMPARE(edit->boundingRect().width(), line.naturalTextWidth() + edit->cursorRectangle().width() + 3);
    QCOMPARE(edit->boundingRect().height(), line.height());

    QQmlComponent cursorComponent(&engine);
    cursorComponent.setData("import QtQuick 2.0\nRectangle { height: 20; width: 8 }", QUrl());

    edit->setCursorDelegate(&cursorComponent);
    edit->setCursorVisible(true);

    // Don't include the size of a cursor delegate as it has its own bounding rect.
    QCOMPARE(edit->boundingRect().x(), qreal(0));
    QCOMPARE(edit->boundingRect().y(), qreal(0));
    QCOMPARE(edit->boundingRect().width(), line.naturalTextWidth());
    QCOMPARE(edit->boundingRect().height(), line.height());

    edit->setHAlign(QQuickTextEdit::AlignRight);
    QCOMPARE(edit->boundingRect().x(), edit->width() - line.naturalTextWidth());
    QCOMPARE(edit->boundingRect().y(), qreal(0));
    QCOMPARE(edit->boundingRect().width(), line.naturalTextWidth());
    QCOMPARE(edit->boundingRect().height(), line.height());

    edit->setWrapMode(QQuickTextEdit::Wrap);
    QCOMPARE(edit->boundingRect().right(), edit->width());
    QCOMPARE(edit->boundingRect().y(), qreal(0));
    QVERIFY(edit->boundingRect().width() < line.naturalTextWidth());
    QVERIFY(edit->boundingRect().height() > line.height());

    edit->setVAlign(QQuickTextEdit::AlignBottom);
    QCOMPARE(edit->boundingRect().right(), edit->width());
    QCOMPARE(edit->boundingRect().bottom(), edit->height());
    QVERIFY(edit->boundingRect().width() < line.naturalTextWidth());
    QVERIFY(edit->boundingRect().height() > line.height());
}

void tst_qquicktextedit::preeditCursorRectangle()
{
    QString preeditText = "super";

    QQuickView view(testFileUrl("inputMethodEvent.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);

    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(view.rootObject());
    QVERIFY(edit);

    QQuickItem *cursor = edit->findChild<QQuickItem *>("cursor");
    QVERIFY(cursor);

    QSignalSpy editSpy(edit, SIGNAL(cursorRectangleChanged()));
    QSignalSpy panelSpy(qGuiApp->inputMethod(), SIGNAL(cursorRectangleChanged()));

    QRectF currentRect;

    QCOMPARE(QGuiApplication::focusObject(), static_cast<QObject *>(edit));
    QInputMethodQueryEvent query(Qt::ImCursorRectangle);
    QCoreApplication::sendEvent(edit, &query);
    QRectF previousRect = query.value(Qt::ImCursorRectangle).toRectF();

    // Verify that the micro focus rect is positioned the same for position 0 as
    // it would be if there was no preedit text.
    QInputMethodEvent imEvent(preeditText, QList<QInputMethodEvent::Attribute>()
            << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, preeditText.length(), QVariant()));
    QCoreApplication::sendEvent(edit, &imEvent);
    QCoreApplication::sendEvent(edit, &query);
    currentRect = query.value(Qt::ImCursorRectangle).toRectF();
    QCOMPARE(edit->cursorRectangle(), currentRect);
    QCOMPARE(cursor->position(), currentRect.topLeft());
    QCOMPARE(currentRect, previousRect);

    // Verify that the micro focus rect moves to the left as the cursor position
    // is incremented.
    editSpy.clear();
    panelSpy.clear();
    for (int i = 1; i <= 5; ++i) {
        QInputMethodEvent imEvent(preeditText, QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, i, preeditText.length(), QVariant()));
        QCoreApplication::sendEvent(edit, &imEvent);
        QCoreApplication::sendEvent(edit, &query);
        currentRect = query.value(Qt::ImCursorRectangle).toRectF();
        QCOMPARE(edit->cursorRectangle(), currentRect);
        QCOMPARE(cursor->position(), currentRect.topLeft());
        QVERIFY(previousRect.left() < currentRect.left());
        QCOMPARE(editSpy.count(), 1); editSpy.clear();
        QCOMPARE(panelSpy.count(), 1); panelSpy.clear();
        previousRect = currentRect;
    }

    // Verify that if the cursor rectangle is updated if the pre-edit text changes
    // but the (non-zero) cursor position is the same.
    editSpy.clear();
    panelSpy.clear();
    {   QInputMethodEvent imEvent("wwwww", QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 5, 1, QVariant()));
        QCoreApplication::sendEvent(edit, &imEvent); }
    QCoreApplication::sendEvent(edit, &query);
    currentRect = query.value(Qt::ImCursorRectangle).toRectF();
    QCOMPARE(edit->cursorRectangle(), currentRect);
    QCOMPARE(cursor->position(), currentRect.topLeft());
    QCOMPARE(editSpy.count(), 1);
    QCOMPARE(panelSpy.count(), 1);

    // Verify that if there is no preedit cursor then the micro focus rect is the
    // same as it would be if it were positioned at the end of the preedit text.
    editSpy.clear();
    panelSpy.clear();
    {   QInputMethodEvent imEvent(preeditText, QList<QInputMethodEvent::Attribute>());
        QCoreApplication::sendEvent(edit, &imEvent); }
    QCoreApplication::sendEvent(edit, &query);
    currentRect = query.value(Qt::ImCursorRectangle).toRectF();
    QCOMPARE(edit->cursorRectangle(), currentRect);
    QCOMPARE(cursor->position(), currentRect.topLeft());
    QCOMPARE(currentRect, previousRect);
    QCOMPARE(editSpy.count(), 1);
    QCOMPARE(panelSpy.count(), 1);
}

void tst_qquicktextedit::inputMethodComposing()
{
    QString text = "supercalifragisiticexpialidocious!";

    QQuickView view(testFileUrl("inputContext.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);

    QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(view.rootObject());
    QVERIFY(edit);
    QCOMPARE(QGuiApplication::focusObject(), static_cast<QObject *>(edit));

    QSignalSpy spy(edit, SIGNAL(inputMethodComposingChanged()));
    edit->setCursorPosition(12);

    QCOMPARE(edit->isInputMethodComposing(), false);

    {
        QInputMethodEvent event(text.mid(3), QList<QInputMethodEvent::Attribute>());
        QGuiApplication::sendEvent(edit, &event);
    }

    QCOMPARE(edit->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 1);

    {
        QInputMethodEvent event(text.mid(12), QList<QInputMethodEvent::Attribute>());
        QGuiApplication::sendEvent(edit, &event);
    }
    QCOMPARE(spy.count(), 1);

    {
        QInputMethodEvent event;
        QGuiApplication::sendEvent(edit, &event);
    }
    QCOMPARE(edit->isInputMethodComposing(), false);
    QCOMPARE(spy.count(), 2);

    // Changing the text while not composing doesn't alter the composing state.
    edit->setText(text.mid(0, 16));
    QCOMPARE(edit->isInputMethodComposing(), false);
    QCOMPARE(spy.count(), 2);

    {
        QInputMethodEvent event(text.mid(16), QList<QInputMethodEvent::Attribute>());
        QGuiApplication::sendEvent(edit, &event);
    }
    QCOMPARE(edit->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 3);

    // Changing the text while composing cancels composition.
    edit->setText(text.mid(0, 12));
    QCOMPARE(edit->isInputMethodComposing(), false);
    QCOMPARE(spy.count(), 4);

    {   // Preedit cursor positioned outside (empty) preedit; composing.
        QInputMethodEvent event(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, -2, 1, QVariant()));
        QGuiApplication::sendEvent(edit, &event);
    }
    QCOMPARE(edit->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 5);

    {   // Cursor hidden; composing
        QInputMethodEvent event(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 0, QVariant()));
        QGuiApplication::sendEvent(edit, &event);
    }
    QCOMPARE(edit->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 5);

    {   // Default cursor attributes; composing.
        QInputMethodEvent event(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 1, QVariant()));
        QGuiApplication::sendEvent(edit, &event);
    }
    QCOMPARE(edit->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 5);

    {   // Selections are persisted: not composing
        QInputMethodEvent event(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 2, 4, QVariant()));
        QGuiApplication::sendEvent(edit, &event);
    }
    QCOMPARE(edit->isInputMethodComposing(), false);
    QCOMPARE(spy.count(), 6);

    edit->setCursorPosition(0);

    {   // Formatting applied; composing.
        QTextCharFormat format;
        format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        QInputMethodEvent event(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, 2, 4, format));
        QGuiApplication::sendEvent(edit, &event);
    }
    QCOMPARE(edit->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 7);

    {
        QInputMethodEvent event;
        QGuiApplication::sendEvent(edit, &event);
    }
    QCOMPARE(edit->isInputMethodComposing(), false);
    QCOMPARE(spy.count(), 8);
}

void tst_qquicktextedit::cursorRectangleSize_data()
{
    QTest::addColumn<bool>("useCursorDelegate");

    QTest::newRow("default cursor") << false;
    QTest::newRow("custom cursor delegate") << true;
}

void tst_qquicktextedit::cursorRectangleSize()
{
    QFETCH(bool, useCursorDelegate);

    QQuickView *window = new QQuickView(testFileUrl("positionAt.qml"));
    QVERIFY(window->rootObject() != 0);
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit *>(window->rootObject());

    QQmlComponent cursorDelegate(window->engine());
    if (useCursorDelegate) {
        cursorDelegate.setData("import QtQuick 2.0\nRectangle { width:10; height:10; }", QUrl());
        textEdit->setCursorDelegate(&cursorDelegate);
    }

    // make sure cursor rectangle is not at (0,0)
    textEdit->setX(10);
    textEdit->setY(10);
    textEdit->setCursorPosition(3);
    QVERIFY(textEdit != 0);
    textEdit->setFocus(true);
    window->show();
    window->requestActivate();
    QTest::qWaitForWindowActive(window);

    QInputMethodQueryEvent event(Qt::ImCursorRectangle);
    qApp->sendEvent(textEdit, &event);
    QRectF cursorRectFromQuery = event.value(Qt::ImCursorRectangle).toRectF();

    QRectF cursorRectFromItem = textEdit->cursorRectangle();
    QRectF cursorRectFromPositionToRectangle = textEdit->positionToRectangle(textEdit->cursorPosition());

    QVERIFY(cursorRectFromItem.isValid());
    QVERIFY(cursorRectFromQuery.isValid());
    QVERIFY(cursorRectFromPositionToRectangle.isValid());

    // item and input query cursor rectangles match
    QCOMPARE(cursorRectFromItem, cursorRectFromQuery);

    // item cursor rectangle and positionToRectangle calculations match
    QCOMPARE(cursorRectFromItem, cursorRectFromPositionToRectangle);

    // item-window transform and input item transform match
    QCOMPARE(QQuickItemPrivate::get(textEdit)->itemToWindowTransform(), qApp->inputMethod()->inputItemTransform());

    // input panel cursorRectangle property and tranformed item cursor rectangle match
    QRectF sceneCursorRect = QQuickItemPrivate::get(textEdit)->itemToWindowTransform().mapRect(cursorRectFromItem);
    QCOMPARE(sceneCursorRect, qApp->inputMethod()->cursorRectangle());

    delete window;
}

void tst_qquicktextedit::getText_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("start");
    QTest::addColumn<int>("end");
    QTest::addColumn<QString>("expectedText");

    const QString richBoldText = QStringLiteral("This is some <b>bold</b> text");
    const QString plainBoldText = QStringLiteral("This is some bold text");
    const QString richBoldTextLB = QStringLiteral("This is some<br/><b>bold</b> text");
    const QString plainBoldTextLB = QString(QStringLiteral("This is some\nbold text")).replace(QLatin1Char('\n'), QChar(QChar::LineSeparator));

    QTest::newRow("all plain text")
            << standard.at(0)
            << 0 << standard.at(0).length()
            << standard.at(0);

    QTest::newRow("plain text sub string")
            << standard.at(0)
            << 0 << 12
            << standard.at(0).mid(0, 12);

    QTest::newRow("plain text sub string reversed")
            << standard.at(0)
            << 12 << 0
            << standard.at(0).mid(0, 12);

    QTest::newRow("plain text cropped beginning")
            << standard.at(0)
            << -3 << 4
            << standard.at(0).mid(0, 4);

    QTest::newRow("plain text cropped end")
            << standard.at(0)
            << 23 << standard.at(0).length() + 8
            << standard.at(0).mid(23);

    QTest::newRow("plain text cropped beginning and end")
            << standard.at(0)
            << -9 << standard.at(0).length() + 4
            << standard.at(0);

    QTest::newRow("all rich text")
            << richBoldText
            << 0 << plainBoldText.length()
            << plainBoldText;

    QTest::newRow("rich text sub string")
            << richBoldText
            << 14 << 21
            << plainBoldText.mid(14, 7);

    // Line break.
    QTest::newRow("all plain text (line break)")
            << standard.at(1)
            << 0 << standard.at(1).length()
            << standard.at(1);

    QTest::newRow("plain text sub string (line break)")
            << standard.at(1)
            << 0 << 12
            << standard.at(1).mid(0, 12);

    QTest::newRow("plain text sub string reversed (line break)")
            << standard.at(1)
            << 12 << 0
            << standard.at(1).mid(0, 12);

    QTest::newRow("plain text cropped beginning (line break)")
            << standard.at(1)
            << -3 << 4
            << standard.at(1).mid(0, 4);

    QTest::newRow("plain text cropped end (line break)")
            << standard.at(1)
            << 23 << standard.at(1).length() + 8
            << standard.at(1).mid(23);

    QTest::newRow("plain text cropped beginning and end (line break)")
            << standard.at(1)
            << -9 << standard.at(1).length() + 4
            << standard.at(1);

    QTest::newRow("all rich text (line break)")
            << richBoldTextLB
            << 0 << plainBoldTextLB.length()
            << plainBoldTextLB;

    QTest::newRow("rich text sub string (line break)")
            << richBoldTextLB
            << 14 << 21
            << plainBoldTextLB.mid(14, 7);
}

void tst_qquicktextedit::getText()
{
    QFETCH(QString, text);
    QFETCH(int, start);
    QFETCH(int, end);
    QFETCH(QString, expectedText);

    QString componentStr = "import QtQuick 2.0\nTextEdit { textFormat: TextEdit.AutoText; text: \"" + text + "\" }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    QCOMPARE(textEdit->getText(start, end), expectedText);
}

void tst_qquicktextedit::getFormattedText_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QQuickTextEdit::TextFormat>("textFormat");
    QTest::addColumn<int>("start");
    QTest::addColumn<int>("end");
    QTest::addColumn<QString>("expectedText");

    const QString richBoldText = QStringLiteral("This is some <b>bold</b> text");
    const QString plainBoldText = QStringLiteral("This is some bold text");

    QTest::newRow("all plain text")
            << standard.at(0)
            << QQuickTextEdit::PlainText
            << 0 << standard.at(0).length()
            << standard.at(0);

    QTest::newRow("plain text sub string")
            << standard.at(0)
            << QQuickTextEdit::PlainText
            << 0 << 12
            << standard.at(0).mid(0, 12);

    QTest::newRow("plain text sub string reversed")
            << standard.at(0)
            << QQuickTextEdit::PlainText
            << 12 << 0
            << standard.at(0).mid(0, 12);

    QTest::newRow("plain text cropped beginning")
            << standard.at(0)
            << QQuickTextEdit::PlainText
            << -3 << 4
            << standard.at(0).mid(0, 4);

    QTest::newRow("plain text cropped end")
            << standard.at(0)
            << QQuickTextEdit::PlainText
            << 23 << standard.at(0).length() + 8
            << standard.at(0).mid(23);

    QTest::newRow("plain text cropped beginning and end")
            << standard.at(0)
            << QQuickTextEdit::PlainText
            << -9 << standard.at(0).length() + 4
            << standard.at(0);

    QTest::newRow("all rich (Auto) text")
            << richBoldText
            << QQuickTextEdit::AutoText
            << 0 << plainBoldText.length()
            << QString("This is some \\<.*\\>bold\\</.*\\> text");

    QTest::newRow("all rich (Rich) text")
            << richBoldText
            << QQuickTextEdit::RichText
            << 0 << plainBoldText.length()
            << QString("This is some \\<.*\\>bold\\</.*\\> text");

    QTest::newRow("all rich (Plain) text")
            << richBoldText
            << QQuickTextEdit::PlainText
            << 0 << richBoldText.length()
            << richBoldText;

    QTest::newRow("rich (Auto) text sub string")
            << richBoldText
            << QQuickTextEdit::AutoText
            << 14 << 21
            << QString("\\<.*\\>old\\</.*\\> tex");

    QTest::newRow("rich (Rich) text sub string")
            << richBoldText
            << QQuickTextEdit::RichText
            << 14 << 21
            << QString("\\<.*\\>old\\</.*\\> tex");

    QTest::newRow("rich (Plain) text sub string")
            << richBoldText
            << QQuickTextEdit::PlainText
            << 17 << 27
            << richBoldText.mid(17, 10);
}

void tst_qquicktextedit::getFormattedText()
{
    QFETCH(QString, text);
    QFETCH(QQuickTextEdit::TextFormat, textFormat);
    QFETCH(int, start);
    QFETCH(int, end);
    QFETCH(QString, expectedText);

    QString componentStr = "import QtQuick 2.0\nTextEdit {}";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    textEdit->setTextFormat(textFormat);
    textEdit->setText(text);

    if (textFormat == QQuickTextEdit::RichText
            || (textFormat == QQuickTextEdit::AutoText && Qt::mightBeRichText(text))) {
        QVERIFY(textEdit->getFormattedText(start, end).contains(QRegExp(expectedText)));
    } else {
        QCOMPARE(textEdit->getFormattedText(start, end), expectedText);
    }
}

void tst_qquicktextedit::append_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QQuickTextEdit::TextFormat>("textFormat");
    QTest::addColumn<int>("selectionStart");
    QTest::addColumn<int>("selectionEnd");
    QTest::addColumn<QString>("appendText");
    QTest::addColumn<QString>("expectedText");
    QTest::addColumn<int>("expectedSelectionStart");
    QTest::addColumn<int>("expectedSelectionEnd");
    QTest::addColumn<int>("expectedCursorPosition");
    QTest::addColumn<bool>("selectionChanged");
    QTest::addColumn<bool>("cursorPositionChanged");

    QTest::newRow("cursor kept intact (beginning)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0
            << QString("Hello")
            << standard.at(0) + QString("\nHello")
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("cursor kept intact (middle)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 18 << 18
            << QString("Hello")
            << standard.at(0) + QString("\nHello")
            << 18 << 18 << 18
            << false << false;

    QTest::newRow("cursor follows (end)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << standard.at(0).length() << standard.at(0).length()
            << QString("Hello")
            << standard.at(0) + QString("\nHello")
            << standard.at(0).length() + 6 << standard.at(0).length() + 6 << standard.at(0).length() + 6
            << false << true;

    QTest::newRow("selection kept intact (beginning)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 18
            << QString("Hello")
            << standard.at(0) + QString("\nHello")
            << 0 << 18 << 18
            << false << false;

    QTest::newRow("selection kept intact (middle)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 14 << 18
            << QString("Hello")
            << standard.at(0) + QString("\nHello")
            << 14 << 18 << 18
            << false << false;

    QTest::newRow("selection kept intact, cursor follows (end)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 18 << standard.at(0).length()
            << QString("Hello")
            << standard.at(0) + QString("\nHello")
            << 18 << standard.at(0).length() + 6 << standard.at(0).length() + 6
            << true << true;

    QTest::newRow("reversed selection kept intact")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 18 << 14
            << QString("Hello")
            << standard.at(0) + QString("\nHello")
            << 14 << 18 << 14
            << false << false;

    QTest::newRow("rich text into plain text")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0
            << QString("<b>Hello</b>")
            << standard.at(0) + QString("\n<b>Hello</b>")
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("rich text into rich text")
            << standard.at(0) << QQuickTextEdit::RichText
            << 0 << 0
            << QString("<b>Hello</b>")
            << standard.at(0) + QChar(QChar::ParagraphSeparator) + QString("Hello")
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("rich text into auto text")
            << standard.at(0) << QQuickTextEdit::AutoText
            << 0 << 0
            << QString("<b>Hello</b>")
            << standard.at(0) + QString("\nHello")
            << 0 << 0 << 0
            << false << false;
}

void tst_qquicktextedit::append()
{
    QFETCH(QString, text);
    QFETCH(QQuickTextEdit::TextFormat, textFormat);
    QFETCH(int, selectionStart);
    QFETCH(int, selectionEnd);
    QFETCH(QString, appendText);
    QFETCH(QString, expectedText);
    QFETCH(int, expectedSelectionStart);
    QFETCH(int, expectedSelectionEnd);
    QFETCH(int, expectedCursorPosition);
    QFETCH(bool, selectionChanged);
    QFETCH(bool, cursorPositionChanged);

    QString componentStr = "import QtQuick 2.2\nTextEdit { text: \"" + text + "\" }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    textEdit->setTextFormat(textFormat);
    textEdit->select(selectionStart, selectionEnd);

    QSignalSpy selectionSpy(textEdit, SIGNAL(selectedTextChanged()));
    QSignalSpy selectionStartSpy(textEdit, SIGNAL(selectionStartChanged()));
    QSignalSpy selectionEndSpy(textEdit, SIGNAL(selectionEndChanged()));
    QSignalSpy textSpy(textEdit, SIGNAL(textChanged()));
    QSignalSpy cursorPositionSpy(textEdit, SIGNAL(cursorPositionChanged()));

    textEdit->append(appendText);

    if (textFormat == QQuickTextEdit::RichText || (textFormat == QQuickTextEdit::AutoText && (
            Qt::mightBeRichText(text) || Qt::mightBeRichText(appendText)))) {
        QCOMPARE(textEdit->getText(0, expectedText.length()), expectedText);
    } else {
        QCOMPARE(textEdit->text(), expectedText);

    }
    QCOMPARE(textEdit->length(), expectedText.length());

    QCOMPARE(textEdit->selectionStart(), expectedSelectionStart);
    QCOMPARE(textEdit->selectionEnd(), expectedSelectionEnd);
    QCOMPARE(textEdit->cursorPosition(), expectedCursorPosition);

    if (selectionStart > selectionEnd)
        qSwap(selectionStart, selectionEnd);

    QCOMPARE(selectionSpy.count() > 0, selectionChanged);
    QCOMPARE(selectionStartSpy.count() > 0, selectionStart != expectedSelectionStart);
    QCOMPARE(selectionEndSpy.count() > 0, selectionEnd != expectedSelectionEnd);
    QCOMPARE(textSpy.count() > 0, text != expectedText);
    QCOMPARE(cursorPositionSpy.count() > 0, cursorPositionChanged);
}

void tst_qquicktextedit::insert_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QQuickTextEdit::TextFormat>("textFormat");
    QTest::addColumn<int>("selectionStart");
    QTest::addColumn<int>("selectionEnd");
    QTest::addColumn<int>("insertPosition");
    QTest::addColumn<QString>("insertText");
    QTest::addColumn<QString>("expectedText");
    QTest::addColumn<int>("expectedSelectionStart");
    QTest::addColumn<int>("expectedSelectionEnd");
    QTest::addColumn<int>("expectedCursorPosition");
    QTest::addColumn<bool>("selectionChanged");
    QTest::addColumn<bool>("cursorPositionChanged");

    QTest::newRow("at cursor position (beginning)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0 << 0
            << QString("Hello")
            << QString("Hello") + standard.at(0)
            << 5 << 5 << 5
            << false << true;

    QTest::newRow("at cursor position (end)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << standard.at(0).length() << standard.at(0).length() << standard.at(0).length()
            << QString("Hello")
            << standard.at(0) + QString("Hello")
            << standard.at(0).length() + 5 << standard.at(0).length() + 5 << standard.at(0).length() + 5
            << false << true;

    QTest::newRow("at cursor position (middle)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 18 << 18 << 18
            << QString("Hello")
            << standard.at(0).mid(0, 18) + QString("Hello") + standard.at(0).mid(18)
            << 23 << 23 << 23
            << false << true;

    QTest::newRow("after cursor position (beginning)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0 << 18
            << QString("Hello")
            << standard.at(0).mid(0, 18) + QString("Hello") + standard.at(0).mid(18)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("before cursor position (end)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << standard.at(0).length() << standard.at(0).length() << 18
            << QString("Hello")
            << standard.at(0).mid(0, 18) + QString("Hello") + standard.at(0).mid(18)
            << standard.at(0).length() + 5 << standard.at(0).length() + 5 << standard.at(0).length() + 5
            << false << true;

    QTest::newRow("before cursor position (middle)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 18 << 18 << 0
            << QString("Hello")
            << QString("Hello") + standard.at(0)
            << 23 << 23 << 23
            << false << true;

    QTest::newRow("after cursor position (middle)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 18 << 18 << standard.at(0).length()
            << QString("Hello")
            << standard.at(0) + QString("Hello")
            << 18 << 18 << 18
            << false << false;

    QTest::newRow("before selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 14 << 19 << 0
            << QString("Hello")
            << QString("Hello") + standard.at(0)
            << 19 << 24 << 24
            << true << true;

    QTest::newRow("before reversed selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 19 << 14 << 0
            << QString("Hello")
            << QString("Hello") + standard.at(0)
            << 19 << 24 << 19
            << true << true;

    QTest::newRow("after selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 14 << 19 << standard.at(0).length()
            << QString("Hello")
            << standard.at(0) + QString("Hello")
            << 14 << 19 << 19
            << false << false;

    QTest::newRow("after reversed selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 19 << 14 << standard.at(0).length()
            << QString("Hello")
            << standard.at(0) + QString("Hello")
            << 14 << 19 << 14
            << false << false;

    QTest::newRow("into selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 14 << 19 << 18
            << QString("Hello")
            << standard.at(0).mid(0, 18) + QString("Hello") + standard.at(0).mid(18)
            << 14 << 24 << 24
            << true << true;

    QTest::newRow("into reversed selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 19 << 14 << 18
            << QString("Hello")
            << standard.at(0).mid(0, 18) + QString("Hello") + standard.at(0).mid(18)
            << 14 << 24 << 14
            << true << false;

    QTest::newRow("rich text into plain text")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0 << 0
            << QString("<b>Hello</b>")
            << QString("<b>Hello</b>") + standard.at(0)
            << 12 << 12 << 12
            << false << true;

    QTest::newRow("rich text into rich text")
            << standard.at(0) << QQuickTextEdit::RichText
            << 0 << 0 << 0
            << QString("<b>Hello</b>")
            << QString("Hello") + standard.at(0)
            << 5 << 5 << 5
            << false << true;

    QTest::newRow("rich text into auto text")
            << standard.at(0) << QQuickTextEdit::AutoText
            << 0 << 0 << 0
            << QString("<b>Hello</b>")
            << QString("Hello") + standard.at(0)
            << 5 << 5 << 5
            << false << true;

    QTest::newRow("before start")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0 << -3
            << QString("Hello")
            << standard.at(0)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("past end")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0 << standard.at(0).length() + 3
            << QString("Hello")
            << standard.at(0)
            << 0 << 0 << 0
            << false << false;
}

void tst_qquicktextedit::insert()
{
    QFETCH(QString, text);
    QFETCH(QQuickTextEdit::TextFormat, textFormat);
    QFETCH(int, selectionStart);
    QFETCH(int, selectionEnd);
    QFETCH(int, insertPosition);
    QFETCH(QString, insertText);
    QFETCH(QString, expectedText);
    QFETCH(int, expectedSelectionStart);
    QFETCH(int, expectedSelectionEnd);
    QFETCH(int, expectedCursorPosition);
    QFETCH(bool, selectionChanged);
    QFETCH(bool, cursorPositionChanged);

    QString componentStr = "import QtQuick 2.0\nTextEdit { text: \"" + text + "\" }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    textEdit->setTextFormat(textFormat);
    textEdit->select(selectionStart, selectionEnd);

    QSignalSpy selectionSpy(textEdit, SIGNAL(selectedTextChanged()));
    QSignalSpy selectionStartSpy(textEdit, SIGNAL(selectionStartChanged()));
    QSignalSpy selectionEndSpy(textEdit, SIGNAL(selectionEndChanged()));
    QSignalSpy textSpy(textEdit, SIGNAL(textChanged()));
    QSignalSpy cursorPositionSpy(textEdit, SIGNAL(cursorPositionChanged()));

    textEdit->insert(insertPosition, insertText);

    if (textFormat == QQuickTextEdit::RichText || (textFormat == QQuickTextEdit::AutoText && (
            Qt::mightBeRichText(text) || Qt::mightBeRichText(insertText)))) {
        QCOMPARE(textEdit->getText(0, expectedText.length()), expectedText);
    } else {
        QCOMPARE(textEdit->text(), expectedText);

    }
    QCOMPARE(textEdit->length(), expectedText.length());

    QCOMPARE(textEdit->selectionStart(), expectedSelectionStart);
    QCOMPARE(textEdit->selectionEnd(), expectedSelectionEnd);
    QCOMPARE(textEdit->cursorPosition(), expectedCursorPosition);

    if (selectionStart > selectionEnd)
        qSwap(selectionStart, selectionEnd);

    QCOMPARE(selectionSpy.count() > 0, selectionChanged);
    QCOMPARE(selectionStartSpy.count() > 0, selectionStart != expectedSelectionStart);
    QCOMPARE(selectionEndSpy.count() > 0, selectionEnd != expectedSelectionEnd);
    QCOMPARE(textSpy.count() > 0, text != expectedText);
    QCOMPARE(cursorPositionSpy.count() > 0, cursorPositionChanged);
}

void tst_qquicktextedit::remove_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QQuickTextEdit::TextFormat>("textFormat");
    QTest::addColumn<int>("selectionStart");
    QTest::addColumn<int>("selectionEnd");
    QTest::addColumn<int>("removeStart");
    QTest::addColumn<int>("removeEnd");
    QTest::addColumn<QString>("expectedText");
    QTest::addColumn<int>("expectedSelectionStart");
    QTest::addColumn<int>("expectedSelectionEnd");
    QTest::addColumn<int>("expectedCursorPosition");
    QTest::addColumn<bool>("selectionChanged");
    QTest::addColumn<bool>("cursorPositionChanged");

    const QString richBoldText = QStringLiteral("This is some <b>bold</b> text");
    const QString plainBoldText = QStringLiteral("This is some bold text");

    QTest::newRow("from cursor position (beginning)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0
            << 0 << 5
            << standard.at(0).mid(5)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("to cursor position (beginning)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0
            << 5 << 0
            << standard.at(0).mid(5)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("to cursor position (end)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << standard.at(0).length() << standard.at(0).length()
            << standard.at(0).length() << standard.at(0).length() - 5
            << standard.at(0).mid(0, standard.at(0).length() - 5)
            << standard.at(0).length() - 5 << standard.at(0).length() - 5 << standard.at(0).length() - 5
            << false << true;

    QTest::newRow("to cursor position (end)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << standard.at(0).length() << standard.at(0).length()
            << standard.at(0).length() - 5 << standard.at(0).length()
            << standard.at(0).mid(0, standard.at(0).length() - 5)
            << standard.at(0).length() - 5 << standard.at(0).length() - 5 << standard.at(0).length() - 5
            << false << true;

    QTest::newRow("from cursor position (middle)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 18 << 18
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 18 << 18 << 18
            << false << false;

    QTest::newRow("to cursor position (middle)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 23 << 23
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 18 << 18 << 18
            << false << true;

    QTest::newRow("after cursor position (beginning)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("before cursor position (end)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << standard.at(0).length() << standard.at(0).length()
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << standard.at(0).length() - 5 << standard.at(0).length() - 5 << standard.at(0).length() - 5
            << false << true;

    QTest::newRow("before cursor position (middle)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 23 << 23
            << 0 << 5
            << standard.at(0).mid(5)
            << 18 << 18 << 18
            << false << true;

    QTest::newRow("after cursor position (middle)")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 18 << 18
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 18 << 18 << 18
            << false << false;

    QTest::newRow("before selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 14 << 19
            << 0 << 5
            << standard.at(0).mid(5)
            << 9 << 14 << 14
            << true << true;

    QTest::newRow("before reversed selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 19 << 14
            << 0 << 5
            << standard.at(0).mid(5)
            << 9 << 14 << 9
            << true << true;

    QTest::newRow("after selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 14 << 19
            << standard.at(0).length() - 5 << standard.at(0).length()
            << standard.at(0).mid(0, standard.at(0).length() - 5)
            << 14 << 19 << 19
            << false << false;

    QTest::newRow("after reversed selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 19 << 14
            << standard.at(0).length() - 5 << standard.at(0).length()
            << standard.at(0).mid(0, standard.at(0).length() - 5)
            << 14 << 19 << 14
            << false << false;

    QTest::newRow("from selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 14 << 24
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 14 << 19 << 19
            << true << true;

    QTest::newRow("from reversed selection")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 24 << 14
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 14 << 19 << 14
            << true << false;

    QTest::newRow("plain text cropped beginning")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0
            << -3 << 4
            << standard.at(0).mid(4)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("plain text cropped end")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0
            << 23 << standard.at(0).length() + 8
            << standard.at(0).mid(0, 23)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("plain text cropped beginning and end")
            << standard.at(0) << QQuickTextEdit::PlainText
            << 0 << 0
            << -9 << standard.at(0).length() + 4
            << QString()
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("all rich text")
            << richBoldText << QQuickTextEdit::RichText
            << 0 << 0
            << 0 << plainBoldText.length()
            << QString()
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("rick text sub string")
            << richBoldText << QQuickTextEdit::RichText
            << 0 << 0
            << 14 << 21
            << plainBoldText.mid(0, 14) + plainBoldText.mid(21)
            << 0 << 0 << 0
            << false << false;
}

void tst_qquicktextedit::remove()
{
    QFETCH(QString, text);
    QFETCH(QQuickTextEdit::TextFormat, textFormat);
    QFETCH(int, selectionStart);
    QFETCH(int, selectionEnd);
    QFETCH(int, removeStart);
    QFETCH(int, removeEnd);
    QFETCH(QString, expectedText);
    QFETCH(int, expectedSelectionStart);
    QFETCH(int, expectedSelectionEnd);
    QFETCH(int, expectedCursorPosition);
    QFETCH(bool, selectionChanged);
    QFETCH(bool, cursorPositionChanged);

    QString componentStr = "import QtQuick 2.0\nTextEdit { text: \"" + text + "\" }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    textEdit->setTextFormat(textFormat);
    textEdit->select(selectionStart, selectionEnd);

    QSignalSpy selectionSpy(textEdit, SIGNAL(selectedTextChanged()));
    QSignalSpy selectionStartSpy(textEdit, SIGNAL(selectionStartChanged()));
    QSignalSpy selectionEndSpy(textEdit, SIGNAL(selectionEndChanged()));
    QSignalSpy textSpy(textEdit, SIGNAL(textChanged()));
    QSignalSpy cursorPositionSpy(textEdit, SIGNAL(cursorPositionChanged()));

    textEdit->remove(removeStart, removeEnd);

    if (textFormat == QQuickTextEdit::RichText
            || (textFormat == QQuickTextEdit::AutoText && Qt::mightBeRichText(text))) {
        QCOMPARE(textEdit->getText(0, expectedText.length()), expectedText);
    } else {
        QCOMPARE(textEdit->text(), expectedText);
    }
    QCOMPARE(textEdit->length(), expectedText.length());

    if (selectionStart > selectionEnd)  //
        qSwap(selectionStart, selectionEnd);

    QCOMPARE(textEdit->selectionStart(), expectedSelectionStart);
    QCOMPARE(textEdit->selectionEnd(), expectedSelectionEnd);
    QCOMPARE(textEdit->cursorPosition(), expectedCursorPosition);

    QCOMPARE(selectionSpy.count() > 0, selectionChanged);
    QCOMPARE(selectionStartSpy.count() > 0, selectionStart != expectedSelectionStart);
    QCOMPARE(selectionEndSpy.count() > 0, selectionEnd != expectedSelectionEnd);
    QCOMPARE(textSpy.count() > 0, text != expectedText);


    if (cursorPositionChanged)  //
        QVERIFY(cursorPositionSpy.count() > 0);
}


void tst_qquicktextedit::keySequence_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QKeySequence>("sequence");
    QTest::addColumn<int>("selectionStart");
    QTest::addColumn<int>("selectionEnd");
    QTest::addColumn<int>("cursorPosition");
    QTest::addColumn<QString>("expectedText");
    QTest::addColumn<QString>("selectedText");
    QTest::addColumn<Qt::Key>("layoutDirection");

    // standard[0] == "the [4]quick [10]brown [16]fox [20]jumped [27]over [32]the [36]lazy [41]dog"

    QTest::newRow("select all")
            << standard.at(0) << QKeySequence(QKeySequence::SelectAll) << 0 << 0
            << 44 << standard.at(0) << standard.at(0)
            << Qt::Key_Direction_L;
    QTest::newRow("select start of line")
            << standard.at(0) << QKeySequence(QKeySequence::SelectStartOfLine) << 5 << 5
            << 0 << standard.at(0) << standard.at(0).mid(0, 5)
            << Qt::Key_Direction_L;
    QTest::newRow("select start of block")
            << standard.at(0) << QKeySequence(QKeySequence::SelectStartOfBlock) << 5 << 5
            << 0 << standard.at(0) << standard.at(0).mid(0, 5)
            << Qt::Key_Direction_L;
    QTest::newRow("select end of line")
            << standard.at(0) << QKeySequence(QKeySequence::SelectEndOfLine) << 5 << 5
            << 44 << standard.at(0) << standard.at(0).mid(5)
            << Qt::Key_Direction_L;
    QTest::newRow("select end of document")
            << standard.at(0) << QKeySequence(QKeySequence::SelectEndOfDocument) << 3 << 3
            << 44 << standard.at(0) << standard.at(0).mid(3)
            << Qt::Key_Direction_L;
    QTest::newRow("select end of block")
            << standard.at(0) << QKeySequence(QKeySequence::SelectEndOfBlock) << 18 << 18
            << 44 << standard.at(0) << standard.at(0).mid(18)
            << Qt::Key_Direction_L;
    QTest::newRow("delete end of line")
            << standard.at(0) << QKeySequence(QKeySequence::DeleteEndOfLine) << 24 << 24
            << 24 << standard.at(0).mid(0, 24) << QString()
            << Qt::Key_Direction_L;
    QTest::newRow("move to start of line")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToStartOfLine) << 31 << 31
            << 0 << standard.at(0) << QString()
            << Qt::Key_Direction_L;
    QTest::newRow("move to start of block")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToStartOfBlock) << 25 << 25
            << 0 << standard.at(0) << QString()
            << Qt::Key_Direction_L;
    QTest::newRow("move to next char")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToNextChar) << 12 << 12
            << 13 << standard.at(0) << QString()
            << Qt::Key_Direction_L;
    QTest::newRow("move to previous char (ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousChar) << 3 << 3
            << 2 << standard.at(0) << QString()
            << Qt::Key_Direction_L;
    QTest::newRow("move to previous char (rtl)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousChar) << 3 << 3
            << 4 << standard.at(0) << QString()
            << Qt::Key_Direction_R;
    QTest::newRow("move to previous char with selection")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousChar) << 3 << 7
            << 3 << standard.at(0) << QString()
            << Qt::Key_Direction_L;
    QTest::newRow("select next char (ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::SelectNextChar) << 23 << 23
            << 24 << standard.at(0) << standard.at(0).mid(23, 1)
            << Qt::Key_Direction_L;
    QTest::newRow("select next char (rtl)")
            << standard.at(0) << QKeySequence(QKeySequence::SelectNextChar) << 23 << 23
            << 22 << standard.at(0) << standard.at(0).mid(22, 1)
            << Qt::Key_Direction_R;
    QTest::newRow("select previous char (ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::SelectPreviousChar) << 19 << 19
            << 18 << standard.at(0) << standard.at(0).mid(18, 1)
            << Qt::Key_Direction_L;
    QTest::newRow("select previous char (rtl)")
            << standard.at(0) << QKeySequence(QKeySequence::SelectPreviousChar) << 19 << 19
            << 20 << standard.at(0) << standard.at(0).mid(19, 1)
            << Qt::Key_Direction_R;
    QTest::newRow("move to next word (ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToNextWord) << 7 << 7
            << 10 << standard.at(0) << QString()
            << Qt::Key_Direction_L;
    QTest::newRow("move to next word (rtl)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToNextWord) << 7 << 7
            << 4 << standard.at(0) << QString()
            << Qt::Key_Direction_R;
    QTest::newRow("move to previous word (ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousWord) << 7 << 7
            << 4 << standard.at(0) << QString()
            << Qt::Key_Direction_L;
    QTest::newRow("move to previous word (rlt)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousWord) << 7 << 7
            << 10 << standard.at(0) << QString()
            << Qt::Key_Direction_R;
    QTest::newRow("select next word")
            << standard.at(0) << QKeySequence(QKeySequence::SelectNextWord) << 11 << 11
            << 16 << standard.at(0) << standard.at(0).mid(11, 5)
            << Qt::Key_Direction_L;
    QTest::newRow("select previous word")
            << standard.at(0) << QKeySequence(QKeySequence::SelectPreviousWord) << 11 << 11
            << 10 << standard.at(0) << standard.at(0).mid(10, 1)
            << Qt::Key_Direction_L;
    QTest::newRow("delete (selection)")
            << standard.at(0) << QKeySequence(QKeySequence::Delete) << 12 << 15
            << 12 << (standard.at(0).mid(0, 12) + standard.at(0).mid(15)) << QString()
            << Qt::Key_Direction_L;
    QTest::newRow("delete (no selection)")
            << standard.at(0) << QKeySequence(QKeySequence::Delete) << 15 << 15
            << 15 << (standard.at(0).mid(0, 15) + standard.at(0).mid(16)) << QString()
            << Qt::Key_Direction_L;
    QTest::newRow("delete end of word")
            << standard.at(0) << QKeySequence(QKeySequence::DeleteEndOfWord) << 24 << 24
            << 24 << (standard.at(0).mid(0, 24) + standard.at(0).mid(27)) << QString()
            << Qt::Key_Direction_L;
    QTest::newRow("delete start of word")
            << standard.at(0) << QKeySequence(QKeySequence::DeleteStartOfWord) << 7 << 7
            << 4 << (standard.at(0).mid(0, 4) + standard.at(0).mid(7)) << QString()
            << Qt::Key_Direction_L;
}

void tst_qquicktextedit::keySequence()
{
    QFETCH(QString, text);
    QFETCH(QKeySequence, sequence);
    QFETCH(int, selectionStart);
    QFETCH(int, selectionEnd);
    QFETCH(int, cursorPosition);
    QFETCH(QString, expectedText);
    QFETCH(QString, selectedText);
    QFETCH(Qt::Key, layoutDirection);

    if (sequence.isEmpty()) {
        QSKIP("Key sequence is undefined");
    }

    QString componentStr = "import QtQuick 2.0\nTextEdit { focus: true; text: \"" + text + "\" }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    QQuickWindow window;
    textEdit->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(textEdit->hasActiveFocus());

    simulateKey(&window, layoutDirection);

    textEdit->select(selectionStart, selectionEnd);

    simulateKeys(&window, sequence);

    QCOMPARE(textEdit->cursorPosition(), cursorPosition);
    QCOMPARE(textEdit->text(), expectedText);
    QCOMPARE(textEdit->selectedText(), selectedText);
}

#define NORMAL 0
#define REPLACE_UNTIL_END 1

void tst_qquicktextedit::undo_data()
{
    QTest::addColumn<QStringList>("insertString");
    QTest::addColumn<IntList>("insertIndex");
    QTest::addColumn<IntList>("insertMode");
    QTest::addColumn<QStringList>("expectedString");
    QTest::addColumn<bool>("use_keys");

    for (int i=0; i<2; i++) {
        QString keys_str = "keyboard";
        bool use_keys = true;
        if (i==0) {
            keys_str = "insert";
            use_keys = false;
        }

        {
            IntList insertIndex;
            IntList insertMode;
            QStringList insertString;
            QStringList expectedString;

            insertIndex << -1;
            insertMode << NORMAL;
            insertString << "1";

            insertIndex << -1;
            insertMode << NORMAL;
            insertString << "5";

            insertIndex << 1;
            insertMode << NORMAL;
            insertString << "3";

            insertIndex << 1;
            insertMode << NORMAL;
            insertString << "2";

            insertIndex << 3;
            insertMode << NORMAL;
            insertString << "4";

            expectedString << "12345";
            expectedString << "1235";
            expectedString << "135";
            expectedString << "15";
            expectedString << "";

            QTest::newRow(QString(keys_str + "_numbers").toLatin1()) <<
                insertString <<
                insertIndex <<
                insertMode <<
                expectedString <<
                bool(use_keys);
        }
        {
            IntList insertIndex;
            IntList insertMode;
            QStringList insertString;
            QStringList expectedString;

            insertIndex << -1;
            insertMode << NORMAL;
            insertString << "World"; // World

            insertIndex << 0;
            insertMode << NORMAL;
            insertString << "Hello"; // HelloWorld

            insertIndex << 0;
            insertMode << NORMAL;
            insertString << "Well"; // WellHelloWorld

            insertIndex << 9;
            insertMode << NORMAL;
            insertString << "There"; // WellHelloThereWorld;

            expectedString << "WellHelloThereWorld";
            expectedString << "WellHelloWorld";
            expectedString << "HelloWorld";
            expectedString << "World";
            expectedString << "";

            QTest::newRow(QString(keys_str + "_helloworld").toLatin1()) <<
                insertString <<
                insertIndex <<
                insertMode <<
                expectedString <<
                bool(use_keys);
        }
        {
            IntList insertIndex;
            IntList insertMode;
            QStringList insertString;
            QStringList expectedString;

            insertIndex << -1;
            insertMode << NORMAL;
            insertString << "Ensuring";

            insertIndex << -1;
            insertMode << NORMAL;
            insertString << " instan";

            insertIndex << 9;
            insertMode << NORMAL;
            insertString << "an ";

            insertIndex << 10;
            insertMode << REPLACE_UNTIL_END;
            insertString << " unique instance.";

            expectedString << "Ensuring a unique instance.";
            expectedString << "Ensuring an instan";
            expectedString << "Ensuring instan";
            expectedString << "";

            QTest::newRow(QString(keys_str + "_patterns").toLatin1()) <<
                insertString <<
                insertIndex <<
                insertMode <<
                expectedString <<
                bool(use_keys);
        }
    }
}

void tst_qquicktextedit::undo()
{
    QFETCH(QStringList, insertString);
    QFETCH(IntList, insertIndex);
    QFETCH(IntList, insertMode);
    QFETCH(QStringList, expectedString);

    QString componentStr = "import QtQuick 2.0\nTextEdit { focus: true }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    QQuickWindow window;
    textEdit->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(textEdit->hasActiveFocus());
    QVERIFY(!textEdit->canUndo());

    QSignalSpy spy(textEdit, SIGNAL(canUndoChanged()));

    int i;

// STEP 1: First build up an undo history by inserting or typing some strings...
    for (i = 0; i < insertString.size(); ++i) {
        if (insertIndex[i] > -1)
            textEdit->setCursorPosition(insertIndex[i]);

 // experimental stuff
        if (insertMode[i] == REPLACE_UNTIL_END) {
            textEdit->select(insertIndex[i], insertIndex[i] + 8);

            // This is what I actually want...
            // QTest::keyClick(testWidget, Qt::Key_End, Qt::ShiftModifier);
        }

        for (int j = 0; j < insertString.at(i).length(); j++)
            QTest::keyClick(&window, insertString.at(i).at(j).toLatin1());
    }

    QCOMPARE(spy.count(), 1);

// STEP 2: Next call undo several times and see if we can restore to the previous state
    for (i = 0; i < expectedString.size() - 1; ++i) {
        QCOMPARE(textEdit->text(), expectedString[i]);
        QVERIFY(textEdit->canUndo());
        textEdit->undo();
    }

// STEP 3: Verify that we have undone everything
    QVERIFY(textEdit->text().isEmpty());
    QVERIFY(!textEdit->canUndo());
    QCOMPARE(spy.count(), 2);
}

void tst_qquicktextedit::redo_data()
{
    QTest::addColumn<QStringList>("insertString");
    QTest::addColumn<IntList>("insertIndex");
    QTest::addColumn<QStringList>("expectedString");

    {
        IntList insertIndex;
        QStringList insertString;
        QStringList expectedString;

        insertIndex << -1;
        insertString << "World"; // World
        insertIndex << 0;
        insertString << "Hello"; // HelloWorld
        insertIndex << 0;
        insertString << "Well"; // WellHelloWorld
        insertIndex << 9;
        insertString << "There"; // WellHelloThereWorld;

        expectedString << "World";
        expectedString << "HelloWorld";
        expectedString << "WellHelloWorld";
        expectedString << "WellHelloThereWorld";

        QTest::newRow("Inserts and setting cursor") << insertString << insertIndex << expectedString;
    }
}

void tst_qquicktextedit::redo()
{
    QFETCH(QStringList, insertString);
    QFETCH(IntList, insertIndex);
    QFETCH(QStringList, expectedString);

    QString componentStr = "import QtQuick 2.0\nTextEdit { focus: true }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    QQuickWindow window;
    textEdit->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(textEdit->hasActiveFocus());

    QVERIFY(!textEdit->canUndo());
    QVERIFY(!textEdit->canRedo());

    QSignalSpy spy(textEdit, SIGNAL(canRedoChanged()));

    int i;
    // inserts the diff strings at diff positions
    for (i = 0; i < insertString.size(); ++i) {
        if (insertIndex[i] > -1)
            textEdit->setCursorPosition(insertIndex[i]);
        for (int j = 0; j < insertString.at(i).length(); j++)
            QTest::keyClick(&window, insertString.at(i).at(j).toLatin1());
        QVERIFY(textEdit->canUndo());
        QVERIFY(!textEdit->canRedo());
    }

    QCOMPARE(spy.count(), 0);

    // undo everything
    while (!textEdit->text().isEmpty()) {
        QVERIFY(textEdit->canUndo());
        textEdit->undo();
        QVERIFY(textEdit->canRedo());
    }

    QCOMPARE(spy.count(), 1);

    for (i = 0; i < expectedString.size(); ++i) {
        QVERIFY(textEdit->canRedo());
        textEdit->redo();
        QCOMPARE(textEdit->text() , expectedString[i]);
        QVERIFY(textEdit->canUndo());
    }
    QVERIFY(!textEdit->canRedo());
    QCOMPARE(spy.count(), 2);
}

void tst_qquicktextedit::undo_keypressevents_data()
{
    QTest::addColumn<KeyList>("keys");
    QTest::addColumn<QStringList>("expectedString");

    {
        KeyList keys;
        QStringList expectedString;

        keys << "AFRAID"
                << Qt::Key_Home
                << "VERY"
                << Qt::Key_Left
                << Qt::Key_Left
                << Qt::Key_Left
                << Qt::Key_Left
                << "BE"
                << Qt::Key_End
                << "!";

        expectedString << "BEVERYAFRAID!";
        expectedString << "BEVERYAFRAID";
        expectedString << "VERYAFRAID";
        expectedString << "AFRAID";

        QTest::newRow("Inserts and moving cursor") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        // inserting '1234'
        keys << "1234" << Qt::Key_Home
                // skipping '12'
                << Qt::Key_Right << Qt::Key_Right
                // selecting '34'
                << (Qt::Key_Right | Qt::ShiftModifier) << (Qt::Key_Right | Qt::ShiftModifier)
                // deleting '34'
                << Qt::Key_Delete;

        expectedString << "12";
        expectedString << "1234";

        QTest::newRow("Inserts,moving,selection and delete") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        // inserting 'AB12'
        keys << "AB12"
                << Qt::Key_Home
                // selecting 'AB'
                << (Qt::Key_Right | Qt::ShiftModifier) << (Qt::Key_Right | Qt::ShiftModifier)
                << Qt::Key_Delete
                << QKeySequence::Undo
                // ### Text is selected in text input
//                << Qt::Key_Right
                << (Qt::Key_Right | Qt::ShiftModifier) << (Qt::Key_Right | Qt::ShiftModifier)
                << Qt::Key_Delete;

        expectedString << "AB";
        expectedString << "AB12";

        QTest::newRow("Inserts,moving,selection, delete and undo") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        // inserting 'ABCD'
        keys << "abcd"
                //move left two
                << Qt::Key_Left << Qt::Key_Left
                // inserting '1234'
                << "1234"
                // selecting '1234'
                << (Qt::Key_Left | Qt::ShiftModifier) << (Qt::Key_Left | Qt::ShiftModifier) << (Qt::Key_Left | Qt::ShiftModifier) << (Qt::Key_Left | Qt::ShiftModifier)
                // overwriting '1234' with '5'
                << "5"
                // undoing deletion of 'AB'
                << QKeySequence::Undo
                // ### Text is selected in text input
                << (Qt::Key_Left | Qt::ShiftModifier) << (Qt::Key_Left | Qt::ShiftModifier) << (Qt::Key_Left | Qt::ShiftModifier) << (Qt::Key_Left | Qt::ShiftModifier)
                // overwriting '1234' with '6'
                << "6";

        expectedString << "ab6cd";
        // for versions previous to 3.2 we overwrite needed two undo operations
        expectedString << "ab1234cd";
        expectedString << "abcd";

        QTest::newRow("Inserts,moving,selection and undo, removing selection") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        // inserting 'ABC'
        keys << "ABC"
                // removes 'C'
                << Qt::Key_Backspace;

        expectedString << "AB";
        expectedString << "ABC";

        QTest::newRow("Inserts,backspace") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        keys << "ABC"
                // removes 'C'
                << Qt::Key_Backspace
                // inserting 'Z'
                << "Z";

        expectedString << "ABZ";
        expectedString << "AB";
        expectedString << "ABC";

        QTest::newRow("Inserts,backspace,inserts") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        // inserting '123'
        keys << "123" << Qt::Key_Home
            // selecting '123'
             << (Qt::Key_End | Qt::ShiftModifier)
            // overwriting '123' with 'ABC'
             << "ABC";

        expectedString << "ABC";
        expectedString << "123";

        QTest::newRow("Inserts,moving,selection and overwriting") << keys << expectedString;
    }

    bool canCopyPaste = PlatformQuirks::isClipboardAvailable();

    if (canCopyPaste) {
        KeyList keys;
        keys    << "123"
                << QKeySequence(QKeySequence::SelectStartOfLine)
                << QKeySequence(QKeySequence::Cut)
                << "ABC"
                << QKeySequence(QKeySequence::Paste);
        QStringList expectedString = QStringList()
                << "ABC123"
                << "ABC"
                << ""
                << "123";
        QTest::newRow("Cut,paste") << keys << expectedString;
    }
    if (canCopyPaste) {
        KeyList keys;
        keys    << "123"
                << QKeySequence(QKeySequence::SelectStartOfLine)
                << QKeySequence(QKeySequence::Copy)
                << "ABC"
                << QKeySequence(QKeySequence::Paste);
        QStringList expectedString = QStringList()
                << "ABC123"
                << "ABC"
                << "123";
        QTest::newRow("Copy,paste") << keys << expectedString;
    }
}

void tst_qquicktextedit::undo_keypressevents()
{
    QFETCH(KeyList, keys);
    QFETCH(QStringList, expectedString);

    QString componentStr = "import QtQuick 2.0\nTextEdit { focus: true }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    QQuickWindow window;
    textEdit->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(textEdit->hasActiveFocus());

    simulateKeys(&window, keys);

    for (int i = 0; i < expectedString.size(); ++i) {
        QCOMPARE(textEdit->text() , expectedString[i]);
        textEdit->undo();
    }
    QVERIFY(textEdit->text().isEmpty());
}

void tst_qquicktextedit::clear()
{
    QString componentStr = "import QtQuick 2.0\nTextEdit { focus: true }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    QQuickWindow window;
    textEdit->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(textEdit->hasActiveFocus());

    QSignalSpy spy(textEdit, SIGNAL(canUndoChanged()));

    textEdit->setText("I am Legend");
    QCOMPARE(textEdit->text(), QString("I am Legend"));
    textEdit->clear();
    QVERIFY(textEdit->text().isEmpty());

    QCOMPARE(spy.count(), 1);

    // checks that clears can be undone
    textEdit->undo();
    QVERIFY(!textEdit->canUndo());
    QCOMPARE(spy.count(), 2);
    QCOMPARE(textEdit->text(), QString("I am Legend"));

    textEdit->setCursorPosition(4);
    QInputMethodEvent preeditEvent("PREEDIT", QList<QInputMethodEvent::Attribute>());
    QGuiApplication::sendEvent(textEdit, &preeditEvent);
    QCOMPARE(textEdit->text(), QString("I am Legend"));
    QCOMPARE(textEdit->preeditText(), QString("PREEDIT"));

    textEdit->clear();
    QVERIFY(textEdit->text().isEmpty());

    QCOMPARE(spy.count(), 3);

    // checks that clears can be undone
    textEdit->undo();
    QVERIFY(!textEdit->canUndo());
    QCOMPARE(spy.count(), 4);
    QCOMPARE(textEdit->text(), QString("I am Legend"));

    textEdit->setText(QString("<i>I am Legend</i>"));
    QCOMPARE(textEdit->text(), QString("<i>I am Legend</i>"));
    textEdit->clear();
    QVERIFY(textEdit->text().isEmpty());

    QCOMPARE(spy.count(), 5);

    // checks that clears can be undone
    textEdit->undo();
    QCOMPARE(spy.count(), 6);
    QCOMPARE(textEdit->text(), QString("<i>I am Legend</i>"));
}

void tst_qquicktextedit::baseUrl()
{
    QUrl localUrl("file:///tests/text.qml");
    QUrl remoteUrl("http://www.qt-project.org/test.qml");

    QQmlComponent textComponent(&engine);
    textComponent.setData("import QtQuick 2.0\n TextEdit {}", localUrl);
    QQuickTextEdit *textObject = qobject_cast<QQuickTextEdit *>(textComponent.create());

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

void tst_qquicktextedit::embeddedImages_data()
{
    QTest::addColumn<QUrl>("qmlfile");
    QTest::addColumn<QString>("error");
    QTest::newRow("local") << testFileUrl("embeddedImagesLocal.qml") << "";
    QTest::newRow("local-error") << testFileUrl("embeddedImagesLocalError.qml")
        << testFileUrl("embeddedImagesLocalError.qml").toString()+":3:1: QML TextEdit: Cannot open: " + testFileUrl("http/notexists.png").toString();
    QTest::newRow("local") << testFileUrl("embeddedImagesLocalRelative.qml") << "";
    QTest::newRow("remote") << testFileUrl("embeddedImagesRemote.qml") << "";
    QTest::newRow("remote-error") << testFileUrl("embeddedImagesRemoteError.qml")
        << testFileUrl("embeddedImagesRemoteError.qml").toString()+":3:1: QML TextEdit: Error transferring {{ServerBaseUrl}}/notexists.png - server replied: Not found";
    QTest::newRow("remote") << testFileUrl("embeddedImagesRemoteRelative.qml") << "";
}

void tst_qquicktextedit::embeddedImages()
{
    QFETCH(QUrl, qmlfile);
    QFETCH(QString, error);

    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(testFile("http"));

    error.replace(QStringLiteral("{{ServerBaseUrl}}"), server.baseUrl().toString());

    if (!error.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, error.toLatin1());

    QQmlComponent textComponent(&engine, qmlfile);
    QQuickTextEdit *textObject = qobject_cast<QQuickTextEdit*>(textComponent.beginCreate(engine.rootContext()));
    QVERIFY(textObject != 0);

    const int baseUrlPropertyIndex = textObject->metaObject()->indexOfProperty("serverBaseUrl");
    if (baseUrlPropertyIndex != -1) {
        QMetaProperty prop = textObject->metaObject()->property(baseUrlPropertyIndex);
        QVERIFY(prop.write(textObject, server.baseUrl().toString()));
    }

    textComponent.completeCreate();

    QTRY_COMPARE(QQuickTextEditPrivate::get(textObject)->document->resourcesLoading(), 0);

    QPixmap pm(testFile("http/exists.png"));
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

void tst_qquicktextedit::emptytags_QTBUG_22058()
{
    QQuickView window(testFileUrl("qtbug-22058.qml"));
    QVERIFY(window.rootObject() != 0);

    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QQuickTextEdit *input = qobject_cast<QQuickTextEdit *>(qvariant_cast<QObject *>(window.rootObject()->property("inputField")));
    QVERIFY(input->hasActiveFocus());

    QInputMethodEvent event("", QList<QInputMethodEvent::Attribute>());
    event.setCommitString("<b>Bold<");
    QGuiApplication::sendEvent(input, &event);
    QCOMPARE(input->text(), QString("<b>Bold<"));
    event.setCommitString(">");
    QGuiApplication::sendEvent(input, &event);
    QCOMPARE(input->text(), QString("<b>Bold<>"));
}

void tst_qquicktextedit::cursorRectangle_QTBUG_38947()
{
    QQuickView window(testFileUrl("qtbug-38947.qml"));

    window.show();
    window.requestActivate();
    QTest::qWaitForWindowExposed(&window);
    QQuickTextEdit *edit = window.rootObject()->findChild<QQuickTextEdit *>("textedit");
    QVERIFY(edit);

    QPoint from = edit->positionToRectangle(0).center().toPoint();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, from);

    QSignalSpy spy(edit, SIGNAL(cursorRectangleChanged()));
    QVERIFY(spy.isValid());

    for (int i = 1; i < edit->length() - 1; ++i) {
        QRectF rect = edit->positionToRectangle(i);
        QTest::mouseMove(&window, rect.center().toPoint());
        QCOMPARE(edit->cursorRectangle(), rect);
        QCOMPARE(spy.count(), i);
    }

    QPoint to = edit->positionToRectangle(edit->length() - 1).center().toPoint();
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, to);
}

void tst_qquicktextedit::textCached_QTBUG_41583()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nTextEdit { property int margin: 10; text: \"TextEdit\"; textMargin: margin; property bool empty: !text; }", QUrl());
    QQuickTextEdit *textedit = qobject_cast<QQuickTextEdit*>(component.create());
    QVERIFY(textedit);
    QCOMPARE(textedit->textMargin(), qreal(10.0));
    QCOMPARE(textedit->text(), QString("TextEdit"));
    QVERIFY(!textedit->property("empty").toBool());
}

void tst_qquicktextedit::doubleSelect_QTBUG_38704()
{
    QString componentStr = "import QtQuick 2.2\nTextEdit { text: \"TextEdit\" }";
    QQmlComponent textEditComponent(&engine);
    textEditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(textEditComponent.create());
    QVERIFY(textEdit != 0);

    QSignalSpy selectionSpy(textEdit, SIGNAL(selectedTextChanged()));

    textEdit->select(0,1); //Select some text initially
    QCOMPARE(selectionSpy.count(), 1);
    textEdit->select(0,1); //No change to selection start/end
    QCOMPARE(selectionSpy.count(), 1);
    textEdit->select(0,2); //Change selection end
    QCOMPARE(selectionSpy.count(), 2);
    textEdit->select(1,2); //Change selection start
    QCOMPARE(selectionSpy.count(), 3);
}

void tst_qquicktextedit::padding()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("padding.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QQuickItem *root = window->rootObject();
    QVERIFY(root);
    QQuickTextEdit *obj = qobject_cast<QQuickTextEdit*>(root);
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

void tst_qquicktextedit::QTBUG_51115_readOnlyResetsSelection()
{
    QQuickView view;
    view.setSource(testFileUrl("qtbug51115.qml"));
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QQuickTextEdit *obj = qobject_cast<QQuickTextEdit*>(view.rootObject());

    QCOMPARE(obj->selectedText(), QString());
}

QTEST_MAIN(tst_qquicktextedit)

#include "tst_qquicktextedit.moc"
