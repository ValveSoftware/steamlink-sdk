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
#include <hunspell/hunspell.h>
#include <QStringList>
#include <QDir>
#include "virtualkeyboarddebug.h"
#include <QTextCodec>
#include <QtCore/QLibraryInfo>

namespace QtVirtualKeyboard {

/*!
    \class QtVirtualKeyboard::HunspellInputMethodPrivate
    \internal
*/

HunspellInputMethodPrivate::HunspellInputMethodPrivate(HunspellInputMethod *q_ptr) :
    AbstractInputMethodPrivate(),
    q_ptr(q_ptr),
    hunspellWorker(new HunspellWorker()),
    locale(),
    word(),
    wordCandidates(),
    activeWordIndex(-1),
    wordCompletionPoint(2),
    ignoreUpdate(false),
    autoSpaceAllowed(false)
{
    if (hunspellWorker)
        hunspellWorker->start();
}

HunspellInputMethodPrivate::~HunspellInputMethodPrivate()
{
}

bool HunspellInputMethodPrivate::createHunspell(const QString &locale)
{
    if (!hunspellWorker)
        return false;
    if (this->locale != locale) {
        hunspellWorker->removeAllTasks();
        QString hunspellDataPath(QString::fromLatin1(qgetenv("QT_VIRTUALKEYBOARD_HUNSPELL_DATA_PATH").constData()));
        const QString pathListSep(
#if defined(Q_OS_WIN32)
            QStringLiteral(";")
#else
            QStringLiteral(":")
#endif
        );
        QStringList searchPaths(hunspellDataPath.split(pathListSep, QString::SkipEmptyParts));
        const QStringList defaultPaths = QStringList()
                << QDir(QLibraryInfo::location(QLibraryInfo::DataPath) + QStringLiteral("/qtvirtualkeyboard/hunspell")).absolutePath()
#if !defined(Q_OS_WIN32)
                << QStringLiteral("/usr/share/hunspell")
                << QStringLiteral("/usr/share/myspell/dicts")
#endif
                   ;
        for (const QString &defaultPath : defaultPaths) {
            if (!searchPaths.contains(defaultPath))
                searchPaths.append(defaultPath);
        }
        QSharedPointer<HunspellLoadDictionaryTask> loadDictionaryTask(new HunspellLoadDictionaryTask(locale, searchPaths));
        hunspellWorker->addTask(loadDictionaryTask);
        this->locale = locale;
    }
    return true;
}

void HunspellInputMethodPrivate::reset()
{
    if (clearSuggestions()) {
        Q_Q(HunspellInputMethod);
        emit q->selectionListChanged(SelectionListModel::WordCandidateList);
        emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList, activeWordIndex);
    }
    word.clear();
    autoSpaceAllowed = false;
}

bool HunspellInputMethodPrivate::updateSuggestions()
{
    bool wordCandidateListChanged = false;
    if (!word.isEmpty()) {
        if (hunspellWorker)
            hunspellWorker->removeAllTasksExcept<HunspellLoadDictionaryTask>();
        if (wordCandidates.isEmpty()) {
            wordCandidates.append(word);
            activeWordIndex = 0;
            wordCandidateListChanged = true;
        } else if (wordCandidates.at(0) != word) {
            wordCandidates.replace(0, word);
            activeWordIndex = 0;
            wordCandidateListChanged = true;
        }
        if (word.length() >= wordCompletionPoint) {
            if (hunspellWorker) {
                QSharedPointer<HunspellWordList> wordList(new HunspellWordList());
                QSharedPointer<HunspellBuildSuggestionsTask> buildSuggestionsTask(new HunspellBuildSuggestionsTask());
                buildSuggestionsTask->word = word;
                buildSuggestionsTask->wordList = wordList;
                buildSuggestionsTask->autoCorrect = false;
                hunspellWorker->addTask(buildSuggestionsTask);
                QSharedPointer<HunspellUpdateSuggestionsTask> updateSuggestionsTask(new HunspellUpdateSuggestionsTask());
                updateSuggestionsTask->wordList = wordList;
                Q_Q(HunspellInputMethod);
                q->connect(updateSuggestionsTask.data(), SIGNAL(updateSuggestions(QStringList, int)), SLOT(updateSuggestions(QStringList, int)));
                hunspellWorker->addTask(updateSuggestionsTask);
            }
        } else if (wordCandidates.length() > 1) {
            wordCandidates.clear();
            wordCandidates.append(word);
            activeWordIndex = 0;
            wordCandidateListChanged = true;
        }
    } else {
        wordCandidateListChanged = clearSuggestions();
    }
    return wordCandidateListChanged;
}

bool HunspellInputMethodPrivate::clearSuggestions()
{
    if (hunspellWorker)
        hunspellWorker->removeAllTasksExcept<HunspellLoadDictionaryTask>();
    if (wordCandidates.isEmpty())
        return false;
    wordCandidates.clear();
    activeWordIndex = -1;
    return true;
}

bool HunspellInputMethodPrivate::hasSuggestions() const
{
    return !wordCandidates.isEmpty();
}

bool HunspellInputMethodPrivate::isAutoSpaceAllowed() const
{
    Q_Q(const HunspellInputMethod);
    if (!autoSpaceAllowed)
        return false;
    if (q->inputEngine()->inputMode() != InputEngine::Latin)
        return false;
    InputContext *ic = q->inputContext();
    if (!ic)
        return false;
    Qt::InputMethodHints inputMethodHints = ic->inputMethodHints();
    return !inputMethodHints.testFlag(Qt::ImhUrlCharactersOnly) &&
           !inputMethodHints.testFlag(Qt::ImhEmailCharactersOnly);
}

bool HunspellInputMethodPrivate::isValidInputChar(const QChar &c) const
{
    if (c.isLetterOrNumber())
        return true;
    if (isJoiner(c))
        return true;
    return false;
}

bool HunspellInputMethodPrivate::isJoiner(const QChar &c) const
{
    if (c.isPunct() || c.isSymbol()) {
        Q_Q(const HunspellInputMethod);
        InputContext *ic = q->inputContext();
        if (ic) {
            Qt::InputMethodHints inputMethodHints = ic->inputMethodHints();
            if (inputMethodHints.testFlag(Qt::ImhUrlCharactersOnly) || inputMethodHints.testFlag(Qt::ImhEmailCharactersOnly))
                return QString(QStringLiteral(":/?#[]@!$&'()*+,;=-_.%")).contains(c);
        }
        ushort unicode = c.unicode();
        if (unicode == Qt::Key_Apostrophe || unicode == Qt::Key_Minus)
            return true;
    }
    return false;
}

} // namespace QtVirtualKeyboard
