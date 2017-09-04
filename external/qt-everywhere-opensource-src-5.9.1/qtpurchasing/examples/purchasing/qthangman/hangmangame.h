/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Purchasing module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef HANGMANGAME_H
#define HANGMANGAME_H

#include <QObject>
#include <QStringList>
#include <QMutex>
#include <QtPurchasing>
#include <QSettings>

class HangmanGame : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString word READ word NOTIFY wordChanged)
    Q_PROPERTY(QString lettersOwned READ lettersOwned NOTIFY lettersOwnedChanged)
    Q_PROPERTY(QString vowels READ vowels CONSTANT)
    Q_PROPERTY(QString consonants READ consonants CONSTANT)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY errorCountChanged)
    Q_PROPERTY(bool vowelsUnlocked READ vowelsUnlocked WRITE setVowelsUnlocked NOTIFY vowelsUnlockedChanged)
    Q_PROPERTY(int vowelsAvailable READ vowelsAvailable WRITE setVowelsAvailable NOTIFY vowelsAvailableChanged)
    Q_PROPERTY(int wordsGiven READ wordsGiven WRITE setWordsGiven NOTIFY wordsGivenChanged)
    Q_PROPERTY(int  wordsGuessedCorrectly READ wordsGuessedCorrectly WRITE setWordsGuessedCorrectly NOTIFY wordsGuessedCorrectlyChanged)
    Q_PROPERTY(int score READ score WRITE setScore NOTIFY scoreChanged)

public:
    explicit HangmanGame(QObject *parent = 0);
    Q_INVOKABLE void reset();
    Q_INVOKABLE void reveal();
    Q_INVOKABLE void gameOverReveal();
    Q_INVOKABLE void requestLetter(const QString &letterString);
    Q_INVOKABLE void guessWord(const QString &word);
    Q_INVOKABLE bool isVowel(const QString &letter);
    Q_INVOKABLE void setVowelsAvailable(int count);
    Q_INVOKABLE void setWordsGiven(int count);
    Q_INVOKABLE void setWordsGuessedCorrectly(int count);
    Q_INVOKABLE void setScore(int score);

    QString word() const { return m_word; }
    QString lettersOwned() const { return m_lettersOwned; }
    QString vowels() const;
    QString consonants() const;
    int errorCount() const;
    bool vowelsUnlocked() const;
    void setVowelsUnlocked(bool vowelsUnlocked);
    int vowelsAvailable() const;
    int wordsGiven() const;
    int wordsGuessedCorrectly() const;
    int score() const;

signals:
    void wordChanged();
    void lettersOwnedChanged();
    void errorCountChanged();
    void vowelBought(const QChar &vowel);
    void purchaseWasSuccessful(bool wasSuccessful);
    void vowelsUnlockedChanged(bool unlocked);
    void vowelsAvailableChanged(int arg);
    void wordsGivenChanged(int arg);
    void wordsGuessedCorrectlyChanged(int arg);
    void scoreChanged(int arg);

private slots:
    void registerLetterBought(const QChar &letter);

private:
    void chooseRandomWord();
    void initWordList();
    int calculateEarnedVowels();
    int calculateEarnedPoints();

    QString m_word;
    QString m_lettersOwned;
    QStringList m_wordList;
    QMutex m_lock;
    bool m_vowelsUnlocked;
    QSettings m_persistentSettings;
    int m_vowelsAvailable;
    int m_wordsGiven;
    int m_wordsGuessedCorrectly;
    int m_score;
};

#endif // HANGMANGAME_H
