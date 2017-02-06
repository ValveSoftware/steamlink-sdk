/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Purchasing module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3-COMM$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "hangmangame.h"
#include <QFile>
#include <QDebug>
#include <time.h>
#include <QBuffer>
#include <QtConcurrent/QtConcurrentRun>

HangmanGame::HangmanGame(QObject *parent)
    : QObject(parent)
    , m_lock(QMutex::Recursive)
    , m_vowelsUnlocked(false)
{
    qsrand(::time(0));
    connect(this, SIGNAL(vowelBought(QChar)), this, SLOT(registerLetterBought(QChar)));

    QtConcurrent::run(this, &HangmanGame::initWordList);

    m_vowelsUnlocked = m_persistentSettings.value("Hangman/vowelsUnlocked", false).toBool();
    m_vowelsAvailable = m_persistentSettings.value("Hangman/vowelsAvailable", 0).toInt();
    m_wordsGiven = m_persistentSettings.value("Hangman/wordsGiven", 0).toInt();
    m_wordsGuessedCorrectly = m_persistentSettings.value("Hangman/wordsGuessedCorrectly", 0).toInt();
    m_score = m_persistentSettings.value("Hangman/score", 0).toInt();
}

void HangmanGame::reset()
{
    m_lettersOwned.clear();
    emit lettersOwnedChanged();
    emit errorCountChanged();
    chooseRandomWord();
}

void HangmanGame::reveal()
{
    m_lettersOwned += vowels() + consonants();
    emit errorCountChanged();
    emit lettersOwnedChanged();
}

void HangmanGame::gameOverReveal()
{
    m_lettersOwned += vowels() + consonants();
    emit lettersOwnedChanged();
}

void HangmanGame::requestLetter(const QString &letterString)
{
    Q_ASSERT(letterString.size() == 1);
    QChar letter = letterString.at(0);
    registerLetterBought(letter);
}

void HangmanGame::guessWord(const QString &word)
{
    if (word.compare(m_word, Qt::CaseInsensitive) == 0) {
        //Determine how many vowels were earned
        setVowelsAvailable(m_vowelsAvailable + calculateEarnedVowels());
        //score is number of remaining consonants + remaining errors
        setScore(m_score + calculateEarnedPoints());
        m_lettersOwned += m_word.toUpper();
    } else {
        // Small hack to get an additional penalty for guessing wrong
        static int i=0;
        Q_ASSERT(i < 10);
        m_lettersOwned += QString::number(i++);
        emit errorCountChanged();
    }
    emit lettersOwnedChanged();
}

bool HangmanGame::isVowel(const QString &letter)
{
    Q_ASSERT(letter.size() == 1);
    QChar letterChar = letter.at(0);
    return vowels().contains(letterChar);
}

QString HangmanGame::vowels() const
{
    return QStringLiteral("AEIOU");
}

QString HangmanGame::consonants() const
{
    return QStringLiteral("BCDFGHJKLMNPQRSTVWXYZ");
}

int HangmanGame::errorCount() const
{
    int count = 0;
    foreach (QChar c, m_lettersOwned) {
        if (!m_word.contains(c))
            ++count;
    }
    return count;
}

bool HangmanGame::vowelsUnlocked() const
{
    return m_vowelsUnlocked;
}

void HangmanGame::setVowelsUnlocked(bool vowelsUnlocked)
{
    if (m_vowelsUnlocked != vowelsUnlocked) {
        m_vowelsUnlocked = vowelsUnlocked;
        m_persistentSettings.setValue("Hangman/vowelsUnlocked", m_vowelsUnlocked);
        emit vowelsUnlockedChanged(m_vowelsUnlocked);
    }
}

int HangmanGame::vowelsAvailable() const
{
    return m_vowelsAvailable;
}

int HangmanGame::wordsGiven() const
{
    return m_wordsGiven;
}

int HangmanGame::wordsGuessedCorrectly() const
{
    return m_wordsGuessedCorrectly;
}

int HangmanGame::score() const
{
    return m_score;
}

void HangmanGame::setScore(int score)
{
    if (m_score != score) {
        m_score = score;
        m_persistentSettings.setValue("Hangman/score", m_score);
        emit scoreChanged(score);
    }
}

void HangmanGame::setWordsGiven(int count)
{
    if (m_wordsGiven != count) {
        m_wordsGiven = count;
        m_persistentSettings.setValue("Hangman/wordsGiven", m_wordsGiven);
        emit wordsGivenChanged(count);
    }
}

void HangmanGame::setWordsGuessedCorrectly(int count)
{
    if (m_wordsGuessedCorrectly != count) {
        m_wordsGuessedCorrectly = count;
        m_persistentSettings.setValue("Hangman/wordsGuessedCorrectly", m_wordsGuessedCorrectly);
        emit wordsGuessedCorrectlyChanged(count);
    }
}

void HangmanGame::setVowelsAvailable(int count)
{
    if (m_vowelsAvailable != count) {
        m_vowelsAvailable = count;
        m_persistentSettings.setValue("Hangman/vowelsAvailable", m_vowelsAvailable);
        emit vowelsAvailableChanged(count);
    }
}

void HangmanGame::registerLetterBought(const QChar &letter)
{
    if (m_lettersOwned.contains(letter))
        return;

    m_lettersOwned.append(letter);
    emit lettersOwnedChanged();

    if (!m_word.contains(letter))
        emit errorCountChanged();
}

void HangmanGame::chooseRandomWord()
{
    QMutexLocker locker(&m_lock);
    if (m_wordList.isEmpty())
        return;

    m_word = m_wordList.at(qrand() % m_wordList.size());
    emit wordChanged();
}

void HangmanGame::initWordList()
{
    QMutexLocker locker(&m_lock);
    qsrand(::time(0) + 1000);
    QFile file(":/enable2.txt");
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray allData = file.readAll();
        QBuffer buffer(&allData);
        if (!buffer.open(QIODevice::ReadOnly))
            qFatal("Couldn't open buffer for reading!");

        while (!buffer.atEnd()) {
            QByteArray ba = buffer.readLine().trimmed().toUpper();
            if (!ba.isEmpty() && ba.length() < 10)
                m_wordList.append(QString::fromLatin1(ba));
        }
    }

    chooseRandomWord();
}

int HangmanGame::calculateEarnedVowels()
{
    int total = 0;
    for (int i = 0; i < m_word.length(); ++i) {
        if (isVowel(QString(m_word[i])) && !m_lettersOwned.contains(QString(m_word[i])))
            total++;
    }
    return total;
}

int HangmanGame::calculateEarnedPoints()
{
    int total = 0;
    for (int i = 0; i < m_word.length(); ++i) {
        if (consonants().contains(QString(m_word[i])) && !m_lettersOwned.contains(QString(m_word[i])))
            total++;
    }
    total += 8 - errorCount();
    return total;
}
