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

#include "openwnninputmethod.h"
#include "inputcontext.h"
#include "virtualkeyboarddebug.h"
#include <openwnnenginejajp.h>
#include <composingtext.h>
#include <romkan.h>
#include <romkanfullkatakana.h>
#include <romkanhalfkatakana.h>
#include <QTextFormat>

namespace QtVirtualKeyboard {

class OpenWnnInputMethodPrivate : public AbstractInputMethodPrivate
{
    Q_DECLARE_PUBLIC(OpenWnnInputMethod)
public:
    enum EngineMode {
        ENGINE_MODE_DEFAULT,
        ENGINE_MODE_DIRECT,
        ENGINE_MODE_NO_LV2_CONV,
        ENGINE_MODE_FULL_KATAKANA,
        ENGINE_MODE_HALF_KATAKANA,
    };

    enum ConvertType {
        CONVERT_TYPE_NONE = 0,
        CONVERT_TYPE_RENBUN = 1,
    };

    enum {
        MAX_COMPOSING_TEXT = 30
    };

    OpenWnnInputMethodPrivate(OpenWnnInputMethod *q_ptr) :
        AbstractInputMethodPrivate(),
        q_ptr(q_ptr),
        inputMode(InputEngine::Latin),
        exactMatchMode(false),
        converter(0),
        converterJAJP(),
        activeConvertType(CONVERT_TYPE_NONE),
        preConverter(0),
        enableLearning(true),
        enablePrediction(true),
        enableConverter(true),
        disableUpdate(false),
        commitCount(0),
        targetLayer(ComposingText::LAYER1),
        activeWordIndex(-1)
    {
    }

    void changeEngineMode(EngineMode mode)
    {
        switch (mode) {
        case ENGINE_MODE_DIRECT:
            /* Full/Half-width number or Full-width alphabet */
            converter = NULL;
            preConverter.reset();
            break;

        case ENGINE_MODE_NO_LV2_CONV:
            converter = NULL;
            preConverter.reset(new Romkan());
            break;

        case ENGINE_MODE_FULL_KATAKANA:
            converter = NULL;
            preConverter.reset(new RomkanFullKatakana());
            break;

        case ENGINE_MODE_HALF_KATAKANA:
            converter = NULL;
            preConverter.reset(new RomkanHalfKatakana());
            break;

        default:
            /* HIRAGANA input mode */
            setDictionary(OpenWnnEngineJAJP::DIC_LANG_JP);
            converter = &converterJAJP;
            preConverter.reset(new Romkan());
            break;
        }
    }

    void setDictionary(OpenWnnEngineJAJP::DictionaryType mode)
    {
        converterJAJP.setDictionary(mode);
    }

    void breakSequence()
    {
        converterJAJP.breakSequence();
    }

    bool isEnableL2Converter()
    {
        return converter != NULL && enableConverter;
    }

    void startConvert(ConvertType convertType)
    {
        if (!isEnableL2Converter())
            return;

        if (activeConvertType != convertType) {
            if (!exactMatchMode) {
                if (convertType == CONVERT_TYPE_RENBUN) {
                    /* not specify */
                    composingText.setCursor(ComposingText::LAYER1, 0);
                } else {
                    if (activeConvertType == CONVERT_TYPE_RENBUN) {
                        exactMatchMode = true;
                    } else {
                        /* specify all range */
                        composingText.setCursor(ComposingText::LAYER1,
                                                composingText.size(ComposingText::LAYER1));
                    }
                }
            }

            if (convertType == CONVERT_TYPE_RENBUN)
                /* clears variables for the prediction */
                exactMatchMode = false;

            /* clears variables for the convert */
            commitCount = 0;

            activeConvertType = convertType;

            updateViewStatus(ComposingText::LAYER2, true, true);

            focusNextCandidate();
        }
    }

    void changeL2Segment(const QSharedPointer<WnnWord> &word)
    {
        if (word.isNull())
            return;
        QList<StrSegment> ss;
        ss.append(composingText.getStrSegment(ComposingText::LAYER2, 0));
        if (!ss[0].clause.isNull())
            ss[0].clause->candidate = word->candidate;
        ss[0].string = word->candidate;
        composingText.replaceStrSegment(ComposingText::LAYER2, ss);
#ifdef QT_VIRTUALKEYBOARD_DEBUG
        composingText.debugout();
#endif
        updateViewStatus(ComposingText::LAYER2, false, false);
    }

    void initializeScreen()
    {
        if (composingText.size(ComposingText::LAYER0) != 0)
            inputEngine->inputContext()->commit("");
        composingText.clear();
        exactMatchMode = false;
        activeConvertType = CONVERT_TYPE_NONE;
        clearCandidates();
    }

    void updateViewStatusForPrediction(bool updateCandidates, bool updateEmptyText)
    {
        activeConvertType = CONVERT_TYPE_NONE;

        updateViewStatus(ComposingText::LAYER1, updateCandidates, updateEmptyText);
    }

    void updateViewStatus(ComposingText::TextLayer layer, bool updateCandidates, bool updateEmptyText)
    {
        targetLayer = layer;

        if (updateCandidates)
            updateCandidateView();

        /* set the text for displaying as the composing text */
        displayText.clear();
        displayText.insert(0, composingText.toString(layer));

        /* add decoration to the text */
        if (!displayText.isEmpty() || updateEmptyText) {

            QList<QInputMethodEvent::Attribute> attributes;

            int cursor = composingText.getCursor(layer);
            if (cursor != 0) {
                int highlightEnd = 0;

                if (exactMatchMode) {

                    QTextCharFormat textFormat;
                    textFormat.setBackground(QBrush(QColor(0x66, 0xCD, 0xAA)));
                    textFormat.setForeground(QBrush(Qt::black));
                    attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, 0, cursor, textFormat));
                    highlightEnd = cursor;

                } else if (layer == ComposingText::LAYER2) {

                    highlightEnd = composingText.toString(layer, 0, 0).length();

                    /* highlights the first segment */
                    QTextCharFormat textFormat;
                    textFormat.setBackground(QBrush(QColor(0x88, 0x88, 0xFF)));
                    textFormat.setForeground(QBrush(Qt::black));
                    attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, 0, highlightEnd, textFormat));
                }

                if (highlightEnd != 0) {
                    /* highlights remaining text */
                    QTextCharFormat textFormat;
                    textFormat.setBackground(QBrush(QColor(0xF0, 0xFF, 0xFF)));
                    textFormat.setForeground(QBrush(Qt::black));
                    attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, highlightEnd, composingText.toString(layer).length(), textFormat));
                }
            }

            QTextCharFormat textFormat;
            textFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);
            attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, 0, displayText.length(), textFormat));

            int displayCursor = composingText.toString(layer, 0, cursor - 1).length();
            attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, displayCursor, 1, QVariant()));

            inputEngine->inputContext()->setPreeditText(displayText, attributes);
        }
    }

    void updateCandidateView()
    {
        switch (targetLayer) {
        case ComposingText::LAYER0:
        case ComposingText::LAYER1: /* prediction */
            if (enablePrediction)
                /* update the candidates view */
                updatePrediction();
            break;
        case ComposingText::LAYER2: /* convert */
            if (commitCount == 0)
                converter->convert(composingText);

            if (converter->makeCandidateListOf(commitCount) != 0) {
                composingText.setCursor(ComposingText::LAYER2, 1);
                displayCandidates();
            } else {
                composingText.setCursor(ComposingText::LAYER1,
                                         composingText.toString(ComposingText::LAYER1).length());
                clearCandidates();
            }
            break;
        default:
            break;
        }
    }

    void updatePrediction()
    {
        int candidates = 0;
        int cursor = composingText.getCursor(ComposingText::LAYER1);
        if (isEnableL2Converter()) {
            if (exactMatchMode)
                /* exact matching */
                candidates = converter->predict(composingText, 0, cursor);
            else
                /* normal prediction */
                candidates = converter->predict(composingText, 0, -1);
        }

        /* update the candidates view */
        if (candidates > 0)
            displayCandidates();
        else
            clearCandidates();
    }

    void displayCandidates()
    {
        int previousActiveWordIndex = activeWordIndex;
        bool wasEmpty = candidateList.isEmpty();
        clearCandidates(true);

        QSharedPointer<WnnWord> result;
        while (result = converter->getNextCandidate()) {
            candidateList.append(result);
        }

        Q_Q(OpenWnnInputMethod);
        if (!candidateList.isEmpty() || !wasEmpty)
            emit q->selectionListChanged(SelectionListModel::WordCandidateList);
        if (previousActiveWordIndex != activeWordIndex)
            emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList, activeWordIndex);
    }

    void clearCandidates(bool deferUpdate = false)
    {
        if (!candidateList.isEmpty()) {
            candidateList.clear();
            if (!deferUpdate) {
                Q_Q(OpenWnnInputMethod);
                emit q->selectionListChanged(SelectionListModel::WordCandidateList);
            }
            clearFocusCandidate(deferUpdate);
        }
    }

    QSharedPointer<WnnWord> focusNextCandidate()
    {
        Q_Q(OpenWnnInputMethod);
        if (candidateList.isEmpty())
            return QSharedPointer<WnnWord>();
        activeWordIndex++;
        if (activeWordIndex >= candidateList.size())
            activeWordIndex = 0;
        emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList, activeWordIndex);
        return candidateList.at(activeWordIndex);
    }

    void clearFocusCandidate(bool deferUpdate = false)
    {
        Q_Q(OpenWnnInputMethod);
        if (activeWordIndex != -1) {
            activeWordIndex = -1;
            if (!deferUpdate)
                emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList, activeWordIndex);
        }
    }

    void fitInputType()
    {
        enablePrediction = true;
        enableConverter = true;

        Qt::InputMethodHints inputMethodHints = inputEngine->inputContext()->inputMethodHints();
        if (inputMethodHints.testFlag(Qt::ImhDigitsOnly) ||
            inputMethodHints.testFlag(Qt::ImhFormattedNumbersOnly) ||
            inputMethodHints.testFlag(Qt::ImhDialableCharactersOnly)) {
            enableConverter = false;
        }

        if (inputMethodHints.testFlag(Qt::ImhLatinOnly)) {
            enableConverter = false;
        }

        if (inputMethodHints.testFlag(Qt::ImhHiddenText) ||
            inputMethodHints.testFlag(Qt::ImhSensitiveData)) {
            enablePrediction = false;
        }

        if (inputMethodHints.testFlag(Qt::ImhNoPredictiveText)) {
            enablePrediction = false;
        }

        activeConvertType = CONVERT_TYPE_NONE;
    }

    void learnWord(WnnWord &word)
    {
        if (enableLearning)
            converter->learn(word);
    }

    void learnWord(int index)
    {
        if (enableLearning && index < composingText.size(ComposingText::LAYER2)) {
            StrSegment seg = composingText.getStrSegment(ComposingText::LAYER2, index);
            if (!seg.clause.isNull()) {
                converter->learn(*seg.clause);
            } else {
                QString stroke = composingText.toString(ComposingText::LAYER1, seg.from, seg.to);
                WnnWord word(seg.string, stroke);
                converter->learn(word);
            }
        }
    }

    void commitAll()
    {
        if (activeConvertType != CONVERT_TYPE_NONE) {
            commitConvertingText();
        } else {
            composingText.setCursor(ComposingText::LAYER1,
                                     composingText.size(ComposingText::LAYER1));
            commitText(true);
        }
    }

    void commitConvertingText()
    {
        if (activeConvertType != CONVERT_TYPE_NONE) {
            int size = composingText.size(ComposingText::LAYER2);
            for (int i = 0; i < size; i++) {
                learnWord(i);
            }

            QString text = composingText.toString(ComposingText::LAYER2);
            disableUpdate = true;
            inputEngine->inputContext()->commit(text);
            disableUpdate = false;

            initializeScreen();
        }
    }

    bool commitText(bool learn = false)
    {
        ComposingText::TextLayer layer = targetLayer;
        int cursor = composingText.getCursor(layer);
        if (cursor == 0) {
            return false;
        }
        QString tmp = composingText.toString(layer, 0, cursor - 1);

        if (converter != NULL) {
            if (learn) {
                if (activeConvertType == CONVERT_TYPE_RENBUN) {
                    learnWord(0); /* select the top of the clauses */
                } else {
                    if (composingText.size(ComposingText::LAYER1) != 0) {
                        QString stroke = composingText.toString(ComposingText::LAYER1, 0, composingText.getCursor(layer) - 1);
                        WnnWord word(tmp, stroke);
                        learnWord(word);
                    }
                }
            } else {
                breakSequence();
            }
        }
        return commitText(tmp);
    }

    bool commitText(const WnnWord &word)
    {
        return commitText(word.candidate);
    }

    bool commitText(const QString &string)
    {
        ComposingText::TextLayer layer = targetLayer;

        disableUpdate = true;
        inputEngine->inputContext()->commit(string);
        disableUpdate = false;

        int cursor = composingText.getCursor(layer);
        if (cursor > 0) {
            composingText.deleteStrSegment(layer, 0, composingText.getCursor(layer) - 1);
            composingText.setCursor(layer, composingText.size(layer));
        }
        exactMatchMode = false;
        commitCount++;

        if ((layer == ComposingText::LAYER2) && (composingText.size(layer) == 0))
            layer = ComposingText::LAYER1; /* for connected prediction */

        if (layer == ComposingText::LAYER2) {
            activeConvertType = CONVERT_TYPE_RENBUN;
            updateViewStatus(layer, true, false);
            focusNextCandidate();
        } else {
            updateViewStatusForPrediction(true, false);
        }

        return composingText.size(ComposingText::LAYER0) > 0;
    }

    bool isAlphabetLast(const QString &str)
    {
        if (str.isEmpty())
            return false;
        ushort ch = str.at(str.length() - 1).unicode();
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    }

    void commitTextWithoutLastAlphabet()
    {
        QString last = composingText.getStrSegment(targetLayer, -1).string;

        if (isAlphabetLast(last)) {
            composingText.moveCursor(ComposingText::LAYER1, -1);
            commitText(false);
            composingText.moveCursor(ComposingText::LAYER1, 1);
        } else {
            commitText(false);
        }
    }

    bool processLeftKeyEvent()
    {
        if (composingText.size(ComposingText::LAYER1) == 0)
            return false;

        if (activeConvertType != CONVERT_TYPE_NONE) {
            if (composingText.getCursor(ComposingText::LAYER1) > 1) {
                composingText.moveCursor(ComposingText::LAYER1, -1);
            }
        } else if (exactMatchMode) {
            composingText.moveCursor(ComposingText::LAYER1, -1);
        } else {
            exactMatchMode = true;
        }

#ifdef QT_VIRTUALKEYBOARD_DEBUG
        composingText.debugout();
#endif

        commitCount = 0; /* retry consecutive clause conversion if necessary. */
        updateViewStatus(targetLayer, true, true);

        if (activeConvertType != CONVERT_TYPE_NONE)
            focusNextCandidate();

        return true;
    }

    bool processRightKeyEvent()
    {
        if (composingText.size(ComposingText::LAYER1) == 0)
            return false;

        ComposingText::TextLayer layer = targetLayer;
        if (exactMatchMode || activeConvertType != CONVERT_TYPE_NONE) {
            int textSize = composingText.size(ComposingText::LAYER1);
            if (composingText.getCursor(ComposingText::LAYER1) == textSize) {
                exactMatchMode = false;
                layer = ComposingText::LAYER1; /* convert -> prediction */
                activeConvertType = CONVERT_TYPE_NONE;
            } else {
                composingText.moveCursor(ComposingText::LAYER1, 1);
            }
        } else {
            if (composingText.getCursor(ComposingText::LAYER1) < composingText.size(ComposingText::LAYER1)) {
                composingText.moveCursor(ComposingText::LAYER1, 1);
            }
        }

#ifdef QT_VIRTUALKEYBOARD_DEBUG
        composingText.debugout();
#endif

        commitCount = 0; /* retry consecutive clause conversion if necessary. */

        updateViewStatus(layer, true, true);

        if (activeConvertType != CONVERT_TYPE_NONE)
            focusNextCandidate();

        return true;
    }

    OpenWnnInputMethod *q_ptr;
    InputEngine::InputMode inputMode;
    bool exactMatchMode;
    QString displayText;
    OpenWnnEngineJAJP *converter;
    OpenWnnEngineJAJP converterJAJP;
    ConvertType activeConvertType;
    ComposingText composingText;
    QScopedPointer<LetterConverter> preConverter;
    bool enableLearning;
    bool enablePrediction;
    bool enableConverter;
    bool disableUpdate;
    int commitCount;
    ComposingText::TextLayer targetLayer;
    QList<QSharedPointer<WnnWord> > candidateList;
    int activeWordIndex;
};

/*!
    \class QtVirtualKeyboard::OpenWnnInputMethod
    \internal
*/

OpenWnnInputMethod::OpenWnnInputMethod(QObject *parent) :
    AbstractInputMethod(*new OpenWnnInputMethodPrivate(this), parent)
{
}

OpenWnnInputMethod::~OpenWnnInputMethod()
{
}

QList<InputEngine::InputMode> OpenWnnInputMethod::inputModes(const QString &locale)
{
    Q_UNUSED(locale)
    return QList<InputEngine::InputMode>()
            << InputEngine::Hiragana
            << InputEngine::Katakana
            << InputEngine::FullwidthLatin
            << InputEngine::Latin;
}

bool OpenWnnInputMethod::setInputMode(const QString &locale, InputEngine::InputMode inputMode)
{
    Q_UNUSED(locale)
    Q_D(OpenWnnInputMethod);
    if (d->inputMode == inputMode)
        return true;
    update();
    switch (inputMode) {
    case InputEngine::Hiragana:
        d->changeEngineMode(OpenWnnInputMethodPrivate::ENGINE_MODE_DEFAULT);
        break;

    case InputEngine::Katakana:
        d->changeEngineMode(OpenWnnInputMethodPrivate::ENGINE_MODE_FULL_KATAKANA);
        break;

    default:
        d->changeEngineMode(OpenWnnInputMethodPrivate::ENGINE_MODE_DIRECT);
        break;
    }
    d->inputMode = inputMode;
    return true;
}

bool OpenWnnInputMethod::setTextCase(InputEngine::TextCase textCase)
{
    Q_UNUSED(textCase)
    return true;
}

bool OpenWnnInputMethod::keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(key)
    Q_UNUSED(text)
    Q_UNUSED(modifiers)
    Q_D(OpenWnnInputMethod);

    if (d->preConverter == NULL && !d->isEnableL2Converter())
        return false;

    switch (key) {
    case Qt::Key_Left:
        if (d->isEnableL2Converter() && d->composingText.size(ComposingText::LAYER1) > 0)
            return d->processLeftKeyEvent();
        else
            return d->commitText(false);
        break;

    case Qt::Key_Right:
        if (d->isEnableL2Converter() && d->composingText.size(ComposingText::LAYER1) > 0)
            return d->processRightKeyEvent();
        else
            return d->commitText(false);
        break;

    case Qt::Key_Backspace:
        if (d->composingText.size(ComposingText::LAYER1) > 0) {
            if (d->activeConvertType == OpenWnnInputMethodPrivate::CONVERT_TYPE_RENBUN) {
                d->composingText.setCursor(ComposingText::LAYER1,
                                           d->composingText.toString(ComposingText::LAYER1).length());
                d->exactMatchMode = false;
                d->clearFocusCandidate();
            } else {
                if ((d->composingText.size(ComposingText::LAYER1) == 1) &&
                     d->composingText.getCursor(ComposingText::LAYER1) != 0) {
                    d->initializeScreen();
                    return true;
                } else {
                    d->composingText.deleteAt(ComposingText::LAYER1, false);
                }
            }
#ifdef QT_VIRTUALKEYBOARD_DEBUG
            d->composingText.debugout();
#endif
            d->updateViewStatusForPrediction(true, true);
            return true;
        }
        break;

    case Qt::Key_Space:
        if (d->composingText.size(ComposingText::LAYER0) == 0) {
            d->clearCandidates();
            d->breakSequence();
        } else {
            if (d->targetLayer == ComposingText::LAYER2)
                d->changeL2Segment(d->focusNextCandidate());
            else if (d->isEnableL2Converter())
                d->startConvert(OpenWnnInputMethodPrivate::CONVERT_TYPE_RENBUN);
            else
                return d->commitText(false);
            return true;
        }
        break;

    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (d->composingText.size(ComposingText::LAYER0) > 0) {
            d->commitText(true);
            return true;
        }
        break;

    default:
        if (key < Qt::Key_Escape && !text.isEmpty() && text.at(0).isPrint()) {
            if (d->composingText.size(ComposingText::LAYER1) < OpenWnnInputMethodPrivate::MAX_COMPOSING_TEXT) {
                if (d->isEnableL2Converter()) {
                    d->commitConvertingText();
                    d->composingText.insertStrSegment(ComposingText::LAYER0, ComposingText::LAYER1, text);
                    if (d->preConverter != NULL)
                        d->preConverter->convert(d->composingText);
                    d->updateViewStatusForPrediction(true, true);
                } else {
                    d->composingText.insertStrSegment(ComposingText::LAYER0, ComposingText::LAYER1, text);
                    QString layer1 = d->composingText.toString(ComposingText::LAYER1);
                    if (!d->isAlphabetLast(layer1)) {
                        d->commitText(false);
                    } else {
                        bool completed = d->preConverter->convert(d->composingText);
                        if (completed) {
                            d->commitTextWithoutLastAlphabet();
                        } else {
                            d->updateViewStatusForPrediction(true, true);
                        }
                    }
                }
            }
#ifdef QT_VIRTUALKEYBOARD_DEBUG
            d->composingText.debugout();
#endif
            return true;
        }
        break;
    }

    return false;
}

QList<SelectionListModel::Type> OpenWnnInputMethod::selectionLists()
{
    return QList<SelectionListModel::Type>() << SelectionListModel::WordCandidateList;
}

int OpenWnnInputMethod::selectionListItemCount(SelectionListModel::Type type)
{
    Q_UNUSED(type)
    Q_D(OpenWnnInputMethod);
    return d->candidateList.size();
}

QVariant OpenWnnInputMethod::selectionListData(SelectionListModel::Type type, int index, int role)
{
    QVariant result;
    Q_UNUSED(type)
    Q_D(OpenWnnInputMethod);
    switch (role) {
    case SelectionListModel::DisplayRole:
        result = QVariant(d->candidateList.at(index)->candidate);
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

void OpenWnnInputMethod::selectionListItemSelected(SelectionListModel::Type type, int index)
{
    Q_UNUSED(type)
    Q_D(OpenWnnInputMethod);
    d->commitText(*d->candidateList.at(index));
}

void OpenWnnInputMethod::reset()
{
    Q_D(OpenWnnInputMethod);
    d->commitAll();
    d->initializeScreen();
    d->fitInputType();
}

void OpenWnnInputMethod::update()
{
    Q_D(OpenWnnInputMethod);
    if (!d->disableUpdate)
        reset();
}

} // namespace QtVirtualKeyboard
