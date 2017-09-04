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

#include "pinyininputmethod.h"
#include "pinyindecoderservice.h"
#include "inputcontext.h"
#include "virtualkeyboarddebug.h"

namespace QtVirtualKeyboard {

class PinyinInputMethodPrivate : public AbstractInputMethodPrivate
{
    Q_DECLARE_PUBLIC(PinyinInputMethod)

public:
    enum State
    {
        Idle,
        Input,
        Predict
    };

    PinyinInputMethodPrivate(PinyinInputMethod *q_ptr) :
        q_ptr(q_ptr),
        inputMode(InputEngine::Pinyin),
        pinyinDecoderService(PinyinDecoderService::getInstance()),
        state(Idle),
        surface(),
        totalChoicesNum(0),
        candidatesList(),
        fixedLen(0),
        composingStr(),
        activeCmpsLen(0),
        finishSelection(true),
        posDelSpl(-1),
        isPosInSpl(false)
    {
    }

    void resetToIdleState()
    {
        Q_Q(PinyinInputMethod);

        InputContext *inputContext = q->inputContext();

        // Disable the user dictionary when entering sensitive data
        if (inputContext) {
            bool userDictionaryEnabled = !inputContext->inputMethodHints().testFlag(Qt::ImhSensitiveData);
            if (userDictionaryEnabled != pinyinDecoderService->isUserDictionaryEnabled())
                pinyinDecoderService->setUserDictionary(userDictionaryEnabled);
        }

        if (state == Idle)
            return;

        state = Idle;
        surface.clear();
        fixedLen = 0;
        finishSelection = true;
        composingStr.clear();
        if (inputContext)
            inputContext->setPreeditText("");
        activeCmpsLen = 0;
        posDelSpl = -1;
        isPosInSpl = false;

        resetCandidates();
    }

    bool addSpellingChar(QChar ch, bool reset)
    {
        if (reset) {
            surface.clear();
            pinyinDecoderService->resetSearch();
        }
        if (ch == Qt::Key_Apostrophe) {
            if (surface.isEmpty())
                return false;
            if (surface.endsWith(ch))
                return true;
        }
        surface.append(ch);
        return true;
    }

    bool removeSpellingChar()
    {
        if (surface.isEmpty())
            return false;
        QVector<int> splStart = pinyinDecoderService->spellingStartPositions();
        isPosInSpl = (surface.length() <= splStart[fixedLen + 1]);
        posDelSpl = isPosInSpl ? fixedLen - 1 : surface.length() - 1;
        return true;
    }

    void chooseAndUpdate(int candId)
    {
        Q_Q(PinyinInputMethod);

        if (state == Predict)
            choosePredictChoice(candId);
        else
            chooseDecodingCandidate(candId);

        if (composingStr.length() > 0) {
            if ((candId >= 0 || finishSelection) && composingStr.length() == fixedLen) {
                QString resultStr = getComposingStrActivePart();
                tryPredict();
                q->inputContext()->commit(resultStr);
            } else if (state == Idle) {
                state = Input;
            }
        } else {
            tryPredict();
        }
    }

    bool chooseAndFinish()
    {
        if (state == Predict || !totalChoicesNum)
            return false;

        chooseAndUpdate(0);
        if (state != Predict && totalChoicesNum > 0)
            chooseAndUpdate(0);

        return true;
    }

    int candidatesCount()
    {
        return totalChoicesNum;
    }

    QString candidateAt(int index)
    {
        if (index < 0 || index >= totalChoicesNum)
            return QString();
        if (index >= candidatesList.size()) {
            int fetchMore = qMin(index + 20, totalChoicesNum - candidatesList.size());
            candidatesList.append(pinyinDecoderService->fetchCandidates(candidatesList.size(), fetchMore, fixedLen));
            if (index == 0 && totalChoicesNum == 1) {
                int surfaceDecodedLen = pinyinDecoderService->pinyinStringLength(true);
                if (surfaceDecodedLen < surface.length())
                    candidatesList[0] = candidatesList[0] + surface.mid(surfaceDecodedLen).toLower();
            }
        }
        return index < candidatesList.size() ? candidatesList[index] : QString();
    }

    void chooseDecodingCandidate(int candId)
    {
        Q_Q(PinyinInputMethod);
        Q_ASSERT(state != Predict);

        int result = 0;
        if (candId < 0) {
            if (surface.length() > 0) {
                if (posDelSpl < 0) {
                    result = pinyinDecoderService->search(surface);
                } else {
                    result = pinyinDecoderService->deleteSearch(posDelSpl, isPosInSpl, false);
                    posDelSpl = -1;
                }
            }
        } else {
            if (totalChoicesNum > 1) {
                result = pinyinDecoderService->chooceCandidate(candId);
            } else {
                QString resultStr;
                if (totalChoicesNum == 1) {
                    QString undecodedStr = candId < candidatesList.length() ? candidatesList.at(candId) : QString();
                    resultStr = pinyinDecoderService->candidateAt(0).mid(0, fixedLen) + undecodedStr;
                }
                resetToIdleState();
                if (!resultStr.isEmpty())
                    q->inputContext()->commit(resultStr);
                return;
            }
        }

        resetCandidates();
        totalChoicesNum = result;

        surface = pinyinDecoderService->pinyinString(false);
        QVector<int> splStart = pinyinDecoderService->spellingStartPositions();
        QString fullSent = pinyinDecoderService->candidateAt(0);
        fixedLen = pinyinDecoderService->fixedLength();
        composingStr = fullSent.mid(0, fixedLen) + surface.mid(splStart[fixedLen + 1]);
        activeCmpsLen = composingStr.length();

        // Prepare the display string.
        QString composingStrDisplay;
        int surfaceDecodedLen = pinyinDecoderService->pinyinStringLength(true);
        if (!surfaceDecodedLen) {
            composingStrDisplay = composingStr.toLower();
            if (!totalChoicesNum)
                totalChoicesNum = 1;
        } else {
            activeCmpsLen = activeCmpsLen - (surface.length() - surfaceDecodedLen);
            composingStrDisplay = fullSent.mid(0, fixedLen);
            for (int pos = fixedLen + 1; pos < splStart.size() - 1; pos++) {
                composingStrDisplay += surface.mid(splStart[pos], splStart[pos + 1] - splStart[pos]).toUpper();
                if (splStart[pos + 1] < surfaceDecodedLen)
                    composingStrDisplay += " ";
            }
            if (surfaceDecodedLen < surface.length())
                composingStrDisplay += surface.mid(surfaceDecodedLen).toLower();
        }
        q->inputContext()->setPreeditText(composingStrDisplay);

        finishSelection = splStart.size() == (fixedLen + 2);
        if (!finishSelection)
            candidateAt(0);
    }

    void choosePredictChoice(int choiceId)
    {
        Q_ASSERT(state == Predict);

        if (choiceId < 0 || choiceId >= totalChoicesNum)
            return;

        QString tmp = candidatesList.at(choiceId);

        resetCandidates();

        candidatesList.append(tmp);
        totalChoicesNum = 1;

        surface.clear();
        fixedLen = tmp.length();
        composingStr = tmp;
        activeCmpsLen = fixedLen;

        finishSelection = true;
    }

    QString getComposingStrActivePart()
    {
        return composingStr.mid(0, activeCmpsLen);
    }

    void resetCandidates()
    {
        candidatesList.clear();
        if (totalChoicesNum) {
            totalChoicesNum = 0;
        }
    }

    void updateCandidateList()
    {
        Q_Q(PinyinInputMethod);
        emit q->selectionListChanged(SelectionListModel::WordCandidateList);
        emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList,
                                               totalChoicesNum > 0 && state == PinyinInputMethodPrivate::Input ? 0 : -1);
    }

    bool canDoPrediction()
    {
        Q_Q(PinyinInputMethod);
        InputContext *inputContext = q->inputContext();
        return inputMode == InputEngine::Pinyin &&
                composingStr.length() == fixedLen &&
                inputContext &&
                !inputContext->inputMethodHints().testFlag(Qt::ImhNoPredictiveText);
    }

    void tryPredict()
    {
        // Try to get the prediction list.
        if (canDoPrediction()) {
            Q_Q(PinyinInputMethod);
            if (state != Predict)
                resetToIdleState();
            InputContext *inputContext = q->inputContext();
            int cursorPosition = inputContext->cursorPosition();
            int historyStart = qMax(0, cursorPosition - 3);
            QString history = inputContext->surroundingText().mid(historyStart, cursorPosition - historyStart);
            candidatesList = pinyinDecoderService->predictionList(history);
            totalChoicesNum = candidatesList.size();
            finishSelection = false;
            state = Predict;
        } else {
            resetCandidates();
        }

        if (!candidatesCount())
            resetToIdleState();
    }

    PinyinInputMethod *q_ptr;
    InputEngine::InputMode inputMode;
    QPointer<PinyinDecoderService> pinyinDecoderService;
    State state;
    QString surface;
    int totalChoicesNum;
    QList<QString> candidatesList;
    int fixedLen;
    QString composingStr;
    int activeCmpsLen;
    bool finishSelection;
    int posDelSpl;
    bool isPosInSpl;
};

class ScopedCandidateListUpdate
{
    Q_DISABLE_COPY(ScopedCandidateListUpdate)
public:
    inline explicit ScopedCandidateListUpdate(PinyinInputMethodPrivate *d) :
        d(d),
        candidatesList(d->candidatesList),
        totalChoicesNum(d->totalChoicesNum),
        state(d->state)
    {
    }

    inline ~ScopedCandidateListUpdate()
    {
        if (totalChoicesNum != d->totalChoicesNum || state != d->state || candidatesList != d->candidatesList)
            d->updateCandidateList();
    }

private:
    PinyinInputMethodPrivate *d;
    QList<QString> candidatesList;
    int totalChoicesNum;
    PinyinInputMethodPrivate::State state;
};

/*!
    \class QtVirtualKeyboard::PinyinInputMethod
    \internal
*/

PinyinInputMethod::PinyinInputMethod(QObject *parent) :
    AbstractInputMethod(*new PinyinInputMethodPrivate(this), parent)
{
}

PinyinInputMethod::~PinyinInputMethod()
{
}

QList<InputEngine::InputMode> PinyinInputMethod::inputModes(const QString &locale)
{
    Q_UNUSED(locale)
    Q_D(PinyinInputMethod);
    QList<InputEngine::InputMode> result;
    if (d->pinyinDecoderService)
        result << InputEngine::Pinyin;
    result << InputEngine::Latin;
    return result;
}

bool PinyinInputMethod::setInputMode(const QString &locale, InputEngine::InputMode inputMode)
{
    Q_UNUSED(locale)
    Q_D(PinyinInputMethod);
    reset();
    if (inputMode == InputEngine::Pinyin && !d->pinyinDecoderService)
        return false;
    d->inputMode = inputMode;
    return true;
}

bool PinyinInputMethod::setTextCase(InputEngine::TextCase textCase)
{
    Q_UNUSED(textCase)
    return true;
}

bool PinyinInputMethod::keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    Q_D(PinyinInputMethod);
    if (d->inputMode == InputEngine::Pinyin) {
        ScopedCandidateListUpdate scopedCandidateListUpdate(d);
        Q_UNUSED(scopedCandidateListUpdate)
        if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key == Qt::Key_Apostrophe)) {
            if (d->state == PinyinInputMethodPrivate::Predict)
                d->resetToIdleState();
            if (d->addSpellingChar(text.at(0), d->state == PinyinInputMethodPrivate::Idle)) {
                d->chooseAndUpdate(-1);
                return true;
            }
        } else if (key == Qt::Key_Space) {
            if (d->state != PinyinInputMethodPrivate::Predict && d->candidatesCount() > 0) {
                d->chooseAndUpdate(0);
                return true;
            }
        } else if (key == Qt::Key_Return) {
            if (d->state != PinyinInputMethodPrivate::Predict && d->candidatesCount() > 0) {
                QString surface = d->surface;
                d->resetToIdleState();
                inputContext()->commit(surface);
                return true;
            }
        } else if (key == Qt::Key_Backspace) {
            if (d->removeSpellingChar()) {
                d->chooseAndUpdate(-1);
                return true;
            }
        } else if (!text.isEmpty()) {
            d->chooseAndFinish();
        }
    }
    return false;
}

QList<SelectionListModel::Type> PinyinInputMethod::selectionLists()
{
    return QList<SelectionListModel::Type>() << SelectionListModel::WordCandidateList;
}

int PinyinInputMethod::selectionListItemCount(SelectionListModel::Type type)
{
    Q_UNUSED(type)
    Q_D(PinyinInputMethod);
    return d->candidatesCount();
}

QVariant PinyinInputMethod::selectionListData(SelectionListModel::Type type, int index, int role)
{
    QVariant result;
    Q_UNUSED(type)
    Q_D(PinyinInputMethod);
    switch (role) {
    case SelectionListModel::DisplayRole:
        result = QVariant(d->candidateAt(index));
        break;
    case SelectionListModel::WordCompletionLengthRole:
        result.setValue(0);
        break;
    default:
        result = AbstractInputMethod::selectionListData(type, index, role);
        break;
    }
    return result;
}

void PinyinInputMethod::selectionListItemSelected(SelectionListModel::Type type, int index)
{
    Q_UNUSED(type)
    Q_D(PinyinInputMethod);
    ScopedCandidateListUpdate scopedCandidateListUpdate(d);
    Q_UNUSED(scopedCandidateListUpdate)
    d->chooseAndUpdate(index);
}

void PinyinInputMethod::reset()
{
    Q_D(PinyinInputMethod);
    ScopedCandidateListUpdate scopedCandidateListUpdate(d);
    Q_UNUSED(scopedCandidateListUpdate)
    d->resetToIdleState();
}

void PinyinInputMethod::update()
{
    Q_D(PinyinInputMethod);
    ScopedCandidateListUpdate scopedCandidateListUpdate(d);
    Q_UNUSED(scopedCandidateListUpdate)
    d->chooseAndFinish();
    d->tryPredict();
}

} // namespace QtVirtualKeyboard
