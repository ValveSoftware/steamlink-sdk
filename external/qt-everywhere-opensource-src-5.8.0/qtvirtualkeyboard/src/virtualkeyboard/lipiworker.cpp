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

#include "lipiworker.h"
#include "virtualkeyboarddebug.h"

#include <QTime>

#include "LTKShapeRecognizer.h"
#include "LTKErrors.h"

namespace QtVirtualKeyboard {

/*!
    \class QtVirtualKeyboard::LipiTask
    \internal
*/

/*!
    \class QtVirtualKeyboard::LipiLoadModelDataTask
    \internal
*/

void LipiLoadModelDataTask::run()
{
    VIRTUALKEYBOARD_DEBUG() << "LipiLoadModelDataTask::run()";
#ifdef QT_VIRTUALKEYBOARD_DEBUG
    QTime perf;
    perf.start();
#endif
    int result = shapeRecognizer->loadModelData();
#ifdef QT_VIRTUALKEYBOARD_DEBUG
    VIRTUALKEYBOARD_DEBUG() << "LipiLoadModelDataTask::run(): time:" << perf.elapsed() << "ms";
#endif
    if (result != SUCCESS)
        qWarning() << QString("Error %1: %2").arg(result).arg(getErrorMessage(result).c_str());
}

/*!
    \class QtVirtualKeyboard::LipiRecognitionTask
    \internal
*/

LipiRecognitionTask::LipiRecognitionTask(const LTKCaptureDevice& deviceInfo,
                                         const LTKScreenContext& screenContext,
                                         const vector<int>& inSubsetOfClasses,
                                         float confThreshold,
                                         int numChoices,
                                         int resultId) :
    LipiTask(),
    deviceInfo(deviceInfo),
    screenContext(screenContext),
    inSubsetOfClasses(inSubsetOfClasses),
    confThreshold(confThreshold),
    numChoices(numChoices),
    resultVector(new vector<LTKShapeRecoResult>()),
    _resultId(resultId),
    stateRunning(false),
    stateCancelled(false)
{
}

void LipiRecognitionTask::run()
{
    VIRTUALKEYBOARD_DEBUG() << "LipiRecognitionTask::run()";

    if (!shapeRecognizer || !resultVector)
        return;

    {
        QMutexLocker stateGuard(&stateLock);
        stateRunning = true;
    }

    resultVector->clear();
    resultVector->reserve(numChoices);

    shapeRecognizer->setDeviceContext(deviceInfo);

#ifdef QT_VIRTUALKEYBOARD_DEBUG
    QTime perf;
    perf.start();
#endif
    shapeRecognizer->recognize(traceGroup, screenContext,
                               inSubsetOfClasses, confThreshold,
                               numChoices, *resultVector);

#ifdef QT_VIRTUALKEYBOARD_DEBUG
    int perfElapsed = perf.elapsed();
#endif

    {
        QMutexLocker stateGuard(&stateLock);
        stateRunning = false;
        if (stateCancelled)
            resultVector->clear();
#ifdef QT_VIRTUALKEYBOARD_DEBUG
        VIRTUALKEYBOARD_DEBUG() << "LipiRecognitionTask::run(): time:" << perfElapsed << "ms" << (stateCancelled ? "(cancelled)" : "");
#endif
    }
}

bool LipiRecognitionTask::cancelRecognition()
{
    QMutexLocker stateGuard(&stateLock);
    stateCancelled = true;
    bool result = (stateRunning && shapeRecognizer);
    if (result)
        shapeRecognizer->requestCancelRecognition();
    return result;
}

int LipiRecognitionTask::resultId() const
{
    return _resultId;
}

/*!
    \class QtVirtualKeyboard::LipiRecognitionResultsTask
    \internal
*/

LipiRecognitionResultsTask::LipiRecognitionResultsTask(QSharedPointer<vector<LTKShapeRecoResult> > resultVector,
                                                       const QMap<int, QChar> &unicodeMap,
                                                       int resultId) :
    LipiTask(),
    resultVector(resultVector),
    unicodeMap(unicodeMap),
    _resultId(resultId)
{
}

void LipiRecognitionResultsTask::run()
{
    if (!resultVector || unicodeMap.isEmpty())
        return;

    QVariantList resultList;
    for (vector<LTKShapeRecoResult>::const_iterator i = resultVector->begin();
         i != resultVector->end(); i++) {
        QVariantMap result;
        int shapeId = i->getShapeId();
        result["resultId"] = _resultId;
        result["shapeId"] = shapeId;
        result["unicode"] = unicodeMap.value(shapeId);
        result["confidence"] = i->getConfidence();
        resultList.append(result);
    }

    if (resultList.isEmpty())
        return;

    emit resultsAvailable(resultList);
}

/*!
    \class QtVirtualKeyboard::LipiWorker
    \internal
*/

LipiWorker::LipiWorker(LTKShapeRecognizer *shapeRecognizer, QObject *parent) :
    QThread(parent),
    taskSema(),
    taskLock(),
    shapeRecognizer(shapeRecognizer),
    abort(false)
{
}

LipiWorker::~LipiWorker()
{
    abort = true;
    taskSema.release();
    wait();
    if (shapeRecognizer)
        shapeRecognizer->unloadModelData();
}

void LipiWorker::addTask(QSharedPointer<LipiTask> task)
{
    if (task) {
        QMutexLocker guard(&taskLock);
        taskList.append(task);
        taskSema.release();
    }
}

int LipiWorker::removeTask(QSharedPointer<LipiTask> task)
{
    int count = 0;
    if (task) {
        QMutexLocker guard(&taskLock);
        count = taskList.removeAll(task);
        taskSema.acquire(qMin(count, taskSema.available()));
    }
    return count;
}

int LipiWorker::removeAllTasks()
{
    QMutexLocker guard(&taskLock);
    int count = taskList.count();
    taskList.clear();
    if (taskSema.available())
        taskSema.acquire(taskSema.available());
    return count;
}

void LipiWorker::run()
{
    while (!abort) {
        taskSema.acquire();
        if (abort)
            break;
        QSharedPointer<LipiTask> currentTask;
        {
            QMutexLocker guard(&taskLock);
            if (!taskList.isEmpty()) {
                currentTask = taskList.front();
                taskList.pop_front();
            }
        }
        if (currentTask) {
            currentTask->shapeRecognizer = shapeRecognizer;
            currentTask->run();
        }
    }
}

} // namespace QtVirtualKeyboard
