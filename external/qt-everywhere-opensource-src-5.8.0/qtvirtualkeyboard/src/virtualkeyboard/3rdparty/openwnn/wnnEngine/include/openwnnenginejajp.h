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

#ifndef OPENWNNENGINEJAJP_H
#define OPENWNNENGINEJAJP_H

#include <QObject>
#include "composingtext.h"

class OpenWnnEngineJAJPPrivate;

class OpenWnnEngineJAJP : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(OpenWnnEngineJAJP)
    Q_DECLARE_PRIVATE(OpenWnnEngineJAJP)
public:
    explicit OpenWnnEngineJAJP(QObject *parent = 0);
    ~OpenWnnEngineJAJP();

    enum DictionaryType {
        /** Dictionary type (default) */
        DIC_LANG_INIT = 0,
        /** Dictionary type (Japanese standard) */
        DIC_LANG_JP = 0,
        /** Dictionary type (English standard) */
        DIC_LANG_EN = 1,
        /** Dictionary type (Japanese person's name) */
        DIC_LANG_JP_PERSON_NAME = 2,
        /** Dictionary type (User dictionary) */
        DIC_USERDIC = 3,
        /** Dictionary type (Japanese EISU-KANA conversion) */
        DIC_LANG_JP_EISUKANA = 4,
        /** Dictionary type (e-mail/URI) */
        DIC_LANG_EN_EMAIL_ADDRESS = 5,
        /** Dictionary type (Japanese postal address) */
        DIC_LANG_JP_POSTAL_ADDRESS = 6,
    };

    enum KeyboardType {
        /** Keyboard type (not defined) */
        KEYBOARD_UNDEF = 0,
        /** Keyboard type (12-keys) */
        KEYBOARD_KEYPAD12 = 1,
        /** Keyboard type (Qwerty) */
        KEYBOARD_QWERTY = 2,
    };

    enum {
        /** Score(frequency value) of word in the learning dictionary */
        FREQ_LEARN = 600,
        /** Score(frequency value) of word in the user dictionary */
        FREQ_USER = 500,
        /** Maximum limit length of output */
        MAX_OUTPUT_LENGTH = 50,
        /** Limitation of predicted candidates */
        PREDICT_LIMIT = 100,
        /** Limitation of candidates one-line */
        LIMIT_OF_CANDIDATES_1LINE = 500,
    };

    bool setDictionary(DictionaryType type);
    int predict(const ComposingText &text, int minLen, int maxLen);
    int convert(ComposingText &text);
    QSharedPointer<WnnWord> getNextCandidate();
    bool learn(WnnWord &word);
    void breakSequence();
    int makeCandidateListOf(int clausePosition);
};

#endif // OPENWNNENGINEJAJP_H
