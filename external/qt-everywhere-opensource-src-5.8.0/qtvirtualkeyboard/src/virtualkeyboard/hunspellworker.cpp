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

#include "hunspellworker.h"
#include "virtualkeyboarddebug.h"
#include <QVector>
#include <QTextCodec>
#include <QFileInfo>
#include <QTime>

namespace QtVirtualKeyboard {

/*!
    \class QtVirtualKeyboard::HunspellTask
    \internal
*/

/*!
    \class QtVirtualKeyboard::HunspellWordList
    \internal
*/

/*!
    \class QtVirtualKeyboard::HunspellLoadDictionaryTask
    \internal
*/

HunspellLoadDictionaryTask::HunspellLoadDictionaryTask(const QString &locale, const QStringList &searchPaths) :
    HunspellTask(),
    hunspellPtr(0),
    locale(locale),
    searchPaths(searchPaths)
{
}

void HunspellLoadDictionaryTask::run()
{
    Q_ASSERT(hunspellPtr != 0);

    VIRTUALKEYBOARD_DEBUG() << "HunspellLoadDictionaryTask::run(): locale:" << locale;

#ifdef QT_VIRTUALKEYBOARD_DEBUG
    QTime perf;
    perf.start();
#endif

    if (*hunspellPtr) {
        Hunspell_destroy(*hunspellPtr);
        *hunspellPtr = 0;
    }

    QString affPath;
    QString dicPath;
    for (const QString &searchPath : searchPaths) {
        affPath = QStringLiteral("%1/%2.aff").arg(searchPath).arg(locale);
        if (QFileInfo::exists(affPath)) {
            dicPath = QStringLiteral("%1/%2.dic").arg(searchPath).arg(locale);
            if (QFileInfo::exists(dicPath))
                break;
            dicPath.clear();
        }
        affPath.clear();
    }

    if (affPath.isEmpty() || dicPath.isEmpty()) {
        VIRTUALKEYBOARD_DEBUG() << "Hunspell dictionary is missing for the" << locale << "language. Search paths" << searchPaths;
        return;
    }

    *hunspellPtr = Hunspell_create(affPath.toUtf8().constData(), dicPath.toUtf8().constData());
    if (*hunspellPtr) {
        /*  Make sure the encoding used by the dictionary is supported
            by the QTextCodec.
        */
        if (!QTextCodec::codecForName(Hunspell_get_dic_encoding(*hunspellPtr))) {
            qWarning() << "The Hunspell dictionary" << dicPath << "cannot be used because it uses an unknown text codec" << QString(Hunspell_get_dic_encoding(*hunspellPtr));
            Hunspell_destroy(*hunspellPtr);
            *hunspellPtr = 0;
        }
    }

#ifdef QT_VIRTUALKEYBOARD_DEBUG
    VIRTUALKEYBOARD_DEBUG() << "HunspellLoadDictionaryTask::run(): time:" << perf.elapsed() << "ms";
#endif
}

/*!
    \class QtVirtualKeyboard::HunspellBuildSuggestionsTask
    \internal
*/

void HunspellBuildSuggestionsTask::run()
{
#ifdef QT_VIRTUALKEYBOARD_DEBUG
    QTime perf;
    perf.start();
#endif

    wordList->list.append(word);
    wordList->index = 0;

    /*  Select text codec based on the dictionary encoding.
        Hunspell_get_dic_encoding() should always return at least
        "ISO8859-1", but you can never be too sure.
     */
    textCodec = QTextCodec::codecForName(Hunspell_get_dic_encoding(hunspell));
    if (!textCodec)
        return;

    char **slst = 0;
    int n = Hunspell_suggest(hunspell, &slst, textCodec->fromUnicode(word).constData());
    if (n > 0) {
        /*  Collect word candidates from the Hunspell suggestions.
            Insert word completions in the beginning of the list.
        */
        const int firstWordCompletionIndex = wordList->list.length();
        int lastWordCompletionIndex = firstWordCompletionIndex;
        bool suggestCapitalization = false;
        for (int i = 0; i < n; i++) {
            QString wordCandidate(textCodec->toUnicode(slst[i]));
            wordCandidate.replace(QChar(0x2019), '\'');
            if (wordCandidate.compare(word) != 0) {
                QString normalizedWordCandidate = removeAccentsAndDiacritics(wordCandidate);
                /*  Prioritize word Capitalization */
                if (!suggestCapitalization && !wordCandidate.compare(word, Qt::CaseInsensitive)) {
                    wordList->list.insert(1, wordCandidate);
                    lastWordCompletionIndex++;
                    suggestCapitalization = true;
                /*  Prioritize word completions, missing punctuation or missing accents */
                } else if (normalizedWordCandidate.startsWith(word) ||
                    wordCandidate.contains(QChar('\''))) {
                    wordList->list.insert(lastWordCompletionIndex++, wordCandidate);
                } else {
                    wordList->list.append(wordCandidate);
                }
            }
        }
        /*  Prioritize words with missing spaces next to word completions.
        */
        for (int i = lastWordCompletionIndex; i < wordList->list.length(); i++) {
            if (QString(wordList->list.at(i)).replace(" ", "").compare(word) == 0) {
                if (i != lastWordCompletionIndex) {
                    wordList->list.move(i, lastWordCompletionIndex);
                }
                lastWordCompletionIndex++;
            }
        }
        /*  Do spell checking and suggest the first candidate, if:
            - the word matches partly the suggested word; or
            - the quality of the suggested word is good enough.

            The quality is measured here using the Levenshtein Distance,
            which may be suboptimal for the purpose, but gives some clue
            how much the suggested word differs from the given word.
        */
        if (autoCorrect && wordList->list.length() > 1 && (!spellCheck(word) || suggestCapitalization)) {
            if (lastWordCompletionIndex > firstWordCompletionIndex || levenshteinDistance(word, wordList->list.at(firstWordCompletionIndex)) < 3)
                wordList->index = firstWordCompletionIndex;
        }
    }
    Hunspell_free_list(hunspell, &slst, n);

#ifdef QT_VIRTUALKEYBOARD_DEBUG
    VIRTUALKEYBOARD_DEBUG() << "HunspellBuildSuggestionsTask::run(): time:" << perf.elapsed() << "ms";
#endif
}

bool HunspellBuildSuggestionsTask::spellCheck(const QString &word)
{
    if (!hunspell)
        return false;
    if (word.contains(QRegExp("[0-9]")))
        return true;
    return Hunspell_spell(hunspell, textCodec->fromUnicode(word).constData()) != 0;
}

// source: http://en.wikipedia.org/wiki/Levenshtein_distance
int HunspellBuildSuggestionsTask::levenshteinDistance(const QString &s, const QString &t)
{
    if (s == t)
        return 0;
    if (s.length() == 0)
        return t.length();
    if (t.length() == 0)
        return s.length();
    QVector<int> v0(t.length() + 1);
    QVector<int> v1(t.length() + 1);
    for (int i = 0; i < v0.size(); i++)
        v0[i] = i;
    for (int i = 0; i < s.size(); i++) {
        v1[0] = i + 1;
        for (int j = 0; j < t.length(); j++) {
            int cost = (s[i].toLower() == t[j].toLower()) ? 0 : 1;
            v1[j + 1] = qMin(qMin(v1[j] + 1, v0[j + 1] + 1), v0[j] + cost);
        }
        for (int j = 0; j < v0.size(); j++)
            v0[j] = v1[j];
    }
    return v1[t.length()];
}

QString HunspellBuildSuggestionsTask::removeAccentsAndDiacritics(const QString& s)
{
    QString normalized = s.normalized(QString::NormalizationForm_D);
    for (int i = 0; i < normalized.length();) {
        QChar::Category category = normalized[i].category();
        if (category <= QChar::Mark_Enclosing) {
            normalized.remove(i, 1);
        } else {
            i++;
        }
    }
    return normalized;
}

/*!
    \class QtVirtualKeyboard::HunspellUpdateSuggestionsTask
    \internal
*/

void HunspellUpdateSuggestionsTask::run()
{
    emit updateSuggestions(wordList->list, wordList->index);
}

/*!
    \class QtVirtualKeyboard::HunspellWorker
    \internal
*/

HunspellWorker::HunspellWorker(QObject *parent) :
    QThread(parent),
    taskSema(),
    taskLock(),
    hunspell(0),
    abort(false)
{
}

HunspellWorker::~HunspellWorker()
{
    abort = true;
    taskSema.release(1);
    wait();
}

void HunspellWorker::addTask(QSharedPointer<HunspellTask> task)
{
    if (task) {
        QMutexLocker guard(&taskLock);
        taskList.append(task);
        taskSema.release();
    }
}

void HunspellWorker::removeAllTasks()
{
    QMutexLocker guard(&taskLock);
    taskList.clear();
}

void HunspellWorker::run()
{
    while (!abort) {
        taskSema.acquire();
        if (abort)
            break;
        QSharedPointer<HunspellTask> currentTask;
        {
            QMutexLocker guard(&taskLock);
            if (!taskList.isEmpty()) {
                currentTask = taskList.front();
                taskList.pop_front();
            }
        }
        if (currentTask) {
            QSharedPointer<HunspellLoadDictionaryTask> loadDictionaryTask(currentTask.objectCast<HunspellLoadDictionaryTask>());
            if (loadDictionaryTask)
                loadDictionaryTask->hunspellPtr = &hunspell;
            else if (hunspell)
                currentTask->hunspell = hunspell;
            else
                continue;
            currentTask->run();
        }
    }
    if (hunspell) {
        Hunspell_destroy(hunspell);
        hunspell = 0;
    }
}

} // namespace QtVirtualKeyboard
