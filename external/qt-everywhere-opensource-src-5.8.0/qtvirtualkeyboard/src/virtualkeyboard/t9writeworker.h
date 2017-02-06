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

#ifndef T9WRITEWORKER_H
#define T9WRITEWORKER_H

#include "trace.h"

#include <QThread>
#include <QSemaphore>
#include <QMutex>
#include <QStringList>
#include <QSharedPointer>
#include <QPointer>
#include <QMap>
#include <QVector>

#include "decuma_hwr.h"

namespace QtVirtualKeyboard {

class T9WriteTask : public QObject
{
    Q_OBJECT
public:
    explicit T9WriteTask(QObject *parent = 0);

    virtual void run() = 0;

    void wait();

    friend class T9WriteWorker;

protected:
    DECUMA_SESSION *decumaSession;

private:
    QSemaphore runSema;
};

class T9WriteDictionaryTask : public T9WriteTask
{
    Q_OBJECT
public:
    explicit T9WriteDictionaryTask(const QString &fileUri,
                                   const DECUMA_MEM_FUNCTIONS &memFuncs);

    void run();

    const QString fileUri;
    const DECUMA_MEM_FUNCTIONS &memFuncs;

signals:
    void completed(const QString &fileUri, void *dictionary);
};

class T9WriteRecognitionResult
{
    Q_DISABLE_COPY(T9WriteRecognitionResult)

public:
    explicit T9WriteRecognitionResult(int id, int maxResults, int maxCharsPerWord);

    QVector<DECUMA_HWR_RESULT> results;
    DECUMA_UINT16 numResults;
    int instantGesture;
    const int id;
    const int maxResults;
    const int maxCharsPerWord;
private:
    QVector<DECUMA_UNICODE> _chars;
    QVector<DECUMA_INT16> _symbolChars;
    QVector<DECUMA_INT16> _symbolStrokes;
};

class T9WriteRecognitionTask : public T9WriteTask
{
    Q_OBJECT
public:
    explicit T9WriteRecognitionTask(QSharedPointer<T9WriteRecognitionResult> result,
                                    const DECUMA_INSTANT_GESTURE_SETTINGS &instantGestureSettings,
                                    BOOST_LEVEL boostLevel,
                                    const QString &stringStart);

    void run();
    bool cancelRecognition();
    int resultId() const;

private:
    static int shouldAbortRecognize(void *pUserData);
    friend int shouldAbortRecognize(void *pUserData);

private:
    QSharedPointer<T9WriteRecognitionResult> result;
    DECUMA_INSTANT_GESTURE_SETTINGS instantGestureSettings;
    BOOST_LEVEL boostLevel;
    QString stringStart;
    QMutex stateLock;
    bool stateCancelled;
};

class T9WriteRecognitionResultsTask : public T9WriteTask
{
    Q_OBJECT
public:
    explicit T9WriteRecognitionResultsTask(QSharedPointer<T9WriteRecognitionResult> result);

    void run();

signals:
    void resultsAvailable(const QVariantList &resultList);

private:
    QSharedPointer<T9WriteRecognitionResult> result;
};

class T9WriteWorker : public QThread
{
    Q_OBJECT
public:
    explicit T9WriteWorker(DECUMA_SESSION *decumaSession, QObject *parent = 0);
    ~T9WriteWorker();

    void addTask(QSharedPointer<T9WriteTask> task);
    int removeTask(QSharedPointer<T9WriteTask> task);
    int removeAllTasks();

    template <class X>
    int removeAllTasks() {
        QMutexLocker guard(&taskLock);
        int count = 0;
        for (int i = 0; i < taskList.size();) {
            QSharedPointer<X> task(taskList[i].objectCast<X>());
            if (task) {
                taskList.removeAt(i);
                ++count;
            } else {
                ++i;
            }
        }
        return count;
    }

protected:
    void run();

private:
    QList<QSharedPointer<T9WriteTask> > taskList;
    QSemaphore taskSema;
    QMutex taskLock;
    DECUMA_SESSION *decumaSession;
    bool abort;
};

} // namespace QtVirtualKeyboard

#endif // T9WRITEWORKER_H
