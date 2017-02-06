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

#include "inputengine.h"
#include "inputcontext.h"
#include "defaultinputmethod.h"
#include "trace.h"
#include "virtualkeyboarddebug.h"

#include <QTimerEvent>
#include <QtCore/private/qobject_p.h>

namespace QtVirtualKeyboard {

class InputEnginePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(InputEngine)

public:
    InputEnginePrivate(InputEngine *q_ptr) :
        QObjectPrivate(),
        q_ptr(q_ptr),
        inputContext(0),
        defaultInputMethod(0),
        textCase(InputEngine::Lower),
        inputMode(InputEngine::Latin),
        activeKey(Qt::Key_unknown),
        activeKeyModifiers(Qt::NoModifier),
        previousKey(Qt::Key_unknown),
        repeatTimer(0),
        repeatCount(0),
        recursiveMethodLock(0)
    {
    }

    virtual ~InputEnginePrivate()
    {
    }

    bool virtualKeyClick(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers, bool isAutoRepeat)
    {
        Q_Q(InputEngine);
        bool accept = false;
        if (inputMethod) {
            accept = inputMethod->keyEvent(key, text, modifiers);
            if (!accept) {
                accept = defaultInputMethod->keyEvent(key, text, modifiers);
            }
            emit q->virtualKeyClicked(key, text, modifiers, isAutoRepeat);
        } else {
            qWarning() << "input method is not set";
        }
        return accept;
    }

    InputEngine* q_ptr;
    InputContext *inputContext;
    QPointer<AbstractInputMethod> inputMethod;
    AbstractInputMethod *defaultInputMethod;
    InputEngine::TextCase textCase;
    InputEngine::InputMode inputMode;
    QMap<SelectionListModel::Type, SelectionListModel *> selectionListModels;
    Qt::Key activeKey;
    QString activeKeyText;
    Qt::KeyboardModifiers activeKeyModifiers;
    Qt::Key previousKey;
    int repeatTimer;
    int repeatCount;
    int recursiveMethodLock;
};

class RecursiveMethodGuard
{
public:
    explicit RecursiveMethodGuard(int &ref) : m_ref(ref)
    {
        m_ref++;
    }
    ~RecursiveMethodGuard()
    {
        m_ref--;
    }
    bool locked() const
    {
        return m_ref > 1;
    }
private:
    int &m_ref;
};

/*!
    \qmltype InputEngine
    \inqmlmodule QtQuick.VirtualKeyboard
    \ingroup qtvirtualkeyboard-qml
    \instantiates QtVirtualKeyboard::InputEngine
    \brief Maps the user input to the input methods.

    The input engine is responsible for routing input events to input
    methods. The actual input logic is implemented by the input methods.

    The input engine also includes the default input method, which takes
    care of default processing if the active input method does not handle
    the event.
*/

/*!
    \class QtVirtualKeyboard::InputEngine
    \inmodule QtVirtualKeyboard
    \brief The InputEngine class provides an input engine
    that supports C++ and QML integration.

    \internal

    The input engine is responsible for routing input events to input
    methods. The actual input logic is implemented by the input methods.

    The input engine also includes the default input method, which takes
    care of default processing if the active input method does not handle
    the event.
*/

/*!
    \internal
    Constructs an input engine with input context as \a parent.
*/
InputEngine::InputEngine(InputContext *parent) :
    QObject(*new InputEnginePrivate(this), parent)
{
    Q_D(InputEngine);
    d->inputContext = parent;
    if (d->inputContext) {
        connect(d->inputContext, SIGNAL(shiftChanged()), SLOT(shiftChanged()));
        connect(d->inputContext, SIGNAL(localeChanged()), SLOT(update()));
    }
    d->defaultInputMethod = new DefaultInputMethod(this);
    if (d->defaultInputMethod)
        d->defaultInputMethod->setInputEngine(this);
    d->selectionListModels[SelectionListModel::WordCandidateList] = new SelectionListModel(this);
}

/*!
    \internal
    Destroys the input engine and frees all allocated resources.
*/
InputEngine::~InputEngine()
{
}

/*!
    \qmlmethod bool InputEngine::virtualKeyPress(int key, string text, int modifiers, bool repeat)

    Called by the keyboard layer to indicate that \a key was pressed, with
    the given \a text and \a modifiers.

    The \a key is set as an active key (down key). The actual key event is
    triggered when the key is released by the virtualKeyRelease() method. The
    key press event can be discarded by calling virtualKeyCancel().

    The key press also initiates the key repeat timer if \a repeat is \c true.

    Returns \c true if the key was accepted by this input engine.

    \sa virtualKeyCancel(), virtualKeyRelease()
*/
/*!
    Called by the keyboard layer to indicate that \a key was pressed, with
    the given \a text and \a modifiers.

    The \a key is set as an active key (down key). The actual key event is
    triggered when the key is released by the virtualKeyRelease() method. The
    key press event can be discarded by calling virtualKeyCancel().

    The key press also initiates the key repeat timer if \a repeat is \c true.

    Returns \c true if the key was accepted by this input engine.

    \sa virtualKeyCancel(), virtualKeyRelease()
*/
bool InputEngine::virtualKeyPress(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers, bool repeat)
{
    Q_D(InputEngine);
    VIRTUALKEYBOARD_DEBUG() << "InputEngine::virtualKeyPress():" << key << text << modifiers << repeat;
    bool accept = false;
    if (d->activeKey == Qt::Key_unknown || d->activeKey == key) {
        d->activeKey = key;
        d->activeKeyText = text;
        d->activeKeyModifiers = modifiers;
        if (repeat) {
            d->repeatTimer = startTimer(600);
        }
        accept = true;
        emit activeKeyChanged(d->activeKey);
    } else {
        qWarning("key press ignored; key is already active");
    }
    return accept;
}

/*!
    \qmlmethod void InputEngine::virtualKeyCancel()

    Reverts the active key state without emitting the key event.
    This method is useful when the user discards the current key and
    the key state needs to be restored.
*/
/*!
    \fn void QtVirtualKeyboard::InputEngine::virtualKeyCancel()

    Reverts the active key state without emitting the key event.
    This method is useful when the user discards the current key and
    the key state needs to be restored.
*/
void InputEngine::virtualKeyCancel()
{
    Q_D(InputEngine);
    VIRTUALKEYBOARD_DEBUG() << "InputEngine::virtualKeyCancel()";
    if (d->activeKey != Qt::Key_unknown) {
        d->activeKey = Qt::Key_unknown;
        d->activeKeyText = QString();
        d->activeKeyModifiers = Qt::KeyboardModifiers();
        if (d->repeatTimer) {
            killTimer(d->repeatTimer);
            d->repeatTimer = 0;
            d->repeatCount = 0;
        }
        emit activeKeyChanged(d->activeKey);
    }
}

/*!
    \qmlmethod bool InputEngine::virtualKeyRelease(int key, string text, int modifiers)

    Releases the key at \a key. The method emits a key event for the input
    method if the event has not been generated by a repeating timer.
    The \a text and \a modifiers are passed to the input method.

    Returns \c true if the key was accepted by the input engine.
*/
/*!
    Releases the key at \a key. The method emits a key event for the input
    method if the event has not been generated by a repeating timer.
    The \a text and \a modifiers are passed to the input method.

    Returns \c true if the key was accepted by the input engine.
*/
bool InputEngine::virtualKeyRelease(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    Q_D(InputEngine);
    VIRTUALKEYBOARD_DEBUG() << "InputEngine::virtualKeyRelease():" << key << text << modifiers;
    bool accept = false;
    if (d->activeKey == key) {
        if (!d->repeatCount) {
            accept = d->virtualKeyClick(key, text, modifiers, false);
        } else {
            accept = true;
        }
    } else {
        qWarning("key release ignored; key is not pressed");
    }
    if (d->activeKey != Qt::Key_unknown) {
        d->previousKey = d->activeKey;
        emit previousKeyChanged(d->previousKey);
        d->activeKey = Qt::Key_unknown;
        d->activeKeyText = QString();
        d->activeKeyModifiers = Qt::KeyboardModifiers();
        if (d->repeatTimer) {
            killTimer(d->repeatTimer);
            d->repeatTimer = 0;
            d->repeatCount = 0;
        }
        emit activeKeyChanged(d->activeKey);
    }
    return accept;
}

/*!
    \qmlmethod bool InputEngine::virtualKeyClick(int key, string text, int modifiers)

    Emits a key click event for the given \a key, \a text and \a modifiers.
    Returns \c true if the key event was accepted by the input engine.
*/
/*!
    Emits a key click event for the given \a key, \a text and \a modifiers.
    Returns \c true if the key event was accepted by the input engine.
*/
bool InputEngine::virtualKeyClick(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    Q_D(InputEngine);
    VIRTUALKEYBOARD_DEBUG() << "InputEngine::virtualKeyClick():" << key << text << modifiers;
    return d->virtualKeyClick(key, text, modifiers, false);
}

/*!
    Returns the \c InputContext instance associated with the input
    engine.
*/
InputContext *InputEngine::inputContext() const
{
    Q_D(const InputEngine);
    return d->inputContext;
}

/*!
    Returns the currently active key, or Qt::Key_unknown if no key is active.
*/
Qt::Key InputEngine::activeKey() const
{
    Q_D(const InputEngine);
    return d->activeKey;
}

/*!
    Returns the previously active key, or Qt::Key_unknown if no key has been
    active.
*/
Qt::Key InputEngine::previousKey() const
{
    Q_D(const InputEngine);
    return d->previousKey;
}

/*!
    Returns the active input method.
*/
AbstractInputMethod *InputEngine::inputMethod() const
{
    Q_D(const InputEngine);
    return d->inputMethod;
}

/*!
    Sets \a inputMethod as the active input method.
*/
void InputEngine::setInputMethod(AbstractInputMethod *inputMethod)
{
    Q_D(InputEngine);
    VIRTUALKEYBOARD_DEBUG() << "InputEngine::setInputMethod():" << inputMethod;
    if (d->inputMethod != inputMethod) {
        update();
        if (d->inputMethod) {
            d->inputMethod->setInputEngine(0);
        }
        d->inputMethod = inputMethod;
        if (d->inputMethod) {
            d->inputMethod->setInputEngine(this);

            // Set current text case
            d->inputMethod->setTextCase(d->textCase);

            // Allocate selection lists for the input method
            const QList<SelectionListModel::Type> activeSelectionLists = d->inputMethod->selectionLists();
            QList<SelectionListModel::Type> inactiveSelectionLists = d->selectionListModels.keys();
            for (const SelectionListModel::Type &selectionListType : activeSelectionLists) {
                auto it = d->selectionListModels.find(selectionListType);
                if (it == d->selectionListModels.end()) {
                    it = d->selectionListModels.insert(selectionListType, new SelectionListModel(this));
                    if (selectionListType == SelectionListModel::WordCandidateList) {
                        emit wordCandidateListModelChanged();
                    }
                }
                it.value()->setDataSource(inputMethod, selectionListType);
                if (selectionListType == SelectionListModel::WordCandidateList) {
                    emit wordCandidateListVisibleHintChanged();
                }
                inactiveSelectionLists.removeAll(selectionListType);
            }

            // Deallocate inactive selection lists
            for (const SelectionListModel::Type &selectionListType : qAsConst(inactiveSelectionLists)) {
                const auto it = d->selectionListModels.constFind(selectionListType);
                if (it != d->selectionListModels.cend()) {
                    it.value()->setDataSource(0, selectionListType);
                    if (selectionListType == SelectionListModel::WordCandidateList) {
                        emit wordCandidateListVisibleHintChanged();
                    }
                }
            }
        }
        emit inputMethodChanged();
        emit inputModesChanged();
        emit patternRecognitionModesChanged();
    }
}

/*!
    Returns the list of available input modes.
*/
QList<int> InputEngine::inputModes() const
{
    Q_D(const InputEngine);
    QList<InputMode> inputModeList;
    if (d->inputMethod) {
        inputModeList = d->inputMethod->inputModes(d->inputContext->locale());
    }
    QList<int> resultList;
    if (inputModeList.isEmpty()) {
        return resultList;
    }
    resultList.reserve(inputModeList.size());
    for (const InputMode &inputMode : qAsConst(inputModeList))
        resultList.append(inputMode);
    return resultList;
}

InputEngine::InputMode InputEngine::inputMode() const
{
    Q_D(const InputEngine);
    return d->inputMode;
}

void InputEngine::setInputMode(InputEngine::InputMode inputMode)
{
    Q_D(InputEngine);
    VIRTUALKEYBOARD_DEBUG() << "InputEngine::setInputMode():" << inputMode;
    if (d->inputMethod) {
        const QString locale(d->inputContext->locale());
        QList<InputEngine::InputMode> inputModeList(d->inputMethod->inputModes(locale));
        if (inputModeList.contains(inputMode)) {
            d->inputMethod->setInputMode(locale, inputMode);
            if (d->inputMode != inputMode) {
                d->inputMode = inputMode;
                emit inputModeChanged();
            }
        } else {
            qWarning() << "the input mode" << inputMode << "is not valid";
        }
    }
}

SelectionListModel *InputEngine::wordCandidateListModel() const
{
    Q_D(const InputEngine);
    return d->selectionListModels[SelectionListModel::WordCandidateList];
}

bool InputEngine::wordCandidateListVisibleHint() const
{
    Q_D(const InputEngine);
    const auto it = d->selectionListModels.constFind(SelectionListModel::WordCandidateList);
    if (it == d->selectionListModels.cend())
        return false;
    return it.value()->dataSource() != 0;
}

/*!
   Returns list of supported pattern recognition modes.
*/
QList<int> InputEngine::patternRecognitionModes() const
{
    Q_D(const InputEngine);
    QList<PatternRecognitionMode> patterRecognitionModeList;
    if (d->inputMethod)
        patterRecognitionModeList = d->inputMethod->patternRecognitionModes();
    QList<int> resultList;
    if (patterRecognitionModeList.isEmpty())
        return resultList;
    resultList.reserve(patterRecognitionModeList.size());
    for (const PatternRecognitionMode &patternRecognitionMode : qAsConst(patterRecognitionModeList))
        resultList.append(patternRecognitionMode);
    return resultList;
}

/*!
    \qmlmethod Trace InputEngine::traceBegin(int traceId, int patternRecognitionMode, var traceCaptureDeviceInfo, var traceScreenInfo)
    \since QtQuick.VirtualKeyboard 2.0

    Starts a trace interaction with the input engine.

    The trace is uniquely identified by the \a traceId. The input engine will assign
    the id to the Trace object if the input method accepts the event.

    The \a patternRecognitionMode specifies the recognition mode used for the pattern.

    If the current input method accepts the event it returns a Trace object associated with this interaction.
    If the input method discards the event, it returns a null value.

    The \a traceCaptureDeviceInfo provides information about the source device and the \a traceScreenInfo
    provides information about the screen context.

    By definition, the Trace object remains valid until the traceEnd() method is called.

    The trace interaction is ended by calling the \l {InputEngine::traceEnd()} {InputEngine.traceEnd()} method.
*/
/*!
    \since QtQuick.VirtualKeyboard 2.0

    Starts a trace interaction with the input engine.

    The trace is uniquely identified by the \a traceId. The input engine will assign
    the id to the Trace object if the input method accepts the event.

    The \a patternRecognitionMode specifies the recognition mode used for the pattern.

    If the current input method accepts the event it returns a Trace object associated with this interaction.
    If the input method discards the event, it returns a NULL value.

    The \a traceCaptureDeviceInfo provides information about the source device and the \a traceScreenInfo
    provides information about the screen context.

    By definition, the Trace object remains valid until the traceEnd() method is called.

    The trace interaction is ended by calling the traceEnd() method.
*/
QtVirtualKeyboard::Trace *InputEngine::traceBegin(int traceId, QtVirtualKeyboard::InputEngine::PatternRecognitionMode patternRecognitionMode,
                                                  const QVariantMap &traceCaptureDeviceInfo, const QVariantMap &traceScreenInfo)
{
    Q_D(InputEngine);
    VIRTUALKEYBOARD_DEBUG() << "InputEngine::traceBegin():"
                            << "traceId:" << traceId
                            << "patternRecognitionMode:" << patternRecognitionMode
                            << "traceCaptureDeviceInfo:" << traceCaptureDeviceInfo
                            << "traceScreenInfo:" << traceScreenInfo;
    if (!d->inputMethod)
        return 0;
    if (patternRecognitionMode == PatternRecognitionDisabled)
        return 0;
    if (!d->inputMethod->patternRecognitionModes().contains(patternRecognitionMode))
        return 0;
    Trace *trace = d->inputMethod->traceBegin(traceId, patternRecognitionMode, traceCaptureDeviceInfo, traceScreenInfo);
    if (trace)
        trace->setTraceId(traceId);
    return trace;
}

/*!
    \qmlmethod bool InputEngine::traceEnd(Trace trace)

    Ends the trace interaction with the input engine.

    The \a trace object may be discarded at any point after calling this function.

    The function returns true if the trace interaction was accepted (i.e. the touch
    events should not be used for anything else).
*/
/*!
    Ends the trace interaction with the input engine.

    The \a trace object may be discarded at any point after calling this function.

    The function returns true if the trace interaction was accepted (i.e. the touch
    events should not be used for anything else).
*/
bool InputEngine::traceEnd(QtVirtualKeyboard::Trace *trace)
{
    Q_D(InputEngine);
    VIRTUALKEYBOARD_DEBUG() << "InputEngine::traceEnd():" << trace;
    Q_ASSERT(trace);
    if (!d->inputMethod)
        return false;
    return d->inputMethod->traceEnd(trace);
}

/*!
    \since QtQuick.VirtualKeyboard 2.0

    This function attempts to reselect a word located at the \a cursorPosition.
    The \a reselectFlags define the rules for how the word should be selected in
    relation to the cursor position.

    The function returns \c true if the word was successfully reselected.
*/
bool InputEngine::reselect(int cursorPosition, const ReselectFlags &reselectFlags)
{
    Q_D(InputEngine);
    VIRTUALKEYBOARD_DEBUG() << "InputEngine::reselect():" << cursorPosition << reselectFlags;
    if (!d->inputMethod || !wordCandidateListVisibleHint())
        return false;
    return d->inputMethod->reselect(cursorPosition, reselectFlags);
}

/*!
    \internal
    Resets the input method.
*/
void InputEngine::reset()
{
    Q_D(InputEngine);
    if (d->inputMethod) {
        RecursiveMethodGuard guard(d->recursiveMethodLock);
        if (!guard.locked()) {
            emit inputMethodReset();
        }
    }
}

/*!
    \internal
    Updates the input method's state. This method is called whenever the input
    context is changed.
*/
void InputEngine::update()
{
    Q_D(InputEngine);
    if (d->inputMethod) {
        RecursiveMethodGuard guard(d->recursiveMethodLock);
        if (!guard.locked()) {
            emit inputMethodUpdate();
        }
    }
}

/*!
    \internal
    Updates the text case for the input method.
*/
void InputEngine::shiftChanged()
{
    Q_D(InputEngine);
    TextCase newCase = d->inputContext->shift() ? Upper : Lower;
    if (d->textCase != newCase) {
        d->textCase = newCase;
        if (d->inputMethod) {
            d->inputMethod->setTextCase(d->textCase);
        }
    }
}

/*!
    \internal
*/
void InputEngine::timerEvent(QTimerEvent *timerEvent)
{
    Q_D(InputEngine);
    if (timerEvent->timerId() == d->repeatTimer) {
        d->repeatTimer = 0;
        d->virtualKeyClick(d->activeKey, d->activeKeyText, d->activeKeyModifiers, true);
        d->repeatTimer = startTimer(50);
        d->repeatCount++;
    }
}

/*!
    \qmlproperty int InputEngine::activeKey

    Currently pressed key.
*/

/*!
    \property QtVirtualKeyboard::InputEngine::activeKey
    \brief the active key.

    Currently pressed key.
*/

/*!
    \qmlproperty int InputEngine::previousKey

    Previously pressed key.
*/
/*!
    \property QtVirtualKeyboard::InputEngine::previousKey
    \brief the previous active key.

    Previously pressed key.
*/

/*!
    \qmlproperty InputMethod InputEngine::inputMethod

    Use this property to set the active input method, or to monitor when the
    active input method changes.
*/

/*!
    \property QtVirtualKeyboard::InputEngine::inputMethod
    \brief the active input method.

    Use this property to set active the input method, or to monitor when the
    active input method changes.
*/

/*!
    \qmlproperty list<int> InputEngine::inputModes

    The list of available input modes is dependent on the input method and
    locale. This property is updated when either of the dependencies change.
*/

/*!
    \property QtVirtualKeyboard::InputEngine::inputModes
    \brief the available input modes for active input method.

    The list of available input modes is dependent on the input method and
    locale. This property is updated when either of the dependencies changes.
*/

/*!
    \qmlproperty int InputEngine::inputMode

    Use this property to get or set the current input mode. The
    InputEngine::inputModes property provides the list of valid input modes
    for the current input method and locale.

    The predefined input modes are:

    \list
        \li \c InputEngine.Latin The default input mode for latin text.
        \li \c InputEngine.Numeric Only numeric input is allowed.
        \li \c InputEngine.Dialable Only dialable input is allowed.
        \li \c InputEngine.Pinyin Pinyin input mode for Chinese.
        \li \c InputEngine.Cangjie Cangjie input mode for Chinese.
        \li \c InputEngine.Zhuyin Zhuyin input mode for Chinese.
        \li \c InputEngine.Hangul Hangul input mode for Korean.
        \li \c InputEngine.Hiragana Hiragana input mode for Japanese.
        \li \c InputEngine.Katakana Katakana input mode for Japanese.
        \li \c InputEngine.FullwidthLatin Fullwidth latin input mode for East Asian languages.
    \endlist
*/

/*!
    \property QtVirtualKeyboard::InputEngine::inputMode
    \brief the current input mode.

    Use this property to get or set the current input mode. The
    InputEngine::inputModes provides list of valid input modes
    for current input method and locale.
*/

/*!
    \qmlproperty SelectionListModel InputEngine::wordCandidateListModel

    Use this property to access the list model for the word candidate
    list.
*/

/*!
    \property QtVirtualKeyboard::InputEngine::wordCandidateListModel
    \brief list model for the word candidate list.

    Use this property to access the list model for the word candidate
    list.
*/

/*!
    \qmlproperty bool InputEngine::wordCandidateListVisibleHint

    Use this property to check if the word candidate list should be visible
    in the UI.
*/

/*!
    \property QtVirtualKeyboard::InputEngine::wordCandidateListVisibleHint
    \brief visible hint for the word candidate list.

    Use this property to check if the word candidate list should be visible
    in the UI.
*/

/*!
    \enum QtVirtualKeyboard::InputEngine::InputMode

    This enum specifies the input mode for the input method.

    \value Latin
           The default input mode for latin text.
    \value Numeric
           Only numeric input is allowed.
    \value Dialable
           Only dialable input is allowed.
    \value Pinyin
           Pinyin input mode for Chinese.
    \value Cangjie
           Cangjie input mode for Chinese.
    \value Zhuyin
           Zhuyin input mode for Chinese.
    \value Hangul
           Hangul input mode for Korean.
    \value Hiragana
           Hiragana input mode for Japanese.
    \value Katakana
           Katakana input mode for Japanese.
    \value FullwidthLatin
           Fullwidth latin input mode for East Asian languages.
*/

/*!
    \enum QtVirtualKeyboard::InputEngine::TextCase

    This enum specifies the text case for the input method.

    \value Lower
           Lower case text.
    \value Upper
           Upper case text.
*/

/*!
    \enum QtVirtualKeyboard::InputEngine::PatternRecognitionMode

    This enum specifies the input mode for the input method.

    \value PatternRecognitionDisabled
           Pattern recognition is not available.
    \value HandwritingRecoginition
           Pattern recognition mode for handwriting recognition.
*/

/*!
    \enum QtVirtualKeyboard::InputEngine::ReselectFlag

    This enum specifies the rules for word reselection.

    \value WordBeforeCursor
           Activate the word before the cursor. When this flag is used exclusively, the word must end exactly at the cursor.
    \value WordAfterCursor
           Activate the word after the cursor. When this flag is used exclusively, the word must start exactly at the cursor.
    \value WordAtCursor
           Activate the word at the cursor. This flag is a combination of the above flags with the exception that the word cannot start or stop at the cursor.
*/

/*!
    \qmlsignal void InputEngine::virtualKeyClicked(int key, string text, int modifiers)

    Indicates that the virtual \a key was clicked with the given \a text and
    \a modifiers.
    This signal is emitted after the input method has processed the key event.
*/

/*!
    \fn void QtVirtualKeyboard::InputEngine::virtualKeyClicked(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers, bool isAutoRepeat)

    Indicates that the virtual \a key was clicked with the given \a text and
    \a modifiers. The \a isAutoRepeat indicates if the event is automatically
    repeated while the key is being pressed.
    This signal is emitted after the input method has processed the key event.
*/

/*!
    \qmlproperty list<int> InputEngine::patternRecognitionModes
    \since QtQuick.VirtualKeyboard 2.0

    The list of available pattern recognition modes.
*/

/*!
    \property QtVirtualKeyboard::InputEngine::patternRecognitionModes
    \since QtQuick.VirtualKeyboard 2.0
    \brief the list of available pattern recognition modes.

    The list of available pattern recognition modes.
*/

/*!
    \qmlsignal void InputEngine::activeKeyChanged(int key)

    Indicates that the active \a key has changed.
*/

/*!
    \fn void QtVirtualKeyboard::InputEngine::activeKeyChanged(Qt::Key key)

    Indicates that the active \a key has changed.
*/

/*!
    \qmlsignal void InputEngine::previousKeyChanged(int key)

    Indicates that the previous \a key has changed.
*/

/*!
    \fn void QtVirtualKeyboard::InputEngine::previousKeyChanged(Qt::Key key)

    Indicates that the previous \a key has changed.
*/

/*!
    \qmlsignal void InputEngine::inputMethodChanged()

    Indicates that the input method has changed.
*/

/*!
    \fn void QtVirtualKeyboard::InputEngine::inputMethodChanged()

    Indicates that the input method has changed.
*/

/*!
    \qmlsignal void InputEngine::inputMethodReset()

    Emitted when the input method needs to be reset.

    \note This signal is automatically connected to AbstractInputMethod::reset()
    and InputMethod::reset() when the input method is activated.
*/

/*!
    \fn void QtVirtualKeyboard::InputEngine::inputMethodReset()

    Emitted when the input method needs to be reset.

    \note This signal is automatically connected to AbstractInputMethod::reset()
    and InputMethod::reset() when the input method is activated.
*/

/*!
    \qmlsignal void InputEngine::inputMethodUpdate()

    \note This signal is automatically connected to AbstractInputMethod::update()
    and InputMethod::update() when the input method is activated.
*/

/*!
    \fn void QtVirtualKeyboard::InputEngine::inputMethodUpdate()

    \note This signal is automatically connected to AbstractInputMethod::update()
    and InputMethod::update() when the input method is activated.
*/

/*!
    \qmlsignal void InputEngine::inputModesChanged()

    Indicates that the available input modes have changed.
*/

/*!
    \fn void QtVirtualKeyboard::InputEngine::inputModesChanged()

    Indicates that the available input modes have changed.
*/

/*!
    \qmlsignal void InputEngine::inputModeChanged()

    Indicates that the input mode has changed.
*/

/*!
    \fn void QtVirtualKeyboard::InputEngine::inputModeChanged()

    Indicates that the input mode has changed.
*/

/*!
    \qmlsignal void InputEngine::patternRecognitionModesChanged()
    \since QtQuick.VirtualKeyboard 2.0

    Indicates that the available pattern recognition modes have changed.

    The predefined pattern recognition modes are:

    \list
        \li \c InputEngine.PatternRecognitionDisabled Pattern recognition is not available.
        \li \c InputEngine.HandwritingRecoginition Pattern recognition mode for handwriting recognition.
    \endlist
*/

/*!
    \fn void QtVirtualKeyboard::InputEngine::patternRecognitionModesChanged()
    \since QtQuick.VirtualKeyboard 2.0

    Indicates that the available pattern recognition modes have changed.
*/

} // namespace QtVirtualKeyboard
