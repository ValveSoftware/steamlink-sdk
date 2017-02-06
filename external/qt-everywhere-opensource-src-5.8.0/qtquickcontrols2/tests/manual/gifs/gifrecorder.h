/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef GIFRECORDER_H
#define GIFRECORDER_H

#include <QObject>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QDir>
#include <QString>
#include <QTimer>

class GifRecorder : public QObject
{
    Q_OBJECT

public:
    GifRecorder();

    void setRecordingDuration(int duration);
    void setRecordCursor(bool recordCursor);
    void setDataDirPath(const QString &path);
    void setOutputDir(const QDir &dir);
    void setOutputFileBaseName(const QString &fileBaseName);
    void setQmlFileName(const QString &fileName);
    void setView(QQuickWindow *mWindow);
    void setHighQuality(bool highQuality);

    QQuickWindow *window() const;

    void start();
    bool hasStarted() const;
    void waitForFinish();

private slots:
    void onByzanzError();
    void onByzanzFinished();

private:
    QString mDataDirPath;
    QDir mOutputDir;
    QString mOutputFileBaseName;
    QString mByzanzOutputFileName;
    QString mGifFileName;
    QString mQmlInputFileName;
    QQmlApplicationEngine mEngine;
    QQuickWindow *mWindow;
    bool mHighQuality;
    int mRecordingDuration;
    bool mRecordCursor;

    QProcess mByzanzProcess;
    bool mByzanzProcessFinished;
    QTimer mEventTimer;
};

#endif // GIFRECORDER_H
