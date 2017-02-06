/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "inputcontext.h"
#include "inputengine.h"
#include "shifthandler.h"
#include "platforminputcontext.h"
#include "virtualkeyboarddebug.h"
#include "enterkeyaction.h"
#include "settings.h"

#include <QTextFormat>
#include <QGuiApplication>
#include <QtCore/private/qobject_p.h>

#ifdef COMPILING_QML
#include <private/qqmlmetatype_p.h>
#endif

QT_BEGIN_NAMESPACE
bool operator==(const QInputMethodEvent::Attribute &attribute1, const QInputMethodEvent::Attribute &attribute2)
{
    return attribute1.start == attribute2.start &&
           attribute1.length == attribute2.length &&
           attribute1.type == attribute2.type &&
           attribute1.value == attribute2.value;
}
QT_END_NAMESPACE

/*!
    \namespace QtVirtualKeyboard
    \inmodule QtVirtualKeyboard

    \brief Namespace for the Qt Virtual Keyboard C++ API.
    \internal
*/

namespace QtVirtualKeyboard {

class InputContextPrivate : public QObjectPrivate
{
public:
    enum StateFlag {
        ReselectEventState = 0x1,
        InputMethodEventState = 0x2,
        KeyEventState = 0x4,
        InputMethodClickState = 0x8
    };
    Q_DECLARE_FLAGS(StateFlags, StateFlag)

    InputContextPrivate() :
        QObjectPrivate(),
        inputContext(0),
        inputEngine(0),
        shiftHandler(0),
        keyboardRect(),
        previewRect(),
        previewVisible(false),
        animating(false),
        focus(false),
        shift(false),
        capsLock(false),
        cursorPosition(0),
        forceCursorPosition(-1),
        inputMethodHints(Qt::ImhNone),
        preeditText(),
        preeditTextAttributes(),
        surroundingText(),
        selectedText(),
        anchorRectangle(),
        cursorRectangle(),
        selectionControlVisible(false),
        anchorRectIntersectsClipRect(false),
        cursorRectIntersectsClipRect(false)
#ifdef QT_VIRTUALKEYBOARD_ARROW_KEY_NAVIGATION
        , activeNavigationKeys()
#endif
    {
    }

    PlatformInputContext *inputContext;
    InputEngine *inputEngine;
    ShiftHandler *shiftHandler;
    QRectF keyboardRect;
    QRectF previewRect;
    bool previewVisible;
    bool animating;
    bool focus;
    bool shift;
    bool capsLock;
    StateFlags stateFlags;
    int cursorPosition;
    int forceCursorPosition;
    Qt::InputMethodHints inputMethodHints;
    QString preeditText;
    QList<QInputMethodEvent::Attribute> preeditTextAttributes;
    QString surroundingText;
    QString selectedText;
    QRectF anchorRectangle;
    QRectF cursorRectangle;
    bool selectionControlVisible;
    bool anchorRectIntersectsClipRect;
    bool cursorRectIntersectsClipRect;
#ifdef QT_VIRTUALKEYBOARD_ARROW_KEY_NAVIGATION
    QSet<int> activeNavigationKeys;
#endif
    QSet<quint32> activeKeys;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(InputContextPrivate::StateFlags)

/*!
    \qmltype InputContext
    \instantiates QtVirtualKeyboard::InputContext
    \inqmlmodule QtQuick.VirtualKeyboard
    \ingroup qtvirtualkeyboard-qml
    \brief Provides access to an input context.

    The InputContext can be accessed as singleton instance.
*/

/*!
    \class QtVirtualKeyboard::InputContext
    \inmodule QtVirtualKeyboard
    \brief Provides access to an input context.
    \internal
*/

/*!
    \internal
    Constructs an input context with \a parent as the platform input
    context.
*/
InputContext::InputContext(PlatformInputContext *parent) :
    QObject(*new InputContextPrivate(), parent)
{
    Q_D(InputContext);
    d->inputContext = parent;
    if (d->inputContext) {
        d->inputContext->setInputContext(this);
        connect(d->inputContext, SIGNAL(focusObjectChanged()), SLOT(onInputItemChanged()));
        connect(d->inputContext, SIGNAL(focusObjectChanged()), SIGNAL(inputItemChanged()));
    }
    d->inputEngine = new InputEngine(this);
    d->shiftHandler = new ShiftHandler(this);
}

/*!
    \internal
    Destroys the input context and frees all allocated resources.
*/
InputContext::~InputContext()
{
}

bool InputContext::focus() const
{
    Q_D(const InputContext);
    return d->focus;
}

bool InputContext::shift() const
{
    Q_D(const InputContext);
    return d->shift;
}

void InputContext::setShift(bool enable)
{
    Q_D(InputContext);
    if (d->shift != enable) {
        d->shift = enable;
        emit shiftChanged();
    }
}

bool InputContext::capsLock() const
{
    Q_D(const InputContext);
    return d->capsLock;
}

void InputContext::setCapsLock(bool enable)
{
    Q_D(InputContext);
    if (d->capsLock != enable) {
        d->capsLock = enable;
        emit capsLockChanged();
    }
}

int InputContext::cursorPosition() const
{
    Q_D(const InputContext);
    return d->cursorPosition;
}

Qt::InputMethodHints InputContext::inputMethodHints() const
{
    Q_D(const InputContext);
    return d->inputMethodHints;
}

QString InputContext::preeditText() const
{
    Q_D(const InputContext);
    return d->preeditText;
}

void InputContext::setPreeditText(const QString &text, QList<QInputMethodEvent::Attribute> attributes, int replaceFrom, int replaceLength)
{
    // Add default attributes
    if (!text.isEmpty()) {
        bool containsTextFormat = false;
        for (QList<QInputMethodEvent::Attribute>::ConstIterator attribute = attributes.constBegin();
             attribute != attributes.constEnd(); attribute++) {
            if (attribute->type == QInputMethodEvent::TextFormat) {
                containsTextFormat = true;
                break;
            }
        }
        if (!containsTextFormat) {
            QTextCharFormat textFormat;
            textFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);
            attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, 0, text.length(), textFormat));
        }
    } else {
        Q_D(InputContext);
        if (d->forceCursorPosition != -1)
            attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::Selection, d->forceCursorPosition, 0, QVariant()));
        d->forceCursorPosition = -1;
    }

    sendPreedit(text, attributes, replaceFrom, replaceLength);
}

QString InputContext::surroundingText() const
{
    Q_D(const InputContext);
    return d->surroundingText;
}

QString InputContext::selectedText() const
{
    Q_D(const InputContext);
    return d->selectedText;
}

QRectF InputContext::anchorRectangle() const
{
    Q_D(const InputContext);
    return d->anchorRectangle;
}

QRectF InputContext::cursorRectangle() const
{
    Q_D(const InputContext);
    return d->cursorRectangle;
}

QRectF InputContext::keyboardRectangle() const
{
    Q_D(const InputContext);
    return d->keyboardRect;
}

void InputContext::setKeyboardRectangle(QRectF rectangle)
{
    Q_D(InputContext);
    if (d->keyboardRect != rectangle) {
        d->keyboardRect = rectangle;
        emit keyboardRectangleChanged();
        d->inputContext->emitKeyboardRectChanged();
    }
}

QRectF InputContext::previewRectangle() const
{
    Q_D(const InputContext);
    return d->previewRect;
}

void InputContext::setPreviewRectangle(QRectF rectangle)
{
    Q_D(InputContext);
    if (d->previewRect != rectangle) {
        d->previewRect = rectangle;
        emit previewRectangleChanged();
    }
}

bool InputContext::previewVisible() const
{
    Q_D(const InputContext);
    return d->previewVisible;
}

void InputContext::setPreviewVisible(bool visible)
{
    Q_D(InputContext);
    if (d->previewVisible != visible) {
        d->previewVisible = visible;
        emit previewVisibleChanged();
    }
}

bool InputContext::animating() const
{
    Q_D(const InputContext);
    return d->animating;
}

void InputContext::setAnimating(bool animating)
{
    Q_D(InputContext);
    if (d->animating != animating) {
        d->animating = animating;
        emit animatingChanged();
        d->inputContext->emitAnimatingChanged();
    }
}


QString InputContext::locale() const
{
    Q_D(const InputContext);
    return d->inputContext->locale().name();
}

void InputContext::setLocale(const QString &locale)
{
    Q_D(InputContext);
    VIRTUALKEYBOARD_DEBUG() << "InputContext::setLocale():" << locale;
    QLocale newLocale(locale);
    if (newLocale != d->inputContext->locale()) {
        d->inputContext->setLocale(newLocale);
        d->inputContext->setInputDirection(newLocale.textDirection());
        emit localeChanged();
    }
}

/*!
    \internal
*/
void InputContext::updateAvailableLocales(const QStringList &availableLocales)
{
    Settings *settings = Settings::instance();
    if (settings)
        settings->setAvailableLocales(availableLocales);
}

QObject *InputContext::inputItem() const
{
    Q_D(const InputContext);
    return d->inputContext ? d->inputContext->focusObject() : 0;
}

ShiftHandler *InputContext::shiftHandler() const
{
    Q_D(const InputContext);
    return d->shiftHandler;
}

InputEngine *InputContext::inputEngine() const
{
    Q_D(const InputContext);
    return d->inputEngine;
}

/*!
    \qmlmethod void InputContext::hideInputPanel()

    This method hides the input panel. This method should only be called
    when the user initiates the hide, e.g. by pressing a dedicated button
    on the keyboard.
*/
/*!
    \fn void QtVirtualKeyboard::InputContext::hideInputPanel()

    This method hides the input panel. This method should only be called
    when the user initiates the hide, e.g. by pressing a dedicated button
    on the keyboard.
*/
void InputContext::hideInputPanel()
{
    Q_D(InputContext);
    d->inputContext->hideInputPanel();
}

/*!
    \qmlmethod void InputContext::sendKeyClick(int key, string text, int modifiers = 0)

    Sends a key click event with the given \a key, \a text and \a modifiers to
    the input item that currently has focus.
*/
/*!
    Sends a key click event with the given \a key, \a text and \a modifiers to
    the input item that currently has focus.
*/
void InputContext::sendKeyClick(int key, const QString &text, int modifiers)
{
    Q_D(InputContext);
    if (d->focus && d->inputContext) {
        QKeyEvent pressEvent(QEvent::KeyPress, key, Qt::KeyboardModifiers(modifiers), text);
        QKeyEvent releaseEvent(QEvent::KeyRelease, key, Qt::KeyboardModifiers(modifiers), text);
        VIRTUALKEYBOARD_DEBUG() << "InputContext::::sendKeyClick():" << key;

        d->stateFlags |= InputContextPrivate::KeyEventState;
        d->inputContext->sendKeyEvent(&pressEvent);
        d->inputContext->sendKeyEvent(&releaseEvent);
        if (d->activeKeys.isEmpty())
            d->stateFlags &= ~InputContextPrivate::KeyEventState;
    } else {
        qWarning() << "InputContext::::sendKeyClick():" << key << "no focus";
    }
}

/*!
    \qmlmethod void InputContext::commit()

    Commits the current pre-edit text.
*/
/*!
    \fn void QtVirtualKeyboard::InputContext::commit()

    Commits the current pre-edit text.
*/
void InputContext::commit()
{
    Q_D(InputContext);
    QString text = d->preeditText;
    commit(text);
}

/*!
    \qmlmethod void InputContext::commit(string text, int replaceFrom = 0, int replaceLength = 0)

    Commits the final \a text to the input item and optionally
    modifies the text relative to the start of the pre-edit text.
    If \a replaceFrom is non-zero, the \a text replaces the
    contents relative to \a replaceFrom with a length of
    \a replaceLength.
*/
/*!
    Commits the final \a text to the input item and optionally
    modifies the text relative to the start of the pre-edit text.
    If \a replaceFrom is non-zero, the \a text replaces the
    contents relative to \a replaceFrom with a length of
    \a replaceLength.
*/
void InputContext::commit(const QString &text, int replaceFrom, int replaceLength)
{
    Q_D(InputContext);
    VIRTUALKEYBOARD_DEBUG() << "InputContext::commit():" << text << replaceFrom << replaceLength;
    bool preeditChanged = !d->preeditText.isEmpty();
    d->preeditText.clear();

    if (d->inputContext) {
        QList<QInputMethodEvent::Attribute> attributes;
        if (d->forceCursorPosition != -1) {
            attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::Selection, d->forceCursorPosition, 0, QVariant()));
            d->forceCursorPosition = -1;
        }
        QInputMethodEvent inputEvent(QString(), attributes);
        inputEvent.setCommitString(text, replaceFrom, replaceLength);
        d->stateFlags |= InputContextPrivate::InputMethodEventState;
        d->inputContext->sendEvent(&inputEvent);
        d->stateFlags &= ~InputContextPrivate::InputMethodEventState;
    }

    if (preeditChanged)
        emit preeditTextChanged();
}

/*!
    \qmlmethod void InputContext::clear()

    Clears the pre-edit text.
*/
/*!
    \fn void QtVirtualKeyboard::InputContext::clear()

    Clears the pre-edit text.
*/
void InputContext::clear()
{
    Q_D(InputContext);
    bool preeditChanged = !d->preeditText.isEmpty();
    d->preeditText.clear();

    if (d->inputContext) {
        QList<QInputMethodEvent::Attribute> attributes;
        if (d->forceCursorPosition != -1) {
            attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::Selection, d->forceCursorPosition, 0, QVariant()));
            d->forceCursorPosition = -1;
        }
        QInputMethodEvent event(QString(), attributes);
        d->stateFlags |= InputContextPrivate::InputMethodEventState;
        d->inputContext->sendEvent(&event);
        d->stateFlags &= ~InputContextPrivate::InputMethodEventState;
    }

    if (preeditChanged)
        emit preeditTextChanged();
}

/*!
    \internal
*/
bool InputContext::fileExists(const QUrl &fileUrl)
{
#ifdef COMPILING_QML
    // workaround that qtquickcompiler removes *.qml file paths from qrc file (QTRD-3268)
    return QQmlMetaType::findCachedCompilationUnit(fileUrl);
#else
    QString fileName;
    if (fileUrl.scheme() == QLatin1String("qrc")) {
        fileName = QLatin1Char(':') + fileUrl.path();
    } else {
        fileName = fileUrl.toLocalFile();
    }
    return QFile::exists(fileName);
#endif
}

/*!
    \internal
*/
bool InputContext::hasEnterKeyAction(QObject *item) const
{
    return item != 0 && qmlAttachedPropertiesObject<EnterKeyAction>(item, false);
}

/*!
    \internal
*/
void InputContext::setSelectionOnFocusObject(const QPointF &anchorPos, const QPointF &cursorPos)
{
    QPlatformInputContext::setSelectionOnFocusObject(anchorPos, cursorPos);
}

bool InputContext::anchorRectIntersectsClipRect() const
{
    Q_D(const InputContext);
    return d->anchorRectIntersectsClipRect;
}

bool InputContext::cursorRectIntersectsClipRect() const
{
    Q_D(const InputContext);
    return d->cursorRectIntersectsClipRect;
}

bool InputContext::selectionControlVisible() const
{
    Q_D(const InputContext);
    return d->selectionControlVisible;
}

void InputContext::onInputItemChanged()
{
    Q_D(InputContext);
    if (!inputItem() && !d->activeKeys.isEmpty()) {
        // After losing keyboard focus it is impossible to track pressed keys
        d->activeKeys.clear();
        d->stateFlags &= ~InputContextPrivate::KeyEventState;
    }
    d->stateFlags &= ~InputContextPrivate::InputMethodClickState;
}

void InputContext::setFocus(bool enable)
{
    Q_D(InputContext);
    if (d->focus != enable) {
        VIRTUALKEYBOARD_DEBUG() << "InputContext::setFocus():" << enable;
        d->focus = enable;
        emit focusChanged();
    }
    emit focusEditorChanged();
}

void InputContext::sendPreedit(const QString &text, const QList<QInputMethodEvent::Attribute> &attributes, int replaceFrom, int replaceLength)
{
    Q_D(InputContext);
    VIRTUALKEYBOARD_DEBUG() << "InputContext::sendPreedit():" << text << replaceFrom << replaceLength;

    bool textChanged = d->preeditText != text;
    bool attributesChanged = d->preeditTextAttributes != attributes;

    if (textChanged || attributesChanged) {
        d->preeditText = text;
        d->preeditTextAttributes = attributes;

        if (d->inputContext) {
            QInputMethodEvent event(text, attributes);
            if (replaceFrom != 0 || replaceLength > 0)
                event.setCommitString(QString(), replaceFrom, replaceLength);
            d->stateFlags |= InputContextPrivate::InputMethodEventState;
            d->inputContext->sendEvent(&event);
            d->stateFlags &= ~InputContextPrivate::InputMethodEventState;
        }

        if (textChanged)
            emit preeditTextChanged();
    }
}

void InputContext::reset()
{
    Q_D(InputContext);
    d->inputEngine->reset();
}

void InputContext::externalCommit()
{
    Q_D(InputContext);
    d->inputEngine->update();
}

void InputContext::update(Qt::InputMethodQueries queries)
{
    Q_D(InputContext);
    Q_UNUSED(queries);

    // fetch
    Qt::InputMethodHints inputMethodHints = Qt::InputMethodHints(d->inputContext->inputMethodQuery(Qt::ImHints).toInt());
    const int cursorPosition = d->inputContext->inputMethodQuery(Qt::ImCursorPosition).toInt();
    const int anchorPosition = d->inputContext->inputMethodQuery(Qt::ImAnchorPosition).toInt();
    QRectF anchorRectangle = qApp->inputMethod()->anchorRectangle();
    QRectF cursorRectangle = qApp->inputMethod()->cursorRectangle();
    QString surroundingText = d->inputContext->inputMethodQuery(Qt::ImSurroundingText).toString();
    QString selectedText = d->inputContext->inputMethodQuery(Qt::ImCurrentSelection).toString();

    // check against changes
    bool newInputMethodHints = inputMethodHints != d->inputMethodHints;
    bool newSurroundingText = surroundingText != d->surroundingText;
    bool newSelectedText = selectedText != d->selectedText;
    bool newCursorPosition = cursorPosition != d->cursorPosition;
    bool newAnchorRectangle = anchorRectangle != d->anchorRectangle;
    bool newCursorRectangle = cursorRectangle != d->cursorRectangle;
    bool selectionControlVisible = d->inputContext->isInputPanelVisible() && (cursorPosition != anchorPosition);
    bool newSelectionControlVisible = selectionControlVisible != d->selectionControlVisible;

    QRectF inputItemClipRect = d->inputContext->inputMethodQuery(Qt::ImInputItemClipRectangle).toRectF();
    QRectF anchorRect = d->inputContext->inputMethodQuery(Qt::ImAnchorRectangle).toRectF();
    QRectF cursorRect = d->inputContext->inputMethodQuery(Qt::ImCursorRectangle).toRectF();

    bool anchorRectIntersectsClipRect = inputItemClipRect.intersects(anchorRect);
    bool newAnchorRectIntersectsClipRect = anchorRectIntersectsClipRect != d->anchorRectIntersectsClipRect;

    bool cursorRectIntersectsClipRect = inputItemClipRect.intersects(cursorRect);
    bool newCursorRectIntersectsClipRect = cursorRectIntersectsClipRect != d->cursorRectIntersectsClipRect;

    // update
    d->inputMethodHints = inputMethodHints;
    d->surroundingText = surroundingText;
    d->selectedText = selectedText;
    d->cursorPosition = cursorPosition;
    d->anchorRectangle = anchorRectangle;
    d->cursorRectangle = cursorRectangle;
    d->selectionControlVisible = selectionControlVisible;
    d->anchorRectIntersectsClipRect = anchorRectIntersectsClipRect;
    d->cursorRectIntersectsClipRect = cursorRectIntersectsClipRect;

    // update input engine
    if ((newSurroundingText || newCursorPosition) &&
            !d->stateFlags.testFlag(InputContextPrivate::InputMethodEventState)) {
        d->inputEngine->update();
    }
    if (newInputMethodHints) {
        d->inputEngine->reset();
    }

    // notify
    if (newInputMethodHints) {
        emit inputMethodHintsChanged();
    }
    if (newSurroundingText) {
        emit surroundingTextChanged();
    }
    if (newSelectedText) {
        emit selectedTextChanged();
    }
    if (newCursorPosition) {
        emit cursorPositionChanged();
    }
    if (newAnchorRectangle) {
        emit anchorRectangleChanged();
    }
    if (newCursorRectangle) {
        emit cursorRectangleChanged();
    }
    if (newSelectionControlVisible) {
        emit selectionControlVisibleChanged();
    }
    if (newAnchorRectIntersectsClipRect) {
        emit anchorRectIntersectsClipRectChanged();
    }
    if (newCursorRectIntersectsClipRect) {
        emit cursorRectIntersectsClipRectChanged();
    }

    // word reselection
    if (newInputMethodHints || newSurroundingText || newSelectedText)
        d->stateFlags &= ~InputContextPrivate::InputMethodClickState;
    if ((newSurroundingText || newCursorPosition) && !newSelectedText && (int)d->stateFlags == 0 &&
            !d->inputMethodHints.testFlag(Qt::ImhNoPredictiveText) &&
            d->cursorPosition > 0 && d->selectedText.isEmpty()) {
        d->stateFlags |= InputContextPrivate::ReselectEventState;
        if (d->inputEngine->reselect(d->cursorPosition, InputEngine::WordAtCursor))
            d->stateFlags |= InputContextPrivate::InputMethodClickState;
        d->stateFlags &= ~InputContextPrivate::ReselectEventState;
    }
}

void InputContext::invokeAction(QInputMethod::Action action, int cursorPosition)
{
    Q_D(InputContext);
    switch (action) {
    case QInputMethod::Click:
        if ((int)d->stateFlags == 0) {
            bool reselect = !d->inputMethodHints.testFlag(Qt::ImhNoPredictiveText) && d->selectedText.isEmpty() && cursorPosition < d->preeditText.length();
            if (reselect) {
                d->stateFlags |= InputContextPrivate::ReselectEventState;
                d->forceCursorPosition = d->cursorPosition + cursorPosition;
                d->inputEngine->update();
                d->inputEngine->reselect(d->cursorPosition, InputEngine::WordBeforeCursor);
                d->stateFlags &= ~InputContextPrivate::ReselectEventState;
            } else if (!d->preeditText.isEmpty() && cursorPosition == d->preeditText.length()) {
                d->inputEngine->update();
            }
        }
        d->stateFlags &= ~InputContextPrivate::InputMethodClickState;
        break;

    case QInputMethod::ContextMenu:
        break;
    }
}

bool InputContext::filterEvent(const QEvent *event)
{
    QEvent::Type type = event->type();
    if (type == QEvent::KeyPress || type == QEvent::KeyRelease) {
        Q_D(InputContext);
        const QKeyEvent *keyEvent = static_cast<const QKeyEvent *>(event);

        // Keep track of pressed keys update key event state
        if (type == QEvent::KeyPress)
            d->activeKeys += keyEvent->nativeScanCode();
        else if (type == QEvent::KeyRelease)
            d->activeKeys -= keyEvent->nativeScanCode();

        if (d->activeKeys.isEmpty())
            d->stateFlags &= ~InputContextPrivate::KeyEventState;
        else
            d->stateFlags |= InputContextPrivate::KeyEventState;

#ifdef QT_VIRTUALKEYBOARD_ARROW_KEY_NAVIGATION
        int key = keyEvent->key();
        if ((key >= Qt::Key_Left && key <= Qt::Key_Down) || key == Qt::Key_Return) {
            if (type == QEvent::KeyPress && d->inputContext->isInputPanelVisible()) {
                d->activeNavigationKeys += key;
                emit navigationKeyPressed(key, keyEvent->isAutoRepeat());
                return true;
            } else if (type == QEvent::KeyRelease && d->activeNavigationKeys.contains(key)) {
                d->activeNavigationKeys -= key;
                emit navigationKeyReleased(key, keyEvent->isAutoRepeat());
                return true;
            }
        }
#endif

        // Break composing text since the virtual keyboard does not support hard keyboard events
        if (!d->preeditText.isEmpty())
            d->inputEngine->update();
    }
    return false;
}

/*!
    \qmlproperty bool InputContext::focus

    This property is changed when the input method receives or loses focus.
*/

/*!
    \property QtVirtualKeyboard::InputContext::focus
    \brief the focus status.

    This property is changed when the input method receives or loses focus.
*/

/*!
    \qmlproperty bool InputContext::shift

    This property is changed when the shift status changes.
*/

/*!
    \property QtVirtualKeyboard::InputContext::shift
    \brief the shift status.

    This property is changed when the shift status changes.
*/

/*!
    \qmlproperty bool InputContext::capsLock

    This property is changed when the caps lock status changes.
*/

/*!
    \property QtVirtualKeyboard::InputContext::capsLock
    \brief the caps lock status.

    This property is changed when the caps lock status changes.
*/

/*!
    \qmlproperty int InputContext::cursorPosition

    This property is changed when the cursor position changes.
*/

/*!
    \property QtVirtualKeyboard::InputContext::cursorPosition
    \brief the cursor position.

    This property is changed when the cursor position changes.
*/

/*!
    \qmlproperty int InputContext::inputMethodHints

    This property is changed when the input method hints changes.
*/

/*!
    \property QtVirtualKeyboard::InputContext::inputMethodHints
    \brief the input method hints.

    This property is changed when the input method hints changes.
*/

/*!
    \qmlproperty string InputContext::preeditText

    This property sets the pre-edit text.
*/

/*!
    \property QtVirtualKeyboard::InputContext::preeditText
    \brief the pre-edit text.

    This property sets the pre-edit text.
*/

/*!
    \qmlproperty string InputContext::surroundingText

    This property is changed when the surrounding text around the cursor changes.
*/

/*!
    \property QtVirtualKeyboard::InputContext::surroundingText
    \brief the surrounding text around cursor.

    This property is changed when the surrounding text around the cursor changes.
*/

/*!
    \qmlproperty string InputContext::selectedText

    This property is changed when the selected text changes.
*/

/*!
    \property QtVirtualKeyboard::InputContext::selectedText
    \brief the selected text.

    This property is changed when the selected text changes.
*/

/*!
    \qmlproperty rect InputContext::anchorRectangle
    \since QtQuick.VirtualKeyboard 2.1

    This property is changed when the anchor rectangle changes.
*/

/*!
    \property QtVirtualKeyboard::InputContext::anchorRectangle
    \brief the anchor rectangle.

    This property is changed when the anchor rectangle changes.
*/

/*!
    \qmlproperty rect InputContext::cursorRectangle

    This property is changed when the cursor rectangle changes.
*/

/*!
    \property QtVirtualKeyboard::InputContext::cursorRectangle
    \brief the cursor rectangle.

    This property is changed when the cursor rectangle changes.
*/

/*!
    \qmlproperty rect InputContext::keyboardRectangle

    Use this property to set the keyboard rectangle.
*/

/*!
    \property QtVirtualKeyboard::InputContext::keyboardRectangle
    \brief the keyboard rectangle.

    Use this property to set the keyboard rectangle.
*/

/*!
    \qmlproperty rect InputContext::previewRectangle

    Use this property to set the preview rectangle.
*/

/*!
    \property QtVirtualKeyboard::InputContext::previewRectangle
    \brief the preview rectangle.

    Use this property to set the preview rectangle.
*/

/*!
    \qmlproperty bool InputContext::previewVisible

    Use this property to set the visibility status of the preview.
*/

/*!
    \property QtVirtualKeyboard::InputContext::previewVisible
    \brief the animating status.

    Use this property to set the visibility status of the preview.
*/

/*!
    \qmlproperty bool InputContext::animating

    Use this property to set the animating status, for example
    during UI transitioning states.
*/

/*!
    \property QtVirtualKeyboard::InputContext::animating
    \brief the animating status.

    Use this property to set the animating status, for example
    during UI transitioning states.
*/

/*!
    \qmlproperty string InputContext::locale

    Sets the locale for this input context.
*/

/*!
    \property QtVirtualKeyboard::InputContext::locale
    \brief the locale.

    Sets the locale for this input context.
*/

/*!
    \qmlproperty QtObject InputContext::inputItem

    This property is changed when the focused input item changes.
*/

/*!
    \property QtVirtualKeyboard::InputContext::inputItem
    \brief the focused input item.

    This property is changed when the focused input item changes.
*/

/*!
    \qmlproperty ShiftHandler InputContext::shiftHandler

    This property stores the shift handler.
*/

/*!
    \property QtVirtualKeyboard::InputContext::shiftHandler
    \brief the shift handler instance.

    This property stores the shift handler.
*/

/*!
    \qmlproperty InputEngine InputContext::inputEngine

    This property stores the input engine.
*/

/*!
    \property QtVirtualKeyboard::InputContext::inputEngine
    \brief the input engine.

    This property stores the input engine.
*/

/*!
    \qmlsignal InputContext::focusEditorChanged()

    This signal is emitted when the focus editor changes.
*/

/*!
    \fn void QtVirtualKeyboard::InputContext::focusEditorChanged()

    This signal is emitted when the focus editor changes.
*/

/*!
    \fn void QtVirtualKeyboard::InputContext::navigationKeyPressed(int key, bool isAutoRepeat)
    \internal
*/

/*!
    \fn void QtVirtualKeyboard::InputContext::navigationKeyReleased(int key, bool isAutoRepeat)
    \internal
*/

} // namespace QtVirtualKeyboard
