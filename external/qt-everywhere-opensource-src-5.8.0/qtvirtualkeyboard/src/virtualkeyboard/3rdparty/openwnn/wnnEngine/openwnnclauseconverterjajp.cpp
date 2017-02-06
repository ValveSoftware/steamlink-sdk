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

#include "openwnnclauseconverterjajp.h"
#include "openwnndictionary.h"
#include "wnnword.h"
#include <QtCore/private/qobject_p.h>

class OpenWnnClauseConverterJAJPPrivate : public QObjectPrivate
{
public:
    OpenWnnClauseConverterJAJPPrivate()
    { }

    bool singleClauseConvert(QList<WnnClause> &clauseList, const QString &input,
                             const WnnPOS &terminal, bool all)
    {
        bool ret = false;

        /* get clauses without ancillary word */
        QList<WnnWord> stems = getIndependentWords(input, all);
        if (!stems.isEmpty()) {
            for (QList<WnnWord>::ConstIterator stemsi = stems.constBegin();
                 stemsi != stems.constEnd(); stemsi++) {
                const WnnWord &stem = *stemsi;
                if (addClause(clauseList, input, stem, NULL, terminal, all)) {
                    ret = true;
                }
            }
        }

        /* get clauses with ancillary word */
        int max = CLAUSE_COST * 2;
        for (int split = 1; split < input.length(); split++) {
            /* get ancillary patterns */
            QString str = input.mid(split);
            QList<WnnWord> fzks = getAncillaryPattern(str);
            if (fzks.isEmpty()) {
                continue;
            }

            /* get candidates of stem in a clause */
            str = input.mid(0, split);
            stems = getIndependentWords(str, all);
            if (stems.isEmpty()) {
                if (mDictionary->searchWord(OpenWnnDictionary::SEARCH_PREFIX, OpenWnnDictionary::ORDER_BY_FREQUENCY, str) <= 0) {
                    break;
                } else {
                    continue;
                }
            }
            /* make clauses */
            for (QList<WnnWord>::ConstIterator stemsi = stems.constBegin();
                 stemsi != stems.constEnd(); stemsi++) {
                const WnnWord &stem = *stemsi;
                if (all || stem.frequency > max) {
                    for (QList<WnnWord>::ConstIterator fzksi = fzks.constBegin();
                         fzksi != fzks.constEnd(); fzksi++) {
                        const WnnWord &fzk = *fzksi;
                        if (addClause(clauseList, input, stem, &fzk, terminal, all)) {
                            ret = true;
                            max = stem.frequency;
                        }
                    }
                }
            }
        }
        return ret;
    }

    bool addClause(QList<WnnClause> &clauseList, const QString &input,
                   const WnnWord &stem, const WnnWord *fzk, const WnnPOS &terminal, bool all)
    {
        QSharedPointer<WnnClause> clause;
        /* check if the part of speech is valid */
        if (fzk == NULL) {
            if (connectible(stem.partOfSpeech.right, terminal.left)) {
                clause.reset(new WnnClause(input, stem));
            }
        } else {
            if (connectible(stem.partOfSpeech.right, fzk->partOfSpeech.left)
                && connectible(fzk->partOfSpeech.right, terminal.left)) {
                clause.reset(new WnnClause(input, stem, *fzk));
            }
        }
        if (clause == NULL) {
            return false;
        }
        /*
        if (mFilter != NULL && !mFilter->isAllowed(clause)) {
            return false;
        }
        */

        /* store to the list */
        if (clauseList.isEmpty()) {
            /* add if the list is empty */
            clauseList.append(*clause);
            return true;
        } else {
            if (!all) {
                /* reserve only the best clause */
                const WnnClause &best = clauseList.first();
                if (best.frequency < clause->frequency) {
                    clauseList.insert(clauseList.begin(), *clause);
                    return true;
                }
            } else {
                /* reserve all clauses */
                QList<WnnClause>::Iterator clauseListi;
                for (clauseListi = clauseList.begin(); clauseListi != clauseList.end(); clauseListi++) {
                    const WnnClause &clausei = *clauseListi;
                    if (clausei.frequency < clause->frequency) {
                        break;
                    }
                }
                clauseList.insert(clauseListi, *clause);
                return true;
            }
        }

        return false;
    }

    bool connectible(int right, int left)
    {
        return left < mConnectMatrix.size() && right < mConnectMatrix.at(left).size() && mConnectMatrix.at(left).at(right);
    }

    QList<WnnWord> getAncillaryPattern(const QString &input)
    {
        if (input.length() == 0) {
            return QList<WnnWord>();
        }

        if (mFzkPatterns.contains(input)) {
            return mFzkPatterns[input];
        }

        /* set dictionaries */
        OpenWnnDictionary *dict = mDictionary;
        dict->clearDictionary();
        dict->clearApproxPattern();
        dict->setDictionary(6, 400, 500);

        for (int start = input.length() - 1; start >= 0; start--) {
            QString key = input.mid(start);

            if (mFzkPatterns.contains(key)) {
                continue;
            }

            QList<WnnWord> fzks;

            /* search ancillary words */
            dict->searchWord(OpenWnnDictionary::SEARCH_EXACT, OpenWnnDictionary::ORDER_BY_FREQUENCY, key);
            QSharedPointer<WnnWord> word;
            while ((word = dict->getNextWord()) != NULL) {
                fzks.append(*word);
            }

            /* concatenate sequence of ancillary words */
            for (int end = input.length() - 1; end > start; end--) {
                QString followKey = input.mid(end);
                if (!mFzkPatterns.contains(followKey))
                    continue;
                QList<WnnWord> &followFzks = mFzkPatterns[followKey];
                if (followFzks.isEmpty())
                    continue;
                dict->searchWord(OpenWnnDictionary::SEARCH_EXACT, OpenWnnDictionary::ORDER_BY_FREQUENCY, input.mid(start, end - start));
                while ((word = dict->getNextWord()) != NULL) {
                    for (QList<WnnWord>::ConstIterator followFzksi = followFzks.constBegin();
                         followFzksi != followFzks.constEnd(); followFzksi++) {
                        const WnnWord &follow = *followFzksi;
                        if (connectible(word->partOfSpeech.right, follow.partOfSpeech.left)) {
                            fzks.append(WnnWord(key, key, WnnPOS(word->partOfSpeech.left, follow.partOfSpeech.right)));
                        }
                    }
                }
            }

            mFzkPatterns[key] = fzks;
        }
        return mFzkPatterns[input];
    }

    QList<WnnWord> getIndependentWords(const QString &input, bool all)
    {
        if (input.length() == 0)
            return QList<WnnWord>();

        QMap<QString, QList<WnnWord> > &wordBag = all ? mAllIndepWordBag : mIndepWordBag;
        if (!wordBag.contains(input)) {
            QList<WnnWord> words;

            /* set dictionaries */
            OpenWnnDictionary *dict = mDictionary;
            dict->clearDictionary();
            dict->clearApproxPattern();
            dict->setDictionary(4, 0, 10);
            dict->setDictionary(5, 400, 500);
            dict->setDictionary(OpenWnnDictionary::INDEX_USER_DICTIONARY, FREQ_USER, FREQ_USER);
            dict->setDictionary(OpenWnnDictionary::INDEX_LEARN_DICTIONARY, FREQ_LEARN, FREQ_LEARN);

            QSharedPointer<WnnWord> word;
            if (all) {
                dict->searchWord(OpenWnnDictionary::SEARCH_EXACT, OpenWnnDictionary::ORDER_BY_FREQUENCY, input);
                /* store all words */
                while ((word = dict->getNextWord()) != NULL) {
                    if (input.compare(word->stroke) == 0) {
                        words.append(*word);
                    }
                }
            } else {
                dict->searchWord(OpenWnnDictionary::SEARCH_EXACT, OpenWnnDictionary::ORDER_BY_FREQUENCY, input);
                /* store a word which has an unique part of speech tag */
                while ((word = dict->getNextWord()) != NULL) {
                    if (input.compare(word->stroke) == 0) {
                        bool found = false;
                        for (QList<WnnWord>::ConstIterator list = words.constBegin();
                             list != words.constEnd(); list++) {
                            const WnnWord &w = *list;
                            if (w.partOfSpeech.right == word->partOfSpeech.right) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            words.append(*word);
                        }
                        if (word->frequency < 400) {
                            break;
                        }
                    }
                }
            }
            addAutoGeneratedCandidates(input, words, all);
            wordBag[input] = words;
        }
        return wordBag[input];
    }

    void addAutoGeneratedCandidates(const QString &input, QList<WnnWord> &wordList, bool all)
    {
        Q_UNUSED(all)
        wordList.append(WnnWord(input, input, mPosDefault, (CLAUSE_COST - 1) * input.length()));
    }

    WnnClause defaultClause(const QString &input)
    {
        return WnnClause(input, input, mPosDefault, (CLAUSE_COST - 1) * input.length());
    }

    /** Score(frequency value) of word in the learning dictionary */
    static const int FREQ_LEARN;
    /** Score(frequency value) of word in the user dictionary */
    static const int FREQ_USER;

    /** Maximum limit length of input */
    static const int MAX_INPUT_LENGTH;

    /** search cache for unique independent words (jiritsugo) */
    QMap<QString, QList<WnnWord> > mIndepWordBag;
    /** search cache for all independent words (jiritsugo) */
    QMap<QString, QList<WnnWord> > mAllIndepWordBag;
    /** search cache for ancillary words (fuzokugo) */
    QMap<QString, QList<WnnWord> > mFzkPatterns;

    /** connect matrix for generating a clause */
    QList<QBitArray> mConnectMatrix;

    /** dictionaries */
    QPointer<OpenWnnDictionary> mDictionary;

    /** part of speech (default) */
    WnnPOS mPosDefault;
    /** part of speech (end of clause/not end of sentence) */
    WnnPOS mPosEndOfClause1;
    /** part of speech (end of clause/any place) */
    WnnPOS mPosEndOfClause2;
    /** part of speech (end of sentence) */
    WnnPOS mPosEndOfClause3;

    /** cost value of a clause */
    static const int CLAUSE_COST;
};

const int OpenWnnClauseConverterJAJPPrivate::FREQ_LEARN = 600;
const int OpenWnnClauseConverterJAJPPrivate::FREQ_USER  = 500;
const int OpenWnnClauseConverterJAJPPrivate::MAX_INPUT_LENGTH = 50;
const int OpenWnnClauseConverterJAJPPrivate::CLAUSE_COST = -1000;

OpenWnnClauseConverterJAJP::OpenWnnClauseConverterJAJP(QObject *parent) :
    QObject(*new OpenWnnClauseConverterJAJPPrivate(), parent)
{
}

OpenWnnClauseConverterJAJP::~OpenWnnClauseConverterJAJP()
{
}

void OpenWnnClauseConverterJAJP::setDictionary(OpenWnnDictionary *dict)
{
    Q_D(OpenWnnClauseConverterJAJP);

    /* get connect matrix */
    d->mConnectMatrix = dict->getConnectMatrix();

    /* clear dictionary settings */
    d->mDictionary = dict;
    dict->clearDictionary();
    dict->clearApproxPattern();

    /* clear work areas */
    d->mIndepWordBag.clear();
    d->mAllIndepWordBag.clear();
    d->mFzkPatterns.clear();

    /* get part of speech tags */
    d->mPosDefault      = dict->getPOS(OpenWnnDictionary::POS_TYPE_MEISI);
    d->mPosEndOfClause1 = dict->getPOS(OpenWnnDictionary::POS_TYPE_V1);
    d->mPosEndOfClause2 = dict->getPOS(OpenWnnDictionary::POS_TYPE_V2);
    d->mPosEndOfClause3 = dict->getPOS(OpenWnnDictionary::POS_TYPE_V3);
}

QList<WnnClause> OpenWnnClauseConverterJAJP::convert(const QString &input)
{
    Q_D(OpenWnnClauseConverterJAJP);
    QList<WnnClause> convertResult;

    /* do nothing if no dictionary is specified */
    if (d->mConnectMatrix.isEmpty() || d->mDictionary == NULL)
        return convertResult;

    /* do nothing if the length of input exceeds the limit */
    if (input.length() > OpenWnnClauseConverterJAJPPrivate::MAX_INPUT_LENGTH)
        return convertResult;

    /* try single clause conversion */
    d->singleClauseConvert(convertResult, input, d->mPosEndOfClause2, true);

    return convertResult;
}

QSharedPointer<WnnSentence> OpenWnnClauseConverterJAJP::consecutiveClauseConvert(const QString &input)
{
    Q_D(OpenWnnClauseConverterJAJP);
    QList<WnnClause> clauses;

    /* clear the cache which is not matched */
    QList<QSharedPointer<WnnSentence> > sentence;
    for (int i = 0; i < input.length(); i++) {
        sentence.append(QSharedPointer<WnnSentence>());
    }

    /* consecutive clause conversion */
    for (int start = 0; start < input.length(); start++) {
        if (start != 0 && sentence[start - 1] == NULL) {
            continue;
        }

        /* limit the length of a clause */
        int end = input.length();
        if (end > start + 20) {
            end = start + 20;
        }
        /* make clauses */
        for ( ; end > start; end--) {
            int idx = end - 1;

            /* cutting a branch */
            if (sentence[idx] != NULL) {
                if (start != 0) {
                    if (sentence[idx]->frequency > sentence[start - 1]->frequency + OpenWnnClauseConverterJAJPPrivate::CLAUSE_COST + OpenWnnClauseConverterJAJPPrivate::FREQ_LEARN) {
                        /* there may be no way to be the best sequence from the 'start' */
                        break;
                    }
                } else {
                    if (sentence[idx]->frequency > OpenWnnClauseConverterJAJPPrivate::CLAUSE_COST + OpenWnnClauseConverterJAJPPrivate::FREQ_LEARN) {
                        /* there may be no way to be the best sequence from the 'start' */
                        break;
                    }
                }
            }

            QString key = input.mid(start, end - start);
            clauses.clear();
            if (end == input.length()) {
                /* get the clause which can be the end of the sentence */
                d->singleClauseConvert(clauses, key, d->mPosEndOfClause1, false);
            } else {
                /* get the clause which is not the end of the sentence */
                d->singleClauseConvert(clauses, key, d->mPosEndOfClause3, false);
            }
            WnnClause bestClause(clauses.isEmpty() ? d->defaultClause(key) : clauses.first());

            /* make a sub-sentence */
            QSharedPointer<WnnSentence> ws(start == 0 ? new WnnSentence(key, bestClause) : new WnnSentence(*sentence[start - 1], bestClause));
            ws->frequency += OpenWnnClauseConverterJAJPPrivate::CLAUSE_COST;

            /* update the best sub-sentence on the cache buffer */
            if (sentence[idx] == NULL || (sentence[idx]->frequency < ws->frequency)) {
                sentence[idx] = ws;
            }
        }
    }

    /* return the result of the consecutive clause conversion */
    if (sentence[input.length() - 1] != NULL) {
        return sentence[input.length() - 1];
    }
    return QSharedPointer<WnnSentence>();
}
