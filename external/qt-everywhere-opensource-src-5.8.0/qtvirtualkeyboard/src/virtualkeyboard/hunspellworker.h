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

#ifndef HUNSPELLWORKER_H
#define HUNSPELLWORKER_H

#include <QThread>
#include <QSemaphore>
#include <QMutex>
#include <QStringList>
#include <QSharedPointer>
#include <hunspell/hunspell.h>

QT_BEGIN_NAMESPACE
class QTextCodec;
QT_END_NAMESPACE

namespace QtVirtualKeyboard {

class HunspellTask : public QObject
{
    Q_OBJECT
public:
    explicit HunspellTask(QObject *parent = 0) :
        QObject(parent),
        hunspell(0)
    {}

    virtual void run() = 0;

    Hunhandle *hunspell;
};

class HunspellLoadDictionaryTask : public HunspellTask
{
    Q_OBJECT
public:
    explicit HunspellLoadDictionaryTask(const QString &locale, const QStringList &searchPaths);

    void run();

    Hunhandle **hunspellPtr;
    const QString locale;
    const QStringList searchPaths;
};

class HunspellWordList
{
public:
    HunspellWordList() :
        list(),
        index(-1)
    {}

    QStringList list;
    int index;
};

class HunspellBuildSuggestionsTask : public HunspellTask
{
    Q_OBJECT
    const QTextCodec *textCodec;
public:
    QString word;
    QSharedPointer<HunspellWordList> wordList;
    bool autoCorrect;

    void run();
    bool spellCheck(const QString &word);
    int levenshteinDistance(const QString &s, const QString &t);
    QString removeAccentsAndDiacritics(const QString& s);
};

class HunspellUpdateSuggestionsTask : public HunspellTask
{
    Q_OBJECT
public:
    QSharedPointer<HunspellWordList> wordList;

    void run();

signals:
    void updateSuggestions(const QStringList &wordList, int activeWordIndex);
};

class HunspellWorker : public QThread
{
    Q_OBJECT
public:
    explicit HunspellWorker(QObject *parent = 0);
    ~HunspellWorker();

    void addTask(QSharedPointer<HunspellTask> task);
    void removeAllTasks();

    template <class X>
    void removeAllTasksExcept() {
        QMutexLocker guard(&taskLock);
        for (int i = 0; i < taskList.size();) {
            QSharedPointer<X> task(taskList[i].objectCast<X>());
            if (!task)
                taskList.removeAt(i);
            else
                i++;
        }
    }

protected:
    void run();

private:
    void createHunspell();

private:
    friend class HunspellLoadDictionaryTask;
    QList<QSharedPointer<HunspellTask> > taskList;
    QSemaphore taskSema;
    QMutex taskLock;
    Hunhandle *hunspell;
    bool abort;
};

} // namespace QtVirtualKeyboard

#endif // HUNSPELLWORKER_H
