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

#include "hunspellinputmethod_p.h"
#include "inputcontext.h"

namespace QtVirtualKeyboard {

/*!
    \class QtVirtualKeyboard::HunspellInputMethod
    \internal
*/

HunspellInputMethod::HunspellInputMethod(HunspellInputMethodPrivate &dd, QObject *parent) :
    AbstractInputMethod(dd, parent)
{
}

HunspellInputMethod::HunspellInputMethod(QObject *parent) :
    AbstractInputMethod(*new HunspellInputMethodPrivate(this), parent)
{
}

HunspellInputMethod::~HunspellInputMethod()
{
}

QList<InputEngine::InputMode> HunspellInputMethod::inputModes(const QString &locale)
{
    Q_UNUSED(locale)
    return QList<InputEngine::InputMode>() << InputEngine::Latin << InputEngine::Numeric;
}

bool HunspellInputMethod::setInputMode(const QString &locale, InputEngine::InputMode inputMode)
{
    Q_UNUSED(inputMode)
    Q_D(HunspellInputMethod);
    return d->createHunspell(locale);
}

bool HunspellInputMethod::setTextCase(InputEngine::TextCase textCase)
{
    Q_UNUSED(textCase)
    return true;
}

bool HunspellInputMethod::keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    Q_D(HunspellInputMethod);
    InputContext *ic = inputContext();
    Qt::InputMethodHints inputMethodHints = ic->inputMethodHints();
    bool accept = false;
    switch (key) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Tab:
    case Qt::Key_Space:
        update();
        break;
    case Qt::Key_Backspace:
        if (!d->word.isEmpty()) {
            d->word.remove(d->word.length() - 1, 1);
            ic->setPreeditText(d->word);
            if (d->updateSuggestions()) {
                emit selectionListChanged(SelectionListModel::WordCandidateList);
                emit selectionListActiveItemChanged(SelectionListModel::WordCandidateList, d->activeWordIndex);
            }
            accept = true;
        }
        break;
    default:
        if (inputMethodHints.testFlag(Qt::ImhNoPredictiveText))
            break;
        if (text.length() > 0) {
            QChar c = text.at(0);
            bool addToWord = d->isValidInputChar(c) && (!d->word.isEmpty() || !d->isJoiner(c));
            if (addToWord) {
                /*  Automatic space insertion. */
                if (d->word.isEmpty()) {
                    QString surroundingText = ic->surroundingText();
                    int cursorPosition = ic->cursorPosition();
                    /*  Rules for automatic space insertion:
                        - Surrounding text is not empty
                        - Cursor is at the end of the line
                        - No space before the cursor
                        - No spefic characters before the cursor; minus and apostrophe
                    */
                    if (!surroundingText.isEmpty() && cursorPosition == surroundingText.length()) {
                        QChar lastChar = surroundingText.at(cursorPosition - 1);
                        if (!lastChar.isSpace() &&
                            lastChar != Qt::Key_Minus &&
                            d->isAutoSpaceAllowed()) {
                            ic->commit(" ");
                        }
                    }
                }
                /*  Ignore possible call to update() function when sending initial
                    pre-edit text. The update is triggered if the text editor has
                    a selection which the pre-edit text will replace.
                */
                d->ignoreUpdate = d->word.isEmpty();
                d->word.append(text);
                ic->setPreeditText(d->word);
                d->ignoreUpdate = false;
                if (d->updateSuggestions()) {
                    emit selectionListChanged(SelectionListModel::WordCandidateList);
                    emit selectionListActiveItemChanged(SelectionListModel::WordCandidateList, d->activeWordIndex);
                }
                accept = true;
            } else if (text.length() > 1) {
                bool addSpace = !d->word.isEmpty() || d->autoSpaceAllowed;
                update();
                d->autoSpaceAllowed = true;
                if (addSpace && d->isAutoSpaceAllowed())
                    ic->commit(" ");
                ic->commit(text);
                d->autoSpaceAllowed = addSpace;
                accept = true;
            } else {
                update();
                inputContext()->sendKeyClick(key, text, modifiers);
                d->autoSpaceAllowed = true;
                accept = true;
            }
        }
        break;
    }
    return accept;
}

QList<SelectionListModel::Type> HunspellInputMethod::selectionLists()
{
    return QList<SelectionListModel::Type>() << SelectionListModel::WordCandidateList;
}

int HunspellInputMethod::selectionListItemCount(SelectionListModel::Type type)
{
    Q_UNUSED(type)
    Q_D(HunspellInputMethod);
    return d->wordCandidates.count();
}

QVariant HunspellInputMethod::selectionListData(SelectionListModel::Type type, int index, int role)
{
    QVariant result;
    Q_UNUSED(type)
    Q_D(HunspellInputMethod);
    switch (role) {
    case SelectionListModel::DisplayRole:
        result = QVariant(d->wordCandidates.at(index));
        break;
    case SelectionListModel::WordCompletionLengthRole:
    {
        const QString wordCandidate(d->wordCandidates.at(index));
        int wordCompletionLength = wordCandidate.length() - d->word.length();
        result.setValue((wordCompletionLength > 0 && wordCandidate.startsWith(d->word)) ? wordCompletionLength : 0);
        break;
    }
    default:
        result = AbstractInputMethod::selectionListData(type, index, role);
        break;
    }
    return result;
}

void HunspellInputMethod::selectionListItemSelected(SelectionListModel::Type type, int index)
{
    Q_UNUSED(type)
    Q_D(HunspellInputMethod);
    QString finalWord = d->wordCandidates.at(index);
    reset();
    inputContext()->commit(finalWord);
    d->autoSpaceAllowed = true;
}

bool HunspellInputMethod::reselect(int cursorPosition, const InputEngine::ReselectFlags &reselectFlags)
{
    Q_D(HunspellInputMethod);
    Q_ASSERT(d->word.isEmpty());

    InputContext *ic = inputContext();
    if (!ic)
        return false;

    const QString surroundingText = ic->surroundingText();
    int replaceFrom = 0;

    if (reselectFlags.testFlag(InputEngine::WordBeforeCursor)) {
        for (int i = cursorPosition - 1; i >= 0; --i) {
            QChar c = surroundingText.at(i);
            if (!d->isValidInputChar(c))
                break;
            d->word.insert(0, c);
            --replaceFrom;
        }

        while (replaceFrom < 0 && d->isJoiner(d->word.at(0))) {
            d->word.remove(0, 1);
            ++replaceFrom;
        }
    }

    if (reselectFlags.testFlag(InputEngine::WordAtCursor) && replaceFrom == 0) {
        d->word.clear();
        return false;
    }

    if (reselectFlags.testFlag(InputEngine::WordAfterCursor)) {
        for (int i = cursorPosition; i < surroundingText.length(); ++i) {
            QChar c = surroundingText.at(i);
            if (!d->isValidInputChar(c))
                break;
            d->word.append(c);
        }

        while (replaceFrom > -d->word.length()) {
            int lastPos = d->word.length() - 1;
            if (!d->isJoiner(d->word.at(lastPos)))
                break;
            d->word.remove(lastPos, 1);
        }
    }

    if (d->word.isEmpty())
        return false;

    if (reselectFlags.testFlag(InputEngine::WordAtCursor) && replaceFrom == -d->word.length()) {
        d->word.clear();
        return false;
    }

    if (d->isJoiner(d->word.at(0))) {
        d->word.clear();
        return false;
    }

    if (d->isJoiner(d->word.at(d->word.length() - 1))) {
        d->word.clear();
        return false;
    }

    ic->setPreeditText(d->word, QList<QInputMethodEvent::Attribute>(), replaceFrom, d->word.length());

    d->autoSpaceAllowed = false;
    if (d->updateSuggestions()) {
        emit selectionListChanged(SelectionListModel::WordCandidateList);
        emit selectionListActiveItemChanged(SelectionListModel::WordCandidateList, d->activeWordIndex);
    }

    return true;
}

void HunspellInputMethod::reset()
{
    Q_D(HunspellInputMethod);
    d->reset();
}

void HunspellInputMethod::update()
{
    Q_D(HunspellInputMethod);
    if (d->ignoreUpdate)
        return;
    if (!d->word.isEmpty()) {
        QString finalWord = d->hasSuggestions() ? d->wordCandidates.at(d->activeWordIndex) : d->word;
        d->reset();
        inputContext()->commit(finalWord);
    }
    d->autoSpaceAllowed = false;
}

void HunspellInputMethod::updateSuggestions(const QStringList &wordList, int activeWordIndex)
{
    Q_D(HunspellInputMethod);
    d->wordCandidates.clear();
    d->wordCandidates.append(wordList);
    // Make sure the exact match is up-to-date
    if (!d->word.isEmpty() && !d->wordCandidates.isEmpty() && d->wordCandidates.at(0) != d->word)
        d->wordCandidates.replace(0, d->word);
    d->activeWordIndex = activeWordIndex;
    emit selectionListChanged(SelectionListModel::WordCandidateList);
    emit selectionListActiveItemChanged(SelectionListModel::WordCandidateList, d->activeWordIndex);
}

} // namespace QtVirtualKeyboard
