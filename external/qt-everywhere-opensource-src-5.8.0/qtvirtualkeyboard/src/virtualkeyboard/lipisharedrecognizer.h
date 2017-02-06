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

#ifndef LIPISHAREDRECOGNIZER_H
#define LIPISHAREDRECOGNIZER_H

#include <QString>
#include <QMap>

#include "lipiworker.h"

class LTKLipiEngineInterface;

namespace QtVirtualKeyboard {

class LipiSharedRecognizer
{
    Q_DISABLE_COPY(LipiSharedRecognizer)
public:
    LipiSharedRecognizer();
    ~LipiSharedRecognizer();

    QString model() const;
    bool setModel(const QString &modelName);

    void subsetOfClasses(const QString &charset, vector<int> &outSubsetOfClasses) const;

    QSharedPointer<LipiRecognitionTask> newRecognition(const LTKCaptureDevice& deviceInfo,
                                                       const LTKScreenContext& screenContext,
                                                       const vector<int>& inSubsetOfClasses,
                                                       float confThreshold,
                                                       int numChoices);
    QSharedPointer<LipiRecognitionResultsTask> startRecognition(QSharedPointer<LipiRecognitionTask> &recognitionTask);
    bool cancelRecognition();
    bool cancelRecognitionTask(QSharedPointer<LipiRecognitionTask> &recognitionTask);

private:
    static int loadLipiInterface();
    static void unloadLipiInterface();
    static int loadLipiEngineConfig();
    static int resolveLogicalNameToProjectProfile(const QString &logicalName, QString &outProjectName, QString &outProfileName);
    static int loadModelData(const QString &logicalName);
    static void unloadModelData();
    static int loadMapping(const QString &mapFile);

    typedef LTKLipiEngineInterface* (*FN_PTR_CREATELTKLIPIENGINE)(void);
    typedef void (*FN_PTR_DELETELTKLIPIENGINE)(void);

    static int s_lipiEngineRefCount;
    static QString s_lipiRoot;
    static QString s_lipiLib;
    static void *s_lipiEngineHandle;
    static FN_PTR_CREATELTKLIPIENGINE s_createLTKLipiEngine;
    static FN_PTR_DELETELTKLIPIENGINE s_deleteLTKLipiEngine;
    static LTKLipiEngineInterface *s_lipiEngine;
    static LTKShapeRecognizer *s_shapeRecognizer;
    static LipiWorker *s_lipiWorker;
    static QMap<int, QChar> s_unicodeMap;
    static QString s_activeModel;
    static stringStringMap s_lipiEngineConfigEntries;
    static int s_recognitionCount;
};

} // namespace QtVirtualKeyboard

#endif // LIPISHAREDRECOGNIZER_H
