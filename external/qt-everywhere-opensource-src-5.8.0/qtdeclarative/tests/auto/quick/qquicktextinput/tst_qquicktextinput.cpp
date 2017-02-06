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
#include "../../shared/util.h"
#include "../../shared/testhttpserver.h"
#include <private/qinputmethod_p.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlexpression.h>
#include <QFile>
#include <QtQuick/qquickview.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qvalidator.h>
#include <QInputMethod>
#include <private/qquicktextinput_p.h>
#include <private/qquicktextinput_p_p.h>
#include <private/qquickvalidator_p.h>
#include <QDebug>
#include <QDir>
#include <math.h>
#include <qmath.h>

#ifdef Q_OS_OSX
#include <Carbon/Carbon.h>
#endif

#include "qplatformdefs.h"
#include "../../shared/platformquirks.h"
#include "../../shared/platforminputcontext.h"

Q_DECLARE_METATYPE(QQuickTextInput::SelectionMode)
Q_DECLARE_METATYPE(QQuickTextInput::EchoMode)
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

template <typename T> static T evaluate(QObject *scope, const QString &expression)
{
    QQmlExpression expr(qmlContext(scope), scope, expression);
    T result = expr.evaluate().value<T>();
    if (expr.hasError())
        qWarning() << expr.error().toString();
    return result;
}

template<typename T, int N> int lengthOf(const T (&)[N]) { return N; }

typedef QPair<int, QChar> Key;

class tst_qquicktextinput : public QQmlDataTest

{
    Q_OBJECT
public:
    tst_qquicktextinput();

private slots:
    void cleanup();
    void text();
    void width();
    void font();
    void color();
    void wrap();
    void selection();
    void persistentSelection();
    void overwriteMode();
    void isRightToLeft_data();
    void isRightToLeft();
    void moveCursorSelection_data();
    void moveCursorSelection();
    void moveCursorSelectionSequence_data();
    void moveCursorSelectionSequence();
    void dragMouseSelection();
    void mouseSelectionMode_data();
    void mouseSelectionMode();
    void mouseSelectionMode_accessors();
    void selectByMouse();
    void renderType();
    void tripleClickSelectsAll();

    void horizontalAlignment_RightToLeft();
    void verticalAlignment();

    void clipRect();
    void boundingRect();

    void positionAt();

    void maxLength();
    void masks();
    void validators();
    void inputMethods();

    void signal_accepted();
    void signal_editingfinished();

    void passwordCharacter();
    void cursorDelegate_data();
    void cursorDelegate();
    void remoteCursorDelegate();
    void cursorVisible();
    void cursorRectangle_data();
    void cursorRectangle();
    void navigation();
    void navigation_RTL();
#ifndef QT_NO_CLIPBOARD
    void copyAndPaste();
    void copyAndPasteKeySequence();
    void canPasteEmpty();
    void canPaste();
    void middleClickPaste();
#endif
    void readOnly();
    void focusOnPress();
    void focusOnPressOnlyOneItem();

    void openInputPanel();
    void setHAlignClearCache();
    void focusOutClearSelection();
    void focusOutNotClearSelection();

    void echoMode();
    void passwordEchoDelay();
    void geometrySignals();
    void contentSize();

    void preeditAutoScroll();
    void preeditCursorRectangle();
    void inputContextMouseHandler();
    void inputMethodComposing();
    void inputMethodUpdate();
    void cursorRectangleSize();

    void getText_data();
    void getText();
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

    void backspaceSurrogatePairs();

    void QTBUG_19956();
    void QTBUG_19956_data();
    void QTBUG_19956_regexp();

    void implicitSize_data();
    void implicitSize();
    void implicitSizeBinding_data();
    void implicitSizeBinding();
    void implicitResize_data();
    void implicitResize();

    void negativeDimensions();


    void setInputMask_data();
    void setInputMask();
    void inputMask_data();
    void inputMask();
    void clearInputMask();
    void keypress_inputMask_data();
    void keypress_inputMask();
    void hasAcceptableInputMask_data();
    void hasAcceptableInputMask();
    void maskCharacter_data();
    void maskCharacter();
    void fixup();
    void baselineOffset_data();
    void baselineOffset();

    void ensureVisible();
    void padding();

    void QTBUG_51115_readOnlyResetsSelection();

private:
    void simulateKey(QWindow *, int key);

    void simulateKeys(QWindow *window, const QList<Key> &keys);
    void simulateKeys(QWindow *window, const QKeySequence &sequence);

    QQmlEngine engine;
    QStringList standard;
    QStringList colorStrings;
};

typedef QList<int> IntList;
Q_DECLARE_METATYPE(IntList)

typedef QList<Key> KeyList;
Q_DECLARE_METATYPE(KeyList)

void tst_qquicktextinput::simulateKeys(QWindow *window, const QList<Key> &keys)
{
    for (int i = 0; i < keys.count(); ++i) {
        const int key = keys.at(i).first & ~Qt::KeyboardModifierMask;
        const int modifiers = keys.at(i).first & Qt::KeyboardModifierMask;
        const QString text = !keys.at(i).second.isNull() ? QString(keys.at(i).second) : QString();

        QKeyEvent press(QEvent::KeyPress, Qt::Key(key), Qt::KeyboardModifiers(modifiers), text);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key(key), Qt::KeyboardModifiers(modifiers), text);

        QGuiApplication::sendEvent(window, &press);
        QGuiApplication::sendEvent(window, &release);
    }
}

void tst_qquicktextinput::simulateKeys(QWindow *window, const QKeySequence &sequence)
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

void tst_qquicktextinput::cleanup()
{
    // ensure not even skipped tests with custom input context leave it dangling
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = 0;
}

tst_qquicktextinput::tst_qquicktextinput()
{
    standard << "the quick brown fox jumped over the lazy dog"
        << "It's supercalifragisiticexpialidocious!"
        << "Hello, world!"
        << "!dlrow ,olleH"
        << " spacey   text ";

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
}

void tst_qquicktextinput::text()
{
    {
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData("import QtQuick 2.0\nTextInput {  text: \"\"  }", QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());

        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->text(), QString(""));
        QCOMPARE(textinputObject->length(), 0);

        delete textinputObject;
    }

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextInput { text: \"" + standard.at(i) + "\" }";
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());

        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->text(), standard.at(i));
        QCOMPARE(textinputObject->length(), standard.at(i).length());

        delete textinputObject;
    }

}

void tst_qquicktextinput::width()
{
    // uses Font metrics to find the width for standard
    {
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData("import QtQuick 2.0\nTextInput {  text: \"\" }", QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());

        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->width(), 0.0);

        delete textinputObject;
    }

    bool requiresUnhintedMetrics = !qmlDisableDistanceField();

    for (int i = 0; i < standard.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextInput { text: \"" + standard.at(i) + "\" }";
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());

        QString s = standard.at(i);
        s.replace(QLatin1Char('\n'), QChar::LineSeparator);

        QTextLayout layout(s);
        layout.setFont(textinputObject->font());
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

        qreal metricWidth = ceil(layout.boundingRect().width());

        QVERIFY(textinputObject != 0);
        int delta = abs(int(int(textinputObject->width()) - metricWidth));
        QVERIFY(delta <= 3.0); // As best as we can hope for cross-platform.

        delete textinputObject;
    }
}

void tst_qquicktextinput::font()
{
    //test size, then bold, then italic, then family
    {
        QString componentStr = "import QtQuick 2.0\nTextInput {  font.pointSize: 40; text: \"Hello World\" }";
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());

        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->font().pointSize(), 40);
        QCOMPARE(textinputObject->font().bold(), false);
        QCOMPARE(textinputObject->font().italic(), false);

        delete textinputObject;
    }

    {
        QString componentStr = "import QtQuick 2.0\nTextInput {  font.bold: true; text: \"Hello World\" }";
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());

        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->font().bold(), true);
        QCOMPARE(textinputObject->font().italic(), false);

        delete textinputObject;
    }

    {
        QString componentStr = "import QtQuick 2.0\nTextInput {  font.italic: true; text: \"Hello World\" }";
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());

        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->font().italic(), true);
        QCOMPARE(textinputObject->font().bold(), false);

        delete textinputObject;
    }

    {
        QString componentStr = "import QtQuick 2.0\nTextInput {  font.family: \"Helvetica\"; text: \"Hello World\" }";
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());

        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->font().family(), QString("Helvetica"));
        QCOMPARE(textinputObject->font().bold(), false);
        QCOMPARE(textinputObject->font().italic(), false);

        delete textinputObject;
    }

    {
        QString componentStr = "import QtQuick 2.0\nTextInput {  font.family: \"\"; text: \"Hello World\" }";
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());

        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->font().family(), QString(""));

        delete textinputObject;
    }
}

void tst_qquicktextinput::color()
{
    //test initial color
    {
        QString componentStr = "import QtQuick 2.0\nTextInput { text: \"Hello World\" }";
        QQmlComponent texteditComponent(&engine);
        texteditComponent.setData(componentStr.toLatin1(), QUrl());
        QScopedPointer<QObject> object(texteditComponent.create());
        QQuickTextInput *textInputObject = qobject_cast<QQuickTextInput*>(object.data());

        QVERIFY(textInputObject);
        QCOMPARE(textInputObject->color(), QColor("black"));
        QCOMPARE(textInputObject->selectionColor(), QColor::fromRgba(0xFF000080));
        QCOMPARE(textInputObject->selectedTextColor(), QColor("white"));

        QSignalSpy colorSpy(textInputObject, SIGNAL(colorChanged()));
        QSignalSpy selectionColorSpy(textInputObject, SIGNAL(selectionColorChanged()));
        QSignalSpy selectedTextColorSpy(textInputObject, SIGNAL(selectedTextColorChanged()));

        textInputObject->setColor(QColor("white"));
        QCOMPARE(textInputObject->color(), QColor("white"));
        QCOMPARE(colorSpy.count(), 1);

        textInputObject->setSelectionColor(QColor("black"));
        QCOMPARE(textInputObject->selectionColor(), QColor("black"));
        QCOMPARE(selectionColorSpy.count(), 1);

        textInputObject->setSelectedTextColor(QColor("blue"));
        QCOMPARE(textInputObject->selectedTextColor(), QColor("blue"));
        QCOMPARE(selectedTextColorSpy.count(), 1);

        textInputObject->setColor(QColor("white"));
        QCOMPARE(colorSpy.count(), 1);

        textInputObject->setSelectionColor(QColor("black"));
        QCOMPARE(selectionColorSpy.count(), 1);

        textInputObject->setSelectedTextColor(QColor("blue"));
        QCOMPARE(selectedTextColorSpy.count(), 1);

        textInputObject->setColor(QColor("black"));
        QCOMPARE(textInputObject->color(), QColor("black"));
        QCOMPARE(colorSpy.count(), 2);

        textInputObject->setSelectionColor(QColor("blue"));
        QCOMPARE(textInputObject->selectionColor(), QColor("blue"));
        QCOMPARE(selectionColorSpy.count(), 2);

        textInputObject->setSelectedTextColor(QColor("white"));
        QCOMPARE(textInputObject->selectedTextColor(), QColor("white"));
        QCOMPARE(selectedTextColorSpy.count(), 2);
    }

    //test color
    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextInput {  color: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());
        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->color(), QColor(colorStrings.at(i)));

        delete textinputObject;
    }

    //test selection color
    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextInput {  selectionColor: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());
        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->selectionColor(), QColor(colorStrings.at(i)));

        delete textinputObject;
    }

    //test selected text color
    for (int i = 0; i < colorStrings.size(); i++)
    {
        QString componentStr = "import QtQuick 2.0\nTextInput {  selectedTextColor: \"" + colorStrings.at(i) + "\"; text: \"Hello World\" }";
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());
        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->selectedTextColor(), QColor(colorStrings.at(i)));

        delete textinputObject;
    }

    {
        QString colorStr = "#AA001234";
        QColor testColor("#001234");
        testColor.setAlpha(170);

        QString componentStr = "import QtQuick 2.0\nTextInput {  color: \"" + colorStr + "\"; text: \"Hello World\" }";
        QQmlComponent textinputComponent(&engine);
        textinputComponent.setData(componentStr.toLatin1(), QUrl());
        QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());

        QVERIFY(textinputObject != 0);
        QCOMPARE(textinputObject->color(), testColor);

        delete textinputObject;
    }
}

void tst_qquicktextinput::wrap()
{
    int textHeight = 0;
    // for specified width and wrap set true
    {
        QQmlComponent textComponent(&engine);
        textComponent.setData("import QtQuick 2.0\nTextInput { text: \"Hello\"; wrapMode: Text.WrapAnywhere; width: 300 }", QUrl::fromLocalFile(""));
        QQuickTextInput *textObject = qobject_cast<QQuickTextInput*>(textComponent.create());
        textHeight = textObject->height();

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->wrapMode(), QQuickTextInput::WrapAnywhere);
        QCOMPARE(textObject->width(), 300.);

        delete textObject;
    }

    for (int i = 0; i < standard.count(); i++) {
        QString componentStr = "import QtQuick 2.0\nTextInput { wrapMode: Text.WrapAnywhere; width: 30; text: \"" + standard.at(i) + "\" }";
        QQmlComponent textComponent(&engine);
        textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickTextInput *textObject = qobject_cast<QQuickTextInput*>(textComponent.create());

        QVERIFY(textObject != 0);
        QCOMPARE(textObject->width(), 30.);
        QVERIFY(textObject->height() > textHeight);

        int oldHeight = textObject->height();
        textObject->setWidth(100);
        QVERIFY(textObject->height() < oldHeight);

        delete textObject;
    }

    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0\n TextInput {}", QUrl());
        QScopedPointer<QObject> object(component.create());
        QQuickTextInput *input = qobject_cast<QQuickTextInput *>(object.data());
        QVERIFY(input);

        QSignalSpy spy(input, SIGNAL(wrapModeChanged()));

        QCOMPARE(input->wrapMode(), QQuickTextInput::NoWrap);

        input->setWrapMode(QQuickTextInput::Wrap);
        QCOMPARE(input->wrapMode(), QQuickTextInput::Wrap);
        QCOMPARE(spy.count(), 1);

        input->setWrapMode(QQuickTextInput::Wrap);
        QCOMPARE(spy.count(), 1);

        input->setWrapMode(QQuickTextInput::NoWrap);
        QCOMPARE(input->wrapMode(), QQuickTextInput::NoWrap);
        QCOMPARE(spy.count(), 2);
    }
}

void tst_qquicktextinput::selection()
{
    QString testStr = standard[0];
    QString componentStr = "import QtQuick 2.0\nTextInput {  text: \""+ testStr +"\"; }";
    QQmlComponent textinputComponent(&engine);
    textinputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());
    QVERIFY(textinputObject != 0);


    //Test selection follows cursor
    for (int i=0; i<= testStr.size(); i++) {
        textinputObject->setCursorPosition(i);
        QCOMPARE(textinputObject->cursorPosition(), i);
        QCOMPARE(textinputObject->selectionStart(), i);
        QCOMPARE(textinputObject->selectionEnd(), i);
        QVERIFY(textinputObject->selectedText().isNull());
    }

    textinputObject->setCursorPosition(0);
    QCOMPARE(textinputObject->cursorPosition(), 0);
    QCOMPARE(textinputObject->selectionStart(), 0);
    QCOMPARE(textinputObject->selectionEnd(), 0);
    QVERIFY(textinputObject->selectedText().isNull());

    // Verify invalid positions are ignored.
    textinputObject->setCursorPosition(-1);
    QCOMPARE(textinputObject->cursorPosition(), 0);
    QCOMPARE(textinputObject->selectionStart(), 0);
    QCOMPARE(textinputObject->selectionEnd(), 0);
    QVERIFY(textinputObject->selectedText().isNull());

    textinputObject->setCursorPosition(textinputObject->text().count()+1);
    QCOMPARE(textinputObject->cursorPosition(), 0);
    QCOMPARE(textinputObject->selectionStart(), 0);
    QCOMPARE(textinputObject->selectionEnd(), 0);
    QVERIFY(textinputObject->selectedText().isNull());

    //Test selection
    for (int i=0; i<= testStr.size(); i++) {
        textinputObject->select(0,i);
        QCOMPARE(testStr.mid(0,i), textinputObject->selectedText());
    }
    for (int i=0; i<= testStr.size(); i++) {
        textinputObject->select(i,testStr.size());
        QCOMPARE(testStr.mid(i,testStr.size()-i), textinputObject->selectedText());
    }

    textinputObject->setCursorPosition(0);
    QCOMPARE(textinputObject->cursorPosition(), 0);
    QCOMPARE(textinputObject->selectionStart(), 0);
    QCOMPARE(textinputObject->selectionEnd(), 0);
    QVERIFY(textinputObject->selectedText().isNull());

    //Test Error Ignoring behaviour
    textinputObject->setCursorPosition(0);
    QVERIFY(textinputObject->selectedText().isNull());
    textinputObject->select(-10,0);
    QVERIFY(textinputObject->selectedText().isNull());
    textinputObject->select(100,110);
    QVERIFY(textinputObject->selectedText().isNull());
    textinputObject->select(0,-10);
    QVERIFY(textinputObject->selectedText().isNull());
    textinputObject->select(0,100);
    QVERIFY(textinputObject->selectedText().isNull());
    textinputObject->select(0,10);
    QCOMPARE(textinputObject->selectedText().size(), 10);
    textinputObject->select(-10,10);
    QCOMPARE(textinputObject->selectedText().size(), 10);
    textinputObject->select(100,101);
    QCOMPARE(textinputObject->selectedText().size(), 10);
    textinputObject->select(0,-10);
    QCOMPARE(textinputObject->selectedText().size(), 10);
    textinputObject->select(0,100);
    QCOMPARE(textinputObject->selectedText().size(), 10);

    textinputObject->deselect();
    QVERIFY(textinputObject->selectedText().isNull());
    textinputObject->select(0,10);
    QCOMPARE(textinputObject->selectedText().size(), 10);
    textinputObject->deselect();
    QVERIFY(textinputObject->selectedText().isNull());

    // test input method selection
    QSignalSpy selectionSpy(textinputObject, SIGNAL(selectedTextChanged()));
    textinputObject->setFocus(true);
    {
        QList<QInputMethodEvent::Attribute> attributes;
        attributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 12, 5, QVariant());
        QInputMethodEvent event("", attributes);
        QGuiApplication::sendEvent(textinputObject, &event);
    }
    QCOMPARE(selectionSpy.count(), 1);
    QCOMPARE(textinputObject->selectionStart(), 12);
    QCOMPARE(textinputObject->selectionEnd(), 17);

    delete textinputObject;
}

void tst_qquicktextinput::persistentSelection()
{
    QQuickView window(testFileUrl("persistentSelection.qml"));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(window.rootObject());
    QVERIFY(input);
    QVERIFY(input->hasActiveFocus());

    QSignalSpy spy(input, SIGNAL(persistentSelectionChanged()));

    QCOMPARE(input->persistentSelection(), false);

    input->setPersistentSelection(false);
    QCOMPARE(input->persistentSelection(), false);
    QCOMPARE(spy.count(), 0);

    input->select(1, 4);
    QCOMPARE(input->property("selected").toString(), QLatin1String("ell"));

    input->setFocus(false);
    QCOMPARE(input->property("selected").toString(), QString());

    input->setFocus(true);
    QCOMPARE(input->property("selected").toString(), QString());

    input->setPersistentSelection(true);
    QCOMPARE(input->persistentSelection(), true);
    QCOMPARE(spy.count(), 1);

    input->select(1, 4);
    QCOMPARE(input->property("selected").toString(), QLatin1String("ell"));

    input->setFocus(false);
    QCOMPARE(input->property("selected").toString(), QLatin1String("ell"));

    input->setFocus(true);
    QCOMPARE(input->property("selected").toString(), QLatin1String("ell"));
}

void tst_qquicktextinput::overwriteMode()
{
    QString componentStr = "import QtQuick 2.0\nTextInput { focus: true; }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    QSignalSpy spy(textInput, SIGNAL(overwriteModeChanged(bool)));

    QQuickWindow window;
    textInput->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(textInput->hasActiveFocus());

    textInput->setOverwriteMode(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(true, textInput->overwriteMode());
    textInput->setOverwriteMode(false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(false, textInput->overwriteMode());

    QVERIFY(!textInput->overwriteMode());
    QString insertString = "Some first text";
    for (int j = 0; j < insertString.length(); j++)
        QTest::keyClick(&window, insertString.at(j).toLatin1());

    QCOMPARE(textInput->text(), QString("Some first text"));

    textInput->setOverwriteMode(true);
    QCOMPARE(spy.count(), 3);
    textInput->setCursorPosition(5);

    insertString = "shiny";
    for (int j = 0; j < insertString.length(); j++)
        QTest::keyClick(&window, insertString.at(j).toLatin1());
    QCOMPARE(textInput->text(), QString("Some shiny text"));
}

void tst_qquicktextinput::isRightToLeft_data()
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

void tst_qquicktextinput::isRightToLeft()
{
    QFETCH(QString, text);
    QFETCH(bool, emptyString);
    QFETCH(bool, firstCharacter);
    QFETCH(bool, lastCharacter);
    QFETCH(bool, middleCharacter);
    QFETCH(bool, startString);
    QFETCH(bool, midString);
    QFETCH(bool, endString);

    QQuickTextInput textInput;
    textInput.setText(text);

    // first test that the right string is delivered to the QString::isRightToLeft()
    QCOMPARE(textInput.isRightToLeft(0,0), text.mid(0,0).isRightToLeft());
    QCOMPARE(textInput.isRightToLeft(0,1), text.mid(0,1).isRightToLeft());
    QCOMPARE(textInput.isRightToLeft(text.count()-2, text.count()-1), text.mid(text.count()-2, text.count()-1).isRightToLeft());
    QCOMPARE(textInput.isRightToLeft(text.count()/2, text.count()/2 + 1), text.mid(text.count()/2, text.count()/2 + 1).isRightToLeft());
    QCOMPARE(textInput.isRightToLeft(0,text.count()/4), text.mid(0,text.count()/4).isRightToLeft());
    QCOMPARE(textInput.isRightToLeft(text.count()/4,3*text.count()/4), text.mid(text.count()/4,3*text.count()/4).isRightToLeft());
    if (text.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML TextInput: isRightToLeft(start, end) called with the end property being smaller than the start.");
    QCOMPARE(textInput.isRightToLeft(3*text.count()/4,text.count()-1), text.mid(3*text.count()/4,text.count()-1).isRightToLeft());

    // then test that the feature actually works
    QCOMPARE(textInput.isRightToLeft(0,0), emptyString);
    QCOMPARE(textInput.isRightToLeft(0,1), firstCharacter);
    QCOMPARE(textInput.isRightToLeft(text.count()-2, text.count()-1), lastCharacter);
    QCOMPARE(textInput.isRightToLeft(text.count()/2, text.count()/2 + 1), middleCharacter);
    QCOMPARE(textInput.isRightToLeft(0,text.count()/4), startString);
    QCOMPARE(textInput.isRightToLeft(text.count()/4,3*text.count()/4), midString);
    if (text.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML TextInput: isRightToLeft(start, end) called with the end property being smaller than the start.");
    QCOMPARE(textInput.isRightToLeft(3*text.count()/4,text.count()-1), endString);
}

void tst_qquicktextinput::moveCursorSelection_data()
{
    QTest::addColumn<QString>("testStr");
    QTest::addColumn<int>("cursorPosition");
    QTest::addColumn<int>("movePosition");
    QTest::addColumn<QQuickTextInput::SelectionMode>("mode");
    QTest::addColumn<int>("selectionStart");
    QTest::addColumn<int>("selectionEnd");
    QTest::addColumn<bool>("reversible");

    // () contains the text selected by the cursor.
    // <> contains the actual selection.

    QTest::newRow("(t)he|characters")
            << standard[0] << 0 << 1 << QQuickTextInput::SelectCharacters << 0 << 1 << true;
    QTest::newRow("do(g)|characters")
            << standard[0] << 43 << 44 << QQuickTextInput::SelectCharacters << 43 << 44 << true;
    QTest::newRow("jum(p)ed|characters")
            << standard[0] << 23 << 24 << QQuickTextInput::SelectCharacters << 23 << 24 << true;
    QTest::newRow("jumped( )over|characters")
            << standard[0] << 26 << 27 << QQuickTextInput::SelectCharacters << 26 << 27 << true;
    QTest::newRow("(the )|characters")
            << standard[0] << 0 << 4 << QQuickTextInput::SelectCharacters << 0 << 4 << true;
    QTest::newRow("( dog)|characters")
            << standard[0] << 40 << 44 << QQuickTextInput::SelectCharacters << 40 << 44 << true;
    QTest::newRow("( jumped )|characters")
            << standard[0] << 19 << 27 << QQuickTextInput::SelectCharacters << 19 << 27 << true;
    QTest::newRow("th(e qu)ick|characters")
            << standard[0] << 2 << 6 << QQuickTextInput::SelectCharacters << 2 << 6 << true;
    QTest::newRow("la(zy d)og|characters")
            << standard[0] << 38 << 42 << QQuickTextInput::SelectCharacters << 38 << 42 << true;
    QTest::newRow("jum(ped ov)er|characters")
            << standard[0] << 23 << 29 << QQuickTextInput::SelectCharacters << 23 << 29 << true;
    QTest::newRow("()the|characters")
            << standard[0] << 0 << 0 << QQuickTextInput::SelectCharacters << 0 << 0 << true;
    QTest::newRow("dog()|characters")
            << standard[0] << 44 << 44 << QQuickTextInput::SelectCharacters << 44 << 44 << true;
    QTest::newRow("jum()ped|characters")
            << standard[0] << 23 << 23 << QQuickTextInput::SelectCharacters << 23 << 23 << true;

    QTest::newRow("<(t)he>|words")
            << standard[0] << 0 << 1 << QQuickTextInput::SelectWords << 0 << 3 << true;
    QTest::newRow("<do(g)>|words")
            << standard[0] << 43 << 44 << QQuickTextInput::SelectWords << 41 << 44 << true;
    QTest::newRow("<jum(p)ed>|words")
            << standard[0] << 23 << 24 << QQuickTextInput::SelectWords << 20 << 26 << true;
    QTest::newRow("<jumped( )>over|words,ltr")
            << standard[0] << 26 << 27 << QQuickTextInput::SelectWords << 20 << 27 << false;
    QTest::newRow("jumped<( )over>|words,rtl")
            << standard[0] << 27 << 26 << QQuickTextInput::SelectWords << 26 << 31 << false;
    QTest::newRow("<(the )>quick|words,ltr")
            << standard[0] << 0 << 4 << QQuickTextInput::SelectWords << 0 << 4 << false;
    QTest::newRow("<(the )quick>|words,rtl")
            << standard[0] << 4 << 0 << QQuickTextInput::SelectWords << 0 << 9 << false;
    QTest::newRow("<lazy( dog)>|words,ltr")
            << standard[0] << 40 << 44 << QQuickTextInput::SelectWords << 36 << 44 << false;
    QTest::newRow("lazy<( dog)>|words,rtl")
            << standard[0] << 44 << 40 << QQuickTextInput::SelectWords << 40 << 44 << false;
    QTest::newRow("<fox( jumped )>over|words,ltr")
            << standard[0] << 19 << 27 << QQuickTextInput::SelectWords << 16 << 27 << false;
    QTest::newRow("fox<( jumped )over>|words,rtl")
            << standard[0] << 27 << 19 << QQuickTextInput::SelectWords << 19 << 31 << false;
    QTest::newRow("<th(e qu)ick>|words")
            << standard[0] << 2 << 6 << QQuickTextInput::SelectWords << 0 << 9 << true;
    QTest::newRow("<la(zy d)og|words>")
            << standard[0] << 38 << 42 << QQuickTextInput::SelectWords << 36 << 44 << true;
    QTest::newRow("<jum(ped ov)er>|words")
            << standard[0] << 23 << 29 << QQuickTextInput::SelectWords << 20 << 31 << true;
    QTest::newRow("<()>the|words")
            << standard[0] << 0 << 0 << QQuickTextInput::SelectWords << 0 << 0 << true;
    QTest::newRow("dog<()>|words")
            << standard[0] << 44 << 44 << QQuickTextInput::SelectWords << 44 << 44 << true;
    QTest::newRow("jum<()>ped|words")
            << standard[0] << 23 << 23 << QQuickTextInput::SelectWords << 23 << 23 << true;

    QTest::newRow("<Hello(,)> |words,ltr")
            << standard[2] << 5 << 6 << QQuickTextInput::SelectWords << 0 << 6 << false;
    QTest::newRow("Hello<(,)> |words,rtl")
            << standard[2] << 6 << 5 << QQuickTextInput::SelectWords << 5 << 6 << false;
    QTest::newRow("<Hello(, )>world|words,ltr")
            << standard[2] << 5 << 7 << QQuickTextInput::SelectWords << 0 << 7 << false;
    QTest::newRow("Hello<(, )world>|words,rtl")
            << standard[2] << 7 << 5 << QQuickTextInput::SelectWords << 5 << 12 << false;
    QTest::newRow("<Hel(lo, )>world|words,ltr")
            << standard[2] << 3 << 7 << QQuickTextInput::SelectWords << 0 << 7 << false;
    QTest::newRow("<Hel(lo, )world>|words,rtl")
            << standard[2] << 7 << 3 << QQuickTextInput::SelectWords << 0 << 12 << false;
    QTest::newRow("<Hel(lo)>,|words")
            << standard[2] << 3 << 5 << QQuickTextInput::SelectWords << 0 << 5 << true;
    QTest::newRow("Hello<()>,|words")
            << standard[2] << 5 << 5 << QQuickTextInput::SelectWords << 5 << 5 << true;
    QTest::newRow("Hello,<()>|words")
            << standard[2] << 6 << 6 << QQuickTextInput::SelectWords << 6 << 6 << true;
    QTest::newRow("Hello,<( )>world|words,ltr")
            << standard[2] << 6 << 7 << QQuickTextInput::SelectWords << 6 << 7 << false;
    QTest::newRow("Hello,<( )world>|words,rtl")
            << standard[2] << 7 << 6 << QQuickTextInput::SelectWords << 6 << 12 << false;
    QTest::newRow("Hello,<( world)>|words")
            << standard[2] << 6 << 12 << QQuickTextInput::SelectWords << 6 << 12 << true;
    QTest::newRow("Hello,<( world!)>|words")
            << standard[2] << 6 << 13 << QQuickTextInput::SelectWords << 6 << 13 << true;
    QTest::newRow("<Hello(, world!)>|words,ltr")
            << standard[2] << 5 << 13 << QQuickTextInput::SelectWords << 0 << 13 << false;
    QTest::newRow("Hello<(, world!)>|words,rtl")
            << standard[2] << 13 << 5 << QQuickTextInput::SelectWords << 5 << 13 << false;
    QTest::newRow("<world(!)>|words,ltr")
            << standard[2] << 12 << 13 << QQuickTextInput::SelectWords << 7 << 13 << false;
    QTest::newRow("world<(!)>|words,rtl")
            << standard[2] << 13 << 12 << QQuickTextInput::SelectWords << 12 << 13 << false;
    QTest::newRow("world!<()>)|words")
            << standard[2] << 13 << 13 << QQuickTextInput::SelectWords << 13 << 13 << true;
    QTest::newRow("world<()>!)|words")
            << standard[2] << 12 << 12 << QQuickTextInput::SelectWords << 12 << 12 << true;

    QTest::newRow("<(,)>olleH |words,ltr")
            << standard[3] << 7 << 8 << QQuickTextInput::SelectWords << 7 << 8 << false;
    QTest::newRow("<(,)olleH> |words,rtl")
            << standard[3] << 8 << 7 << QQuickTextInput::SelectWords << 7 << 13 << false;
    QTest::newRow("<dlrow( ,)>olleH|words,ltr")
            << standard[3] << 6 << 8 << QQuickTextInput::SelectWords << 1 << 8 << false;
    QTest::newRow("dlrow<( ,)olleH>|words,rtl")
            << standard[3] << 8 << 6 << QQuickTextInput::SelectWords << 6 << 13 << false;
    QTest::newRow("<dlrow( ,ol)leH>|words,ltr")
            << standard[3] << 6 << 10 << QQuickTextInput::SelectWords << 1 << 13 << false;
    QTest::newRow("dlrow<( ,ol)leH>|words,rtl")
            << standard[3] << 10 << 6 << QQuickTextInput::SelectWords << 6 << 13 << false;
    QTest::newRow(",<(ol)leH>,|words")
            << standard[3] << 8 << 10 << QQuickTextInput::SelectWords << 8 << 13 << true;
    QTest::newRow(",<()>olleH|words")
            << standard[3] << 8 << 8 << QQuickTextInput::SelectWords << 8 << 8 << true;
    QTest::newRow("<()>,olleH|words")
            << standard[3] << 7 << 7 << QQuickTextInput::SelectWords << 7 << 7 << true;
    QTest::newRow("<dlrow( )>,olleH|words,ltr")
            << standard[3] << 6 << 7 << QQuickTextInput::SelectWords << 1 << 7 << false;
    QTest::newRow("dlrow<( )>,olleH|words,rtl")
            << standard[3] << 7 << 6 << QQuickTextInput::SelectWords << 6 << 7 << false;
    QTest::newRow("<(dlrow )>,olleH|words")
            << standard[3] << 1 << 7 << QQuickTextInput::SelectWords << 1 << 7 << true;
    QTest::newRow("<(!dlrow )>,olleH|words")
            << standard[3] << 0 << 7 << QQuickTextInput::SelectWords << 0 << 7 << true;
    QTest::newRow("<(!dlrow ,)>olleH|words,ltr")
            << standard[3] << 0 << 8 << QQuickTextInput::SelectWords << 0 << 8 << false;
    QTest::newRow("<(!dlrow ,)olleH>|words,rtl")
            << standard[3] << 8 << 0 << QQuickTextInput::SelectWords << 0 << 13 << false;
    QTest::newRow("<(!)>dlrow|words,ltr")
            << standard[3] << 0 << 1 << QQuickTextInput::SelectWords << 0 << 1 << false;
    QTest::newRow("<(!)dlrow|words,rtl")
            << standard[3] << 1 << 0 << QQuickTextInput::SelectWords << 0 << 6 << false;
    QTest::newRow("<()>!dlrow|words")
            << standard[3] << 0 << 0 << QQuickTextInput::SelectWords << 0 << 0 << true;
    QTest::newRow("!<()>dlrow|words")
            << standard[3] << 1 << 1 << QQuickTextInput::SelectWords << 1 << 1 << true;

    QTest::newRow(" <s(pac)ey>   text |words")
            << standard[4] << 1 << 4 << QQuickTextInput::SelectWords << 1 << 7 << true;
    QTest::newRow(" spacey   <t(ex)t> |words")
            << standard[4] << 11 << 13 << QQuickTextInput::SelectWords << 10 << 14 << true;
    QTest::newRow("<( )>spacey   text |words|ltr")
            << standard[4] << 0 << 1 << QQuickTextInput::SelectWords << 0 << 1 << false;
    QTest::newRow("<( )spacey>   text |words|rtl")
            << standard[4] << 1 << 0 << QQuickTextInput::SelectWords << 0 << 7 << false;
    QTest::newRow("spacey   <text( )>|words|ltr")
            << standard[4] << 14 << 15 << QQuickTextInput::SelectWords << 10 << 15 << false;
    QTest::newRow("spacey   text<( )>|words|rtl")
            << standard[4] << 15 << 14 << QQuickTextInput::SelectWords << 14 << 15 << false;
    QTest::newRow("<()> spacey   text |words")
            << standard[4] << 0 << 0 << QQuickTextInput::SelectWords << 0 << 0 << false;
    QTest::newRow(" spacey   text <()>|words")
            << standard[4] << 15 << 15 << QQuickTextInput::SelectWords << 15 << 15 << false;
}

void tst_qquicktextinput::moveCursorSelection()
{
    QFETCH(QString, testStr);
    QFETCH(int, cursorPosition);
    QFETCH(int, movePosition);
    QFETCH(QQuickTextInput::SelectionMode, mode);
    QFETCH(int, selectionStart);
    QFETCH(int, selectionEnd);
    QFETCH(bool, reversible);

    QString componentStr = "import QtQuick 2.0\nTextInput {  text: \""+ testStr +"\"; }";
    QQmlComponent textinputComponent(&engine);
    textinputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());
    QVERIFY(textinputObject != 0);

    textinputObject->setCursorPosition(cursorPosition);
    textinputObject->moveCursorSelection(movePosition, mode);

    QCOMPARE(textinputObject->selectedText(), testStr.mid(selectionStart, selectionEnd - selectionStart));
    QCOMPARE(textinputObject->selectionStart(), selectionStart);
    QCOMPARE(textinputObject->selectionEnd(), selectionEnd);

    if (reversible) {
        textinputObject->setCursorPosition(movePosition);
        textinputObject->moveCursorSelection(cursorPosition, mode);

        QCOMPARE(textinputObject->selectedText(), testStr.mid(selectionStart, selectionEnd - selectionStart));
        QCOMPARE(textinputObject->selectionStart(), selectionStart);
        QCOMPARE(textinputObject->selectionEnd(), selectionEnd);
    }

    delete textinputObject;
}

void tst_qquicktextinput::moveCursorSelectionSequence_data()
{
    QTest::addColumn<QString>("testStr");
    QTest::addColumn<int>("cursorPosition");
    QTest::addColumn<int>("movePosition1");
    QTest::addColumn<int>("movePosition2");
    QTest::addColumn<int>("selection1Start");
    QTest::addColumn<int>("selection1End");
    QTest::addColumn<int>("selection2Start");
    QTest::addColumn<int>("selection2End");

    // () contains the text selected by the cursor.
    // <> contains the actual selection.
    // ^ is the revised cursor position.
    // {} contains the revised selection.

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
    QTest::newRow("the quick<( {^bro)wn>} fox|rtl")
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

    QTest::newRow("{<(^} sp)acey>   text |ltr")
            << standard[4]
            << 0 << 3 << 0
            << 0 << 7
            << 0 << 0;
    QTest::newRow("{<( ^}sp)acey>   text |ltr")
            << standard[4]
            << 0 << 3 << 1
            << 0 << 7
            << 0 << 1;
    QTest::newRow("<( {s^p)acey>}   text |rtl")
            << standard[4]
            << 3 << 0 << 2
            << 0 << 7
            << 1 << 7;
    QTest::newRow("<( {^sp)acey>}   text |rtl")
            << standard[4]
            << 3 << 0 << 1
            << 0 << 7
            << 1 << 7;

    QTest::newRow(" spacey   <te(xt {^)>}|rtl")
            << standard[4]
            << 15 << 12 << 15
            << 10 << 15
            << 15 << 15;
    QTest::newRow(" spacey   <te(xt{^ )>}|rtl")
            << standard[4]
            << 15 << 12 << 14
            << 10 << 15
            << 14 << 15;
    QTest::newRow(" spacey   {<te(x^t} )>|ltr")
            << standard[4]
            << 12 << 15 << 13
            << 10 << 15
            << 10 << 14;
    QTest::newRow(" spacey   {<te(xt^} )>|ltr")
            << standard[4]
            << 12 << 15 << 14
            << 10 << 15
            << 10 << 14;
}

void tst_qquicktextinput::moveCursorSelectionSequence()
{
    QFETCH(QString, testStr);
    QFETCH(int, cursorPosition);
    QFETCH(int, movePosition1);
    QFETCH(int, movePosition2);
    QFETCH(int, selection1Start);
    QFETCH(int, selection1End);
    QFETCH(int, selection2Start);
    QFETCH(int, selection2End);

    QString componentStr = "import QtQuick 2.0\nTextInput {  text: \""+ testStr +"\"; }";
    QQmlComponent textinputComponent(&engine);
    textinputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput*>(textinputComponent.create());
    QVERIFY(textinputObject != 0);

    textinputObject->setCursorPosition(cursorPosition);

    textinputObject->moveCursorSelection(movePosition1, QQuickTextInput::SelectWords);
    QCOMPARE(textinputObject->selectedText(), testStr.mid(selection1Start, selection1End - selection1Start));
    QCOMPARE(textinputObject->selectionStart(), selection1Start);
    QCOMPARE(textinputObject->selectionEnd(), selection1End);

    textinputObject->moveCursorSelection(movePosition2, QQuickTextInput::SelectWords);
    QCOMPARE(textinputObject->selectedText(), testStr.mid(selection2Start, selection2End - selection2Start));
    QCOMPARE(textinputObject->selectionStart(), selection2Start);
    QCOMPARE(textinputObject->selectionEnd(), selection2End);

    delete textinputObject;
}

void tst_qquicktextinput::dragMouseSelection()
{
    QString qmlfile = testFile("mouseselection_true.qml");

    QQuickView window(QUrl::fromLocalFile(qmlfile));

    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);
    QQuickTextInput *textInputObject = qobject_cast<QQuickTextInput *>(window.rootObject());
    QVERIFY(textInputObject != 0);

    // press-and-drag-and-release from x1 to x2
    int x1 = 10;
    int x2 = 70;
    int y = textInputObject->height()/2;
    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(x1,y));
    QTest::mouseMove(&window, QPoint(x2, y));
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(x2,y));
    QString str1;
    QTRY_VERIFY((str1 = textInputObject->selectedText()).length() > 3);
    QTRY_VERIFY(str1.length() > 3);

    // press and drag the current selection.
    x1 = 40;
    x2 = 100;
    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(x1,y));
    QTest::mouseMove(&window, QPoint(x2, y));
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(x2,y));
    QString str2 = textInputObject->selectedText();
    QTRY_VERIFY(str2.length() > 3);

    QTRY_VERIFY(str1 != str2);
}

void tst_qquicktextinput::mouseSelectionMode_data()
{
    QTest::addColumn<QString>("qmlfile");
    QTest::addColumn<bool>("selectWords");
    QTest::addColumn<bool>("focus");
    QTest::addColumn<bool>("focusOnPress");

    // import installed
    QTest::newRow("SelectWords focused") << testFile("mouseselectionmode_words.qml") << true << true << true;
    QTest::newRow("SelectCharacters focused") << testFile("mouseselectionmode_characters.qml") << false << true << true;
    QTest::newRow("default focused") << testFile("mouseselectionmode_default.qml") << false << true << true;
    QTest::newRow("SelectWords unfocused") << testFile("mouseselectionmode_words.qml") << true << false << false;
    QTest::newRow("SelectCharacters unfocused") << testFile("mouseselectionmode_characters.qml") << false << false << false;
    QTest::newRow("default unfocused") << testFile("mouseselectionmode_default.qml") << false << true << false;
    QTest::newRow("SelectWords focuss on press") << testFile("mouseselectionmode_words.qml") << true << false << true;
    QTest::newRow("SelectCharacters focus on press") << testFile("mouseselectionmode_characters.qml") << false << false << true;
    QTest::newRow("default focus on press") << testFile("mouseselectionmode_default.qml") << false << false << true;
}

void tst_qquicktextinput::mouseSelectionMode()
{
    QFETCH(QString, qmlfile);
    QFETCH(bool, selectWords);
    QFETCH(bool, focus);
    QFETCH(bool, focusOnPress);

    QString text = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    QQuickView window(QUrl::fromLocalFile(qmlfile));

    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);
    QQuickTextInput *textInputObject = qobject_cast<QQuickTextInput *>(window.rootObject());
    QVERIFY(textInputObject != 0);

    textInputObject->setFocus(focus);
    textInputObject->setFocusOnPress(focusOnPress);

    // press-and-drag-and-release from x1 to x2
    int x1 = 10;
    int x2 = 70;
    int y = textInputObject->height()/2;
    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(x1,y));
    QTest::mouseMove(&window, QPoint(x2,y)); // doesn't work
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(x2,y));
    if (selectWords) {
        QTRY_COMPARE(textInputObject->selectedText(), text);
    } else {
        QTRY_VERIFY(textInputObject->selectedText().length() > 3);
        QVERIFY(textInputObject->selectedText() != text);
    }
}

void tst_qquicktextinput::mouseSelectionMode_accessors()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n TextInput {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(object.data());
    QVERIFY(input);

    QSignalSpy spy(input, &QQuickTextInput::mouseSelectionModeChanged);

    QCOMPARE(input->mouseSelectionMode(), QQuickTextInput::SelectCharacters);

    input->setMouseSelectionMode(QQuickTextInput::SelectWords);
    QCOMPARE(input->mouseSelectionMode(), QQuickTextInput::SelectWords);
    QCOMPARE(spy.count(), 1);

    input->setMouseSelectionMode(QQuickTextInput::SelectWords);
    QCOMPARE(spy.count(), 1);

    input->setMouseSelectionMode(QQuickTextInput::SelectCharacters);
    QCOMPARE(input->mouseSelectionMode(), QQuickTextInput::SelectCharacters);
    QCOMPARE(spy.count(), 2);
}

void tst_qquicktextinput::selectByMouse()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n TextInput {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(object.data());
    QVERIFY(input);

    QSignalSpy spy(input, SIGNAL(selectByMouseChanged(bool)));

    QCOMPARE(input->selectByMouse(), false);

    input->setSelectByMouse(true);
    QCOMPARE(input->selectByMouse(), true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);

    input->setSelectByMouse(true);
    QCOMPARE(spy.count(), 1);

    input->setSelectByMouse(false);
    QCOMPARE(input->selectByMouse(), false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

void tst_qquicktextinput::renderType()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n TextInput {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(object.data());
    QVERIFY(input);

    QSignalSpy spy(input, SIGNAL(renderTypeChanged()));

    QCOMPARE(input->renderType(), QQuickTextInput::QtRendering);

    input->setRenderType(QQuickTextInput::NativeRendering);
    QCOMPARE(input->renderType(), QQuickTextInput::NativeRendering);
    QCOMPARE(spy.count(), 1);

    input->setRenderType(QQuickTextInput::NativeRendering);
    QCOMPARE(spy.count(), 1);

    input->setRenderType(QQuickTextInput::QtRendering);
    QCOMPARE(input->renderType(), QQuickTextInput::QtRendering);
    QCOMPARE(spy.count(), 2);
}

void tst_qquicktextinput::horizontalAlignment_RightToLeft()
{
    PlatformInputContext platformInputContext;
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = &platformInputContext;

    QQuickView window(testFileUrl("horizontalAlignment_RightToLeft.qml"));
    QQuickTextInput *textInput = window.rootObject()->findChild<QQuickTextInput*>("text");
    QVERIFY(textInput != 0);
    window.show();

    const QString rtlText = textInput->text();

    QVERIFY(textInput->boundingRect().right() >= textInput->width() - 1);
    QVERIFY(textInput->boundingRect().right() <= textInput->width() + 1);

    // implicit alignment should follow the reading direction of RTL text
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignRight);
    QCOMPARE(textInput->effectiveHAlign(), textInput->hAlign());
    QVERIFY(textInput->boundingRect().right() >= textInput->width() - 1);
    QVERIFY(textInput->boundingRect().right() <= textInput->width() + 1);

    // explicitly left aligned
    textInput->setHAlign(QQuickTextInput::AlignLeft);
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignLeft);
    QCOMPARE(textInput->effectiveHAlign(), textInput->hAlign());
    QCOMPARE(textInput->boundingRect().left(), qreal(0));

    // explicitly right aligned
    textInput->setHAlign(QQuickTextInput::AlignRight);
    QCOMPARE(textInput->effectiveHAlign(), textInput->hAlign());
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignRight);
    QVERIFY(textInput->boundingRect().right() >= textInput->width() - 1);
    QVERIFY(textInput->boundingRect().right() <= textInput->width() + 1);

    // explicitly center aligned
    textInput->setHAlign(QQuickTextInput::AlignHCenter);
    QCOMPARE(textInput->effectiveHAlign(), textInput->hAlign());
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignHCenter);
    QVERIFY(textInput->boundingRect().left() > 0);
    QVERIFY(textInput->boundingRect().right() < textInput->width());

    // reseted alignment should go back to following the text reading direction
    textInput->resetHAlign();
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignRight);
    QCOMPARE(textInput->effectiveHAlign(), textInput->hAlign());
    QVERIFY(textInput->boundingRect().right() >= textInput->width() - 1);
    QVERIFY(textInput->boundingRect().right() <= textInput->width() + 1);

    // mirror the text item
    QQuickItemPrivate::get(textInput)->setLayoutMirror(true);

    // mirrored implicit alignment should continue to follow the reading direction of the text
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignRight);
    QCOMPARE(textInput->effectiveHAlign(), textInput->hAlign());
    QVERIFY(textInput->boundingRect().right() >= textInput->width() - 1);
    QVERIFY(textInput->boundingRect().right() <= textInput->width() + 1);

    // explicitly right aligned behaves as left aligned
    textInput->setHAlign(QQuickTextInput::AlignRight);
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignRight);
    QCOMPARE(textInput->effectiveHAlign(), QQuickTextInput::AlignLeft);
    QCOMPARE(textInput->boundingRect().left(), qreal(0));

    // mirrored explicitly left aligned behaves as right aligned
    textInput->setHAlign(QQuickTextInput::AlignLeft);
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignLeft);
    QCOMPARE(textInput->effectiveHAlign(), QQuickTextInput::AlignRight);
    QVERIFY(textInput->boundingRect().right() >= textInput->width() - 1);
    QVERIFY(textInput->boundingRect().right() <= textInput->width() + 1);

    // disable mirroring
    QQuickItemPrivate::get(textInput)->setLayoutMirror(false);
    QCOMPARE(textInput->effectiveHAlign(), textInput->hAlign());
    textInput->resetHAlign();

    // English text should be implicitly left aligned
    textInput->setText("Hello world!");
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignLeft);
    QCOMPARE(textInput->boundingRect().left(), qreal(0));

    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(textInput->hasActiveFocus());

    // If there is no committed text, the preedit text should determine the alignment.
    textInput->setText(QString());
    { QInputMethodEvent ev(rtlText, QList<QInputMethodEvent::Attribute>()); QGuiApplication::sendEvent(textInput, &ev); }
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignRight);
    { QInputMethodEvent ev("Hello world!", QList<QInputMethodEvent::Attribute>()); QGuiApplication::sendEvent(textInput, &ev); }
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignLeft);

    // Clear pre-edit text.  TextInput should maybe do this itself on setText, but that may be
    // redundant as an actual input method may take care of it.
    { QInputMethodEvent ev; QGuiApplication::sendEvent(textInput, &ev); }

    // empty text with implicit alignment follows the system locale-based
    // keyboard input direction from QInputMethod::inputDirection()
    textInput->setText("");
    platformInputContext.setInputDirection(Qt::LeftToRight);
    QCOMPARE(qApp->inputMethod()->inputDirection(), Qt::LeftToRight);
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignLeft);
    QCOMPARE(textInput->boundingRect().left(), qreal(0));

    QSignalSpy cursorRectangleSpy(textInput, SIGNAL(cursorRectangleChanged()));
    platformInputContext.setInputDirection(Qt::RightToLeft);
    QCOMPARE(qApp->inputMethod()->inputDirection(), Qt::RightToLeft);
    QCOMPARE(cursorRectangleSpy.count(), 1);
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignRight);
    QVERIFY(textInput->boundingRect().right() >= textInput->width() - 1);
    QVERIFY(textInput->boundingRect().right() <= textInput->width() + 1);

    // set input direction while having content
    platformInputContext.setInputDirection(Qt::LeftToRight);
    textInput->setText("a");
    platformInputContext.setInputDirection(Qt::RightToLeft);
    QTest::keyClick(&window, Qt::Key_Backspace);
    QVERIFY(textInput->text().isEmpty());
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignRight);
    QVERIFY(textInput->boundingRect().right() >= textInput->width() - 1);
    QVERIFY(textInput->boundingRect().right() <= textInput->width() + 1);

    // input direction changed while not having focus
    platformInputContext.setInputDirection(Qt::LeftToRight);
    textInput->setFocus(false);
    platformInputContext.setInputDirection(Qt::RightToLeft);
    textInput->setFocus(true);
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignRight);
    QVERIFY(textInput->boundingRect().right() >= textInput->width() - 1);
    QVERIFY(textInput->boundingRect().right() <= textInput->width() + 1);

    textInput->setHAlign(QQuickTextInput::AlignRight);
    QCOMPARE(textInput->hAlign(), QQuickTextInput::AlignRight);
    QVERIFY(textInput->boundingRect().right() >= textInput->width() - 1);
    QVERIFY(textInput->boundingRect().right() <= textInput->width() + 1);

    // neutral text should fall back to input direction
    textInput->setFocus(true);
    textInput->resetHAlign();
    textInput->setText(" ()((=<>");
    platformInputContext.setInputDirection(Qt::LeftToRight);
    QCOMPARE(textInput->effectiveHAlign(), QQuickTextInput::AlignLeft);
    platformInputContext.setInputDirection(Qt::RightToLeft);
    QCOMPARE(textInput->effectiveHAlign(), QQuickTextInput::AlignRight);

    // changing width keeps right aligned cursor on proper position
    textInput->setText("");
    textInput->setWidth(500);
    QVERIFY(textInput->positionToRectangle(0).x() > textInput->width() / 2);
}

void tst_qquicktextinput::verticalAlignment()
{
    QQuickView window(testFileUrl("horizontalAlignment.qml"));
    QQuickTextInput *textInput = window.rootObject()->findChild<QQuickTextInput*>("text");
    QVERIFY(textInput != 0);
    window.showNormal();

    QCOMPARE(textInput->vAlign(), QQuickTextInput::AlignTop);
    QVERIFY(textInput->boundingRect().bottom() < window.height() / 2);
    QVERIFY(textInput->cursorRectangle().bottom() < window.height() / 2);
    QVERIFY(textInput->positionToRectangle(0).bottom() < window.height() / 2);

    // bottom aligned
    textInput->setVAlign(QQuickTextInput::AlignBottom);
    QCOMPARE(textInput->vAlign(), QQuickTextInput::AlignBottom);
    QVERIFY(textInput->boundingRect().top() > window.height() / 2);
    QVERIFY(textInput->cursorRectangle().top() > window.height() / 2);
    QVERIFY(textInput->positionToRectangle(0).top() > window.height() / 2);

    // explicitly center aligned
    textInput->setVAlign(QQuickTextInput::AlignVCenter);
    QCOMPARE(textInput->vAlign(), QQuickTextInput::AlignVCenter);
    QVERIFY(textInput->boundingRect().top() < window.height() / 2);
    QVERIFY(textInput->boundingRect().bottom() > window.height() / 2);
    QVERIFY(textInput->cursorRectangle().top() < window.height() / 2);
    QVERIFY(textInput->cursorRectangle().bottom() > window.height() / 2);
    QVERIFY(textInput->positionToRectangle(0).top() < window.height() / 2);
    QVERIFY(textInput->positionToRectangle(0).bottom() > window.height() / 2);
}

void tst_qquicktextinput::clipRect()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n TextInput {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(object.data());
    QVERIFY(input);

    QCOMPARE(input->clipRect().x(), qreal(0));
    QCOMPARE(input->clipRect().y(), qreal(0));
    QCOMPARE(input->clipRect().width(), input->width() + input->cursorRectangle().width());
    QCOMPARE(input->clipRect().height(), input->height());

    input->setText("Hello World");
    QCOMPARE(input->clipRect().x(), qreal(0));
    QCOMPARE(input->clipRect().y(), qreal(0));
    QCOMPARE(input->clipRect().width(), input->width() + input->cursorRectangle().width());
    QCOMPARE(input->clipRect().height(), input->height());

    // clip rect shouldn't exceed the size of the item, expect for the cursor width;
    input->setWidth(input->width() / 2);
    QCOMPARE(input->clipRect().x(), qreal(0));
    QCOMPARE(input->clipRect().y(), qreal(0));
    QCOMPARE(input->clipRect().width(), input->width() + input->cursorRectangle().width());
    QCOMPARE(input->clipRect().height(), input->height());

    input->setHeight(input->height() * 2);
    QCOMPARE(input->clipRect().x(), qreal(0));
    QCOMPARE(input->clipRect().y(), qreal(0));
    QCOMPARE(input->clipRect().width(), input->width() + input->cursorRectangle().width());
    QCOMPARE(input->clipRect().height(), input->height());

    QQmlComponent cursorComponent(&engine);
    cursorComponent.setData("import QtQuick 2.0\nRectangle { height: 20; width: 8 }", QUrl());

    input->setCursorDelegate(&cursorComponent);
    input->setCursorVisible(true);

    // If a cursor delegate is used it's size should determine the excess width.
    QCOMPARE(input->clipRect().x(), qreal(0));
    QCOMPARE(input->clipRect().y(), qreal(0));
    QCOMPARE(input->clipRect().width(), input->width() + 8);
    QCOMPARE(input->clipRect().height(), input->height());

    // Alignment, auto scroll, wrapping all don't affect the clip rect.
    input->setAutoScroll(false);
    QCOMPARE(input->clipRect().x(), qreal(0));
    QCOMPARE(input->clipRect().y(), qreal(0));
    QCOMPARE(input->clipRect().width(), input->width() + 8);
    QCOMPARE(input->clipRect().height(), input->height());

    input->setHAlign(QQuickTextInput::AlignRight);
    QCOMPARE(input->clipRect().x(), qreal(0));
    QCOMPARE(input->clipRect().y(), qreal(0));
    QCOMPARE(input->clipRect().width(), input->width() + 8);
    QCOMPARE(input->clipRect().height(), input->height());

    input->setWrapMode(QQuickTextInput::Wrap);
    QCOMPARE(input->clipRect().x(), qreal(0));
    QCOMPARE(input->clipRect().y(), qreal(0));
    QCOMPARE(input->clipRect().width(), input->width() + 8);
    QCOMPARE(input->clipRect().height(), input->height());

    input->setVAlign(QQuickTextInput::AlignBottom);
    QCOMPARE(input->clipRect().x(), qreal(0));
    QCOMPARE(input->clipRect().y(), qreal(0));
    QCOMPARE(input->clipRect().width(), input->width() + 8);
    QCOMPARE(input->clipRect().height(), input->height());
}

void tst_qquicktextinput::boundingRect()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n TextInput {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(object.data());
    QVERIFY(input);

    QTextLayout layout;
    layout.setFont(input->font());

    if (!qmlDisableDistanceField()) {
        QTextOption option;
        option.setUseDesignMetrics(true);
        layout.setTextOption(option);
    }
    layout.beginLayout();
    QTextLine line = layout.createLine();
    layout.endLayout();

    QCOMPARE(input->boundingRect().x(), qreal(0));
    QCOMPARE(input->boundingRect().y(), qreal(0));
    QCOMPARE(input->boundingRect().width(), input->cursorRectangle().width());
    QCOMPARE(input->boundingRect().height(), line.height());

    input->setText("Hello World");

    layout.setText(input->text());
    layout.beginLayout();
    line = layout.createLine();
    layout.endLayout();

    QCOMPARE(input->boundingRect().x(), qreal(0));
    QCOMPARE(input->boundingRect().y(), qreal(0));
    QCOMPARE(input->boundingRect().width(), line.naturalTextWidth() + input->cursorRectangle().width());
    QCOMPARE(input->boundingRect().height(), line.height());

    // the size of the bounding rect shouldn't be bounded by the size of item.
    input->setWidth(input->width() / 2);
    QCOMPARE(input->boundingRect().x(), input->width() - line.naturalTextWidth());
    QCOMPARE(input->boundingRect().y(), qreal(0));
    QCOMPARE(input->boundingRect().width(), line.naturalTextWidth() + input->cursorRectangle().width());
    QCOMPARE(input->boundingRect().height(), line.height());

    input->setHeight(input->height() * 2);
    QCOMPARE(input->boundingRect().x(), input->width() - line.naturalTextWidth());
    QCOMPARE(input->boundingRect().y(), qreal(0));
    QCOMPARE(input->boundingRect().width(), line.naturalTextWidth() + input->cursorRectangle().width());
    QCOMPARE(input->boundingRect().height(), line.height());

    QQmlComponent cursorComponent(&engine);
    cursorComponent.setData("import QtQuick 2.0\nRectangle { height: 20; width: 8 }", QUrl());

    input->setCursorDelegate(&cursorComponent);
    input->setCursorVisible(true);

    // Don't include the size of a cursor delegate as it has its own bounding rect.
    QCOMPARE(input->boundingRect().x(), input->width() - line.naturalTextWidth());
    QCOMPARE(input->boundingRect().y(), qreal(0));
    QCOMPARE(input->boundingRect().width(), line.naturalTextWidth());
    QCOMPARE(input->boundingRect().height(), line.height());

    // Bounding rect left aligned when auto scroll is disabled;
    input->setAutoScroll(false);
    QCOMPARE(input->boundingRect().x(), qreal(0));
    QCOMPARE(input->boundingRect().y(), qreal(0));
    QCOMPARE(input->boundingRect().width(), line.naturalTextWidth());
    QCOMPARE(input->boundingRect().height(), line.height());

    input->setHAlign(QQuickTextInput::AlignRight);
    QCOMPARE(input->boundingRect().x(), input->width() - line.naturalTextWidth());
    QCOMPARE(input->boundingRect().y(), qreal(0));
    QCOMPARE(input->boundingRect().width(), line.naturalTextWidth());
    QCOMPARE(input->boundingRect().height(), line.height());

    input->setWrapMode(QQuickTextInput::Wrap);
    QCOMPARE(input->boundingRect().right(), input->width());
    QCOMPARE(input->boundingRect().y(), qreal(0));
    QVERIFY(input->boundingRect().width() < line.naturalTextWidth());
    QVERIFY(input->boundingRect().height() > line.height());

    input->setVAlign(QQuickTextInput::AlignBottom);
    QCOMPARE(input->boundingRect().right(), input->width());
    QCOMPARE(input->boundingRect().bottom(), input->height());
    QVERIFY(input->boundingRect().width() < line.naturalTextWidth());
    QVERIFY(input->boundingRect().height() > line.height());
}

void tst_qquicktextinput::positionAt()
{
    QQuickView window(testFileUrl("positionAt.qml"));
    QVERIFY(window.rootObject() != 0);
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput *>(window.rootObject());
    QVERIFY(textinputObject != 0);

    // Check autoscrolled...

    int pos = evaluate<int>(textinputObject, QString("positionAt(%1)").arg(textinputObject->width()/2));

    QTextLayout layout(textinputObject->text());
    layout.setFont(textinputObject->font());

    if (!qmlDisableDistanceField()) {
        QTextOption option;
        option.setUseDesignMetrics(true);
        layout.setTextOption(option);
    }
    layout.beginLayout();
    QTextLine line = layout.createLine();
    layout.endLayout();

    int textLeftWidthBegin = floor(line.cursorToX(pos - 1));
    int textLeftWidthEnd = ceil(line.cursorToX(pos + 1));
    int textWidth = floor(line.horizontalAdvance());

    QVERIFY(textLeftWidthBegin <= textWidth - textinputObject->width() / 2);
    QVERIFY(textLeftWidthEnd >= textWidth - textinputObject->width() / 2);

    int x = textinputObject->positionToRectangle(pos + 1).x() - 1;
    QCOMPARE(evaluate<int>(textinputObject, QString("positionAt(%1, 0, TextInput.CursorBetweenCharacters)").arg(x)), pos + 1);
    QCOMPARE(evaluate<int>(textinputObject, QString("positionAt(%1, 0, TextInput.CursorOnCharacter)").arg(x)), pos);

    // Check without autoscroll...
    textinputObject->setAutoScroll(false);
    pos = evaluate<int>(textinputObject, QString("positionAt(%1)").arg(textinputObject->width() / 2));

    textLeftWidthBegin = floor(line.cursorToX(pos - 1));
    textLeftWidthEnd = ceil(line.cursorToX(pos + 1));

    QVERIFY(textLeftWidthBegin <= textinputObject->width() / 2);
    QVERIFY(textLeftWidthEnd >= textinputObject->width() / 2);

    x = textinputObject->positionToRectangle(pos + 1).x() - 1;
    QCOMPARE(evaluate<int>(textinputObject, QString("positionAt(%1, 0, TextInput.CursorBetweenCharacters)").arg(x)), pos + 1);
    QCOMPARE(evaluate<int>(textinputObject, QString("positionAt(%1, 0, TextInput.CursorOnCharacter)").arg(x)), pos);

    const qreal x0 = textinputObject->positionToRectangle(pos).x();
    const qreal x1 = textinputObject->positionToRectangle(pos + 1).x();

    QString preeditText = textinputObject->text().mid(0, pos);
    textinputObject->setText(textinputObject->text().mid(pos));
    textinputObject->setCursorPosition(0);

    {   QInputMethodEvent inputEvent(preeditText, QList<QInputMethodEvent::Attribute>());
        QVERIFY(qGuiApp->focusObject());
        QGuiApplication::sendEvent(textinputObject, &inputEvent); }

    // Check all points within the preedit text return the same position.
    QCOMPARE(evaluate<int>(textinputObject, QString("positionAt(%1)").arg(0)), 0);
    QCOMPARE(evaluate<int>(textinputObject, QString("positionAt(%1)").arg(x0 / 2)), 0);
    QCOMPARE(evaluate<int>(textinputObject, QString("positionAt(%1)").arg(x0)), 0);

    // Verify positioning returns to normal after the preedit text.
    QCOMPARE(evaluate<int>(textinputObject, QString("positionAt(%1)").arg(x1)), 1);
    QCOMPARE(textinputObject->positionToRectangle(1).x(), x1);

    {   QInputMethodEvent inputEvent;
        QVERIFY(qGuiApp->focusObject());
        QGuiApplication::sendEvent(textinputObject, &inputEvent); }

    // With wrapping.
    textinputObject->setWrapMode(QQuickTextInput::WrapAnywhere);

    const qreal y0 = line.height() / 2;
    const qreal y1 = line.height() * 3 / 2;

    QCOMPARE(evaluate<int>(textinputObject, QString("positionAt(%1, %2)").arg(x0).arg(y0)), pos);
    QCOMPARE(evaluate<int>(textinputObject, QString("positionAt(%1, %2)").arg(x1).arg(y0)), pos + 1);

    int newLinePos = evaluate<int>(textinputObject, QString("positionAt(%1, %2)").arg(x0).arg(y1));
    QVERIFY(newLinePos > pos);
    QCOMPARE(evaluate<int>(textinputObject, QString("positionAt(%1, %2)").arg(x1).arg(y1)), newLinePos + 1);
}

void tst_qquicktextinput::maxLength()
{
    QQuickView window(testFileUrl("maxLength.qml"));
    QVERIFY(window.rootObject() != 0);
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput *>(window.rootObject());
    QVERIFY(textinputObject != 0);
    QVERIFY(textinputObject->text().isEmpty());
    QCOMPARE(textinputObject->maxLength(), 10);
    foreach (const QString &str, standard) {
        QVERIFY(textinputObject->text().length() <= 10);
        textinputObject->setText(str);
        QVERIFY(textinputObject->text().length() <= 10);
    }

    textinputObject->setText("");
    QTRY_VERIFY(textinputObject->hasActiveFocus());
    for (int i=0; i<20; i++) {
        QTRY_COMPARE(textinputObject->text().length(), qMin(i,10));
        //simulateKey(&window, Qt::Key_A);
        QTest::keyPress(&window, Qt::Key_A);
        QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    }
}

void tst_qquicktextinput::masks()
{
    //Not a comprehensive test of the possible masks, that's done elsewhere (QLineEdit)
    //QString componentStr = "import QtQuick 2.0\nTextInput {  inputMask: 'HHHHhhhh'; }";
    QQuickView window(testFileUrl("masks.qml"));
    window.show();
    window.requestActivate();
    QVERIFY(window.rootObject() != 0);
    QQuickTextInput *textinputObject = qobject_cast<QQuickTextInput *>(window.rootObject());
    QVERIFY(textinputObject != 0);
    QTRY_VERIFY(textinputObject->hasActiveFocus());
    QCOMPARE(textinputObject->text().length(), 0);
    QCOMPARE(textinputObject->inputMask(), QString("HHHHhhhh; "));
    QCOMPARE(textinputObject->length(), 8);
    for (int i=0; i<10; i++) {
        QTRY_COMPARE(qMin(i,8), textinputObject->text().length());
        QCOMPARE(textinputObject->length(), 8);
        QCOMPARE(textinputObject->getText(0, qMin(i, 8)), QString(qMin(i, 8), 'a'));
        QCOMPARE(textinputObject->getText(qMin(i, 8), 8), QString(8 - qMin(i, 8), ' '));
        QCOMPARE(i>=4, textinputObject->hasAcceptableInput());
        //simulateKey(&window, Qt::Key_A);
        QTest::keyPress(&window, Qt::Key_A);
        QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    }
}

void tst_qquicktextinput::validators()
{
    // Note that this test assumes that the validators are working properly
    // so you may need to run their tests first. All validators are checked
    // here to ensure that their exposure to QML is working.

    QLocale::setDefault(QLocale(QStringLiteral("C")));

    QQuickView window(testFileUrl("validators.qml"));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);

    QLocale defaultLocale;
    QLocale enLocale("en");
    QLocale deLocale("de_DE");

    QQuickTextInput *intInput = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("intInput")));
    QVERIFY(intInput);
    QSignalSpy intSpy(intInput, SIGNAL(acceptableInputChanged()));

    QQuickIntValidator *intValidator = qobject_cast<QQuickIntValidator *>(intInput->validator());
    QVERIFY(intValidator);
    QCOMPARE(intValidator->localeName(), defaultLocale.name());
    QCOMPARE(intInput->validator()->locale(), defaultLocale);
    intValidator->setLocaleName(enLocale.name());
    QCOMPARE(intValidator->localeName(), enLocale.name());
    QCOMPARE(intInput->validator()->locale(), enLocale);
    intValidator->resetLocaleName();
    QCOMPARE(intValidator->localeName(), defaultLocale.name());
    QCOMPARE(intInput->validator()->locale(), defaultLocale);

    intInput->setFocus(true);
    QTRY_VERIFY(intInput->hasActiveFocus());
    QCOMPARE(intInput->hasAcceptableInput(), false);
    QCOMPARE(intInput->property("acceptable").toBool(), false);
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QTRY_COMPARE(intInput->text(), QLatin1String("1"));
    QCOMPARE(intInput->hasAcceptableInput(), false);
    QCOMPARE(intInput->property("acceptable").toBool(), false);
    QCOMPARE(intSpy.count(), 0);
    QTest::keyPress(&window, Qt::Key_2);
    QTest::keyRelease(&window, Qt::Key_2, Qt::NoModifier);
    QTRY_COMPARE(intInput->text(), QLatin1String("1"));
    QCOMPARE(intInput->hasAcceptableInput(), false);
    QCOMPARE(intInput->property("acceptable").toBool(), false);
    QCOMPARE(intSpy.count(), 0);
    QTest::keyPress(&window, Qt::Key_Period);
    QTest::keyRelease(&window, Qt::Key_Period, Qt::NoModifier);
    QTRY_COMPARE(intInput->text(), QLatin1String("1"));
    QCOMPARE(intInput->hasAcceptableInput(), false);
    QTest::keyPress(&window, Qt::Key_Comma);
    QTest::keyRelease(&window, Qt::Key_Comma, Qt::NoModifier);
    QTRY_COMPARE(intInput->text(), QLatin1String("1,"));
    QCOMPARE(intInput->hasAcceptableInput(), false);
    QTest::keyPress(&window, Qt::Key_Backspace);
    QTest::keyRelease(&window, Qt::Key_Backspace, Qt::NoModifier);
    QTRY_COMPARE(intInput->text(), QLatin1String("1"));
    QCOMPARE(intInput->hasAcceptableInput(), false);
    intValidator->setLocaleName(deLocale.name());
    QTest::keyPress(&window, Qt::Key_Period);
    QTest::keyRelease(&window, Qt::Key_Period, Qt::NoModifier);
    QTRY_COMPARE(intInput->text(), QLatin1String("1."));
    QCOMPARE(intInput->hasAcceptableInput(), false);
    QTest::keyPress(&window, Qt::Key_Backspace);
    QTest::keyRelease(&window, Qt::Key_Backspace, Qt::NoModifier);
    QTRY_COMPARE(intInput->text(), QLatin1String("1"));
    QCOMPARE(intInput->hasAcceptableInput(), false);
    intValidator->resetLocaleName();
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QCOMPARE(intInput->text(), QLatin1String("11"));
    QCOMPARE(intInput->hasAcceptableInput(), true);
    QCOMPARE(intInput->property("acceptable").toBool(), true);
    QCOMPARE(intSpy.count(), 1);
    QTest::keyPress(&window, Qt::Key_0);
    QTest::keyRelease(&window, Qt::Key_0, Qt::NoModifier);
    QCOMPARE(intInput->text(), QLatin1String("11"));
    QCOMPARE(intInput->hasAcceptableInput(), true);
    QCOMPARE(intInput->property("acceptable").toBool(), true);
    QCOMPARE(intSpy.count(), 1);

    QQuickTextInput *dblInput = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("dblInput")));
    QVERIFY(dblInput);
    QSignalSpy dblSpy(dblInput, SIGNAL(acceptableInputChanged()));

    QQuickDoubleValidator *dblValidator = qobject_cast<QQuickDoubleValidator *>(dblInput->validator());
    QVERIFY(dblValidator);
    QCOMPARE(dblValidator->localeName(), defaultLocale.name());
    QCOMPARE(dblInput->validator()->locale(), defaultLocale);
    dblValidator->setLocaleName(enLocale.name());
    QCOMPARE(dblValidator->localeName(), enLocale.name());
    QCOMPARE(dblInput->validator()->locale(), enLocale);
    dblValidator->resetLocaleName();
    QCOMPARE(dblValidator->localeName(), defaultLocale.name());
    QCOMPARE(dblInput->validator()->locale(), defaultLocale);

    dblInput->setFocus(true);
    QVERIFY(dblInput->hasActiveFocus());
    QCOMPARE(dblInput->hasAcceptableInput(), false);
    QCOMPARE(dblInput->property("acceptable").toBool(), false);
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("1"));
    QCOMPARE(dblInput->hasAcceptableInput(), false);
    QCOMPARE(dblInput->property("acceptable").toBool(), false);
    QCOMPARE(dblSpy.count(), 0);
    QTest::keyPress(&window, Qt::Key_2);
    QTest::keyRelease(&window, Qt::Key_2, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12"));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QCOMPARE(dblInput->property("acceptable").toBool(), true);
    QCOMPARE(dblSpy.count(), 1);
    QTest::keyPress(&window, Qt::Key_Comma);
    QTest::keyRelease(&window, Qt::Key_Comma, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12,"));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12,"));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    dblValidator->setLocaleName(deLocale.name());
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12,1"));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12,11"));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QTest::keyPress(&window, Qt::Key_Backspace);
    QTest::keyRelease(&window, Qt::Key_Backspace, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12,1"));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QTest::keyPress(&window, Qt::Key_Backspace);
    QTest::keyRelease(&window, Qt::Key_Backspace, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12,"));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QTest::keyPress(&window, Qt::Key_Backspace);
    QTest::keyRelease(&window, Qt::Key_Backspace, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12"));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    dblValidator->resetLocaleName();
    QTest::keyPress(&window, Qt::Key_Period);
    QTest::keyRelease(&window, Qt::Key_Period, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12."));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QCOMPARE(dblInput->property("acceptable").toBool(), true);
    QCOMPARE(dblSpy.count(), 1);
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12.1"));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QCOMPARE(dblInput->property("acceptable").toBool(), true);
    QCOMPARE(dblSpy.count(), 1);
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12.11"));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QCOMPARE(dblInput->property("acceptable").toBool(), true);
    QCOMPARE(dblSpy.count(), 1);
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12.11"));
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QCOMPARE(dblInput->property("acceptable").toBool(), true);
    QCOMPARE(dblSpy.count(), 1);

    // Ensure the validator doesn't prevent characters being removed.
    dblInput->setValidator(intInput->validator());
    QCOMPARE(dblInput->text(), QLatin1String("12.11"));
    QCOMPARE(dblInput->hasAcceptableInput(), false);
    QCOMPARE(dblInput->property("acceptable").toBool(), false);
    QCOMPARE(dblSpy.count(), 2);
    QTest::keyPress(&window, Qt::Key_Backspace);
    QTest::keyRelease(&window, Qt::Key_Backspace, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12.1"));
    QCOMPARE(dblInput->hasAcceptableInput(), false);
    QCOMPARE(dblInput->property("acceptable").toBool(), false);
    QCOMPARE(dblSpy.count(), 2);
    // Once unacceptable input is in anything goes until it reaches an acceptable state again.
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12.11"));
    QCOMPARE(dblInput->hasAcceptableInput(), false);
    QCOMPARE(dblSpy.count(), 2);
    QTest::keyPress(&window, Qt::Key_Backspace);
    QTest::keyRelease(&window, Qt::Key_Backspace, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12.1"));
    QCOMPARE(dblInput->hasAcceptableInput(), false);
    QCOMPARE(dblInput->property("acceptable").toBool(), false);
    QCOMPARE(dblSpy.count(), 2);
    QTest::keyPress(&window, Qt::Key_Backspace);
    QTest::keyRelease(&window, Qt::Key_Backspace, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12."));
    QCOMPARE(dblInput->hasAcceptableInput(), false);
    QCOMPARE(dblInput->property("acceptable").toBool(), false);
    QCOMPARE(dblSpy.count(), 2);
    QTest::keyPress(&window, Qt::Key_Backspace);
    QTest::keyRelease(&window, Qt::Key_Backspace, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("12"));
    QCOMPARE(dblInput->hasAcceptableInput(), false);
    QCOMPARE(dblInput->property("acceptable").toBool(), false);
    QCOMPARE(dblSpy.count(), 2);
    QTest::keyPress(&window, Qt::Key_Backspace);
    QTest::keyRelease(&window, Qt::Key_Backspace, Qt::NoModifier);
    QTRY_COMPARE(dblInput->text(), QLatin1String("1"));
    QCOMPARE(dblInput->hasAcceptableInput(), false);
    QCOMPARE(dblInput->property("acceptable").toBool(), false);
    QCOMPARE(dblSpy.count(), 2);
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QCOMPARE(dblInput->text(), QLatin1String("11"));
    QCOMPARE(dblInput->property("acceptable").toBool(), true);
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QCOMPARE(dblSpy.count(), 3);

    // Changing the validator properties will re-evaluate whether the input is acceptable.
    intValidator->setTop(10);
    QCOMPARE(dblInput->property("acceptable").toBool(), false);
    QCOMPARE(dblInput->hasAcceptableInput(), false);
    QCOMPARE(dblSpy.count(), 4);
    intValidator->setTop(12);
    QCOMPARE(dblInput->property("acceptable").toBool(), true);
    QCOMPARE(dblInput->hasAcceptableInput(), true);
    QCOMPARE(dblSpy.count(), 5);

    QQuickTextInput *strInput = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("strInput")));
    QVERIFY(strInput);
    QSignalSpy strSpy(strInput, SIGNAL(acceptableInputChanged()));
    strInput->setFocus(true);
    QVERIFY(strInput->hasActiveFocus());
    QCOMPARE(strInput->hasAcceptableInput(), false);
    QCOMPARE(strInput->property("acceptable").toBool(), false);
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QTRY_COMPARE(strInput->text(), QLatin1String(""));
    QCOMPARE(strInput->hasAcceptableInput(), false);
    QCOMPARE(strInput->property("acceptable").toBool(), false);
    QCOMPARE(strSpy.count(), 0);
    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(strInput->text(), QLatin1String("a"));
    QCOMPARE(strInput->hasAcceptableInput(), false);
    QCOMPARE(strInput->property("acceptable").toBool(), false);
    QCOMPARE(strSpy.count(), 0);
    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(strInput->text(), QLatin1String("aa"));
    QCOMPARE(strInput->hasAcceptableInput(), true);
    QCOMPARE(strInput->property("acceptable").toBool(), true);
    QCOMPARE(strSpy.count(), 1);
    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(strInput->text(), QLatin1String("aaa"));
    QCOMPARE(strInput->hasAcceptableInput(), true);
    QCOMPARE(strInput->property("acceptable").toBool(), true);
    QCOMPARE(strSpy.count(), 1);
    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(strInput->text(), QLatin1String("aaaa"));
    QCOMPARE(strInput->hasAcceptableInput(), true);
    QCOMPARE(strInput->property("acceptable").toBool(), true);
    QCOMPARE(strSpy.count(), 1);
    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(strInput->text(), QLatin1String("aaaa"));
    QCOMPARE(strInput->hasAcceptableInput(), true);
    QCOMPARE(strInput->property("acceptable").toBool(), true);
    QCOMPARE(strSpy.count(), 1);

    QQuickTextInput *unvalidatedInput = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("unvalidatedInput")));
    QVERIFY(unvalidatedInput);
    QSignalSpy unvalidatedSpy(unvalidatedInput, SIGNAL(acceptableInputChanged()));
    unvalidatedInput->setFocus(true);
    QVERIFY(unvalidatedInput->hasActiveFocus());
    QCOMPARE(unvalidatedInput->hasAcceptableInput(), true);
    QCOMPARE(unvalidatedInput->property("acceptable").toBool(), true);
    QTest::keyPress(&window, Qt::Key_1);
    QTest::keyRelease(&window, Qt::Key_1, Qt::NoModifier);
    QTRY_COMPARE(unvalidatedInput->text(), QLatin1String("1"));
    QCOMPARE(unvalidatedInput->hasAcceptableInput(), true);
    QCOMPARE(unvalidatedInput->property("acceptable").toBool(), true);
    QCOMPARE(unvalidatedSpy.count(), 0);
    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(unvalidatedInput->text(), QLatin1String("1a"));
    QCOMPARE(unvalidatedInput->hasAcceptableInput(), true);
    QCOMPARE(unvalidatedInput->property("acceptable").toBool(), true);
    QCOMPARE(unvalidatedSpy.count(), 0);
}

void tst_qquicktextinput::inputMethods()
{
    QQuickView window(testFileUrl("inputmethods.qml"));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    // test input method hints
    QVERIFY(window.rootObject() != 0);
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(window.rootObject());
    QVERIFY(input != 0);
    QVERIFY(input->inputMethodHints() & Qt::ImhNoPredictiveText);
    QSignalSpy inputMethodHintSpy(input, SIGNAL(inputMethodHintsChanged()));
    input->setInputMethodHints(Qt::ImhUppercaseOnly);
    QVERIFY(input->inputMethodHints() & Qt::ImhUppercaseOnly);
    QCOMPARE(inputMethodHintSpy.count(), 1);
    input->setInputMethodHints(Qt::ImhUppercaseOnly);
    QCOMPARE(inputMethodHintSpy.count(), 1);

    // default value
    QQuickTextInput plainInput;
    QCOMPARE(plainInput.inputMethodHints(), Qt::ImhNone);

    input->setFocus(true);
    QVERIFY(input->hasActiveFocus());
    // test that input method event is committed
    QInputMethodEvent event;
    event.setCommitString( "My ", -12, 0);
    QTRY_COMPARE(qGuiApp->focusObject(), input);
    QGuiApplication::sendEvent(input, &event);
    QCOMPARE(input->text(), QString("My Hello world!"));
    QCOMPARE(input->displayText(), QString("My Hello world!"));

    input->setCursorPosition(2);
    event.setCommitString("Your", -2, 2);
    QGuiApplication::sendEvent(input, &event);
    QCOMPARE(input->text(), QString("Your Hello world!"));
    QCOMPARE(input->displayText(), QString("Your Hello world!"));
    QCOMPARE(input->cursorPosition(), 4);

    input->setCursorPosition(7);
    event.setCommitString("Goodbye", -2, 5);
    QGuiApplication::sendEvent(input, &event);
    QCOMPARE(input->text(), QString("Your Goodbye world!"));
    QCOMPARE(input->displayText(), QString("Your Goodbye world!"));
    QCOMPARE(input->cursorPosition(), 12);

    input->setCursorPosition(8);
    event.setCommitString("Our", -8, 4);
    QGuiApplication::sendEvent(input, &event);
    QCOMPARE(input->text(), QString("Our Goodbye world!"));
    QCOMPARE(input->displayText(), QString("Our Goodbye world!"));
    QCOMPARE(input->cursorPosition(), 7);

    QInputMethodEvent preeditEvent("PREEDIT", QList<QInputMethodEvent::Attribute>());
    QGuiApplication::sendEvent(input, &preeditEvent);
    QCOMPARE(input->text(), QString("Our Goodbye world!"));
    QCOMPARE(input->displayText(), QString("Our GooPREEDITdbye world!"));
    QCOMPARE(input->preeditText(), QString("PREEDIT"));

    QInputMethodEvent preeditEvent2("PREEDIT2", QList<QInputMethodEvent::Attribute>());
    QGuiApplication::sendEvent(input, &preeditEvent2);
    QCOMPARE(input->text(), QString("Our Goodbye world!"));
    QCOMPARE(input->displayText(), QString("Our GooPREEDIT2dbye world!"));
    QCOMPARE(input->preeditText(), QString("PREEDIT2"));

    QInputMethodEvent preeditEvent3("", QList<QInputMethodEvent::Attribute>());
    QGuiApplication::sendEvent(input, &preeditEvent3);
    QCOMPARE(input->text(), QString("Our Goodbye world!"));
    QCOMPARE(input->displayText(), QString("Our Goodbye world!"));
    QCOMPARE(input->preeditText(), QString(""));

    // input should reset selection even if replacement parameters are out of bounds
    input->setText("text");
    input->setCursorPosition(0);
    input->moveCursorSelection(input->text().length());
    event.setCommitString("replacement", -input->text().length(), input->text().length());
    QGuiApplication::sendEvent(input, &event);
    QCOMPARE(input->selectionStart(), input->selectionEnd());
    QCOMPARE(input->text(), QString("replacement"));
    QCOMPARE(input->displayText(), QString("replacement"));

    QInputMethodQueryEvent enabledQueryEvent(Qt::ImEnabled);
    QGuiApplication::sendEvent(input, &enabledQueryEvent);
    QCOMPARE(enabledQueryEvent.value(Qt::ImEnabled).toBool(), true);

    input->setReadOnly(true);
    QGuiApplication::sendEvent(input, &enabledQueryEvent);
    QCOMPARE(enabledQueryEvent.value(Qt::ImEnabled).toBool(), false);
}

void tst_qquicktextinput::signal_accepted()
{
    QQuickView window(testFileUrl("signal_accepted.qml"));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);

    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("input")));
    QVERIFY(input);
    QSignalSpy acceptedSpy(input, SIGNAL(accepted()));
    QSignalSpy inputSpy(input, SIGNAL(acceptableInputChanged()));

    input->setFocus(true);
    QTRY_VERIFY(input->hasActiveFocus());
    QCOMPARE(input->hasAcceptableInput(), false);
    QCOMPARE(input->property("acceptable").toBool(), false);

    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(input->text(), QLatin1String("a"));
    QCOMPARE(input->hasAcceptableInput(), false);
    QCOMPARE(input->property("acceptable").toBool(), false);
    QTRY_COMPARE(inputSpy.count(), 0);

    QTest::keyPress(&window, Qt::Key_Enter);
    QTest::keyRelease(&window, Qt::Key_Enter, Qt::NoModifier);
    QTRY_COMPARE(acceptedSpy.count(), 0);

    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(input->text(), QLatin1String("aa"));
    QCOMPARE(input->hasAcceptableInput(), true);
    QCOMPARE(input->property("acceptable").toBool(), true);
    QTRY_COMPARE(inputSpy.count(), 1);

    QTest::keyPress(&window, Qt::Key_Enter);
    QTest::keyRelease(&window, Qt::Key_Enter, Qt::NoModifier);
    QTRY_COMPARE(acceptedSpy.count(), 1);
}

void tst_qquicktextinput::signal_editingfinished()
{
    QQuickView window(testFileUrl("signal_editingfinished.qml"));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);

    QQuickTextInput *input1 = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("input1")));
    QVERIFY(input1);
    QQuickTextInput *input2 = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("input2")));
    QVERIFY(input2);
    QSignalSpy editingFinished1Spy(input1, SIGNAL(editingFinished()));
    QSignalSpy input1Spy(input1, SIGNAL(acceptableInputChanged()));

    input1->setFocus(true);
    QTRY_VERIFY(input1->hasActiveFocus());
    QTRY_VERIFY(!input2->hasActiveFocus());

    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(input1->text(), QLatin1String("a"));
    QCOMPARE(input1->hasAcceptableInput(), false);
    QCOMPARE(input1->property("acceptable").toBool(), false);
    QTRY_COMPARE(input1Spy.count(), 0);

    QTest::keyPress(&window, Qt::Key_Enter);
    QTest::keyRelease(&window, Qt::Key_Enter, Qt::NoModifier);
    QTRY_COMPARE(editingFinished1Spy.count(), 0);

    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(input1->text(), QLatin1String("aa"));
    QCOMPARE(input1->hasAcceptableInput(), true);
    QCOMPARE(input1->property("acceptable").toBool(), true);
    QTRY_COMPARE(input1Spy.count(), 1);

    QTest::keyPress(&window, Qt::Key_Enter);
    QTest::keyRelease(&window, Qt::Key_Enter, Qt::NoModifier);
    QTRY_COMPARE(editingFinished1Spy.count(), 1);
    QTRY_COMPARE(input1Spy.count(), 1);

    QSignalSpy editingFinished2Spy(input2, SIGNAL(editingFinished()));
    QSignalSpy input2Spy(input2, SIGNAL(acceptableInputChanged()));

    input2->setFocus(true);
    QTRY_VERIFY(!input1->hasActiveFocus());
    QTRY_VERIFY(input2->hasActiveFocus());

    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(input2->text(), QLatin1String("a"));
    QCOMPARE(input2->hasAcceptableInput(), false);
    QCOMPARE(input2->property("acceptable").toBool(), false);
    QTRY_COMPARE(input2Spy.count(), 0);

    QTest::keyPress(&window, Qt::Key_A);
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier);
    QTRY_COMPARE(input2->text(), QLatin1String("aa"));
    QCOMPARE(input2->hasAcceptableInput(), true);
    QCOMPARE(input2->property("acceptable").toBool(), true);
    QTRY_COMPARE(input2Spy.count(), 1);

    input1->setFocus(true);
    QTRY_VERIFY(input1->hasActiveFocus());
    QTRY_VERIFY(!input2->hasActiveFocus());
    QTRY_COMPARE(editingFinished2Spy.count(), 1);
}

/*
TextInput element should only handle left/right keys until the cursor reaches
the extent of the text, then they should ignore the keys.

*/
void tst_qquicktextinput::navigation()
{
    QQuickView window(testFileUrl("navigation.qml"));
    window.show();
    window.requestActivate();

    QVERIFY(window.rootObject() != 0);

    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("myInput")));

    QVERIFY(input != 0);
    input->setCursorPosition(0);
    QTRY_VERIFY(input->hasActiveFocus());
    simulateKey(&window, Qt::Key_Left);
    QVERIFY(!input->hasActiveFocus());
    simulateKey(&window, Qt::Key_Right);
    QVERIFY(input->hasActiveFocus());
    //QT-2944: If text is selected, ensure we deselect upon cursor motion
    input->setCursorPosition(input->text().length());
    input->select(0,input->text().length());
    QVERIFY(input->selectionStart() != input->selectionEnd());
    simulateKey(&window, Qt::Key_Right);
    QCOMPARE(input->selectionStart(), input->selectionEnd());
    QCOMPARE(input->selectionStart(), input->text().length());
    QVERIFY(input->hasActiveFocus());
    simulateKey(&window, Qt::Key_Right);
    QVERIFY(!input->hasActiveFocus());
    simulateKey(&window, Qt::Key_Left);
    QVERIFY(input->hasActiveFocus());

    // Up and Down should NOT do Home/End, even on OS X (QTBUG-10438).
    input->setCursorPosition(2);
    QCOMPARE(input->cursorPosition(),2);
    simulateKey(&window, Qt::Key_Up);
    QCOMPARE(input->cursorPosition(),2);
    simulateKey(&window, Qt::Key_Down);
    QCOMPARE(input->cursorPosition(),2);

    // Test left and right navigation works if the TextInput is empty (QTBUG-25447).
    input->setText(QString());
    QCOMPARE(input->cursorPosition(), 0);
    simulateKey(&window, Qt::Key_Right);
    QCOMPARE(input->hasActiveFocus(), false);
    simulateKey(&window, Qt::Key_Left);
    QCOMPARE(input->hasActiveFocus(), true);
    simulateKey(&window, Qt::Key_Left);
    QCOMPARE(input->hasActiveFocus(), false);
}

void tst_qquicktextinput::navigation_RTL()
{
    QQuickView window(testFileUrl("navigation.qml"));
    window.show();
    window.requestActivate();

    QVERIFY(window.rootObject() != 0);

    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("myInput")));

    QVERIFY(input != 0);
    const quint16 arabic_str[] = { 0x0638, 0x0643, 0x00646, 0x0647, 0x0633, 0x0638, 0x0643, 0x00646, 0x0647, 0x0633, 0x0647};
    input->setText(QString::fromUtf16(arabic_str, 11));

    input->setCursorPosition(0);
    QTRY_VERIFY(input->hasActiveFocus());

    // move off
    simulateKey(&window, Qt::Key_Right);
    QVERIFY(!input->hasActiveFocus());

    // move back
    simulateKey(&window, Qt::Key_Left);
    QVERIFY(input->hasActiveFocus());

    input->setCursorPosition(input->text().length());
    QVERIFY(input->hasActiveFocus());

    // move off
    simulateKey(&window, Qt::Key_Left);
    QVERIFY(!input->hasActiveFocus());

    // move back
    simulateKey(&window, Qt::Key_Right);
    QVERIFY(input->hasActiveFocus());
}

#ifndef QT_NO_CLIPBOARD
void tst_qquicktextinput::copyAndPaste()
{
    if (!PlatformQuirks::isClipboardAvailable())
        QSKIP("This machine doesn't support the clipboard");

    QString componentStr = "import QtQuick 2.0\nTextInput { text: \"Hello world!\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    // copy and paste
    QCOMPARE(textInput->text().length(), 12);
    textInput->select(0, textInput->text().length());
    textInput->copy();
    QCOMPARE(textInput->selectedText(), QString("Hello world!"));
    QCOMPARE(textInput->selectedText().length(), 12);
    textInput->setCursorPosition(0);
    QTRY_VERIFY(textInput->canPaste());
    textInput->paste();
    QCOMPARE(textInput->text(), QString("Hello world!Hello world!"));
    QCOMPARE(textInput->text().length(), 24);

    // can paste
    QVERIFY(textInput->canPaste());
    textInput->setReadOnly(true);
    QVERIFY(!textInput->canPaste());
    textInput->paste();
    QCOMPARE(textInput->text(), QString("Hello world!Hello world!"));
    QCOMPARE(textInput->text().length(), 24);
    textInput->setReadOnly(false);
    QVERIFY(textInput->canPaste());

    // cut: no selection
    textInput->cut();
    QCOMPARE(textInput->text(), QString("Hello world!Hello world!"));

    // select word
    textInput->setCursorPosition(0);
    textInput->selectWord();
    QCOMPARE(textInput->selectedText(), QString("Hello"));

    // cut: read only.
    textInput->setReadOnly(true);
    textInput->cut();
    QCOMPARE(textInput->text(), QString("Hello world!Hello world!"));
    textInput->setReadOnly(false);

    // select all and cut
    textInput->selectAll();
    textInput->cut();
    QCOMPARE(textInput->text().length(), 0);
    textInput->paste();
    QCOMPARE(textInput->text(), QString("Hello world!Hello world!"));
    QCOMPARE(textInput->text().length(), 24);

    // Copy first word.
    textInput->setCursorPosition(0);
    textInput->selectWord();
    textInput->copy();
    // copy: no selection, previous copy retained;
    textInput->setCursorPosition(0);
    QCOMPARE(textInput->selectedText(), QString());
    textInput->copy();
    textInput->setText(QString());
    textInput->paste();
    QCOMPARE(textInput->text(), QString("Hello"));

    // clear copy buffer
    QClipboard *clipboard = QGuiApplication::clipboard();
    QVERIFY(clipboard);
    clipboard->clear();
    QTRY_VERIFY(!textInput->canPaste());

    // test that copy functionality is disabled
    // when echo mode is set to hide text/password mode
    int index = 0;
    while (index < 4) {
        QQuickTextInput::EchoMode echoMode = QQuickTextInput::EchoMode(index);
        textInput->setEchoMode(echoMode);
        textInput->setText("My password");
        textInput->select(0, textInput->text().length());
        textInput->copy();
        if (echoMode == QQuickTextInput::Normal) {
            QVERIFY(!clipboard->text().isEmpty());
            QCOMPARE(clipboard->text(), QString("My password"));
            clipboard->clear();
        } else {
            QVERIFY(clipboard->text().isEmpty());
        }
        index++;
    }

    delete textInput;
}
#endif

#ifndef QT_NO_CLIPBOARD
void tst_qquicktextinput::copyAndPasteKeySequence()
{
    if (!PlatformQuirks::isClipboardAvailable())
        QSKIP("This machine doesn't support the clipboard");

    QString componentStr = "import QtQuick 2.0\nTextInput { text: \"Hello world!\"; focus: true }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    QQuickWindow window;
    textInput->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    // copy and paste
    QVERIFY(textInput->hasActiveFocus());
    QCOMPARE(textInput->text().length(), 12);
    textInput->select(0, textInput->text().length());
    simulateKeys(&window, QKeySequence::Copy);
    QCOMPARE(textInput->selectedText(), QString("Hello world!"));
    QCOMPARE(textInput->selectedText().length(), 12);
    textInput->setCursorPosition(0);
    QVERIFY(textInput->canPaste());
    simulateKeys(&window, QKeySequence::Paste);
    QCOMPARE(textInput->text(), QString("Hello world!Hello world!"));
    QCOMPARE(textInput->text().length(), 24);

    // select all and cut
    simulateKeys(&window, QKeySequence::SelectAll);
    simulateKeys(&window, QKeySequence::Cut);
    QCOMPARE(textInput->text().length(), 0);
    simulateKeys(&window, QKeySequence::Paste);
    QCOMPARE(textInput->text(), QString("Hello world!Hello world!"));
    QCOMPARE(textInput->text().length(), 24);

    // clear copy buffer
    QClipboard *clipboard = QGuiApplication::clipboard();
    QVERIFY(clipboard);
    clipboard->clear();
    QTRY_VERIFY(!textInput->canPaste());

    // test that copy functionality is disabled
    // when echo mode is set to hide text/password mode
    int index = 0;
    while (index < 4) {
        QQuickTextInput::EchoMode echoMode = QQuickTextInput::EchoMode(index);
        textInput->setEchoMode(echoMode);
        textInput->setText("My password");
        textInput->select(0, textInput->text().length());
        simulateKeys(&window, QKeySequence::Copy);
        if (echoMode == QQuickTextInput::Normal) {
            QVERIFY(!clipboard->text().isEmpty());
            QCOMPARE(clipboard->text(), QString("My password"));
            clipboard->clear();
        } else {
            QVERIFY(clipboard->text().isEmpty());
        }
        index++;
    }

    delete textInput;
}
#endif

#ifndef QT_NO_CLIPBOARD
void tst_qquicktextinput::canPasteEmpty()
{
    QGuiApplication::clipboard()->clear();

    QString componentStr = "import QtQuick 2.0\nTextInput { text: \"Hello world!\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    bool cp = !textInput->isReadOnly() && QGuiApplication::clipboard()->text().length() != 0;
    QCOMPARE(textInput->canPaste(), cp);
}
#endif

#ifndef QT_NO_CLIPBOARD
void tst_qquicktextinput::canPaste()
{
    QGuiApplication::clipboard()->setText("Some text");

    QString componentStr = "import QtQuick 2.0\nTextInput { text: \"Hello world!\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    bool cp = !textInput->isReadOnly() && QGuiApplication::clipboard()->text().length() != 0;
    QCOMPARE(textInput->canPaste(), cp);
}
#endif

#ifndef QT_NO_CLIPBOARD
void tst_qquicktextinput::middleClickPaste()
{
    if (!PlatformQuirks::isClipboardAvailable())
        QSKIP("This machine doesn't support the clipboard");

    QQuickView window(testFileUrl("mouseselection_true.qml"));

    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);
    QQuickTextInput *textInputObject = qobject_cast<QQuickTextInput *>(window.rootObject());
    QVERIFY(textInputObject != 0);

    textInputObject->setFocus(true);

    QString originalText = textInputObject->text();
    QString selectedText = "234567";

    // press-and-drag-and-release from x1 to x2
    const QPoint p1 = textInputObject->positionToRectangle(2).center().toPoint();
    const QPoint p2 = textInputObject->positionToRectangle(8).center().toPoint();
    const QPoint p3 = textInputObject->positionToRectangle(1).center().toPoint();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, p1);
    QTest::mouseMove(&window, p2);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, p2);
    QTRY_COMPARE(textInputObject->selectedText(), selectedText);

    // Middle click pastes the selected text, assuming the platform supports it.
    QTest::mouseClick(&window, Qt::MiddleButton, Qt::NoModifier, p3);

    // ### This is to prevent double click detection from carrying over to the next test.
    QTest::qWait(QGuiApplication::styleHints()->mouseDoubleClickInterval() + 10);

    if (QGuiApplication::clipboard()->supportsSelection())
        QCOMPARE(textInputObject->text().mid(1, selectedText.length()), selectedText);
    else
        QCOMPARE(textInputObject->text(), originalText);
}
#endif

void tst_qquicktextinput::passwordCharacter()
{
    QString componentStr = "import QtQuick 2.0\nTextInput { text: \"Hello world!\"; font.family: \"Helvetica\"; echoMode: TextInput.Password }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    textInput->setPasswordCharacter("X");
    qreal implicitWidth = textInput->implicitWidth();
    textInput->setPasswordCharacter(".");

    // QTBUG-12383 content is updated and redrawn
    QVERIFY(textInput->implicitWidth() < implicitWidth);

    delete textInput;
}

void tst_qquicktextinput::cursorDelegate_data()
{
    QTest::addColumn<QUrl>("source");
    QTest::newRow("out of line") << testFileUrl("cursorTest.qml");
    QTest::newRow("in line") << testFileUrl("cursorTestInline.qml");
    QTest::newRow("external") << testFileUrl("cursorTestExternal.qml");
}

void tst_qquicktextinput::cursorDelegate()
{
    QFETCH(QUrl, source);
    QQuickView view(source);
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QQuickTextInput *textInputObject = view.rootObject()->findChild<QQuickTextInput*>("textInputObject");
    QVERIFY(textInputObject != 0);
    // Delegate is created on demand, and so won't be available immediately.  Focus in or
    // setCursorVisible(true) will trigger creation.
    QTRY_VERIFY(!textInputObject->findChild<QQuickItem*>("cursorInstance"));
    QVERIFY(!textInputObject->isCursorVisible());
    //Test Delegate gets created
    textInputObject->setFocus(true);
    QVERIFY(textInputObject->isCursorVisible());
    QQuickItem* delegateObject = textInputObject->findChild<QQuickItem*>("cursorInstance");
    QVERIFY(delegateObject);
    QCOMPARE(delegateObject->property("localProperty").toString(), QString("Hello"));
    //Test Delegate gets moved
    for (int i=0; i<= textInputObject->text().length(); i++) {
        textInputObject->setCursorPosition(i);
        QCOMPARE(textInputObject->cursorRectangle().x(), delegateObject->x());
        QCOMPARE(textInputObject->cursorRectangle().y(), delegateObject->y());
    }
    textInputObject->setCursorPosition(0);
    QCOMPARE(textInputObject->cursorRectangle().x(), delegateObject->x());
    QCOMPARE(textInputObject->cursorRectangle().y(), delegateObject->y());

    // Test delegate gets moved on mouse press.
    textInputObject->setSelectByMouse(true);
    textInputObject->setCursorPosition(0);
    const QPoint point1 = textInputObject->positionToRectangle(5).center().toPoint();
    QTest::qWait(400);  //ensure this isn't treated as a double-click
    QTest::mouseClick(&view, Qt::LeftButton, 0, point1);
    QTest::qWait(50);
    QTRY_VERIFY(textInputObject->cursorPosition() != 0);
    QCOMPARE(textInputObject->cursorRectangle().x(), delegateObject->x());
    QCOMPARE(textInputObject->cursorRectangle().y(), delegateObject->y());

    // Test delegate gets moved on mouse drag
    textInputObject->setCursorPosition(0);
    const QPoint point2 = textInputObject->positionToRectangle(10).center().toPoint();
    QTest::qWait(400);  //ensure this isn't treated as a double-click
    QTest::mousePress(&view, Qt::LeftButton, 0, point1);
    QMouseEvent mv(QEvent::MouseMove, point2, Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
    QGuiApplication::sendEvent(&view, &mv);
    QTest::mouseRelease(&view, Qt::LeftButton, 0, point2);
    QTest::qWait(50);
    QTRY_COMPARE(textInputObject->cursorRectangle().x(), delegateObject->x());
    QCOMPARE(textInputObject->cursorRectangle().y(), delegateObject->y());

    textInputObject->setReadOnly(true);
    textInputObject->setCursorPosition(0);
    QTest::qWait(400);  //ensure this isn't treated as a double-click
    QTest::mouseClick(&view, Qt::LeftButton, 0, textInputObject->positionToRectangle(5).center().toPoint());
    QTest::qWait(50);
    QTRY_VERIFY(textInputObject->cursorPosition() != 0);
    QCOMPARE(textInputObject->cursorRectangle().x(), delegateObject->x());
    QCOMPARE(textInputObject->cursorRectangle().y(), delegateObject->y());

    textInputObject->setCursorPosition(0);
    QTest::qWait(400);  //ensure this isn't treated as a double-click
    QTest::mouseClick(&view, Qt::LeftButton, 0, textInputObject->positionToRectangle(5).center().toPoint());
    QTest::qWait(50);
    QTRY_VERIFY(textInputObject->cursorPosition() != 0);
    QCOMPARE(textInputObject->cursorRectangle().x(), delegateObject->x());
    QCOMPARE(textInputObject->cursorRectangle().y(), delegateObject->y());

    textInputObject->setCursorPosition(0);
    QCOMPARE(textInputObject->cursorRectangle().x(), delegateObject->x());
    QCOMPARE(textInputObject->cursorRectangle().y(), delegateObject->y());

    textInputObject->setReadOnly(false);

    // Delegate moved when text is entered
    textInputObject->setText(QString());
    for (int i = 0; i < 20; ++i) {
        QTest::keyClick(&view, Qt::Key_A);
        QCOMPARE(textInputObject->cursorRectangle().x(), delegateObject->x());
        QCOMPARE(textInputObject->cursorRectangle().y(), delegateObject->y());
    }

    // Delegate moved when text is entered by im.
    textInputObject->setText(QString());
    for (int i = 0; i < 20; ++i) {
        QInputMethodEvent event;
        event.setCommitString("w");
        QGuiApplication::sendEvent(textInputObject, &event);
        QCOMPARE(textInputObject->cursorRectangle().x(), delegateObject->x());
        QCOMPARE(textInputObject->cursorRectangle().y(), delegateObject->y());
    }
    // Delegate moved when text is removed by im.
    for (int i = 19; i > 1; --i) {
        QInputMethodEvent event;
        event.setCommitString(QString(), -1, 1);
        QGuiApplication::sendEvent(textInputObject, &event);
        QCOMPARE(textInputObject->cursorRectangle().x(), delegateObject->x());
        QCOMPARE(textInputObject->cursorRectangle().y(), delegateObject->y());
    }
    {   // Delegate moved the text is changed in place by im.
        QInputMethodEvent event;
        event.setCommitString("i", -1, 1);
        QGuiApplication::sendEvent(textInputObject, &event);
        QCOMPARE(textInputObject->cursorRectangle().x(), delegateObject->x());
        QCOMPARE(textInputObject->cursorRectangle().y(), delegateObject->y());
    }

    //Test Delegate gets deleted
    textInputObject->setCursorDelegate(0);
    QVERIFY(!textInputObject->findChild<QQuickItem*>("cursorInstance"));
}

void tst_qquicktextinput::remoteCursorDelegate()
{
    ThreadedTestHTTPServer server(dataDirectory(), TestHTTPServer::Delay);
    QQuickView view;

    QQmlComponent component(view.engine(), server.url("/RemoteCursor.qml"));

    view.rootContext()->setContextProperty("contextDelegate", &component);
    view.setSource(testFileUrl("cursorTestRemote.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QQuickTextInput *textInputObject = view.rootObject()->findChild<QQuickTextInput*>("textInputObject");
    QVERIFY(textInputObject != 0);

    // Delegate is created on demand, and so won't be available immediately.  Focus in or
    // setCursorVisible(true) will trigger creation.
    QTRY_VERIFY(!textInputObject->findChild<QQuickItem*>("cursorInstance"));
    QVERIFY(!textInputObject->isCursorVisible());

    textInputObject->setFocus(true);
    QVERIFY(textInputObject->isCursorVisible());

    // Wait for component to load.
    QTRY_COMPARE(component.status(), QQmlComponent::Ready);
    QVERIFY(textInputObject->findChild<QQuickItem*>("cursorInstance"));
}

void tst_qquicktextinput::cursorVisible()
{
    QSKIP("This test is unstable");
    QQuickTextInput input;
    input.componentComplete();
    QSignalSpy spy(&input, SIGNAL(cursorVisibleChanged(bool)));

    QQuickView view(testFileUrl("cursorVisible.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);

    QCOMPARE(input.isCursorVisible(), false);

    input.setCursorVisible(true);
    QCOMPARE(input.isCursorVisible(), true);
    QCOMPARE(spy.count(), 1);

    input.setCursorVisible(false);
    QCOMPARE(input.isCursorVisible(), false);
    QCOMPARE(spy.count(), 2);

    input.setFocus(true);
    QCOMPARE(input.isCursorVisible(), false);
    QCOMPARE(spy.count(), 2);

    input.setParentItem(view.rootObject());
    QCOMPARE(input.isCursorVisible(), true);
    QCOMPARE(spy.count(), 3);

    input.setFocus(false);
    QCOMPARE(input.isCursorVisible(), false);
    QCOMPARE(spy.count(), 4);

    input.setFocus(true);
    QCOMPARE(input.isCursorVisible(), true);
    QCOMPARE(spy.count(), 5);

    QQuickView alternateView;
    alternateView.show();
    alternateView.requestActivate();
    QTest::qWaitForWindowActive(&alternateView);

    QCOMPARE(input.isCursorVisible(), false);
    QCOMPARE(spy.count(), 6);

    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QCOMPARE(input.isCursorVisible(), true);
    QCOMPARE(spy.count(), 7);

    {   // Cursor attribute with 0 length hides cursor.
        QInputMethodEvent ev(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 0, QVariant()));
        QCoreApplication::sendEvent(&input, &ev);
    }
    QCOMPARE(input.isCursorVisible(), false);
    QCOMPARE(spy.count(), 8);

    {   // Cursor attribute with non zero length shows cursor.
        QInputMethodEvent ev(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 1, QVariant()));
        QCoreApplication::sendEvent(&input, &ev);
    }
    QCOMPARE(input.isCursorVisible(), true);
    QCOMPARE(spy.count(), 9);

    {   // If the cursor is hidden by the input method and the text is changed it should be visible again.
        QInputMethodEvent ev(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 0, QVariant()));
        QCoreApplication::sendEvent(&input, &ev);
    }
    QCOMPARE(input.isCursorVisible(), false);
    QCOMPARE(spy.count(), 10);

    input.setText("something");
    QCOMPARE(input.isCursorVisible(), true);
    QCOMPARE(spy.count(), 11);

    {   // If the cursor is hidden by the input method and the cursor position is changed it should be visible again.
        QInputMethodEvent ev(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 0, QVariant()));
        QCoreApplication::sendEvent(&input, &ev);
    }
    QCOMPARE(input.isCursorVisible(), false);
    QCOMPARE(spy.count(), 12);

    input.setCursorPosition(5);
    QCOMPARE(input.isCursorVisible(), true);
    QCOMPARE(spy.count(), 13);
}

void tst_qquicktextinput::cursorRectangle_data()
{
    const quint16 arabic_str[] = { 0x0638, 0x0643, 0x00646, 0x0647, 0x0633, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0638, 0x0643, 0x00646, 0x0647, 0x0633, 0x0647};

    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("positionAtWidth");
    QTest::addColumn<int>("wrapPosition");
    QTest::addColumn<QString>("shortText");
    QTest::addColumn<bool>("leftToRight");

    QTest::newRow("left to right")
            << "Hello      World!" << 5 << 11
            << "Hi"
            << true;
    QTest::newRow("right to left")
            << QString::fromUtf16(arabic_str, lengthOf(arabic_str)) << 5 << 11
            << QString::fromUtf16(arabic_str, 3)
            << false;
}

#ifndef QT_NO_IM
#define COMPARE_INPUT_METHOD_QUERY(type, input, property, method, result) \
    QCOMPARE((type) input->inputMethodQuery(property).method(), result);
#else
#define COMPARE_INPUT_METHOD_QUERY(type, input, property, method, result) \
    qt_noop()
#endif

void tst_qquicktextinput::cursorRectangle()
{
    QFETCH(QString, text);
    QFETCH(int, positionAtWidth);
    QFETCH(int, wrapPosition);
    QFETCH(QString, shortText);
    QFETCH(bool, leftToRight);

    QQuickTextInput input;
    input.setText(text);
    input.componentComplete();

    QTextLayout layout(text);
    layout.setFont(input.font());
    if (!qmlDisableDistanceField()) {
        QTextOption option;
        option.setUseDesignMetrics(true);
        layout.setTextOption(option);
    }
    layout.beginLayout();
    QTextLine line = layout.createLine();
    layout.endLayout();

    qreal offset = 0;
    if (leftToRight) {
        input.setWidth(line.cursorToX(positionAtWidth, QTextLine::Leading));
    } else {
        input.setWidth(line.horizontalAdvance() - line.cursorToX(positionAtWidth, QTextLine::Leading));
        offset = line.horizontalAdvance() - input.width();
    }
    input.setHeight(qCeil(line.height() * 3 / 2));

    QRectF r;

    for (int i = 0; i <= positionAtWidth; ++i) {
        input.setCursorPosition(i);
        r = input.cursorRectangle();

        QCOMPARE(r.left(), line.cursorToX(i, QTextLine::Leading) - offset);
        COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
        QCOMPARE(input.positionToRectangle(i), r);
    }

    // Check the cursor rectangle remains within the input bounding rect when auto scrolling.
    QCOMPARE(r.left(), leftToRight ? input.width() : 0);

    for (int i = positionAtWidth + 1; i < text.length(); ++i) {
        input.setCursorPosition(i);
        QCOMPARE(r, input.cursorRectangle());
        COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
        QCOMPARE(input.positionToRectangle(i), r);
    }

    for (int i = text.length() - 2; i >= 0; --i) {
        input.setCursorPosition(i);
        r = input.cursorRectangle();
        QCOMPARE(r.top(), 0.);
        if (leftToRight) {
            QVERIFY(r.right() >= 0);
        } else {
            QVERIFY(r.left() <= input.width());
        }
        COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
        QCOMPARE(input.positionToRectangle(i), r);
    }

    // Check position with word wrap.
    input.setWrapMode(QQuickTextInput::WordWrap);
    input.setAutoScroll(false);
    for (int i = 0; i < wrapPosition; ++i) {
        input.setCursorPosition(i);
        r = input.cursorRectangle();

        QCOMPARE(r.left(), line.cursorToX(i, QTextLine::Leading) - offset);
        QCOMPARE(r.top(), 0.);
        COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
        QCOMPARE(input.positionToRectangle(i), r);
    }

    input.setCursorPosition(wrapPosition);
    r = input.cursorRectangle();
    if (leftToRight) {
        QCOMPARE(r.left(), 0.);
    } else {
        QCOMPARE(r.left(), input.width());
    }
    // we can't be exact here, as the space character can have a different ascent/descent from the arabic chars
    // this then leads to different line heights between the wrapped and non wrapped texts
    QVERIFY(r.top() >= line.height() - 5);
    COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
    QCOMPARE(input.positionToRectangle(11), r);

    for (int i = wrapPosition + 1; i < text.length(); ++i) {
        input.setCursorPosition(i);
        r = input.cursorRectangle();
        QVERIFY(r.top() >= line.height() - 5);
        COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
        QCOMPARE(input.positionToRectangle(i), r);
    }

    // Check vertical scrolling with word wrap.
    input.setAutoScroll(true);
    for (int i = 0; i <= positionAtWidth; ++i) {
        input.setCursorPosition(i);
        r = input.cursorRectangle();

        QCOMPARE(r.left(), line.cursorToX(i, QTextLine::Leading) - offset);
        QCOMPARE(r.top(), 0.);
        COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
        QCOMPARE(input.positionToRectangle(i), r);
    }

    // Whitespace doesn't wrap, so scroll horizontally until the until the cursor
    // reaches the next non-whitespace character.
    QCOMPARE(r.left(), leftToRight ? input.width() : 0);
    for (int i = positionAtWidth + 1; i < wrapPosition && leftToRight; ++i) {
        input.setCursorPosition(i);
        QCOMPARE(r, input.cursorRectangle());
        COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
        QCOMPARE(input.positionToRectangle(i), r);
    }

    input.setCursorPosition(wrapPosition);
    r = input.cursorRectangle();
    if (leftToRight) {
        QCOMPARE(r.left(), 0.);
    } else {
        QCOMPARE(r.left(), input.width());
    }
    QVERIFY(r.bottom() >= input.height());
    COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
    QCOMPARE(input.positionToRectangle(11), r);

    for (int i = wrapPosition + 1; i < text.length(); ++i) {
        input.setCursorPosition(i);
        r = input.cursorRectangle();
        QVERIFY(r.bottom() >= input.height());
        COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
        QCOMPARE(input.positionToRectangle(i), r);
    }

    for (int i = text.length() - 2; i >= wrapPosition; --i) {
        input.setCursorPosition(i);
        r = input.cursorRectangle();
        QVERIFY(r.bottom() >= input.height());
        COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
        QCOMPARE(input.positionToRectangle(i), r);
    }

    input.setCursorPosition(wrapPosition - 1);
    r = input.cursorRectangle();
    QCOMPARE(r.top(), 0.);
    QCOMPARE(r.left(), leftToRight ? input.width() : 0);
    COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
    QCOMPARE(input.positionToRectangle(10), r);

    for (int i = wrapPosition - 2; i >= positionAtWidth + 1; --i) {
        input.setCursorPosition(i);
        QCOMPARE(r, input.cursorRectangle());
        COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
        QCOMPARE(input.positionToRectangle(i), r);
    }

    for (int i = positionAtWidth; i >= 0; --i) {
        input.setCursorPosition(i);
        r = input.cursorRectangle();
        QCOMPARE(r.top(), 0.);
        COMPARE_INPUT_METHOD_QUERY(QRectF, (&input), Qt::ImCursorRectangle, toRectF, r);
        QCOMPARE(input.positionToRectangle(i), r);
    }

    input.setText(shortText);
    input.setHAlign(leftToRight ? QQuickTextInput::AlignRight : QQuickTextInput::AlignLeft);
    r = input.cursorRectangle();
    QCOMPARE(r.left(), leftToRight ? input.width() : 0);

    QSignalSpy cursorRectangleSpy(&input, SIGNAL(cursorRectangleChanged()));

    QString widerText = shortText;
    widerText[1] = 'W'; // Assumes shortText is at least two characters long.
    input.setText(widerText);

    QCOMPARE(cursorRectangleSpy.count(), 1);
}

void tst_qquicktextinput::readOnly()
{
    QQuickView window(testFileUrl("readOnly.qml"));
    window.show();
    window.requestActivate();

    QVERIFY(window.rootObject() != 0);

    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("myInput")));

    QVERIFY(input != 0);
    QTRY_VERIFY(input->hasActiveFocus());
    QVERIFY(input->isReadOnly());
    QVERIFY(!input->isCursorVisible());
    QString initial = input->text();
    for (int k=Qt::Key_0; k<=Qt::Key_Z; k++)
        simulateKey(&window, k);
    simulateKey(&window, Qt::Key_Return);
    simulateKey(&window, Qt::Key_Space);
    simulateKey(&window, Qt::Key_Escape);
    QCOMPARE(input->text(), initial);

    input->setCursorPosition(3);
    input->setReadOnly(false);
    QCOMPARE(input->isReadOnly(), false);
    QCOMPARE(input->cursorPosition(), input->text().length());
    QVERIFY(input->isCursorVisible());
}

void tst_qquicktextinput::echoMode()
{
    QQuickView window(testFileUrl("echoMode.qml"));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);

    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("myInput")));

    QVERIFY(input != 0);
    QTRY_VERIFY(input->hasActiveFocus());
    QString initial = input->text();
    Qt::InputMethodHints ref;
    QCOMPARE(initial, QLatin1String("ABCDefgh"));
    QCOMPARE(input->echoMode(), QQuickTextInput::Normal);
    QCOMPARE(input->displayText(), input->text());
    const QString passwordMaskCharacter = qApp->styleHints()->passwordMaskCharacter();
    //Normal
    ref &= ~Qt::ImhHiddenText;
    ref &= ~(Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText | Qt::ImhSensitiveData);
    COMPARE_INPUT_METHOD_QUERY(Qt::InputMethodHints, input, Qt::ImHints, toInt, ref);
    input->setEchoMode(QQuickTextInput::NoEcho);
    QCOMPARE(input->text(), initial);
    QCOMPARE(input->displayText(), QLatin1String(""));
    QCOMPARE(input->passwordCharacter(), passwordMaskCharacter);
    //NoEcho
    ref |= Qt::ImhHiddenText;
    ref |= (Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText | Qt::ImhSensitiveData);
    COMPARE_INPUT_METHOD_QUERY(Qt::InputMethodHints, input, Qt::ImHints, toInt, ref);
    input->setEchoMode(QQuickTextInput::Password);
    //Password
    ref |= Qt::ImhHiddenText;
    ref |= (Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText | Qt::ImhSensitiveData);
    QCOMPARE(input->text(), initial);
    QCOMPARE(input->displayText(), QString(8, passwordMaskCharacter.at(0)));
    COMPARE_INPUT_METHOD_QUERY(Qt::InputMethodHints, input, Qt::ImHints, toInt, ref);
    // clearing input hints do not clear bits set by echo mode
    input->setInputMethodHints(Qt::ImhNone);
    COMPARE_INPUT_METHOD_QUERY(Qt::InputMethodHints, input, Qt::ImHints, toInt, ref);
    input->setPasswordCharacter(QChar('Q'));
    QCOMPARE(input->passwordCharacter(), QLatin1String("Q"));
    QCOMPARE(input->text(), initial);
    QCOMPARE(input->displayText(), QLatin1String("QQQQQQQQ"));
    input->setEchoMode(QQuickTextInput::PasswordEchoOnEdit);
    //PasswordEchoOnEdit
    ref &= ~Qt::ImhHiddenText;
    ref |= (Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText | Qt::ImhSensitiveData);
    COMPARE_INPUT_METHOD_QUERY(Qt::InputMethodHints, input, Qt::ImHints, toInt, ref);
    QCOMPARE(input->text(), initial);
    QCOMPARE(input->displayText(), QLatin1String("QQQQQQQQ"));
    COMPARE_INPUT_METHOD_QUERY(QString, input, Qt::ImSurroundingText, toString,
                               QLatin1String("QQQQQQQQ"));
    QTest::keyPress(&window, Qt::Key_A);//Clearing previous entry is part of PasswordEchoOnEdit
    QTest::keyRelease(&window, Qt::Key_A, Qt::NoModifier ,10);
    QCOMPARE(input->text(), QLatin1String("a"));
    QCOMPARE(input->displayText(), QLatin1String("a"));
    COMPARE_INPUT_METHOD_QUERY(QString, input, Qt::ImSurroundingText, toString, QLatin1String("a"));
    input->setFocus(false);
    QVERIFY(!input->hasActiveFocus());
    QCOMPARE(input->displayText(), QLatin1String("Q"));
    COMPARE_INPUT_METHOD_QUERY(QString, input, Qt::ImSurroundingText, toString, QLatin1String("Q"));
    input->setFocus(true);
    QVERIFY(input->hasActiveFocus());
    QInputMethodEvent inputEvent;
    inputEvent.setCommitString(initial);
    QGuiApplication::sendEvent(input, &inputEvent);
    QCOMPARE(input->text(), initial);
    QCOMPARE(input->displayText(), initial);
    COMPARE_INPUT_METHOD_QUERY(QString, input, Qt::ImSurroundingText, toString, initial);
}

void tst_qquicktextinput::passwordEchoDelay()
{
    int maskDelay = qGuiApp->styleHints()->passwordMaskDelay();
    if (maskDelay <= 0)
        QSKIP("No mask delay in use");
    QQuickView window(testFileUrl("echoMode.qml"));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(window.rootObject() != 0);

    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(qvariant_cast<QObject *>(window.rootObject()->property("myInput")));
    QVERIFY(input);
    QVERIFY(input->hasActiveFocus());

    QQuickItem *cursor = input->findChild<QQuickItem *>("cursor");
    QVERIFY(cursor);

    QChar fillChar = qApp->styleHints()->passwordMaskCharacter();

    input->setEchoMode(QQuickTextInput::Password);
    QCOMPARE(input->displayText(), QString(8, fillChar));
    input->setText(QString());
    QCOMPARE(input->displayText(), QString());

    QTest::keyPress(&window, '0');
    QTest::keyPress(&window, '1');
    QTest::keyPress(&window, '2');
    QCOMPARE(input->displayText(), QString(2, fillChar) + QLatin1Char('2'));
    QTest::keyPress(&window, '3');
    QTest::keyPress(&window, '4');
    QCOMPARE(input->displayText(), QString(4, fillChar) + QLatin1Char('4'));
    QTest::keyPress(&window, Qt::Key_Backspace);
    QCOMPARE(input->displayText(), QString(4, fillChar));
    QTest::keyPress(&window, '4');
    QCOMPARE(input->displayText(), QString(4, fillChar) + QLatin1Char('4'));
    QCOMPARE(input->cursorRectangle().topLeft(), cursor->position());

    // Verify the last character entered is replaced by the fill character after a delay.
    // Also check the cursor position is updated to accomdate a size difference between
    // the fill character and the replaced character.
    QSignalSpy cursorSpy(input, SIGNAL(cursorRectangleChanged()));
    QTest::qWait(maskDelay);
    QTRY_COMPARE(input->displayText(), QString(5, fillChar));
    QCOMPARE(cursorSpy.count(), 1);
    QCOMPARE(input->cursorRectangle().topLeft(), cursor->position());

    QTest::keyPress(&window, '5');
    QCOMPARE(input->displayText(), QString(5, fillChar) + QLatin1Char('5'));
    input->setFocus(false);
    QVERIFY(!input->hasFocus());
    QCOMPARE(input->displayText(), QString(6, fillChar));
    input->setFocus(true);
    QTRY_VERIFY(input->hasFocus());
    QCOMPARE(input->displayText(), QString(6, fillChar));
    QTest::keyPress(&window, '6');
    QCOMPARE(input->displayText(), QString(6, fillChar) + QLatin1Char('6'));

    QInputMethodEvent ev;
    ev.setCommitString(QLatin1String("7"));
    QGuiApplication::sendEvent(input, &ev);
    QCOMPARE(input->displayText(), QString(7, fillChar) + QLatin1Char('7'));

    input->setCursorPosition(3);
    QCOMPARE(input->displayText(), QString(7, fillChar) + QLatin1Char('7'));
    QTest::keyPress(&window, 'a');
    QCOMPARE(input->displayText(), QString(3, fillChar) + QLatin1Char('a') + QString(5, fillChar));
    QTest::keyPress(&window, Qt::Key_Backspace);
    QCOMPARE(input->displayText(), QString(8, fillChar));
}


void tst_qquicktextinput::simulateKey(QWindow *view, int key)
{
    QKeyEvent press(QKeyEvent::KeyPress, key, 0);
    QKeyEvent release(QKeyEvent::KeyRelease, key, 0);

    QGuiApplication::sendEvent(view, &press);
    QGuiApplication::sendEvent(view, &release);
}


void tst_qquicktextinput::focusOnPress()
{
    QString componentStr =
            "import QtQuick 2.0\n"
            "TextInput {\n"
                "property bool selectOnFocus: false\n"
                "width: 100; height: 50\n"
                "activeFocusOnPress: true\n"
                "text: \"Hello World\"\n"
                "onFocusChanged: { if (focus && selectOnFocus) selectAll() }"
            " }";
    QQmlComponent texteditComponent(&engine);
    texteditComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInputObject = qobject_cast<QQuickTextInput*>(texteditComponent.create());
    QVERIFY(textInputObject != 0);
    QCOMPARE(textInputObject->focusOnPress(), true);
    QCOMPARE(textInputObject->hasFocus(), false);

    QSignalSpy focusSpy(textInputObject, SIGNAL(focusChanged(bool)));
    QSignalSpy activeFocusSpy(textInputObject, SIGNAL(focusChanged(bool)));
    QSignalSpy activeFocusOnPressSpy(textInputObject, SIGNAL(activeFocusOnPressChanged(bool)));

    textInputObject->setFocusOnPress(true);
    QCOMPARE(textInputObject->focusOnPress(), true);
    QCOMPARE(activeFocusOnPressSpy.count(), 0);

    QQuickWindow window;
    window.resize(100, 50);
    textInputObject->setParentItem(window.contentItem());
    window.showNormal();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QCOMPARE(textInputObject->hasFocus(), false);
    QCOMPARE(textInputObject->hasActiveFocus(), false);

    Qt::KeyboardModifiers noModifiers = 0;
    QTest::mousePress(&window, Qt::LeftButton, noModifiers);
    QGuiApplication::processEvents();
    QCOMPARE(textInputObject->hasFocus(), true);
    QCOMPARE(textInputObject->hasActiveFocus(), true);
    QCOMPARE(focusSpy.count(), 1);
    QCOMPARE(activeFocusSpy.count(), 1);
    QCOMPARE(textInputObject->selectedText(), QString());
    QTest::mouseRelease(&window, Qt::LeftButton, noModifiers);

    textInputObject->setFocusOnPress(false);
    QCOMPARE(textInputObject->focusOnPress(), false);
    QCOMPARE(activeFocusOnPressSpy.count(), 1);

    textInputObject->setFocus(false);
    QCOMPARE(textInputObject->hasFocus(), false);
    QCOMPARE(textInputObject->hasActiveFocus(), false);
    QCOMPARE(focusSpy.count(), 2);
    QCOMPARE(activeFocusSpy.count(), 2);

    // Wait for double click timeout to expire before clicking again.
    QTest::qWait(400);
    QTest::mousePress(&window, Qt::LeftButton, noModifiers);
    QGuiApplication::processEvents();
    QCOMPARE(textInputObject->hasFocus(), false);
    QCOMPARE(textInputObject->hasActiveFocus(), false);
    QCOMPARE(focusSpy.count(), 2);
    QCOMPARE(activeFocusSpy.count(), 2);
    QTest::mouseRelease(&window, Qt::LeftButton, noModifiers);

    textInputObject->setFocusOnPress(true);
    QCOMPARE(textInputObject->focusOnPress(), true);
    QCOMPARE(activeFocusOnPressSpy.count(), 2);

    // Test a selection made in the on(Active)FocusChanged handler isn't overwritten.
    textInputObject->setProperty("selectOnFocus", true);

    QTest::qWait(400);
    QTest::mousePress(&window, Qt::LeftButton, noModifiers);
    QGuiApplication::processEvents();
    QCOMPARE(textInputObject->hasFocus(), true);
    QCOMPARE(textInputObject->hasActiveFocus(), true);
    QCOMPARE(focusSpy.count(), 3);
    QCOMPARE(activeFocusSpy.count(), 3);
    QCOMPARE(textInputObject->selectedText(), textInputObject->text());
    QTest::mouseRelease(&window, Qt::LeftButton, noModifiers);
}

void tst_qquicktextinput::focusOnPressOnlyOneItem()
{
    QQuickView window(testFileUrl("focusOnlyOneOnPress.qml"));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QQuickTextInput *first = window.rootObject()->findChild<QQuickTextInput*>("first");
    QQuickTextInput *second = window.rootObject()->findChild<QQuickTextInput*>("second");
    QQuickTextInput *third = window.rootObject()->findChild<QQuickTextInput*>("third");

    // second is focused onComplete
    QVERIFY(second->hasActiveFocus());

    // and first will try focus when we press it
    QVERIFY(first->focusOnPress());

    // write some text to start editing
    QTest::keyClick(&window, Qt::Key_A);

    // click the first input. naturally, we are giving focus on press, but
    // second's editingFinished also attempts to assign focus. lastly, focus
    // should bounce back to second from first's editingFinished signal.
    //
    // this is a contrived example to be sure, but at the end of this, the
    // important thing is that only one thing should have activeFocus.
    Qt::KeyboardModifiers noModifiers = 0;
    QTest::mousePress(&window, Qt::LeftButton, noModifiers, QPoint(10, 10));

    // make sure the press is processed.
    QGuiApplication::processEvents();

    QVERIFY(second->hasActiveFocus()); // make sure it's still there
    QVERIFY(!third->hasActiveFocus()); // make sure it didn't end up anywhere else
    QVERIFY(!first->hasActiveFocus()); // make sure it didn't end up anywhere else

    // reset state
    QTest::mouseRelease(&window, Qt::LeftButton, noModifiers, QPoint(10, 10));
}

void tst_qquicktextinput::openInputPanel()
{
    PlatformInputContext platformInputContext;
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = &platformInputContext;

    QQuickView view(testFileUrl("openInputPanel.qml"));
    view.showNormal();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);

    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(view.rootObject());
    QVERIFY(input);

    // check default values
    QVERIFY(input->focusOnPress());
    QVERIFY(!input->hasActiveFocus());
    QVERIFY(qApp->focusObject() != input);
    QCOMPARE(qApp->inputMethod()->isVisible(), false);

    // input panel should open on focus
    Qt::KeyboardModifiers noModifiers = 0;
    QTest::mousePress(&view, Qt::LeftButton, noModifiers);
    QGuiApplication::processEvents();
    QVERIFY(input->hasActiveFocus());
    QCOMPARE(qApp->focusObject(), input);
    QCOMPARE(qApp->inputMethod()->isVisible(), true);
    QTest::mouseRelease(&view, Qt::LeftButton, noModifiers);

    // input panel should be re-opened when pressing already focused TextInput
    qApp->inputMethod()->hide();
    QCOMPARE(qApp->inputMethod()->isVisible(), false);
    QVERIFY(input->hasActiveFocus());
    QTest::mousePress(&view, Qt::LeftButton, noModifiers);
    QGuiApplication::processEvents();
    QCOMPARE(qApp->inputMethod()->isVisible(), true);
    QTest::mouseRelease(&view, Qt::LeftButton, noModifiers);

    // input panel should stay visible if focus is lost to another text inputor
    QSignalSpy inputPanelVisibilitySpy(qApp->inputMethod(), SIGNAL(visibleChanged()));
    QQuickTextInput anotherInput;
    anotherInput.componentComplete();
    anotherInput.setParentItem(view.rootObject());
    anotherInput.setFocus(true);
    QCOMPARE(qApp->inputMethod()->isVisible(), true);
    QCOMPARE(qApp->focusObject(), qobject_cast<QObject*>(&anotherInput));
    QCOMPARE(inputPanelVisibilitySpy.count(), 0);

    anotherInput.setFocus(false);
    QVERIFY(qApp->focusObject() != &anotherInput);
    QCOMPARE(view.activeFocusItem(), view.contentItem());
    anotherInput.setFocus(true);

    qApp->inputMethod()->hide();

    // input panel should not be opened if TextInput is read only
    input->setReadOnly(true);
    input->setFocus(true);
    QCOMPARE(qApp->inputMethod()->isVisible(), false);
    QTest::mousePress(&view, Qt::LeftButton, noModifiers);
    QTest::mouseRelease(&view, Qt::LeftButton, noModifiers);
    QGuiApplication::processEvents();
    QCOMPARE(qApp->inputMethod()->isVisible(), false);

    // input panel should not be opened if focusOnPress is set to false
    input->setFocusOnPress(false);
    input->setFocus(false);
    input->setFocus(true);
    QCOMPARE(qApp->inputMethod()->isVisible(), false);
    QTest::mousePress(&view, Qt::LeftButton, noModifiers);
    QTest::mouseRelease(&view, Qt::LeftButton, noModifiers);
    QCOMPARE(qApp->inputMethod()->isVisible(), false);
}

class MyTextInput : public QQuickTextInput
{
public:
    MyTextInput(QQuickItem *parent = 0) : QQuickTextInput(parent)
    {
        nbPaint = 0;
    }
    virtual QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data)
    {
       nbPaint++;
       return QQuickTextInput::updatePaintNode(node, data);
    }
    int nbPaint;
};

void tst_qquicktextinput::setHAlignClearCache()
{
    QQuickView view;
    view.resize(200, 200);
    MyTextInput input;
    input.setText("Hello world");
    input.setParentItem(view.contentItem());
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QTRY_COMPARE(input.nbPaint, 1);
    input.setHAlign(QQuickTextInput::AlignRight);
    //Changing the alignment should trigger a repaint
    QTRY_COMPARE(input.nbPaint, 2);
}

void tst_qquicktextinput::focusOutClearSelection()
{
    QQuickView view;
    QQuickTextInput input;
    QQuickTextInput input2;
    input.setText(QLatin1String("Hello world"));
    input.setFocus(true);
    input2.setParentItem(view.contentItem());
    input.setParentItem(view.contentItem());
    input.componentComplete();
    input2.componentComplete();
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QVERIFY(input.hasActiveFocus());
    input.select(2,5);
    //The selection should work
    QTRY_COMPARE(input.selectedText(), QLatin1String("llo"));
    input2.setFocus(true);
    QGuiApplication::processEvents();
    //The input lost the focus selection should be cleared
    QTRY_COMPARE(input.selectedText(), QLatin1String(""));
}

void tst_qquicktextinput::focusOutNotClearSelection()
{
    QQuickView view;
    QQuickTextInput input;
    input.setText(QLatin1String("Hello world"));
    input.setFocus(true);
    input.setParentItem(view.contentItem());
    input.componentComplete();
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);

    QVERIFY(input.hasActiveFocus());
    input.select(2,5);
    QTRY_COMPARE(input.selectedText(), QLatin1String("llo"));

    // The selection should not be cleared when the focus
    // out event has one of the following reason:
    // Qt::ActiveWindowFocusReason
    // Qt::PopupFocusReason

    input.setFocus(false, Qt::ActiveWindowFocusReason);
    QGuiApplication::processEvents();
    QTRY_COMPARE(input.selectedText(), QLatin1String("llo"));
    QTRY_COMPARE(input.hasActiveFocus(), false);

    input.setFocus(true);
    QTRY_COMPARE(input.hasActiveFocus(), true);

    input.setFocus(false, Qt::PopupFocusReason);
    QGuiApplication::processEvents();
    QTRY_COMPARE(input.selectedText(), QLatin1String("llo"));
    // QTBUG-36332 and 36292: a popup window does not take focus
    QTRY_COMPARE(input.hasActiveFocus(), true);

    input.setFocus(true);
    QTRY_COMPARE(input.hasActiveFocus(), true);

    input.setFocus(false, Qt::OtherFocusReason);
    QGuiApplication::processEvents();
    QTRY_COMPARE(input.selectedText(), QLatin1String(""));
    QTRY_COMPARE(input.hasActiveFocus(), false);
}

void tst_qquicktextinput::geometrySignals()
{
    QQmlComponent component(&engine, testFileUrl("geometrySignals.qml"));
    QObject *o = component.create();
    QVERIFY(o);
    QCOMPARE(o->property("bindingWidth").toInt(), 400);
    QCOMPARE(o->property("bindingHeight").toInt(), 500);
    delete o;
}

void tst_qquicktextinput::contentSize()
{
    QString componentStr = "import QtQuick 2.0\nTextInput { width: 75; height: 16; font.pixelSize: 10 }";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QScopedPointer<QObject> object(textComponent.create());
    QQuickTextInput *textObject = qobject_cast<QQuickTextInput *>(object.data());

    QSignalSpy spy(textObject, SIGNAL(contentSizeChanged()));

    textObject->setText("The quick red fox jumped over the lazy brown dog");

    QVERIFY(textObject->contentWidth() > textObject->width());
    QVERIFY(textObject->contentHeight() < textObject->height());
    QCOMPARE(spy.count(), 1);

    textObject->setWrapMode(QQuickTextInput::WordWrap);
    QVERIFY(textObject->contentWidth() <= textObject->width());
    QVERIFY(textObject->contentHeight() > textObject->height());
    QCOMPARE(spy.count(), 2);

    textObject->setText("The quickredfoxjumpedoverthe lazy brown dog");

    QVERIFY(textObject->contentWidth() > textObject->width());
    QVERIFY(textObject->contentHeight() > textObject->height());
    QCOMPARE(spy.count(), 3);

    textObject->setText("The quick red fox jumped over the lazy brown dog");
    for (int w = 60; w < 120; ++w) {
        textObject->setWidth(w);
        QVERIFY(textObject->contentWidth() <= textObject->width());
        QVERIFY(textObject->contentHeight() > textObject->height());
    }
}

static void sendPreeditText(QQuickItem *item, const QString &text, int cursor)
{
    QInputMethodEvent event(text, QList<QInputMethodEvent::Attribute>()
            << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, cursor, text.length(), QVariant()));
    QCoreApplication::sendEvent(item, &event);
}

void tst_qquicktextinput::preeditAutoScroll()
{
    QString preeditText = "califragisiticexpialidocious!";

    QQuickView view(testFileUrl("preeditAutoScroll.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(view.rootObject());
    QVERIFY(input);
    QVERIFY(input->hasActiveFocus());

    input->setWidth(input->implicitWidth());

    QSignalSpy cursorRectangleSpy(input, SIGNAL(cursorRectangleChanged()));
    int cursorRectangleChanges = 0;

    // test the text is scrolled so the preedit is visible.
    sendPreeditText(input, preeditText.mid(0, 3), 1);
    QVERIFY(evaluate<int>(input, QString("positionAt(0)")) != 0);
    QVERIFY(input->cursorRectangle().left() < input->boundingRect().width());
    QCOMPARE(cursorRectangleSpy.count(), ++cursorRectangleChanges);

    // test the text is scrolled back when the preedit is removed.
    QInputMethodEvent imEvent;
    QCoreApplication::sendEvent(input, &imEvent);
    QCOMPARE(evaluate<int>(input, QString("positionAt(%1)").arg(0)), 0);
    QCOMPARE(evaluate<int>(input, QString("positionAt(%1)").arg(input->width())), 5);
    QCOMPARE(cursorRectangleSpy.count(), ++cursorRectangleChanges);

    QTextLayout layout(preeditText);
    layout.setFont(input->font());
    if (!qmlDisableDistanceField()) {
        QTextOption option;
        option.setUseDesignMetrics(true);
        layout.setTextOption(option);
    }
    layout.beginLayout();
    QTextLine line = layout.createLine();
    layout.endLayout();

    // test if the preedit is larger than the text input that the
    // character preceding the cursor is still visible.
    qreal x = input->positionToRectangle(0).x();
    for (int i = 0; i < 3; ++i) {
        sendPreeditText(input, preeditText, i + 1);
        int width = ceil(line.cursorToX(i, QTextLine::Trailing)) - floor(line.cursorToX(i));
        QVERIFY(input->cursorRectangle().right() >= width - 3);
        QVERIFY(input->positionToRectangle(0).x() < x);
        QCOMPARE(cursorRectangleSpy.count(), ++cursorRectangleChanges);
        x = input->positionToRectangle(0).x();
    }
    for (int i = 1; i >= 0; --i) {
        sendPreeditText(input, preeditText, i + 1);
        int width = ceil(line.cursorToX(i, QTextLine::Trailing)) - floor(line.cursorToX(i));
        QVERIFY(input->cursorRectangle().right() >= width - 3);
        QVERIFY(input->positionToRectangle(0).x() > x);
        QCOMPARE(cursorRectangleSpy.count(), ++cursorRectangleChanges);
        x = input->positionToRectangle(0).x();
    }

    // Test incrementing the preedit cursor doesn't cause further
    // scrolling when right most text is visible.
    sendPreeditText(input, preeditText, preeditText.length() - 3);
    QCOMPARE(cursorRectangleSpy.count(), ++cursorRectangleChanges);
    x = input->positionToRectangle(0).x();
    for (int i = 2; i >= 0; --i) {
        sendPreeditText(input, preeditText, preeditText.length() - i);
        QCOMPARE(input->positionToRectangle(0).x(), x);
        QCOMPARE(cursorRectangleSpy.count(), ++cursorRectangleChanges);
    }
    for (int i = 1; i <  3; ++i) {
        sendPreeditText(input, preeditText, preeditText.length() - i);
        QCOMPARE(input->positionToRectangle(0).x(), x);
        QCOMPARE(cursorRectangleSpy.count(), ++cursorRectangleChanges);
    }

    // Test disabling auto scroll.
    QCoreApplication::sendEvent(input, &imEvent);

    input->setAutoScroll(false);
    sendPreeditText(input, preeditText.mid(0, 3), 1);
    QCOMPARE(evaluate<int>(input, QString("positionAt(%1)").arg(0)), 0);
    QCOMPARE(evaluate<int>(input, QString("positionAt(%1)").arg(input->width())), 5);
}

void tst_qquicktextinput::preeditCursorRectangle()
{
    QString preeditText = "super";

    QQuickView view(testFileUrl("inputMethodEvent.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(view.rootObject());
    QVERIFY(input);
    QVERIFY(input->hasActiveFocus());

    QQuickItem *cursor = input->findChild<QQuickItem *>("cursor");
    QVERIFY(cursor);

    QRectF currentRect;

    QInputMethodQueryEvent query(Qt::ImCursorRectangle);
    QCoreApplication::sendEvent(input, &query);
    QRectF previousRect = query.value(Qt::ImCursorRectangle).toRectF();

    // Verify that the micro focus rect is positioned the same for position 0 as
    // it would be if there was no preedit text.
    sendPreeditText(input, preeditText, 0);
    QCoreApplication::sendEvent(input, &query);
    currentRect = query.value(Qt::ImCursorRectangle).toRectF();
    QCOMPARE(currentRect, previousRect);
    QCOMPARE(input->cursorRectangle(), currentRect);
    QCOMPARE(cursor->position(), currentRect.topLeft());

    QSignalSpy inputSpy(input, SIGNAL(cursorRectangleChanged()));
    QSignalSpy panelSpy(qGuiApp->inputMethod(), SIGNAL(cursorRectangleChanged()));

    // Verify that the micro focus rect moves to the left as the cursor position
    // is incremented.
    for (int i = 1; i <= 5; ++i) {
        sendPreeditText(input, preeditText, i);
        QCoreApplication::sendEvent(input, &query);
        currentRect = query.value(Qt::ImCursorRectangle).toRectF();
        QVERIFY(previousRect.left() < currentRect.left());
        QCOMPARE(input->cursorRectangle(), currentRect);
        QCOMPARE(cursor->position(), currentRect.topLeft());
        QVERIFY(inputSpy.count() > 0); inputSpy.clear();
        QVERIFY(panelSpy.count() > 0); panelSpy.clear();
        previousRect = currentRect;
    }

    // Verify that if the cursor rectangle is updated if the pre-edit text changes
    // but the (non-zero) cursor position is the same.
    inputSpy.clear();
    panelSpy.clear();
    sendPreeditText(input, "wwwww", 5);
    QCoreApplication::sendEvent(input, &query);
    currentRect = query.value(Qt::ImCursorRectangle).toRectF();
    QCOMPARE(input->cursorRectangle(), currentRect);
    QCOMPARE(cursor->position(), currentRect.topLeft());
    QCOMPARE(inputSpy.count(), 1);
    QCOMPARE(panelSpy.count(), 1);

    // Verify that if there is no preedit cursor then the micro focus rect is the
    // same as it would be if it were positioned at the end of the preedit text.
    inputSpy.clear();
    panelSpy.clear();
    {   QInputMethodEvent imEvent(preeditText, QList<QInputMethodEvent::Attribute>());
        QCoreApplication::sendEvent(input, &imEvent); }
    QCoreApplication::sendEvent(input, &query);
    currentRect = query.value(Qt::ImCursorRectangle).toRectF();
    QCOMPARE(currentRect, previousRect);
    QCOMPARE(input->cursorRectangle(), currentRect);
    QCOMPARE(cursor->position(), currentRect.topLeft());
    QCOMPARE(inputSpy.count(), 1);
    QCOMPARE(panelSpy.count(), 1);
}

void tst_qquicktextinput::inputContextMouseHandler()
{
    PlatformInputContext platformInputContext;
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = &platformInputContext;

    QString text = "supercalifragisiticexpialidocious!";
    QQuickView view(testFileUrl("inputContext.qml"));
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(view.rootObject());
    QVERIFY(input);

    input->setFocus(true);
    input->setText("");

    view.showNormal();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);

    QTextLayout layout(text);
    layout.setFont(input->font());
    if (!qmlDisableDistanceField()) {
        QTextOption option;
        option.setUseDesignMetrics(true);
        layout.setTextOption(option);
    }
    layout.beginLayout();
    QTextLine line = layout.createLine();
    layout.endLayout();

    const qreal x = line.cursorToX(2, QTextLine::Leading);
    const qreal y = line.height() / 2;
    QPoint position = QPointF(x, y).toPoint();

    QInputMethodEvent inputEvent(text.mid(0, 5), QList<QInputMethodEvent::Attribute>());
    QGuiApplication::sendEvent(input, &inputEvent);

    QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, position);
    QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, position);
    QGuiApplication::processEvents();

    QCOMPARE(platformInputContext.m_action, QInputMethod::Click);
    QCOMPARE(platformInputContext.m_invokeActionCallCount, 1);
    QCOMPARE(platformInputContext.m_cursorPosition, 2);
}

void tst_qquicktextinput::inputMethodComposing()
{
    QString text = "supercalifragisiticexpialidocious!";

    QQuickView view(testFileUrl("inputContext.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(view.rootObject());
    QVERIFY(input);
    QVERIFY(input->hasActiveFocus());
    QSignalSpy spy(input, SIGNAL(inputMethodComposingChanged()));

    QCOMPARE(input->isInputMethodComposing(), false);
    {
        QInputMethodEvent event(text.mid(3), QList<QInputMethodEvent::Attribute>());
        QGuiApplication::sendEvent(input, &event);
    }
    QCOMPARE(input->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 1);

    {
        QInputMethodEvent event(text.mid(12), QList<QInputMethodEvent::Attribute>());
        QGuiApplication::sendEvent(input, &event);
    }
    QCOMPARE(spy.count(), 1);

    {
        QInputMethodEvent event;
        QGuiApplication::sendEvent(input, &event);
    }
    QCOMPARE(input->isInputMethodComposing(), false);
    QCOMPARE(spy.count(), 2);

    // Changing the text while not composing doesn't alter the composing state.
    input->setText(text.mid(0, 16));
    QCOMPARE(input->isInputMethodComposing(), false);
    QCOMPARE(spy.count(), 2);

    {
        QInputMethodEvent event(text.mid(16), QList<QInputMethodEvent::Attribute>());
        QGuiApplication::sendEvent(input, &event);
    }
    QCOMPARE(input->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 3);

    // Changing the text while composing cancels composition.
    input->setText(text.mid(0, 12));
    QCOMPARE(input->isInputMethodComposing(), false);
    QCOMPARE(spy.count(), 4);

    {   // Preedit cursor positioned outside (empty) preedit; composing.
        QInputMethodEvent event(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, -2, 1, QVariant()));
        QGuiApplication::sendEvent(input, &event);
    }
    QCOMPARE(input->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 5);


    {   // Cursor hidden; composing
        QInputMethodEvent event(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 0, QVariant()));
        QGuiApplication::sendEvent(input, &event);
    }
    QCOMPARE(input->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 5);

    {   // Default cursor attributes; composing.
        QInputMethodEvent event(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 1, QVariant()));
        QGuiApplication::sendEvent(input, &event);
    }
    QCOMPARE(input->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 5);

    {   // Selections are persisted: not composing
        QInputMethodEvent event(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, -5, 4, QVariant()));
        QGuiApplication::sendEvent(input, &event);
    }
    QCOMPARE(input->isInputMethodComposing(), false);
    QCOMPARE(spy.count(), 6);

    input->setCursorPosition(12);

    {   // Formatting applied; composing.
        QTextCharFormat format;
        format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        QInputMethodEvent event(QString(), QList<QInputMethodEvent::Attribute>()
                << QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, -5, 4, format));
        QGuiApplication::sendEvent(input, &event);
    }
    QCOMPARE(input->isInputMethodComposing(), true);
    QCOMPARE(spy.count(), 7);

    {
        QInputMethodEvent event;
        QGuiApplication::sendEvent(input, &event);
    }
    QCOMPARE(input->isInputMethodComposing(), false);
    QCOMPARE(spy.count(), 8);
}

void tst_qquicktextinput::inputMethodUpdate()
{
    PlatformInputContext platformInputContext;
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = &platformInputContext;

    QQuickView view(testFileUrl("inputContext.qml"));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(view.rootObject());
    QVERIFY(input);
    QVERIFY(input->hasActiveFocus());

    // text change even without cursor position change needs to trigger update
    input->setText("test");
    platformInputContext.clear();
    input->setText("xxxx");
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // input method event replacing text
    platformInputContext.clear();
    {
        QInputMethodEvent inputMethodEvent;
        inputMethodEvent.setCommitString("y", -1, 1);
        QGuiApplication::sendEvent(input, &inputMethodEvent);
    }
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // input method changing selection
    platformInputContext.clear();
    {
        QList<QInputMethodEvent::Attribute> attributes;
        attributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 0, 2, QVariant());
        QInputMethodEvent inputMethodEvent("", attributes);
        QGuiApplication::sendEvent(input, &inputMethodEvent);
    }
    QVERIFY(input->selectionStart() != input->selectionEnd());
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // programmatical selections trigger update
    platformInputContext.clear();
    input->selectAll();
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // font changes
    platformInputContext.clear();
    QFont font = input->font();
    font.setBold(!font.bold());
    input->setFont(font);
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // normal input
    platformInputContext.clear();
    {
        QInputMethodEvent inputMethodEvent;
        inputMethodEvent.setCommitString("y");
        QGuiApplication::sendEvent(input, &inputMethodEvent);
    }
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // changing cursor position
    platformInputContext.clear();
    input->setCursorPosition(0);
    QVERIFY(platformInputContext.m_updateCallCount > 0);

    // read only disabled input method
    platformInputContext.clear();
    input->setReadOnly(true);
    QVERIFY(platformInputContext.m_updateCallCount > 0);
    input->setReadOnly(false);

    // no updates while no focus
    input->setFocus(false);
    platformInputContext.clear();
    input->setText("Foo");
    QCOMPARE(platformInputContext.m_updateCallCount, 0);
    input->setCursorPosition(1);
    QCOMPARE(platformInputContext.m_updateCallCount, 0);
    input->selectAll();
    QCOMPARE(platformInputContext.m_updateCallCount, 0);
    input->setReadOnly(true);
    QCOMPARE(platformInputContext.m_updateCallCount, 0);
}

void tst_qquicktextinput::cursorRectangleSize()
{
    QQuickView *window = new QQuickView(testFileUrl("positionAt.qml"));
    QVERIFY(window->rootObject() != 0);
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput *>(window->rootObject());

    // make sure cursor rectangle is not at (0,0)
    textInput->setX(10);
    textInput->setY(10);
    textInput->setCursorPosition(3);
    QVERIFY(textInput != 0);
    textInput->setFocus(true);
    window->show();
    window->requestActivate();
    QTest::qWaitForWindowActive(window);
    QVERIFY(textInput->hasActiveFocus());

    QInputMethodQueryEvent event(Qt::ImCursorRectangle);
    qApp->sendEvent(textInput, &event);
    QRectF cursorRectFromQuery = event.value(Qt::ImCursorRectangle).toRectF();

    QRectF cursorRectFromItem = textInput->cursorRectangle();
    QRectF cursorRectFromPositionToRectangle = textInput->positionToRectangle(textInput->cursorPosition());

    // item and input query cursor rectangles match
    QCOMPARE(cursorRectFromItem, cursorRectFromQuery);

    // item cursor rectangle and positionToRectangle calculations match
    QCOMPARE(cursorRectFromItem, cursorRectFromPositionToRectangle);

    // item-window transform and input item transform match
    QCOMPARE(QQuickItemPrivate::get(textInput)->itemToWindowTransform(), qApp->inputMethod()->inputItemTransform());

    // input panel cursorRectangle property and tranformed item cursor rectangle match
    QRectF sceneCursorRect = QQuickItemPrivate::get(textInput)->itemToWindowTransform().mapRect(cursorRectFromItem);
    QCOMPARE(sceneCursorRect, qApp->inputMethod()->cursorRectangle());

    delete window;
}

void tst_qquicktextinput::tripleClickSelectsAll()
{
    QString qmlfile = testFile("positionAt.qml");
    QQuickView view(QUrl::fromLocalFile(qmlfile));
    view.show();
    view.requestActivate();
    QTest::qWaitForWindowActive(&view);

    QQuickTextInput* input = qobject_cast<QQuickTextInput*>(view.rootObject());
    QVERIFY(input);

    QLatin1String hello("Hello world!");
    input->setSelectByMouse(true);
    input->setText(hello);

    // Clicking on the same point inside TextInput three times in a row
    // should trigger a triple click, thus selecting all the text.
    QPoint pointInside = input->position().toPoint() + QPoint(2,2);
    QTest::mouseDClick(&view, Qt::LeftButton, 0, pointInside);
    QTest::mouseClick(&view, Qt::LeftButton, 0, pointInside);
    QGuiApplication::processEvents();
    QCOMPARE(input->selectedText(), hello);

    // Now it simulates user moving the mouse between the second and the third click.
    // In this situation, we don't expect a triple click.
    QPoint pointInsideButFar = QPoint(input->width(),input->height()) - QPoint(2,2);
    QTest::mouseDClick(&view, Qt::LeftButton, 0, pointInside);
    QTest::mouseClick(&view, Qt::LeftButton, 0, pointInsideButFar);
    QGuiApplication::processEvents();
    QVERIFY(input->selectedText().isEmpty());

    // And now we press the third click too late, so no triple click event is triggered.
    QTest::mouseDClick(&view, Qt::LeftButton, 0, pointInside);
    QGuiApplication::processEvents();
    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 1);
    QTest::mouseClick(&view, Qt::LeftButton, 0, pointInside);
    QGuiApplication::processEvents();
    QVERIFY(input->selectedText().isEmpty());
}

void tst_qquicktextinput::QTBUG_19956_data()
{
    QTest::addColumn<QString>("url");
    QTest::newRow("intvalidator") << "qtbug-19956int.qml";
    QTest::newRow("doublevalidator") << "qtbug-19956double.qml";
}


void tst_qquicktextinput::getText_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("inputMask");
    QTest::addColumn<int>("start");
    QTest::addColumn<int>("end");
    QTest::addColumn<QString>("expectedText");

    QTest::newRow("all plain text")
            << standard.at(0)
            << QString()
            << 0 << standard.at(0).length()
            << standard.at(0);

    QTest::newRow("plain text sub string")
            << standard.at(0)
            << QString()
            << 0 << 12
            << standard.at(0).mid(0, 12);

    QTest::newRow("plain text sub string reversed")
            << standard.at(0)
            << QString()
            << 12 << 0
            << standard.at(0).mid(0, 12);

    QTest::newRow("plain text cropped beginning")
            << standard.at(0)
            << QString()
            << -3 << 4
            << standard.at(0).mid(0, 4);

    QTest::newRow("plain text cropped end")
            << standard.at(0)
            << QString()
            << 23 << standard.at(0).length() + 8
            << standard.at(0).mid(23);

    QTest::newRow("plain text cropped beginning and end")
            << standard.at(0)
            << QString()
            << -9 << standard.at(0).length() + 4
            << standard.at(0);
}

void tst_qquicktextinput::getText()
{
    QFETCH(QString, text);
    QFETCH(QString, inputMask);
    QFETCH(int, start);
    QFETCH(int, end);
    QFETCH(QString, expectedText);

    QString componentStr = "import QtQuick 2.0\nTextInput { text: \"" + text + "\"; inputMask: \"" + inputMask + "\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    QCOMPARE(textInput->getText(start, end), expectedText);
}

void tst_qquicktextinput::insert_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("inputMask");
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
            << standard.at(0)
            << QString()
            << 0 << 0 << 0
            << QString("Hello")
            << QString("Hello") + standard.at(0)
            << 5 << 5 << 5
            << false << true;

    QTest::newRow("at cursor position (end)")
            << standard.at(0)
            << QString()
            << standard.at(0).length() << standard.at(0).length() << standard.at(0).length()
            << QString("Hello")
            << standard.at(0) + QString("Hello")
            << standard.at(0).length() + 5 << standard.at(0).length() + 5 << standard.at(0).length() + 5
            << false << true;

    QTest::newRow("at cursor position (middle)")
            << standard.at(0)
            << QString()
            << 18 << 18 << 18
            << QString("Hello")
            << standard.at(0).mid(0, 18) + QString("Hello") + standard.at(0).mid(18)
            << 23 << 23 << 23
            << false << true;

    QTest::newRow("after cursor position (beginning)")
            << standard.at(0)
            << QString()
            << 0 << 0 << 18
            << QString("Hello")
            << standard.at(0).mid(0, 18) + QString("Hello") + standard.at(0).mid(18)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("before cursor position (end)")
            << standard.at(0)
            << QString()
            << standard.at(0).length() << standard.at(0).length() << 18
            << QString("Hello")
            << standard.at(0).mid(0, 18) + QString("Hello") + standard.at(0).mid(18)
            << standard.at(0).length() + 5 << standard.at(0).length() + 5 << standard.at(0).length() + 5
            << false << true;

    QTest::newRow("before cursor position (middle)")
            << standard.at(0)
            << QString()
            << 18 << 18 << 0
            << QString("Hello")
            << QString("Hello") + standard.at(0)
            << 23 << 23 << 23
            << false << true;

    QTest::newRow("after cursor position (middle)")
            << standard.at(0)
            << QString()
            << 18 << 18 << standard.at(0).length()
            << QString("Hello")
            << standard.at(0) + QString("Hello")
            << 18 << 18 << 18
            << false << false;

    QTest::newRow("before selection")
            << standard.at(0)
            << QString()
            << 14 << 19 << 0
            << QString("Hello")
            << QString("Hello") + standard.at(0)
            << 19 << 24 << 24
            << false << true;

    QTest::newRow("before reversed selection")
            << standard.at(0)
            << QString()
            << 19 << 14 << 0
            << QString("Hello")
            << QString("Hello") + standard.at(0)
            << 19 << 24 << 19
            << false << true;

    QTest::newRow("after selection")
            << standard.at(0)
            << QString()
            << 14 << 19 << standard.at(0).length()
            << QString("Hello")
            << standard.at(0) + QString("Hello")
            << 14 << 19 << 19
            << false << false;

    QTest::newRow("after reversed selection")
            << standard.at(0)
            << QString()
            << 19 << 14 << standard.at(0).length()
            << QString("Hello")
            << standard.at(0) + QString("Hello")
            << 14 << 19 << 14
            << false << false;

    QTest::newRow("into selection")
            << standard.at(0)
            << QString()
            << 14 << 19 << 18
            << QString("Hello")
            << standard.at(0).mid(0, 18) + QString("Hello") + standard.at(0).mid(18)
            << 14 << 24 << 24
            << true << true;

    QTest::newRow("into reversed selection")
            << standard.at(0)
            << QString()
            << 19 << 14 << 18
            << QString("Hello")
            << standard.at(0).mid(0, 18) + QString("Hello") + standard.at(0).mid(18)
            << 14 << 24 << 14
            << true << false;

    QTest::newRow("rich text into plain text")
            << standard.at(0)
            << QString()
            << 0 << 0 << 0
            << QString("<b>Hello</b>")
            << QString("<b>Hello</b>") + standard.at(0)
            << 12 << 12 << 12
            << false << true;

    QTest::newRow("before start")
            << standard.at(0)
            << QString()
            << 0 << 0 << -3
            << QString("Hello")
            << standard.at(0)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("past end")
            << standard.at(0)
            << QString()
            << 0 << 0 << standard.at(0).length() + 3
            << QString("Hello")
            << standard.at(0)
            << 0 << 0 << 0
            << false << false;

    const QString inputMask = "009.009.009.009";
    const QString ip = "192.168.2.14";

    QTest::newRow("mask: at cursor position (beginning)")
            << ip
            << inputMask
            << 0 << 0 << 0
            << QString("125")
            << QString("125.168.2.14")
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("mask: at cursor position (end)")
            << ip
            << inputMask
            << inputMask.length() << inputMask.length() << inputMask.length()
            << QString("8")
            << ip
            << inputMask.length() << inputMask.length() << inputMask.length()
            << false << false;

    QTest::newRow("mask: at cursor position (middle)")
            << ip
            << inputMask
            << 6 << 6 << 6
            << QString("75.2")
            << QString("192.167.5.24")
            << 6 << 6 << 6
            << false << false;

    QTest::newRow("mask: after cursor position (beginning)")
            << ip
            << inputMask
            << 0 << 0 << 6
            << QString("75.2")
            << QString("192.167.5.24")
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("mask: before cursor position (end)")
            << ip
            << inputMask
            << inputMask.length() << inputMask.length() << 6
            << QString("75.2")
            << QString("192.167.5.24")
            << inputMask.length() << inputMask.length() << inputMask.length()
            << false << false;

    QTest::newRow("mask: before cursor position (middle)")
            << ip
            << inputMask
            << 6 << 6 << 0
            << QString("125")
            << QString("125.168.2.14")
            << 6 << 6 << 6
            << false << false;

    QTest::newRow("mask: after cursor position (middle)")
            << ip
            << inputMask
            << 6 << 6 << 13
            << QString("8")
            << "192.168.2.18"
            << 6 << 6 << 6
            << false << false;

    QTest::newRow("mask: before selection")
            << ip
            << inputMask
            << 6 << 8 << 0
            << QString("125")
            << QString("125.168.2.14")
            << 6 << 8 << 8
            << false << false;

    QTest::newRow("mask: before reversed selection")
            << ip
            << inputMask
            << 8 << 6 << 0
            << QString("125")
            << QString("125.168.2.14")
            << 6 << 8 << 6
            << false << false;

    QTest::newRow("mask: after selection")
            << ip
            << inputMask
            << 6 << 8 << 13
            << QString("8")
            << "192.168.2.18"
            << 6 << 8 << 8
            << false << false;

    QTest::newRow("mask: after reversed selection")
            << ip
            << inputMask
            << 8 << 6 << 13
            << QString("8")
            << "192.168.2.18"
            << 6 << 8 << 6
            << false << false;

    QTest::newRow("mask: into selection")
            << ip
            << inputMask
            << 5 << 8 << 6
            << QString("75.2")
            << QString("192.167.5.24")
            << 5 << 8 << 8
            << true << false;

    QTest::newRow("mask: into reversed selection")
            << ip
            << inputMask
            << 8 << 5 << 6
            << QString("75.2")
            << QString("192.167.5.24")
            << 5 << 8 << 5
            << true << false;

    QTest::newRow("mask: before start")
            << ip
            << inputMask
            << 0 << 0 << -3
            << QString("4")
            << ip
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("mask: past end")
            << ip
            << inputMask
            << 0 << 0 << ip.length() + 3
            << QString("4")
            << ip
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("mask: invalid characters")
            << ip
            << inputMask
            << 0 << 0 << 0
            << QString("abc")
            << QString("192.168.2.14")
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("mask: mixed validity")
            << ip
            << inputMask
            << 0 << 0 << 0
            << QString("a1b2c5")
            << QString("125.168.2.14")
            << 0 << 0 << 0
            << false << false;
}

void tst_qquicktextinput::insert()
{
    QFETCH(QString, text);
    QFETCH(QString, inputMask);
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

    QString componentStr = "import QtQuick 2.0\nTextInput { text: \"" + text + "\"; inputMask: \"" + inputMask + "\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    textInput->select(selectionStart, selectionEnd);

    QSignalSpy selectionSpy(textInput, SIGNAL(selectedTextChanged()));
    QSignalSpy selectionStartSpy(textInput, SIGNAL(selectionStartChanged()));
    QSignalSpy selectionEndSpy(textInput, SIGNAL(selectionEndChanged()));
    QSignalSpy textSpy(textInput, SIGNAL(textChanged()));
    QSignalSpy cursorPositionSpy(textInput, SIGNAL(cursorPositionChanged()));

    textInput->insert(insertPosition, insertText);

    QCOMPARE(textInput->text(), expectedText);
    QCOMPARE(textInput->length(), inputMask.isEmpty() ? expectedText.length() : inputMask.length());

    QCOMPARE(textInput->selectionStart(), expectedSelectionStart);
    QCOMPARE(textInput->selectionEnd(), expectedSelectionEnd);
    QCOMPARE(textInput->cursorPosition(), expectedCursorPosition);

    if (selectionStart > selectionEnd)
        qSwap(selectionStart, selectionEnd);

    QCOMPARE(selectionSpy.count() > 0, selectionChanged);
    QCOMPARE(selectionStartSpy.count() > 0, selectionStart != expectedSelectionStart);
    QCOMPARE(selectionEndSpy.count() > 0, selectionEnd != expectedSelectionEnd);
    QCOMPARE(textSpy.count() > 0, text != expectedText);
    QCOMPARE(cursorPositionSpy.count() > 0, cursorPositionChanged);
}

void tst_qquicktextinput::remove_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("inputMask");
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

    QTest::newRow("from cursor position (beginning)")
            << standard.at(0)
            << QString()
            << 0 << 0
            << 0 << 5
            << standard.at(0).mid(5)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("to cursor position (beginning)")
            << standard.at(0)
            << QString()
            << 0 << 0
            << 5 << 0
            << standard.at(0).mid(5)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("to cursor position (end)")
            << standard.at(0)
            << QString()
            << standard.at(0).length() << standard.at(0).length()
            << standard.at(0).length() << standard.at(0).length() - 5
            << standard.at(0).mid(0, standard.at(0).length() - 5)
            << standard.at(0).length() - 5 << standard.at(0).length() - 5 << standard.at(0).length() - 5
            << false << true;

    QTest::newRow("to cursor position (end)")
            << standard.at(0)
            << QString()
            << standard.at(0).length() << standard.at(0).length()
            << standard.at(0).length() - 5 << standard.at(0).length()
            << standard.at(0).mid(0, standard.at(0).length() - 5)
            << standard.at(0).length() - 5 << standard.at(0).length() - 5 << standard.at(0).length() - 5
            << false << true;

    QTest::newRow("from cursor position (middle)")
            << standard.at(0)
            << QString()
            << 18 << 18
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 18 << 18 << 18
            << false << false;

    QTest::newRow("to cursor position (middle)")
            << standard.at(0)
            << QString()
            << 23 << 23
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 18 << 18 << 18
            << false << true;

    QTest::newRow("after cursor position (beginning)")
            << standard.at(0)
            << QString()
            << 0 << 0
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("before cursor position (end)")
            << standard.at(0)
            << QString()
            << standard.at(0).length() << standard.at(0).length()
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << standard.at(0).length() - 5 << standard.at(0).length() - 5 << standard.at(0).length() - 5
            << false << true;

    QTest::newRow("before cursor position (middle)")
            << standard.at(0)
            << QString()
            << 23 << 23
            << 0 << 5
            << standard.at(0).mid(5)
            << 18 << 18 << 18
            << false << true;

    QTest::newRow("after cursor position (middle)")
            << standard.at(0)
            << QString()
            << 18 << 18
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 18 << 18 << 18
            << false << false;

    QTest::newRow("before selection")
            << standard.at(0)
            << QString()
            << 14 << 19
            << 0 << 5
            << standard.at(0).mid(5)
            << 9 << 14 << 14
            << false << true;

    QTest::newRow("before reversed selection")
            << standard.at(0)
            << QString()
            << 19 << 14
            << 0 << 5
            << standard.at(0).mid(5)
            << 9 << 14 << 9
            << false << true;

    QTest::newRow("after selection")
            << standard.at(0)
            << QString()
            << 14 << 19
            << standard.at(0).length() - 5 << standard.at(0).length()
            << standard.at(0).mid(0, standard.at(0).length() - 5)
            << 14 << 19 << 19
            << false << false;

    QTest::newRow("after reversed selection")
            << standard.at(0)
            << QString()
            << 19 << 14
            << standard.at(0).length() - 5 << standard.at(0).length()
            << standard.at(0).mid(0, standard.at(0).length() - 5)
            << 14 << 19 << 14
            << false << false;

    QTest::newRow("from selection")
            << standard.at(0)
            << QString()
            << 14 << 24
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 14 << 19 << 19
            << true << true;

    QTest::newRow("from reversed selection")
            << standard.at(0)
            << QString()
            << 24 << 14
            << 18 << 23
            << standard.at(0).mid(0, 18) + standard.at(0).mid(23)
            << 14 << 19 << 14
            << true << false;

    QTest::newRow("cropped beginning")
            << standard.at(0)
            << QString()
            << 0 << 0
            << -3 << 4
            << standard.at(0).mid(4)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("cropped end")
            << standard.at(0)
            << QString()
            << 0 << 0
            << 23 << standard.at(0).length() + 8
            << standard.at(0).mid(0, 23)
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("cropped beginning and end")
            << standard.at(0)
            << QString()
            << 0 << 0
            << -9 << standard.at(0).length() + 4
            << QString()
            << 0 << 0 << 0
            << false << false;

    const QString inputMask = "009.009.009.009";
    const QString ip = "192.168.2.14";

    QTest::newRow("mask: from cursor position")
            << ip
            << inputMask
            << 6 << 6
            << 6 << 9
            << QString("192.16..14")
            << 6 << 6 << 6
            << false << false;

    QTest::newRow("mask: to cursor position")
            << ip
            << inputMask
            << 6 << 6
            << 2 << 6
            << QString("19.8.2.14")
            << 6 << 6 << 6
            << false << false;

    QTest::newRow("mask: before cursor position")
            << ip
            << inputMask
            << 6 << 6
            << 0 << 2
            << QString("2.168.2.14")
            << 6 << 6 << 6
            << false << false;

    QTest::newRow("mask: after cursor position")
            << ip
            << inputMask
            << 6 << 6
            << 12 << 16
            << QString("192.168.2.")
            << 6 << 6 << 6
            << false << false;

    QTest::newRow("mask: before selection")
            << ip
            << inputMask
            << 6 << 8
            << 0 << 2
            << QString("2.168.2.14")
            << 6 << 8 << 8
            << false << false;

    QTest::newRow("mask: before reversed selection")
            << ip
            << inputMask
            << 8 << 6
            << 0 << 2
            << QString("2.168.2.14")
            << 6 << 8 << 6
            << false << false;

    QTest::newRow("mask: after selection")
            << ip
            << inputMask
            << 6 << 8
            << 12 << 16
            << QString("192.168.2.")
            << 6 << 8 << 8
            << false << false;

    QTest::newRow("mask: after reversed selection")
            << ip
            << inputMask
            << 8 << 6
            << 12 << 16
            << QString("192.168.2.")
            << 6 << 8 << 6
            << false << false;

    QTest::newRow("mask: from selection")
            << ip
            << inputMask
            << 6 << 13
            << 8 << 10
            << QString("192.168..14")
            << 6 << 13 << 13
            << true << false;

    QTest::newRow("mask: from reversed selection")
            << ip
            << inputMask
            << 13 << 6
            << 8 << 10
            << QString("192.168..14")
            << 6 << 13 << 6
            << true << false;

    QTest::newRow("mask: cropped beginning")
            << ip
            << inputMask
            << 0 << 0
            << -3 << 4
            << QString(".168.2.14")
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("mask: cropped end")
            << ip
            << inputMask
            << 0 << 0
            << 13 << 28
            << QString("192.168.2.1")
            << 0 << 0 << 0
            << false << false;

    QTest::newRow("mask: cropped beginning and end")
            << ip
            << inputMask
            << 0 << 0
            << -9 << 28
            << QString("...")
            << 0 << 0 << 0
            << false << false;
}

void tst_qquicktextinput::remove()
{
    QFETCH(QString, text);
    QFETCH(QString, inputMask);
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

    QString componentStr = "import QtQuick 2.0\nTextInput { text: \"" + text + "\"; inputMask: \"" + inputMask + "\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    textInput->select(selectionStart, selectionEnd);

    QSignalSpy selectionSpy(textInput, SIGNAL(selectedTextChanged()));
    QSignalSpy selectionStartSpy(textInput, SIGNAL(selectionStartChanged()));
    QSignalSpy selectionEndSpy(textInput, SIGNAL(selectionEndChanged()));
    QSignalSpy textSpy(textInput, SIGNAL(textChanged()));
    QSignalSpy cursorPositionSpy(textInput, SIGNAL(cursorPositionChanged()));

    textInput->remove(removeStart, removeEnd);

    QCOMPARE(textInput->text(), expectedText);
    QCOMPARE(textInput->length(), inputMask.isEmpty() ? expectedText.length() : inputMask.length());

    if (selectionStart > selectionEnd)  //
        qSwap(selectionStart, selectionEnd);

    QCOMPARE(textInput->selectionStart(), expectedSelectionStart);
    QCOMPARE(textInput->selectionEnd(), expectedSelectionEnd);
    QCOMPARE(textInput->cursorPosition(), expectedCursorPosition);

    QCOMPARE(selectionSpy.count() > 0, selectionChanged);
    QCOMPARE(selectionStartSpy.count() > 0, selectionStart != expectedSelectionStart);
    QCOMPARE(selectionEndSpy.count() > 0, selectionEnd != expectedSelectionEnd);
    QCOMPARE(textSpy.count() > 0, text != expectedText);

    if (cursorPositionChanged)  //
        QVERIFY(cursorPositionSpy.count() > 0);
}

void tst_qquicktextinput::keySequence_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QKeySequence>("sequence");
    QTest::addColumn<int>("selectionStart");
    QTest::addColumn<int>("selectionEnd");
    QTest::addColumn<int>("cursorPosition");
    QTest::addColumn<QString>("expectedText");
    QTest::addColumn<QString>("selectedText");
    QTest::addColumn<QQuickTextInput::EchoMode>("echoMode");
    QTest::addColumn<Qt::Key>("layoutDirection");

    // standard[0] == "the [4]quick [10]brown [16]fox [20]jumped [27]over [32]the [36]lazy [41]dog"

    QTest::newRow("select all")
            << standard.at(0) << QKeySequence(QKeySequence::SelectAll) << 0 << 0
            << 44 << standard.at(0) << standard.at(0)
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("select start of line")
            << standard.at(0) << QKeySequence(QKeySequence::SelectStartOfLine) << 5 << 5
            << 0 << standard.at(0) << standard.at(0).mid(0, 5)
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("select start of block")
            << standard.at(0) << QKeySequence(QKeySequence::SelectStartOfBlock) << 5 << 5
            << 0 << standard.at(0) << standard.at(0).mid(0, 5)
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("select end of line")
            << standard.at(0) << QKeySequence(QKeySequence::SelectEndOfLine) << 5 << 5
            << 44 << standard.at(0) << standard.at(0).mid(5)
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("select end of document") // ### Not handled.
            << standard.at(0) << QKeySequence(QKeySequence::SelectEndOfDocument) << 3 << 3
            << 3 << standard.at(0) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("select end of block")
            << standard.at(0) << QKeySequence(QKeySequence::SelectEndOfBlock) << 18 << 18
            << 44 << standard.at(0) << standard.at(0).mid(18)
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("delete end of line")
            << standard.at(0) << QKeySequence(QKeySequence::DeleteEndOfLine) << 24 << 24
            << 24 << standard.at(0).mid(0, 24) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("move to start of line")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToStartOfLine) << 31 << 31
            << 0 << standard.at(0) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("move to start of block")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToStartOfBlock) << 25 << 25
            << 0 << standard.at(0) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("move to next char")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToNextChar) << 12 << 12
            << 13 << standard.at(0) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("move to previous char (ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousChar) << 3 << 3
            << 2 << standard.at(0) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("move to previous char (rtl)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousChar) << 3 << 3
            << 4 << standard.at(0) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_R;
    QTest::newRow("move to previous char with selection")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousChar) << 3 << 7
            << 3 << standard.at(0) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("select next char (ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::SelectNextChar) << 23 << 23
            << 24 << standard.at(0) << standard.at(0).mid(23, 1)
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("select next char (rtl)")
            << standard.at(0) << QKeySequence(QKeySequence::SelectNextChar) << 23 << 23
            << 22 << standard.at(0) << standard.at(0).mid(22, 1)
            << QQuickTextInput::Normal << Qt::Key_Direction_R;
    QTest::newRow("select previous char (ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::SelectPreviousChar) << 19 << 19
            << 18 << standard.at(0) << standard.at(0).mid(18, 1)
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("select previous char (rtl)")
            << standard.at(0) << QKeySequence(QKeySequence::SelectPreviousChar) << 19 << 19
            << 20 << standard.at(0) << standard.at(0).mid(19, 1)
            << QQuickTextInput::Normal << Qt::Key_Direction_R;
    QTest::newRow("move to next word (ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToNextWord) << 7 << 7
            << 10 << standard.at(0) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("move to next word (rtl)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToNextWord) << 7 << 7
            << 4 << standard.at(0) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_R;
    QTest::newRow("move to next word (password,ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToNextWord) << 7 << 7
            << 44 << standard.at(0) << QString()
            << QQuickTextInput::Password << Qt::Key_Direction_L;
    QTest::newRow("move to next word (password,rtl)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToNextWord) << 7 << 7
            << 0 << standard.at(0) << QString()
            << QQuickTextInput::Password << Qt::Key_Direction_R;
    QTest::newRow("move to previous word (ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousWord) << 7 << 7
            << 4 << standard.at(0) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("move to previous word (rlt)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousWord) << 7 << 7
            << 10 << standard.at(0) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_R;
    QTest::newRow("move to previous word (password,ltr)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousWord) << 7 << 7
            << 0 << standard.at(0) << QString()
            << QQuickTextInput::Password << Qt::Key_Direction_L;
    QTest::newRow("move to previous word (password,rtl)")
            << standard.at(0) << QKeySequence(QKeySequence::MoveToPreviousWord) << 7 << 7
            << 44 << standard.at(0) << QString()
            << QQuickTextInput::Password << Qt::Key_Direction_R;
    QTest::newRow("select next word")
            << standard.at(0) << QKeySequence(QKeySequence::SelectNextWord) << 11 << 11
            << 16 << standard.at(0) << standard.at(0).mid(11, 5)
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("select previous word")
            << standard.at(0) << QKeySequence(QKeySequence::SelectPreviousWord) << 11 << 11
            << 10 << standard.at(0) << standard.at(0).mid(10, 1)
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("delete (selection)")
            << standard.at(0) << QKeySequence(QKeySequence::Delete) << 12 << 15
            << 12 << (standard.at(0).mid(0, 12) + standard.at(0).mid(15)) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("delete (no selection)")
            << standard.at(0) << QKeySequence(QKeySequence::Delete) << 15 << 15
            << 15 << (standard.at(0).mid(0, 15) + standard.at(0).mid(16)) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("delete end of word")
            << standard.at(0) << QKeySequence(QKeySequence::DeleteEndOfWord) << 24 << 24
            << 24 << (standard.at(0).mid(0, 24) + standard.at(0).mid(27)) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("delete start of word")
            << standard.at(0) << QKeySequence(QKeySequence::DeleteStartOfWord) << 7 << 7
            << 4 << (standard.at(0).mid(0, 4) + standard.at(0).mid(7)) << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
    QTest::newRow("delete complete line")
            << standard.at(0) << QKeySequence(QKeySequence::DeleteCompleteLine) << 0 << 0
            << 0 << QString() << QString()
            << QQuickTextInput::Normal << Qt::Key_Direction_L;
}

void tst_qquicktextinput::keySequence()
{
    QFETCH(QString, text);
    QFETCH(QKeySequence, sequence);
    QFETCH(int, selectionStart);
    QFETCH(int, selectionEnd);
    QFETCH(int, cursorPosition);
    QFETCH(QString, expectedText);
    QFETCH(QString, selectedText);
    QFETCH(QQuickTextInput::EchoMode, echoMode);
    QFETCH(Qt::Key, layoutDirection);

    if (sequence.isEmpty()) {
        QSKIP("Key sequence is undefined");
    }

    QString componentStr = "import QtQuick 2.0\nTextInput { focus: true; text: \"" + text + "\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);
    textInput->setEchoMode(echoMode);

    QQuickWindow window;
    textInput->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(textInput->hasActiveFocus());

    simulateKey(&window, layoutDirection);

    textInput->select(selectionStart, selectionEnd);

    simulateKeys(&window, sequence);

    QCOMPARE(textInput->cursorPosition(), cursorPosition);
    QCOMPARE(textInput->text(), expectedText);
    QCOMPARE(textInput->selectedText(), selectedText);
}

#define NORMAL 0
#define REPLACE_UNTIL_END 1

void tst_qquicktextinput::undo_data()
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

void tst_qquicktextinput::undo()
{
    QFETCH(QStringList, insertString);
    QFETCH(IntList, insertIndex);
    QFETCH(IntList, insertMode);
    QFETCH(QStringList, expectedString);

    QString componentStr = "import QtQuick 2.0\nTextInput { focus: true }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    QQuickWindow window;
    textInput->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(textInput->hasActiveFocus());

    QVERIFY(!textInput->canUndo());

    QSignalSpy spy(textInput, SIGNAL(canUndoChanged()));

    int i;

// STEP 1: First build up an undo history by inserting or typing some strings...
    for (i = 0; i < insertString.size(); ++i) {
        if (insertIndex[i] > -1)
            textInput->setCursorPosition(insertIndex[i]);

 // experimental stuff
        if (insertMode[i] == REPLACE_UNTIL_END) {
            textInput->select(insertIndex[i], insertIndex[i] + 8);

            // This is what I actually want...
            // QTest::keyClick(testWidget, Qt::Key_End, Qt::ShiftModifier);
        }

        for (int j = 0; j < insertString.at(i).length(); j++)
            QTest::keyClick(&window, insertString.at(i).at(j).toLatin1());
    }

    QCOMPARE(spy.count(), 1);

// STEP 2: Next call undo several times and see if we can restore to the previous state
    for (i = 0; i < expectedString.size() - 1; ++i) {
        QCOMPARE(textInput->text(), expectedString[i]);
        QVERIFY(textInput->canUndo());
        textInput->undo();
    }

// STEP 3: Verify that we have undone everything
    QVERIFY(textInput->text().isEmpty());
    QVERIFY(!textInput->canUndo());
    QCOMPARE(spy.count(), 2);
}

void tst_qquicktextinput::redo_data()
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

void tst_qquicktextinput::redo()
{
    QFETCH(QStringList, insertString);
    QFETCH(IntList, insertIndex);
    QFETCH(QStringList, expectedString);

    QString componentStr = "import QtQuick 2.0\nTextInput { focus: true }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    QQuickWindow window;
    textInput->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QVERIFY(textInput->hasActiveFocus());
    QVERIFY(!textInput->canUndo());
    QVERIFY(!textInput->canRedo());

    QSignalSpy spy(textInput, SIGNAL(canRedoChanged()));

    int i;
    // inserts the diff strings at diff positions
    for (i = 0; i < insertString.size(); ++i) {
        if (insertIndex[i] > -1)
            textInput->setCursorPosition(insertIndex[i]);
        for (int j = 0; j < insertString.at(i).length(); j++)
            QTest::keyClick(&window, insertString.at(i).at(j).toLatin1());
        QVERIFY(textInput->canUndo());
        QVERIFY(!textInput->canRedo());
    }

    QCOMPARE(spy.count(), 0);

    // undo everything
    while (!textInput->text().isEmpty()) {
        QVERIFY(textInput->canUndo());
        textInput->undo();
        QVERIFY(textInput->canRedo());
    }

    QCOMPARE(spy.count(), 1);

    for (i = 0; i < expectedString.size(); ++i) {
        QVERIFY(textInput->canRedo());
        textInput->redo();
        QCOMPARE(textInput->text() , expectedString[i]);
        QVERIFY(textInput->canUndo());
    }
    QVERIFY(!textInput->canRedo());
    QCOMPARE(spy.count(), 2);
}

void tst_qquicktextinput::undo_keypressevents_data()
{
    QTest::addColumn<KeyList>("keys");
    QTest::addColumn<QStringList>("expectedString");

    {
        KeyList keys;
        QStringList expectedString;

        keys << "AFRAID"
                << QKeySequence::MoveToStartOfLine
                << "VERY"
                << Qt::Key_Left
                << Qt::Key_Left
                << Qt::Key_Left
                << Qt::Key_Left
                << "BE"
                << QKeySequence::MoveToEndOfLine
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
        keys << "1234" << QKeySequence::MoveToStartOfLine
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
                << QKeySequence::MoveToStartOfLine
                // selecting 'AB'
                << (Qt::Key_Right | Qt::ShiftModifier) << (Qt::Key_Right | Qt::ShiftModifier)
                << Qt::Key_Backspace
                << QKeySequence::Undo
                << Qt::Key_Right
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
        keys << "123" << QKeySequence::MoveToStartOfLine
            // selecting '123'
             << QKeySequence::SelectEndOfLine
            // overwriting '123' with 'ABC'
             << "ABC";

        expectedString << "ABC";
        expectedString << "123";

        QTest::newRow("Inserts,moving,selection and overwriting") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        // inserting '123'
        keys << "123"
                << QKeySequence::Undo
                << QKeySequence::Redo;

        expectedString << "123";
        expectedString << QString();

        QTest::newRow("Insert,undo,redo") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        keys << "hello world"
             << (Qt::Key_Backspace | Qt::ControlModifier)
             << QKeySequence::Undo
             << QKeySequence::Redo
             << "hello";

        expectedString
                << "hello hello"
                << "hello "
                << "hello world"
                << QString();

        QTest::newRow("Insert,delete previous word,undo,redo,insert") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        keys << "hello world"
             << QKeySequence::SelectPreviousWord
             << (Qt::Key_Backspace)
             << QKeySequence::Undo
             << "hello";

        expectedString
                << "hello hello"
                << "hello world"
                << QString();

        QTest::newRow("Insert,select previous word,remove,undo,insert") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        keys << "hello world"
             << QKeySequence::DeleteStartOfWord
             << QKeySequence::Undo
             << "hello";

        expectedString
                << "hello worldhello"
                << "hello world"
                << QString();

        QTest::newRow("Insert,delete previous word,undo,insert") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        keys << "hello world"
             << QKeySequence::MoveToPreviousWord
             << QKeySequence::DeleteEndOfWord
             << QKeySequence::Undo
             << "hello";

        expectedString
                << "hello helloworld"
                << "hello world"
                << QString();

        QTest::newRow("Insert,move,delete next word,undo,insert") << keys << expectedString;
    }
    if (!QKeySequence(QKeySequence::DeleteEndOfLine).isEmpty()) {   // X11 only.
        KeyList keys;
        QStringList expectedString;

        keys << "hello world"
             << QKeySequence::MoveToStartOfLine
             << Qt::Key_Right
             << QKeySequence::DeleteEndOfLine
             << QKeySequence::Undo
             << "hello";

        expectedString
                << "hhelloello world"
                << "hello world"
                << QString();

        QTest::newRow("Insert,move,delete end of line,undo,insert") << keys << expectedString;
    } {
        KeyList keys;
        QStringList expectedString;

        keys << "hello world"
             << QKeySequence::MoveToPreviousWord
             << (Qt::Key_Left | Qt::ShiftModifier)
             << (Qt::Key_Left | Qt::ShiftModifier)
             << QKeySequence::DeleteEndOfWord
             << QKeySequence::Undo
             << "hello";

        expectedString
                << "hellhelloworld"
                << "hello world"
                << QString();

        QTest::newRow("Insert,move,select,delete next word,undo,insert") << keys << expectedString;
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
                // TextEdit: ""
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
                // TextEdit: "A"
                << "123";
        QTest::newRow("Copy,paste") << keys << expectedString;
    }
}

void tst_qquicktextinput::undo_keypressevents()
{
    QFETCH(KeyList, keys);
    QFETCH(QStringList, expectedString);

    QString componentStr = "import QtQuick 2.0\nTextInput { focus: true }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    QQuickWindow window;
    textInput->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(textInput->hasActiveFocus());

    simulateKeys(&window, keys);

    for (int i = 0; i < expectedString.size(); ++i) {
        QCOMPARE(textInput->text() , expectedString[i]);
        textInput->undo();
    }
    QVERIFY(textInput->text().isEmpty());
}

void tst_qquicktextinput::clear()
{
    QString componentStr = "import QtQuick 2.0\nTextInput { focus: true }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    QQuickWindow window;
    textInput->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(textInput->hasActiveFocus());
    QVERIFY(!textInput->canUndo());

    QSignalSpy spy(textInput, SIGNAL(canUndoChanged()));

    textInput->setText("I am Legend");
    QCOMPARE(textInput->text(), QString("I am Legend"));
    textInput->clear();
    QVERIFY(textInput->text().isEmpty());

    QCOMPARE(spy.count(), 1);

    // checks that clears can be undone
    textInput->undo();
    QVERIFY(!textInput->canUndo());
    QCOMPARE(spy.count(), 2);
    QCOMPARE(textInput->text(), QString("I am Legend"));

    textInput->setCursorPosition(4);
    QInputMethodEvent preeditEvent("PREEDIT", QList<QInputMethodEvent::Attribute>());
    QGuiApplication::sendEvent(textInput, &preeditEvent);
    QCOMPARE(textInput->text(), QString("I am Legend"));
    QCOMPARE(textInput->displayText(), QString("I amPREEDIT Legend"));
    QCOMPARE(textInput->preeditText(), QString("PREEDIT"));

    textInput->clear();
    QVERIFY(textInput->text().isEmpty());

    QCOMPARE(spy.count(), 3);

    // checks that clears can be undone
    textInput->undo();
    QVERIFY(!textInput->canUndo());
    QCOMPARE(spy.count(), 4);
    QCOMPARE(textInput->text(), QString("I am Legend"));
}

void tst_qquicktextinput::backspaceSurrogatePairs()
{
    // Test backspace, and delete remove both characters in a surrogate pair.
    static const quint16 textData[] = { 0xd800, 0xdf00, 0xd800, 0xdf01, 0xd800, 0xdf02, 0xd800, 0xdf03, 0xd800, 0xdf04 };
    const QString text = QString::fromUtf16(textData, lengthOf(textData));

    QString componentStr = "import QtQuick 2.0\nTextInput { focus: true }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);
    textInput->setText(text);
    textInput->setCursorPosition(text.length());

    QQuickWindow window;
    textInput->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    QCOMPARE(QGuiApplication::focusWindow(), &window);

    for (int i = text.length(); i >= 0; i -= 2) {
        QCOMPARE(textInput->text(), text.mid(0, i));
        QTest::keyClick(&window, Qt::Key_Backspace, Qt::NoModifier);
    }
    QCOMPARE(textInput->text(), QString());

    textInput->setText(text);
    textInput->setCursorPosition(0);

    for (int i = 0; i < text.length(); i += 2) {
        QCOMPARE(textInput->text(), text.mid(i));
        QTest::keyClick(&window, Qt::Key_Delete, Qt::NoModifier);
    }
    QCOMPARE(textInput->text(), QString());
}

void tst_qquicktextinput::QTBUG_19956()
{
    QFETCH(QString, url);

    QQuickView window(testFileUrl(url));
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(window.rootObject() != 0);
    QQuickTextInput *input = qobject_cast<QQuickTextInput*>(window.rootObject());
    QVERIFY(input);
    input->setFocus(true);
    QVERIFY(input->hasActiveFocus());

    QCOMPARE(window.rootObject()->property("topvalue").toInt(), 30);
    QCOMPARE(window.rootObject()->property("bottomvalue").toInt(), 10);
    QCOMPARE(window.rootObject()->property("text").toString(), QString("20"));
    QVERIFY(window.rootObject()->property("acceptableInput").toBool());

    window.rootObject()->setProperty("topvalue", 15);
    QCOMPARE(window.rootObject()->property("topvalue").toInt(), 15);
    QVERIFY(!window.rootObject()->property("acceptableInput").toBool());

    window.rootObject()->setProperty("topvalue", 25);
    QCOMPARE(window.rootObject()->property("topvalue").toInt(), 25);
    QVERIFY(window.rootObject()->property("acceptableInput").toBool());

    window.rootObject()->setProperty("bottomvalue", 21);
    QCOMPARE(window.rootObject()->property("bottomvalue").toInt(), 21);
    QVERIFY(!window.rootObject()->property("acceptableInput").toBool());

    window.rootObject()->setProperty("bottomvalue", 10);
    QCOMPARE(window.rootObject()->property("bottomvalue").toInt(), 10);
    QVERIFY(window.rootObject()->property("acceptableInput").toBool());
}

void tst_qquicktextinput::QTBUG_19956_regexp()
{
    QUrl url = testFileUrl("qtbug-19956regexp.qml");

    QString warning = url.toString() + ":11:17: Unable to assign [undefined] to QRegExp";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));

    QQuickView window(url);
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(window.rootObject() != 0);
    QQuickTextInput *input = qobject_cast<QQuickTextInput*>(window.rootObject());
    QVERIFY(input);
    input->setFocus(true);
    QVERIFY(input->hasActiveFocus());

    window.rootObject()->setProperty("regexvalue", QRegExp("abc"));
    QCOMPARE(window.rootObject()->property("regexvalue").toRegExp(), QRegExp("abc"));
    QCOMPARE(window.rootObject()->property("text").toString(), QString("abc"));
    QVERIFY(window.rootObject()->property("acceptableInput").toBool());

    window.rootObject()->setProperty("regexvalue", QRegExp("abcd"));
    QCOMPARE(window.rootObject()->property("regexvalue").toRegExp(), QRegExp("abcd"));
    QVERIFY(!window.rootObject()->property("acceptableInput").toBool());

    window.rootObject()->setProperty("regexvalue", QRegExp("abc"));
    QCOMPARE(window.rootObject()->property("regexvalue").toRegExp(), QRegExp("abc"));
    QVERIFY(window.rootObject()->property("acceptableInput").toBool());
}

void tst_qquicktextinput::implicitSize_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("wrap");
    QTest::newRow("plain") << "The quick red fox jumped over the lazy brown dog" << "TextInput.NoWrap";
    QTest::newRow("plain_wrap") << "The quick red fox jumped over the lazy brown dog" << "TextInput.Wrap";
}

void tst_qquicktextinput::implicitSize()
{
    QFETCH(QString, text);
    QFETCH(QString, wrap);
    QString componentStr = "import QtQuick 2.0\nTextInput { text: \"" + text + "\"; width: 50; wrapMode: " + wrap + " }";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickTextInput *textObject = qobject_cast<QQuickTextInput*>(textComponent.create());

    QVERIFY(textObject->width() < textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());

    textObject->resetWidth();
    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());
}

void tst_qquicktextinput::implicitSizeBinding_data()
{
    implicitSize_data();
}

void tst_qquicktextinput::implicitSizeBinding()
{
    QFETCH(QString, text);
    QFETCH(QString, wrap);
    QString componentStr = "import QtQuick 2.0\nTextInput { text: \"" + text + "\"; width: implicitWidth; height: implicitHeight; wrapMode: " + wrap + " }";
    QQmlComponent textComponent(&engine);
    textComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QScopedPointer<QObject> object(textComponent.create());
    QQuickTextInput *textObject = qobject_cast<QQuickTextInput *>(object.data());

    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());

    textObject->resetWidth();
    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());

    textObject->resetHeight();
    QCOMPARE(textObject->width(), textObject->implicitWidth());
    QCOMPARE(textObject->height(), textObject->implicitHeight());
}

void tst_qquicktextinput::implicitResize_data()
{
    QTest::addColumn<int>("alignment");
    QTest::newRow("left") << int(Qt::AlignLeft);
    QTest::newRow("center") << int(Qt::AlignHCenter);
    QTest::newRow("right") << int(Qt::AlignRight);
}

void tst_qquicktextinput::implicitResize()
{
    QFETCH(int, alignment);

    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nTextInput { }", QUrl::fromLocalFile(""));

    QScopedPointer<QQuickTextInput> textInput(qobject_cast<QQuickTextInput *>(component.create()));
    QVERIFY(!textInput.isNull());

    QScopedPointer<QQuickTextInput> textField(qobject_cast<QQuickTextInput *>(component.create()));
    QVERIFY(!textField.isNull());
    QQuickTextInputPrivate::get(textField.data())->setImplicitResizeEnabled(false);

    textInput->setWidth(200);
    textField->setImplicitWidth(200);

    textInput->setHAlign(QQuickTextInput::HAlignment(alignment));
    textField->setHAlign(QQuickTextInput::HAlignment(alignment));

    textInput->setText("Qt");
    textField->setText("Qt");

    QCOMPARE(textField->positionToRectangle(0), textInput->positionToRectangle(0));
}

void tst_qquicktextinput::negativeDimensions()
{
    // Verify this doesn't assert during initialization.
    QQmlComponent component(&engine, testFileUrl("negativeDimensions.qml"));
    QScopedPointer<QObject> o(component.create());
    QVERIFY(o);
    QQuickTextInput *input = o->findChild<QQuickTextInput *>("input");
    QVERIFY(input);
    QCOMPARE(input->width(), qreal(-1));
    QCOMPARE(input->height(), qreal(-1));
}


void tst_qquicktextinput::setInputMask_data()
{
    QTest::addColumn<QString>("mask");
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expectedText");
    QTest::addColumn<QString>("expectedDisplay");
    QTest::addColumn<bool>("insert_text");

    // both keyboard and insert()
    for (int i=0; i<2; i++) {
        bool insert_text = i==0 ? false : true;
        QString insert_mode = "keys ";
        if (insert_text)
            insert_mode = "insert ";

        QTest::newRow(QString(insert_mode + "ip_localhost").toLatin1())
            << QString("000.000.000.000")
            << QString("127.0.0.1")
            << QString("127.0.0.1")
            << QString("127.0  .0  .1  ")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "mac").toLatin1())
            << QString("HH:HH:HH:HH:HH:HH;#")
            << QString("00:E0:81:21:9E:8E")
            << QString("00:E0:81:21:9E:8E")
            << QString("00:E0:81:21:9E:8E")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "mac2").toLatin1())
            << QString("<HH:>HH:!HH:HH:HH:HH;#")
            << QString("AAe081219E8E")
            << QString("aa:E0:81:21:9E:8E")
            << QString("aa:E0:81:21:9E:8E")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "byte").toLatin1())
            << QString("BBBBBBBB;0")
            << QString("11011001")
            << QString("11111")
            << QString("11011001")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "halfbytes").toLatin1())
            << QString("bbbb.bbbb;-")
            << QString("110. 0001")
            << QString("110.0001")
            << QString("110-.0001")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "blank char same type as content").toLatin1())
            << QString("000.000.000.000;0")
            << QString("127.0.0.1")
            << QString("127...1")
            << QString("127.000.000.100")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "parts of ip_localhost").toLatin1())
            << QString("000.000.000.000")
            << QString(".0.0.1")
            << QString(".0.0.1")
            << QString("   .0  .0  .1  ")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "ip_null").toLatin1())
            << QString("000.000.000.000")
            << QString()
            << QString("...")
            << QString("   .   .   .   ")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "ip_null_hash").toLatin1())
            << QString("000.000.000.000;#")
            << QString()
            << QString("...")
            << QString("###.###.###.###")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "ip_overflow").toLatin1())
            << QString("000.000.000.000")
            << QString("1234123412341234")
            << QString("123.412.341.234")
            << QString("123.412.341.234")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "uppercase").toLatin1())
            << QString(">AAAA")
            << QString("AbCd")
            << QString("ABCD")
            << QString("ABCD")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "lowercase").toLatin1())
            << QString("<AAAA")
            << QString("AbCd")
            << QString("abcd")
            << QString("abcd")
            << bool(insert_text);

        QTest::newRow(QString(insert_mode + "nocase").toLatin1())
            << QString("!AAAA")
            << QString("AbCd")
            << QString("AbCd")
            << QString("AbCd")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "nocase1").toLatin1())
            << QString("!A!A!A!A")
            << QString("AbCd")
            << QString("AbCd")
            << QString("AbCd")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "nocase2").toLatin1())
            << QString("AAAA")
            << QString("AbCd")
            << QString("AbCd")
            << QString("AbCd")
            << bool(insert_text);

        QTest::newRow(QString(insert_mode + "reserved").toLatin1())
            << QString("{n}[0]")
            << QString("A9")
            << QString("A9")
            << QString("A9")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "escape01").toLatin1())
            << QString("\\\\N\\\\n00")
            << QString("9")
            << QString("Nn9")
            << QString("Nn9 ")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "escape02").toLatin1())
            << QString("\\\\\\\\00")
            << QString("0")
            << QString("\\0")
            << QString("\\0 ")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "escape03").toLatin1())
            << QString("\\\\(00\\\\)")
            << QString("0")
            << QString("(0)")
            << QString("(0 )")
            << bool(insert_text);

        QTest::newRow(QString(insert_mode + "upper_lower_nocase1").toLatin1())
            << QString(">AAAA<AAAA!AAAA")
            << QString("AbCdEfGhIjKl")
            << QString("ABCDefghIjKl")
            << QString("ABCDefghIjKl")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "upper_lower_nocase2").toLatin1())
            << QString(">aaaa<aaaa!aaaa")
            << QString("AbCdEfGhIjKl")
            << QString("ABCDefghIjKl")
            << QString("ABCDefghIjKl")
            << bool(insert_text);

        QTest::newRow(QString(insert_mode + "exact_case1").toLatin1())
            << QString(">A<A<A>A>A<A!A!A")
            << QString("AbCdEFGH")
            << QString("AbcDEfGH")
            << QString("AbcDEfGH")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "exact_case2").toLatin1())
            << QString(">A<A<A>A>A<A!A!A")
            << QString("aBcDefgh")
            << QString("AbcDEfgh")
            << QString("AbcDEfgh")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "exact_case3").toLatin1())
            << QString(">a<a<a>a>a<a!a!a")
            << QString("AbCdEFGH")
            << QString("AbcDEfGH")
            << QString("AbcDEfGH")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "exact_case4").toLatin1())
            << QString(">a<a<a>a>a<a!a!a")
            << QString("aBcDefgh")
            << QString("AbcDEfgh")
            << QString("AbcDEfgh")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "exact_case5").toLatin1())
            << QString(">H<H<H>H>H<H!H!H")
            << QString("aBcDef01")
            << QString("AbcDEf01")
            << QString("AbcDEf01")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "exact_case6").toLatin1())
            << QString(">h<h<h>h>h<h!h!h")
            << QString("aBcDef92")
            << QString("AbcDEf92")
            << QString("AbcDEf92")
            << bool(insert_text);

        QTest::newRow(QString(insert_mode + "illegal_keys1").toLatin1())
            << QString("AAAAAAAA")
            << QString("A2#a;.0!")
            << QString("Aa")
            << QString("Aa      ")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "illegal_keys2").toLatin1())
            << QString("AAAA")
            << QString("f4f4f4f4")
            << QString("ffff")
            << QString("ffff")
            << bool(insert_text);
        QTest::newRow(QString(insert_mode + "blank=input").toLatin1())
            << QString("9999;0")
            << QString("2004")
            << QString("2004")
            << QString("2004")
            << bool(insert_text);
    }
}

void tst_qquicktextinput::setInputMask()
{
    QFETCH(QString, mask);
    QFETCH(QString, input);
    QFETCH(QString, expectedText);
    QFETCH(QString, expectedDisplay);
    QFETCH(bool, insert_text);

    QString componentStr = "import QtQuick 2.0\nTextInput { focus: true; inputMask: \"" + mask + "\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    // then either insert using insert() or keyboard
    if (insert_text) {
        textInput->insert(0, input);
    } else {
        QQuickWindow window;
        textInput->setParentItem(window.contentItem());
        window.show();
        window.requestActivate();
        QTest::qWaitForWindowActive(&window);
        QVERIFY(textInput->hasActiveFocus());

        simulateKey(&window, Qt::Key_Home);
        for (int i = 0; i < input.length(); i++)
            QTest::keyClick(&window, input.at(i).toLatin1());
    }

    QEXPECT_FAIL( "keys blank=input", "To eat blanks or not? Known issue. Task 43172", Abort);
    QEXPECT_FAIL( "insert blank=input", "To eat blanks or not? Known issue. Task 43172", Abort);

    QCOMPARE(textInput->text(), expectedText);
    QCOMPARE(textInput->displayText(), expectedDisplay);
}

void tst_qquicktextinput::inputMask_data()
{
    QTest::addColumn<QString>("mask");
    QTest::addColumn<QString>("expectedMask");

    // if no mask is set a nul string should be returned
    QTest::newRow("nul 1") << QString("") << QString();
    QTest::newRow("nul 2") << QString() << QString();

    // try different masks
    QTest::newRow("mask 1") << QString("000.000.000.000") << QString("000.000.000.000; ");
    QTest::newRow("mask 2") << QString("000.000.000.000;#") << QString("000.000.000.000;#");
    QTest::newRow("mask 3") << QString("AAA.aa.999.###;") << QString("AAA.aa.999.###; ");
    QTest::newRow("mask 4") << QString(">abcdef<GHIJK") << QString(">abcdef<GHIJK; ");

    // set an invalid input mask...
    // the current behaviour is that this exact (faulty) string is returned.
    QTest::newRow("invalid") << QString("ABCDEFGHIKLMNOP;") << QString("ABCDEFGHIKLMNOP; ");

    // verify that we can unset the mask again
    QTest::newRow("unset") << QString("") << QString();
}

void tst_qquicktextinput::inputMask()
{
    QFETCH(QString, mask);
    QFETCH(QString, expectedMask);

    QString componentStr = "import QtQuick 2.0\nTextInput { focus: true; inputMask: \"" + mask + "\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    QCOMPARE(textInput->inputMask(), expectedMask);
}

void tst_qquicktextinput::clearInputMask()
{
    QString componentStr = "import QtQuick 2.0\nTextInput { focus: true; inputMask: \"000.000.000.000\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    QVERIFY(!textInput->inputMask().isEmpty());
    textInput->setInputMask(QString());
    QCOMPARE(textInput->inputMask(), QString());
}

void tst_qquicktextinput::keypress_inputMask_data()
{
    QTest::addColumn<QString>("mask");
    QTest::addColumn<KeyList>("keys");
    QTest::addColumn<QString>("expectedText");
    QTest::addColumn<QString>("expectedDisplayText");

    {
        KeyList keys;
        // inserting 'A1.2B'
        keys << Qt::Key_Home << "A1.2B";
        QTest::newRow("jumping on period(separator)") << QString("000.000;_") << keys << QString("1.2") << QString("1__.2__");
    }
    {
        KeyList keys;
        // inserting '0!P3'
        keys << Qt::Key_Home << "0!P3";
        QTest::newRow("jumping on input") << QString("D0.AA.XX.AA.00;_") << keys << QString("0..!P..3") << QString("_0.__.!P.__.3_");
    }
    {
        KeyList keys;
        // pressing delete
        keys << Qt::Key_Home
             << Qt::Key_Delete;
        QTest::newRow("delete") << QString("000.000;_") << keys << QString(".") << QString("___.___");
    }
    {
        KeyList keys;
        // selecting all and delete
        keys << Qt::Key_Home
             << Key(Qt::ShiftModifier, Qt::Key_End)
             << Qt::Key_Delete;
        QTest::newRow("deleting all") << QString("000.000;_") << keys << QString(".") << QString("___.___");
    }
    {
        KeyList keys;
        // inserting '12.12' then two backspaces
        keys << Qt::Key_Home << "12.12" << Qt::Key_Backspace << Qt::Key_Backspace;
        QTest::newRow("backspace") << QString("000.000;_") << keys << QString("12.") << QString("12_.___");
    }
    {
        KeyList keys;
        // inserting '12ab'
        keys << Qt::Key_Home << "12ab";
        QTest::newRow("uppercase") << QString("9999 >AA;_") << keys << QString("12 AB") << QString("12__ AB");
    }
    {
        KeyList keys;
        // inserting '12ab'
        keys << Qt::Key_Right << Qt::Key_Right << "1";
        QTest::newRow("Move in mask") << QString("#0:00;*") << keys << QString(":1") << QString("**:1*");
    }
}

void tst_qquicktextinput::keypress_inputMask()
{
    QFETCH(QString, mask);
    QFETCH(KeyList, keys);
    QFETCH(QString, expectedText);
    QFETCH(QString, expectedDisplayText);

    QString componentStr = "import QtQuick 2.0\nTextInput { focus: true; inputMask: \"" + mask + "\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    QQuickWindow window;
    textInput->setParentItem(window.contentItem());
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(textInput->hasActiveFocus());

    simulateKeys(&window, keys);

    QCOMPARE(textInput->text(), expectedText);
    QCOMPARE(textInput->displayText(), expectedDisplayText);
}


void tst_qquicktextinput::hasAcceptableInputMask_data()
{
    QTest::addColumn<QString>("optionalMask");
    QTest::addColumn<QString>("requiredMask");
    QTest::addColumn<QString>("invalid");
    QTest::addColumn<QString>("valid");

    QTest::newRow("Alphabetic optional and required")
        << QString("aaaa") << QString("AAAA") << QString("ab") << QString("abcd");
    QTest::newRow("Alphanumeric optional and require")
        << QString("nnnn") << QString("NNNN") << QString("R2") << QString("R2D2");
    QTest::newRow("Any optional and required")
        << QString("xxxx") << QString("XXXX") << QString("+-") << QString("+-*/");
    QTest::newRow("Numeric (0-9) required")
        << QString("0000") << QString("9999") << QString("11") << QString("1138");
    QTest::newRow("Numeric (1-9) optional and required")
        << QString("dddd") << QString("DDDD") << QString("12") << QString("1234");
}

void tst_qquicktextinput::hasAcceptableInputMask()
{
    QFETCH(QString, optionalMask);
    QFETCH(QString, requiredMask);
    QFETCH(QString, invalid);
    QFETCH(QString, valid);

    QString componentStr = "import QtQuick 2.0\nTextInput { inputMask: \"" + optionalMask + "\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    // test that invalid input (for required) work for optionalMask
    textInput->setText(invalid);
    QVERIFY(textInput->hasAcceptableInput());

    // test requiredMask
    textInput->setInputMask(requiredMask);
    textInput->setText(invalid);
    // invalid text gets the input mask applied when setting, text becomes acceptable.
    QVERIFY(textInput->hasAcceptableInput());

    textInput->setText(valid);
    QVERIFY(textInput->hasAcceptableInput());
}

void tst_qquicktextinput::maskCharacter_data()
{
    QTest::addColumn<QString>("mask");
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("expectedValid");

    QTest::newRow("Hex") << QString("H")
                         << QString("0123456789abcdefABCDEF") << true;
    QTest::newRow("hex") << QString("h")
                         << QString("0123456789abcdefABCDEF") << true;
    QTest::newRow("HexInvalid") << QString("H")
                                << QString("ghijklmnopqrstuvwxyzGHIJKLMNOPQRSTUVWXYZ")
                                << false;
    QTest::newRow("hexInvalid") << QString("h")
                                << QString("ghijklmnopqrstuvwxyzGHIJKLMNOPQRSTUVWXYZ")
                                << false;
    QTest::newRow("Bin") << QString("B")
                         << QString("01") << true;
    QTest::newRow("bin") << QString("b")
                         << QString("01") << true;
    QTest::newRow("BinInvalid") << QString("B")
                                << QString("23456789qwertyuiopasdfghjklzxcvbnm")
                                << false;
    QTest::newRow("binInvalid") << QString("b")
                                << QString("23456789qwertyuiopasdfghjklzxcvbnm")
                                << false;
}

void tst_qquicktextinput::maskCharacter()
{
    QFETCH(QString, mask);
    QFETCH(QString, input);
    QFETCH(bool, expectedValid);

    QString componentStr = "import QtQuick 2.0\nTextInput { inputMask: \"" + mask + "\" }";
    QQmlComponent textInputComponent(&engine);
    textInputComponent.setData(componentStr.toLatin1(), QUrl());
    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(textInputComponent.create());
    QVERIFY(textInput != 0);

    for (int i = 0; i < input.size(); ++i) {
        QString in = QString(input.at(i));
        QString expected = expectedValid ? in : QString();
        textInput->setText(QString(input.at(i)));
        QCOMPARE(textInput->text(), expected);
    }
}

class TestValidator : public QValidator
{
public:
    TestValidator(QObject *parent = 0) : QValidator(parent) { }

    State validate(QString &input, int &) const { return input == QStringLiteral("ok") ? Acceptable : Intermediate; }
    void fixup(QString &input) const { input = QStringLiteral("ok"); }
};

void tst_qquicktextinput::fixup()
{
    QQuickWindow window;
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);

    QQuickTextInput *input = new QQuickTextInput(window.contentItem());
    input->setValidator(new TestValidator(input));

    // fixup() on accept
    input->setFocus(true);
    QVERIFY(input->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Enter);
    QCOMPARE(input->text(), QStringLiteral("ok"));

    // fixup() on defocus
    input->setText(QString());
    input->setFocus(false);
    QVERIFY(!input->hasActiveFocus());
    QCOMPARE(input->text(), QStringLiteral("ok"));
}

typedef qreal (*ExpectedBaseline)(QQuickTextInput *item);
Q_DECLARE_METATYPE(ExpectedBaseline)

static qreal expectedBaselineTop(QQuickTextInput *item)
{
    QFontMetricsF fm(item->font());
    return fm.ascent() + item->topPadding();
}

static qreal expectedBaselineBottom(QQuickTextInput *item)
{
    QFontMetricsF fm(item->font());
    return item->height() - item->contentHeight() - item->bottomPadding() + fm.ascent();
}

static qreal expectedBaselineCenter(QQuickTextInput *item)
{
    QFontMetricsF fm(item->font());
    return ((item->height() - item->contentHeight() - item->topPadding() - item->bottomPadding()) / 2) + fm.ascent() + item->topPadding();
}

static qreal expectedBaselineMultilineBottom(QQuickTextInput *item)
{
    QFontMetricsF fm(item->font());
    return item->height() - item->contentHeight() - item->bottomPadding() + fm.ascent();
}

void tst_qquicktextinput::baselineOffset_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QByteArray>("bindings");
    QTest::addColumn<qreal>("setHeight");
    QTest::addColumn<ExpectedBaseline>("expectedBaseline");
    QTest::addColumn<ExpectedBaseline>("expectedBaselineEmpty");

    QTest::newRow("normal")
            << "Typography"
            << QByteArray()
            << -1.
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("top align")
            << "Typography"
            << QByteArray("height: 200; verticalAlignment: Text.AlignTop")
            << -1.
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("bottom align")
            << "Typography"
            << QByteArray("height: 200; verticalAlignment: Text.AlignBottom")
            << 100.
            << &expectedBaselineBottom
            << &expectedBaselineBottom;

    QTest::newRow("center align")
            << "Typography"
            << QByteArray("height: 200; verticalAlignment: Text.AlignVCenter")
            << 100.
            << &expectedBaselineCenter
            << &expectedBaselineCenter;

    QTest::newRow("multiline bottom aligned")
            << "The quick brown fox jumps over the lazy dog"
            << QByteArray("height: 200; width: 30; verticalAlignment: Text.AlignBottom; wrapMode: TextInput.WordWrap")
            << -1.
            << &expectedBaselineMultilineBottom
            << &expectedBaselineBottom;

    QTest::newRow("padding")
            << "Typography"
            << QByteArray("topPadding: 10; bottomPadding: 20")
            << -1.
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("top align with padding")
            << "Typography"
            << QByteArray("height: 200; verticalAlignment: Text.AlignTop; topPadding: 10; bottomPadding: 20")
            << -1.
            << &expectedBaselineTop
            << &expectedBaselineTop;

    QTest::newRow("bottom align with padding")
            << "Typography"
            << QByteArray("height: 200; verticalAlignment: Text.AlignBottom; topPadding: 10; bottomPadding: 20")
            << 100.
            << &expectedBaselineBottom
            << &expectedBaselineBottom;

    QTest::newRow("center align with padding")
            << "Typography"
            << QByteArray("height: 200; verticalAlignment: Text.AlignVCenter; topPadding: 10; bottomPadding: 20")
            << 100.
            << &expectedBaselineCenter
            << &expectedBaselineCenter;

    QTest::newRow("multiline bottom aligned with padding")
            << "The quick brown fox jumps over the lazy dog"
            << QByteArray("height: 200; width: 30; verticalAlignment: Text.AlignBottom; wrapMode: TextInput.WordWrap; topPadding: 10; bottomPadding: 20")
            << -1.
            << &expectedBaselineMultilineBottom
            << &expectedBaselineBottom;
}

void tst_qquicktextinput::baselineOffset()
{
    QFETCH(QString, text);
    QFETCH(QByteArray, bindings);
    QFETCH(qreal, setHeight);
    QFETCH(ExpectedBaseline, expectedBaseline);
    QFETCH(ExpectedBaseline, expectedBaselineEmpty);

    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.6\n"
            "TextInput {\n"
                + bindings + "\n"
            "}", QUrl());

    QScopedPointer<QObject> object(component.create());
    QQuickTextInput *item = qobject_cast<QQuickTextInput *>(object.data());

    int passes = setHeight >= 0 ? 2 : 1;
    while (passes--) {
        QVERIFY(item);
        QCOMPARE(item->baselineOffset(), expectedBaselineEmpty(item));
        item->setText(text);
        QCOMPARE(item->baselineOffset(), expectedBaseline(item));
        item->setText(QString());
        QCOMPARE(item->baselineOffset(), expectedBaselineEmpty(item));
        if (setHeight >= 0)
            item->setHeight(setHeight);
    }
}

void tst_qquicktextinput::ensureVisible()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n TextInput {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickTextInput *input = qobject_cast<QQuickTextInput *>(object.data());
    QVERIFY(input);

    input->setWidth(QFontMetrics(input->font()).averageCharWidth() * 3);
    input->setText("Hello World");

    QTextLayout layout;
    layout.setText(input->text());
    layout.setFont(input->font());

    if (!qmlDisableDistanceField()) {
        QTextOption option;
        option.setUseDesignMetrics(true);
        layout.setTextOption(option);
    }
    layout.beginLayout();
    QTextLine line = layout.createLine();
    layout.endLayout();

    input->ensureVisible(0);

    QCOMPARE(input->boundingRect().x(), qreal(0));
    QCOMPARE(input->boundingRect().y(), qreal(0));
    QCOMPARE(input->boundingRect().width(), line.naturalTextWidth() + input->cursorRectangle().width());
    QCOMPARE(input->boundingRect().height(), line.height());

    QSignalSpy cursorSpy(input, SIGNAL(cursorRectangleChanged()));
    QVERIFY(cursorSpy.isValid());

    input->ensureVisible(input->length());

    QCOMPARE(cursorSpy.count(), 1);

    QCOMPARE(input->boundingRect().x(), input->width() - line.naturalTextWidth());
    QCOMPARE(input->boundingRect().y(), qreal(0));
    QCOMPARE(input->boundingRect().width(), line.naturalTextWidth() + input->cursorRectangle().width());
    QCOMPARE(input->boundingRect().height(), line.height());
}

void tst_qquicktextinput::padding()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("padding.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QQuickItem *root = window->rootObject();
    QVERIFY(root);
    QQuickTextInput *obj = qobject_cast<QQuickTextInput*>(root);
    QVERIFY(obj != 0);

    qreal cw = obj->contentWidth();
    qreal ch = obj->contentHeight();

    QVERIFY(cw > 0);
    QVERIFY(ch > 0);

    QCOMPARE(obj->padding(), 10.0);
    QCOMPARE(obj->topPadding(), 20.0);
    QCOMPARE(obj->leftPadding(), 30.0);
    QCOMPARE(obj->rightPadding(), 40.0);
    QCOMPARE(obj->bottomPadding(), 50.0);

    QCOMPARE(obj->implicitWidth(), qCeil(cw) + obj->leftPadding() + obj->rightPadding());
    QCOMPARE(obj->implicitHeight(), qCeil(ch) + obj->topPadding() + obj->bottomPadding());

    obj->setTopPadding(2.25);
    QCOMPARE(obj->topPadding(), 2.25);
    QCOMPARE(obj->implicitHeight(), qCeil(ch) + obj->topPadding() + obj->bottomPadding());

    obj->setLeftPadding(3.75);
    QCOMPARE(obj->leftPadding(), 3.75);
    QCOMPARE(obj->implicitWidth(), qCeil(cw) + obj->leftPadding() + obj->rightPadding());

    obj->setRightPadding(4.4);
    QCOMPARE(obj->rightPadding(), 4.4);
    QCOMPARE(obj->implicitWidth(), qCeil(cw) + obj->leftPadding() + obj->rightPadding());

    obj->setBottomPadding(1.11);
    QCOMPARE(obj->bottomPadding(), 1.11);
    QCOMPARE(obj->implicitHeight(), qCeil(ch) + obj->topPadding() + obj->bottomPadding());

    obj->setText("Qt");
    QVERIFY(obj->contentWidth() < cw);
    QCOMPARE(obj->contentHeight(), ch);
    cw = obj->contentWidth();

    QCOMPARE(obj->implicitWidth(), qCeil(cw) + obj->leftPadding() + obj->rightPadding());
    QCOMPARE(obj->implicitHeight(), qCeil(ch) + obj->topPadding() + obj->bottomPadding());

    obj->setFont(QFont("Courier", 96));
    QVERIFY(obj->contentWidth() > cw);
    QVERIFY(obj->contentHeight() > ch);
    cw = obj->contentWidth();
    ch = obj->contentHeight();

    QCOMPARE(obj->implicitWidth(), qCeil(cw) + obj->leftPadding() + obj->rightPadding());
    QCOMPARE(obj->implicitHeight(), qCeil(ch) + obj->topPadding() + obj->bottomPadding());

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

void tst_qquicktextinput::QTBUG_51115_readOnlyResetsSelection()
{
    QQuickView view;
    view.setSource(testFileUrl("qtbug51115.qml"));
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QQuickTextInput *obj = qobject_cast<QQuickTextInput*>(view.rootObject());

    QCOMPARE(obj->selectedText(), QString());
}

QTEST_MAIN(tst_qquicktextinput)

#include "tst_qquicktextinput.moc"
