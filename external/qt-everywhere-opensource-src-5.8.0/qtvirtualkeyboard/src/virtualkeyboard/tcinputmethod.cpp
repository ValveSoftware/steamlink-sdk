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

#include "tcinputmethod.h"
#include "inputengine.h"
#include "inputcontext.h"
#if defined(HAVE_TCIME_CANGJIE)
#include "cangjiedictionary.h"
#include "cangjietable.h"
#endif
#if defined(HAVE_TCIME_ZHUYIN)
#include "zhuyindictionary.h"
#include "zhuyintable.h"
#endif
#include "phrasedictionary.h"
#include "virtualkeyboarddebug.h"

#include <QLibraryInfo>

namespace QtVirtualKeyboard {

using namespace tcime;

class TCInputMethodPrivate : public AbstractInputMethodPrivate
{
    Q_DECLARE_PUBLIC(TCInputMethod)
public:

    TCInputMethodPrivate(TCInputMethod *q_ptr) :
        AbstractInputMethodPrivate(),
        q_ptr(q_ptr),
        inputMode(InputEngine::Latin),
        wordDictionary(0),
        highlightIndex(-1)
    {}

    bool setCandidates(const QStringList &values, bool highlightDefault)
    {
        bool candidatesChanged = candidates != values;
        candidates = values;
        highlightIndex = !candidates.isEmpty() && highlightDefault ? 0 : -1;
        return candidatesChanged;
    }

    bool clearCandidates()
    {
        if (candidates.isEmpty())
            return false;

        candidates.clear();
        highlightIndex = -1;
        return true;
    }

    QString pickHighlighted() const
    {
        return (highlightIndex >= 0 && highlightIndex < candidates.count()) ? candidates[highlightIndex] : QString();
    }

    void reset()
    {
        if (clearCandidates()) {
            Q_Q(TCInputMethod);
            emit q->selectionListChanged(SelectionListModel::WordCandidateList);
            emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList, highlightIndex);
        }
        input.clear();
    }

    bool compose(const QChar &c)
    {
        bool accept;
        Q_Q(TCInputMethod);
        InputContext *ic = q->inputContext();
        switch (inputMode)
        {
#if defined(HAVE_TCIME_CANGJIE)
        case InputEngine::Cangjie:
            accept = composeCangjie(ic, c);
            break;
#endif
#if defined(HAVE_TCIME_ZHUYIN)
        case InputEngine::Zhuyin:
            accept = composeZhuyin(ic, c);
            break;
#endif
        default:
            accept = false;
            break;
        }
        return accept;
    }

#if defined(HAVE_TCIME_CANGJIE)
    bool composeCangjie(InputContext *ic, const QChar &c)
    {
        bool accept = false;
        if (!input.contains(0x91CD) && CangjieTable::isLetter(c)) {
            if (input.length() < (cangjieDictionary.simplified() ? CangjieTable::MAX_SIMPLIFIED_CODE_LENGTH : CangjieTable::MAX_CODE_LENGTH)) {
                input.append(c);
                ic->setPreeditText(input);
                if (setCandidates(wordDictionary->getWords(input), true)) {
                    Q_Q(TCInputMethod);
                    emit q->selectionListChanged(SelectionListModel::WordCandidateList);
                    emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList, highlightIndex);
                }
            }
            accept = true;
        } else if (c.unicode() == 0x91CD) {
            if (input.isEmpty()) {
                input.append(c);
                ic->setPreeditText(input);
                checkSpecialCharInput();
            }
            accept = true;
        } else if (c.unicode() == 0x96E3) {
            if (input.length() == 1) {
                Q_ASSERT(input.at(0).unicode() == 0x91CD);
                input.append(c);
                ic->setPreeditText(input);
                checkSpecialCharInput();
            }
            accept = true;
        }
        return accept;
    }

    bool checkSpecialCharInput()
    {
        if (input.length() == 1 && input.at(0).unicode() == 0x91CD) {
            static const QStringList specialChars1 = QStringList()
                    << QChar(0xFF01) << QChar(0x2018) << QChar(0x3000) << QChar(0xFF0C)
                    << QChar(0x3001) << QChar(0x3002) << QChar(0xFF0E) << QChar(0xFF1B)
                    << QChar(0xFF1A) << QChar(0xFF1F) << QChar(0x300E) << QChar(0x300F)
                    << QChar(0x3010) << QChar(0x3011) << QChar(0xFE57) << QChar(0x2026)
                    << QChar(0x2025) << QChar(0xFE50) << QChar(0xFE51) << QChar(0xFE52)
                    << QChar(0x00B7) << QChar(0xFE54) << QChar(0x2574) << QChar(0x2027)
                    << QChar(0x2032) << QChar(0x2035) << QChar(0x301E) << QChar(0x301D)
                    << QChar(0x201D) << QChar(0x201C) << QChar(0x2019) << QChar(0xFE55)
                    << QChar(0xFE5D) << QChar(0xFE5E) << QChar(0xFE59) << QChar(0xFE5A)
                    << QChar(0xFE5B) << QChar(0xFE5C) << QChar(0xFE43) << QChar(0xFE44);
            Q_Q(TCInputMethod);
            if (setCandidates(specialChars1, true)) {
                emit q->selectionListChanged(SelectionListModel::WordCandidateList);
                emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList, highlightIndex);
            }
            q->inputContext()->setPreeditText(candidates[highlightIndex]);
            return true;
        } else if (input.length() == 2 && input.at(0).unicode() == 0x91CD && input.at(1).unicode() == 0x96E3) {
            static const QStringList specialChars2 = QStringList()
                    << QChar(0x3008) << QChar(0x3009) << QChar(0xFE31) << QChar(0x2013)
                    << QChar(0xFF5C) << QChar(0x300C) << QChar(0x300D) << QChar(0xFE40)
                    << QChar(0xFE3F) << QChar(0x2014) << QChar(0xFE3E) << QChar(0xFE3D)
                    << QChar(0x300A) << QChar(0x300B) << QChar(0xFE3B) << QChar(0xFE3C)
                    << QChar(0xFE56) << QChar(0xFE30) << QChar(0xFE39) << QChar(0xFE3A)
                    << QChar(0x3014) << QChar(0x3015) << QChar(0xFE37) << QChar(0xFE38)
                    << QChar(0xFE41) << QChar(0xFE42) << QChar(0xFF5B) << QChar(0xFF5D)
                    << QChar(0xFE35) << QChar(0xFE36) << QChar(0xFF08) << QChar(0xFF09)
                    << QChar(0xFE4F) << QChar(0xFE34) << QChar(0xFE33);
            Q_Q(TCInputMethod);
            if (setCandidates(specialChars2, true)) {
                emit q->selectionListChanged(SelectionListModel::WordCandidateList);
                emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList, highlightIndex);
            }
            q->inputContext()->setPreeditText(candidates[highlightIndex]);
            return true;
        }
        return false;
    }
#endif

#if defined(HAVE_TCIME_ZHUYIN)
    bool composeZhuyin(InputContext *ic, const QChar &c)
    {
        if (ZhuyinTable::isTone(c)) {
            if (input.isEmpty())
                // Tones are accepted only when there's text in composing.
                return false;

            QStringList pair = ZhuyinTable::stripTones(input);
            if (pair.isEmpty())
                // Tones cannot be composed if there's no syllables.
                return false;

            // Replace the original tone with the new tone, but the default tone
            // character should not be composed into the composing text.
            QChar tone = pair[1].at(0);
            if (c == ZhuyinTable::DEFAULT_TONE) {
                if (tone != ZhuyinTable::DEFAULT_TONE)
                    input.remove(input.length() - 1, 1);
            } else {
                if (tone == ZhuyinTable::DEFAULT_TONE)
                    input.append(c);
                else
                    input.replace(input.length() - 1, 1, c);
            }
        } else if (ZhuyinTable::getInitials(c) > 0) {
            // Insert the initial or replace the original initial.
            if (input.isEmpty() || !ZhuyinTable::getInitials(input.at(0)))
                input.insert(0, c);
            else
                input.replace(0, 1, c);
        } else if (ZhuyinTable::getFinals(QString(c)) > 0) {
            // Replace the finals in the decomposed of syllables and tones.
            QList<QChar> decomposed = decomposeZhuyin();
            if (ZhuyinTable::isYiWuYuFinals(c)) {
                decomposed[1] = c;
            } else {
                decomposed[2] = c;
            }

            // Compose back the text after the finals replacement.
            input.clear();
            for (int i = 0; i < decomposed.length(); ++i) {
                if (decomposed[i] != 0)
                    input.append(decomposed[i]);
            }
        } else {
            return false;
        }

        ic->setPreeditText(input);
        if (setCandidates(wordDictionary->getWords(input), true)) {
            Q_Q(TCInputMethod);
            emit q->selectionListChanged(SelectionListModel::WordCandidateList);
            emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList, highlightIndex);
        }

        return true;
    }

    QList<QChar> decomposeZhuyin()
    {
        QList<QChar> results = QList<QChar>() << 0 << 0 << 0 << 0;
        QStringList pair = ZhuyinTable::stripTones(input);
        if (!pair.isEmpty()) {
            // Decompose tones.
            QChar tone = pair[1].at(0);
            if (tone != ZhuyinTable::DEFAULT_TONE)
                results[3] = tone;

            // Decompose initials.
            QString syllables = pair[0];
            if (ZhuyinTable::getInitials(syllables.at(0)) > 0) {
                results[0] = syllables.at(0);
                syllables = syllables.mid(1);
            }

            // Decompose finals.
            if (!syllables.isEmpty()) {
                if (ZhuyinTable::isYiWuYuFinals(syllables.at(0))) {
                    results[1] = syllables.at(0);
                    if (syllables.length() > 1)
                        results[2] = syllables.at(1);
                } else {
                    results[2] = syllables.at(0);
                }
            }
        }
        return results;
    }
#endif

    TCInputMethod *q_ptr;
    InputEngine::InputMode inputMode;
#if defined(HAVE_TCIME_CANGJIE)
    CangjieDictionary cangjieDictionary;
#endif
#if defined(HAVE_TCIME_ZHUYIN)
    ZhuyinDictionary zhuyinDictionary;
#endif
    PhraseDictionary phraseDictionary;
    WordDictionary *wordDictionary;
    QString input;
    QStringList candidates;
    int highlightIndex;
};

/*!
    \class QtVirtualKeyboard::TCInputMethod
    \internal
*/

TCInputMethod::TCInputMethod(QObject *parent) :
    AbstractInputMethod(*new TCInputMethodPrivate(this), parent)
{
}

TCInputMethod::~TCInputMethod()
{
}

bool TCInputMethod::simplified() const
{
#if defined(HAVE_TCIME_CANGJIE)
    Q_D(const TCInputMethod);
    return d->cangjieDictionary.simplified();
#else
    return false;
#endif
}

void TCInputMethod::setSimplified(bool simplified)
{
    VIRTUALKEYBOARD_DEBUG() << "TCInputMethod::setSimplified(): " << simplified;
#if defined(HAVE_TCIME_CANGJIE)
    Q_D(TCInputMethod);
    if (d->cangjieDictionary.simplified() != simplified) {
        d->reset();
        InputContext *ic = inputContext();
        if (ic)
            ic->clear();
        d->cangjieDictionary.setSimplified(simplified);
        emit simplifiedChanged();
    }
#else
    Q_UNUSED(simplified)
#endif
}

QList<InputEngine::InputMode> TCInputMethod::inputModes(const QString &locale)
{
    Q_UNUSED(locale)
    return QList<InputEngine::InputMode>()
#if defined(HAVE_TCIME_ZHUYIN)
            << InputEngine::Zhuyin
#endif
#if defined(HAVE_TCIME_CANGJIE)
           << InputEngine::Cangjie
#endif
               ;
}

bool TCInputMethod::setInputMode(const QString &locale, InputEngine::InputMode inputMode)
{
    Q_UNUSED(locale)
    Q_D(TCInputMethod);
    if (d->inputMode == inputMode)
        return true;
    update();
    bool result = false;
    d->inputMode = inputMode;
    d->wordDictionary = 0;
#if defined(HAVE_TCIME_CANGJIE)
    if (inputMode == InputEngine::Cangjie) {
        if (d->cangjieDictionary.isEmpty()) {
            QString cangjieDictionary(QString::fromLatin1(qgetenv("QT_VIRTUALKEYBOARD_CANGJIE_DICTIONARY").constData()));
            if (cangjieDictionary.isEmpty())
                cangjieDictionary = QLibraryInfo::location(QLibraryInfo::DataPath) + "/qtvirtualkeyboard/tcime/dict_cangjie.dat";
            d->cangjieDictionary.load(cangjieDictionary);
        }
        d->wordDictionary = &d->cangjieDictionary;
    }
#endif
#if defined(HAVE_TCIME_ZHUYIN)
    if (inputMode == InputEngine::Zhuyin) {
        if (d->zhuyinDictionary.isEmpty()) {
            QString zhuyinDictionary(QString::fromLatin1(qgetenv("QT_VIRTUALKEYBOARD_ZHUYIN_DICTIONARY").constData()));
            if (zhuyinDictionary.isEmpty())
                zhuyinDictionary = QLibraryInfo::location(QLibraryInfo::DataPath) + "/qtvirtualkeyboard/tcime/dict_zhuyin.dat";
            d->zhuyinDictionary.load(zhuyinDictionary);
        }
        d->wordDictionary = &d->zhuyinDictionary;
    }
#endif
    result = d->wordDictionary && !d->wordDictionary->isEmpty();
    if (result && d->phraseDictionary.isEmpty()) {
        QString phraseDictionary(QString::fromLatin1(qgetenv("QT_VIRTUALKEYBOARD_PHRASE_DICTIONARY").constData()));
        if (phraseDictionary.isEmpty())
            phraseDictionary = QLibraryInfo::location(QLibraryInfo::DataPath) + "/qtvirtualkeyboard/tcime/dict_phrases.dat";
        d->phraseDictionary.load(phraseDictionary);
    }
    if (!result)
        inputMode = InputEngine::Latin;
    return result;
}

bool TCInputMethod::setTextCase(InputEngine::TextCase textCase)
{
    Q_UNUSED(textCase)
    return true;
}

bool TCInputMethod::keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(key)
    Q_UNUSED(text)
    Q_UNUSED(modifiers)
    Q_D(TCInputMethod);
    InputContext *ic = inputContext();
    bool accept = false;
    switch (key) {
    case Qt::Key_Context1:
        // Do nothing on symbol mode switch
        accept = true;
        break;

    case Qt::Key_Enter:
    case Qt::Key_Return:
        update();
        break;

    case Qt::Key_Tab:
    case Qt::Key_Space:
        if (!d->input.isEmpty()) {
            accept = true;
            if (d->highlightIndex >= 0) {
                QString finalWord = d->pickHighlighted();
                d->reset();
                inputContext()->commit(finalWord);
                if (d->setCandidates(d->phraseDictionary.getWords(finalWord.left(1)), false)) {
                    emit selectionListChanged(SelectionListModel::WordCandidateList);
                    emit selectionListActiveItemChanged(SelectionListModel::WordCandidateList, d->highlightIndex);
                }
            }
        } else {
            update();
        }
        break;

    case Qt::Key_Backspace:
        if (!d->input.isEmpty()) {
            d->input.remove(d->input.length() - 1, 1);
            ic->setPreeditText(d->input);
#if defined(HAVE_TCIME_CANGJIE)
            if (!d->checkSpecialCharInput()) {
#endif
                if (d->setCandidates(d->wordDictionary->getWords(d->input), true)) {
                    emit selectionListChanged(SelectionListModel::WordCandidateList);
                    emit selectionListActiveItemChanged(SelectionListModel::WordCandidateList, d->highlightIndex);
                }
#if defined(HAVE_TCIME_CANGJIE)
            }
#endif
            accept = true;
        } else if (d->clearCandidates()) {
            emit selectionListChanged(SelectionListModel::WordCandidateList);
            emit selectionListActiveItemChanged(SelectionListModel::WordCandidateList, d->highlightIndex);
        }
        break;

    default:
        if (text.length() == 1)
            accept = d->compose(text.at(0));
        if (!accept)
            update();
        break;
    }
    return accept;
}

QList<SelectionListModel::Type> TCInputMethod::selectionLists()
{
    return QList<SelectionListModel::Type>() << SelectionListModel::WordCandidateList;
}

int TCInputMethod::selectionListItemCount(SelectionListModel::Type type)
{
    Q_UNUSED(type)
    Q_D(TCInputMethod);
    return d->candidates.count();
}

QVariant TCInputMethod::selectionListData(SelectionListModel::Type type, int index, int role)
{
    QVariant result;
    Q_D(TCInputMethod);
    switch (role) {
    case SelectionListModel::DisplayRole:
        result = QVariant(d->candidates.at(index));
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

void TCInputMethod::selectionListItemSelected(SelectionListModel::Type type, int index)
{
    Q_UNUSED(type)
    Q_D(TCInputMethod);
    QString finalWord = d->candidates.at(index);
    reset();
    inputContext()->commit(finalWord);
    if (d->setCandidates(d->phraseDictionary.getWords(finalWord.left(1)), false)) {
        emit selectionListChanged(SelectionListModel::WordCandidateList);
        emit selectionListActiveItemChanged(SelectionListModel::WordCandidateList, d->highlightIndex);
    }
}

void TCInputMethod::reset()
{
    Q_D(TCInputMethod);
    d->reset();
}

void TCInputMethod::update()
{
    Q_D(TCInputMethod);
    if (d->highlightIndex >= 0) {
        QString finalWord = d->pickHighlighted();
        d->reset();
        inputContext()->commit(finalWord);
    } else {
        inputContext()->clear();
        d->reset();
    }
}

} // namespace QtVirtualKeyboard
