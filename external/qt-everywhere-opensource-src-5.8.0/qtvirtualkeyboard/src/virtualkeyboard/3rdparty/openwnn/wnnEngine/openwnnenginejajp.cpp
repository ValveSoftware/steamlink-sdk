/*
 * Qt implementation of OpenWnn library
 * This file is part of the Qt Virtual Keyboard module.
 * Contact: http://www.qt.io/licensing/
 *
 * Copyright (C) 2015  The Qt Company
 * Copyright (C) 2008-2012  OMRON SOFTWARE Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "openwnnenginejajp.h"
#include "openwnndictionary.h"
#include "openwnnclauseconverterjajp.h"
#include "wnnword.h"
#include "kanaconverter.h"
#include <QtCore/private/qobject_p.h>

class OpenWnnEngineJAJPPrivate : public QObjectPrivate
{
public:
    OpenWnnEngineJAJPPrivate() :
        QObjectPrivate(),
        mDictType(OpenWnnEngineJAJP::DIC_LANG_INIT),
        mKeyboardType(OpenWnnEngineJAJP::KEYBOARD_QWERTY),
        mOutputNum(0),
        mGetCandidateFrom(0),
        mExactMatchMode(false),
        mSingleClauseMode(false)
    {
        /* clear dictionary settings */
        mDictionaryJP.clearDictionary();
        mDictionaryJP.clearApproxPattern();

        mClauseConverter.setDictionary(&mDictionaryJP);
        mKanaConverter.setDictionary(&mDictionaryJP);
    }

    void setDictionaryForPrediction(int strlen)
    {
        OpenWnnDictionary &dict = mDictionaryJP;

        dict.clearDictionary();

        if (mDictType != OpenWnnEngineJAJP::DIC_LANG_JP_EISUKANA) {
            dict.clearApproxPattern();
            if (strlen == 0) {
                dict.setDictionary(2, 245, 245);
                dict.setDictionary(3, 100, 244);

                dict.setDictionary(OpenWnnDictionary::INDEX_LEARN_DICTIONARY, OpenWnnEngineJAJP::FREQ_LEARN, OpenWnnEngineJAJP::FREQ_LEARN);
            } else {
                dict.setDictionary(0, 100, 400);
                if (strlen > 1) {
                    dict.setDictionary(1, 100, 400);
                }
                dict.setDictionary(2, 245, 245);
                dict.setDictionary(3, 100, 244);

                dict.setDictionary(OpenWnnDictionary::INDEX_USER_DICTIONARY, OpenWnnEngineJAJP::FREQ_USER, OpenWnnEngineJAJP::FREQ_USER);
                dict.setDictionary(OpenWnnDictionary::INDEX_LEARN_DICTIONARY, OpenWnnEngineJAJP::FREQ_LEARN, OpenWnnEngineJAJP::FREQ_LEARN);
                if (mKeyboardType != OpenWnnEngineJAJP::KEYBOARD_QWERTY) {
                    dict.setApproxPattern(OpenWnnDictionary::APPROX_PATTERN_JAJP_12KEY_NORMAL);
                }
            }
        }
    }

    QSharedPointer<WnnWord> getCandidate(int index)
    {
        QSharedPointer<WnnWord> word;

        if (mGetCandidateFrom == 0) {
            if (mDictType == OpenWnnEngineJAJP::DIC_LANG_JP_EISUKANA) {
                /* skip to Kana conversion if EISU-KANA conversion mode */
                mGetCandidateFrom = 2;
            } else if (mSingleClauseMode) {
                /* skip to single clause conversion if single clause conversion mode */
                mGetCandidateFrom = 1;
            } else {
                if (mConvResult.size() < OpenWnnEngineJAJP::PREDICT_LIMIT) {
                    /* get prefix matching words from the dictionaries */
                    while (index >= mConvResult.size()) {
                        if ((word = mDictionaryJP.getNextWord()) == NULL) {
                            mGetCandidateFrom = 1;
                            break;
                        }
                        if (!mExactMatchMode || mInputHiragana.compare(word->stroke) == 0) {
                            addCandidate(word);
                            if (mConvResult.size() >= OpenWnnEngineJAJP::PREDICT_LIMIT) {
                                mGetCandidateFrom = 1;
                                break;
                            }
                        }
                    }
                } else {
                    mGetCandidateFrom = 1;
                }
            }
        }

        /* get candidates by single clause conversion */
        if (mGetCandidateFrom == 1) {
            QList<WnnClause> convResult = mClauseConverter.convert(mInputHiragana);
            if (!convResult.isEmpty()) {
                for (QList<WnnClause>::ConstIterator it = convResult.constBegin();
                     it != convResult.constEnd(); it++) {
                    addCandidate(QSharedPointer<WnnWord>(new WnnWord(*it)));
                }
            }
            /* end of candidates by single clause conversion */
            mGetCandidateFrom = 2;
        }

        /* get candidates from Kana converter */
        if (mGetCandidateFrom == 2) {
            QList<WnnWord> addCandidateList = mKanaConverter.createPseudoCandidateList(mInputHiragana, mInputRomaji);

            for (QList<WnnWord>::ConstIterator it = addCandidateList.constBegin();
                 it != addCandidateList.constEnd(); it++) {
                addCandidate(QSharedPointer<WnnWord>(new WnnWord(*it)));
            }

            mGetCandidateFrom = 3;
        }

        if (index >= mConvResult.size()) {
            return QSharedPointer<WnnWord>();
        }
        return mConvResult.at(index);
    }

    bool addCandidate(QSharedPointer<WnnWord> word)
    {
        if (word.isNull() || word->candidate.isEmpty() || mCandTable.contains(word->candidate)
                || word->candidate.length() > OpenWnnEngineJAJP::MAX_OUTPUT_LENGTH) {
            return false;
        }
        /*
        if (mFilter != NULL && !mFilter->isAllowed(word)) {
            return false;
        }
        */
        mCandTable.insert(word->candidate, word);
        mConvResult.append(word);
        return true;
    }

    void clearCandidates()
    {
        mConvResult.clear();
        mCandTable.clear();
        mOutputNum = 0;
        mInputHiragana.clear();
        mInputRomaji.clear();
        mGetCandidateFrom = 0;
        mSingleClauseMode = false;
    }

    int setSearchKey(const ComposingText &text, int maxLen)
    {
        QString input = text.toString(ComposingText::LAYER1);
        if (0 <= maxLen && maxLen <= input.length()) {
            input = input.mid(0, maxLen);
            mExactMatchMode = true;
        } else {
            mExactMatchMode = false;
        }

        if (input.length() == 0) {
            mInputHiragana = "";
            mInputRomaji = "";
            return 0;
        }

        mInputHiragana = input;
        mInputRomaji = text.toString(ComposingText::LAYER0);

        return input.length();
    }

    void clearPreviousWord()
    {
        mPreviousWord.reset();
    }

    OpenWnnEngineJAJP::DictionaryType mDictType;
    OpenWnnEngineJAJP::KeyboardType mKeyboardType;
    OpenWnnDictionary mDictionaryJP;
    QList<QSharedPointer<WnnWord> > mConvResult;
    QMap<QString, QSharedPointer<WnnWord> > mCandTable;
    QString mInputHiragana;
    QString mInputRomaji;
    int mOutputNum;
    int mGetCandidateFrom;
    QSharedPointer<WnnWord> mPreviousWord;
    OpenWnnClauseConverterJAJP mClauseConverter;
    KanaConverter mKanaConverter;
    bool mExactMatchMode;
    bool mSingleClauseMode;
    QSharedPointer<WnnSentence> mConvertSentence;
};

OpenWnnEngineJAJP::OpenWnnEngineJAJP(QObject *parent) :
    QObject(*new OpenWnnEngineJAJPPrivate(), parent)
{

}

OpenWnnEngineJAJP::~OpenWnnEngineJAJP()
{
}

bool OpenWnnEngineJAJP::setDictionary(DictionaryType type)
{
    Q_D(OpenWnnEngineJAJP);
    d->mDictType = type;
    return true;
}

int OpenWnnEngineJAJP::predict(const ComposingText &text, int minLen, int maxLen)
{
    Q_D(OpenWnnEngineJAJP);
    Q_UNUSED(minLen)

    d->clearCandidates();

    /* set mInputHiragana and mInputRomaji */
    int len = d->setSearchKey(text, maxLen);

    /* set dictionaries by the length of input */
    d->setDictionaryForPrediction(len);

    /* search dictionaries */
    d->mDictionaryJP.setInUseState(true);

    if (len == 0) {
        /* search by previously selected word */
        if (d->mPreviousWord.isNull())
            return -1;
        return d->mDictionaryJP.searchWord(OpenWnnDictionary::SEARCH_LINK, OpenWnnDictionary::ORDER_BY_FREQUENCY,
                                           d->mInputHiragana, *d->mPreviousWord);
    } else {
        if (d->mExactMatchMode) {
            /* exact matching */
            d->mDictionaryJP.searchWord(OpenWnnDictionary::SEARCH_EXACT, OpenWnnDictionary::ORDER_BY_FREQUENCY,
                                        d->mInputHiragana);
        } else {
            /* prefix matching */
            d->mDictionaryJP.searchWord(OpenWnnDictionary::SEARCH_PREFIX, OpenWnnDictionary::ORDER_BY_FREQUENCY,
                                        d->mInputHiragana);
        }
        return 1;
    }
}

int OpenWnnEngineJAJP::convert(ComposingText &text)
{
    Q_D(OpenWnnEngineJAJP);

    d->clearCandidates();

    d->mDictionaryJP.setInUseState(true);

    int cursor = text.getCursor(ComposingText::LAYER1);
    QString input;
    QSharedPointer<WnnClause> head;
    if (cursor > 0) {
        /* convert previous part from cursor */
        input = text.toString(ComposingText::LAYER1, 0, cursor - 1);
        QList<WnnClause> headCandidates = d->mClauseConverter.convert(input);
        if (headCandidates.isEmpty()) {
            return 0;
        }
        head.reset(new WnnClause(input, headCandidates.first()));

        /* set the rest of input string */
        input = text.toString(ComposingText::LAYER1, cursor, text.size(ComposingText::LAYER1) - 1);
    } else {
        /* set whole of input string */
        input = text.toString(ComposingText::LAYER1);
    }

    QSharedPointer<WnnSentence> sentence;
    if (input.length() != 0) {
        sentence = d->mClauseConverter.consecutiveClauseConvert(input);
    }
    if (!head.isNull()) {
        sentence.reset(new WnnSentence(*head, sentence.data()));
    }
    if (sentence.isNull()) {
        return 0;
    }

    QList<StrSegment> ss;
    int pos = 0;
    for (QList<WnnClause>::ConstIterator it = sentence->elements.constBegin();
         it != sentence->elements.constEnd(); it++) {
        const WnnClause &clause = *it;
        int len = clause.stroke.length();
        ss.append(StrSegment(clause, pos, pos + len - 1));
        pos += len;
    }
    text.setCursor(ComposingText::LAYER2, text.size(ComposingText::LAYER2));
    text.replaceStrSegment(ComposingText::LAYER2, ss,
                           text.getCursor(ComposingText::LAYER2));
    d->mConvertSentence = sentence;

    return 0;
}

QSharedPointer<WnnWord> OpenWnnEngineJAJP::getNextCandidate()
{
    Q_D(OpenWnnEngineJAJP);

    if (d->mInputHiragana.isEmpty()) {
        return QSharedPointer<WnnWord>();
    }
    QSharedPointer<WnnWord> word = d->getCandidate(d->mOutputNum);
    if (!word.isNull()) {
        d->mOutputNum++;
    }
    return word;
}

bool OpenWnnEngineJAJP::learn(WnnWord &word)
{
    Q_D(OpenWnnEngineJAJP);

    int ret = -1;
    if (word.partOfSpeech.right == 0) {
        word.partOfSpeech = d->mDictionaryJP.getPOS(OpenWnnDictionary::POS_TYPE_MEISI);
    }

    OpenWnnDictionary &dict = d->mDictionaryJP;
    if (word.isSentence()) {
        const WnnSentence *sentence = static_cast<const WnnSentence *>(&word);
        for (QList<WnnClause>::ConstIterator clauses = sentence->elements.constBegin();
             clauses != sentence->elements.constEnd(); clauses++) {
            const WnnWord &wd = *clauses;
            ret = dict.learnWord(wd, d->mPreviousWord.data());
            d->mPreviousWord.reset(static_cast<WnnWord *>(new WnnSentence(*sentence)));
            if (ret != 0) {
                break;
            }
        }
    } else {
        ret = dict.learnWord(word, d->mPreviousWord.data());
        d->mPreviousWord.reset(new WnnWord(word));
        d->mClauseConverter.setDictionary(&dict);
    }

    return (ret == 0);
}

void OpenWnnEngineJAJP::breakSequence()
{
    Q_D(OpenWnnEngineJAJP);

    d->clearPreviousWord();
}

int OpenWnnEngineJAJP::makeCandidateListOf(int clausePosition)
{
    Q_D(OpenWnnEngineJAJP);

    d->clearCandidates();

    if ((d->mConvertSentence == NULL) || (d->mConvertSentence->elements.size() <= clausePosition)) {
        return 0;
    }
    d->mSingleClauseMode = true;
    const WnnClause &clause = d->mConvertSentence->elements.at(clausePosition);
    d->mInputHiragana = clause.stroke;
    d->mInputRomaji = clause.candidate;

    return 1;
}
