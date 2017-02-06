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

#ifndef LIPIWORKER_H
#define LIPIWORKER_H

#include <QThread>
#include <QSemaphore>
#include <QMutex>
#include <QStringList>
#include <QSharedPointer>
#include <QMap>

#include "LTKTypes.h"
#include "LTKCaptureDevice.h"
#include "LTKScreenContext.h"
#include "LTKTraceGroup.h"
#include "LTKChannel.h"
#include "LTKTraceFormat.h"
#include "LTKTrace.h"
#include "LTKShapeRecognizer.h"
#include "LTKShapeRecoResult.h"

namespace QtVirtualKeyboard {

class LipiTask : public QObject
{
    Q_OBJECT
public:
    explicit LipiTask(QObject *parent = 0) :
        QObject(parent),
        shapeRecognizer(0)
    {}

    virtual void run() = 0;

    LTKShapeRecognizer *shapeRecognizer;
};

class LipiLoadModelDataTask : public LipiTask
{
    Q_OBJECT
public:
    void run();
};

class LipiRecognitionTask : public LipiTask
{
    Q_OBJECT
public:
    explicit LipiRecognitionTask(const LTKCaptureDevice& deviceInfo,
                                 const LTKScreenContext& screenContext,
                                 const vector<int>& inSubsetOfClasses,
                                 float confThreshold,
                                 int numChoices,
                                 int resultId);

    void run();
    bool cancelRecognition();
    int resultId() const;

    LTKTraceGroup traceGroup;

private:
    friend class LipiSharedRecognizer;
    const QMap<int, QChar> unicodeMap;
    const LTKCaptureDevice deviceInfo;
    const LTKScreenContext screenContext;
    const vector<int> inSubsetOfClasses;
    const float confThreshold;
    const int numChoices;
    QSharedPointer<vector<LTKShapeRecoResult> > resultVector;
    const int _resultId;
    QMutex stateLock;
    bool stateRunning;
    bool stateCancelled;
};

class LipiRecognitionResultsTask : public LipiTask
{
    Q_OBJECT
public:
    explicit LipiRecognitionResultsTask(QSharedPointer<vector<LTKShapeRecoResult> > resultVector,
                                        const QMap<int, QChar> &unicodeMap,
                                        int resultId);

    void run();

signals:
    void resultsAvailable(const QVariantList &resultList);

private:
    QSharedPointer<vector<LTKShapeRecoResult> > resultVector;
    const QMap<int, QChar> &unicodeMap;
    const int _resultId;
};

class LipiWorker : public QThread
{
    Q_OBJECT
public:
    explicit LipiWorker(LTKShapeRecognizer *shapeRecognizer, QObject *parent = 0);
    ~LipiWorker();

    void addTask(QSharedPointer<LipiTask> task);
    int removeTask(QSharedPointer<LipiTask> task);
    int removeAllTasks();

protected:
    void run();

private:
    QList<QSharedPointer<LipiTask> > taskList;
    QSemaphore taskSema;
    QMutex taskLock;
    LTKShapeRecognizer *shapeRecognizer;
    bool abort;
};

} // namespace QtVirtualKeyboard

#endif // LIPIWORKER_H
