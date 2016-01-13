/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef AVFMEDIARECORDERCONTROL_H
#define AVFMEDIARECORDERCONTROL_H

#include <QtCore/qurl.h>
#include <QtMultimedia/qmediarecordercontrol.h>

#import <AVFoundation/AVFoundation.h>
#include "avfstoragelocation.h"

@class AVFMediaRecorderDelegate;

QT_BEGIN_NAMESPACE

class AVFCameraSession;
class AVFCameraControl;
class AVFCameraService;

class AVFMediaRecorderControl : public QMediaRecorderControl
{
Q_OBJECT
public:
    AVFMediaRecorderControl(AVFCameraService *service, QObject *parent = 0);
    ~AVFMediaRecorderControl();

    QUrl outputLocation() const;
    bool setOutputLocation(const QUrl &location);

    QMediaRecorder::State state() const;
    QMediaRecorder::Status status() const;

    qint64 duration() const;

    bool isMuted() const;
    qreal volume() const;

    void applySettings();

public Q_SLOTS:
    void setState(QMediaRecorder::State state);
    void setMuted(bool muted);
    void setVolume(qreal volume);

    void handleRecordingStarted();
    void handleRecordingFinished();
    void handleRecordingFailed(const QString &message);

private Q_SLOTS:
    void reconnectMovieOutput();
    void updateStatus();

private:
    AVFCameraControl *m_cameraControl;
    AVFCameraSession *m_session;

    bool m_connected;
    QUrl m_outputLocation;
    QMediaRecorder::State m_state;
    QMediaRecorder::Status m_lastStatus;

    bool m_recordingStarted;
    bool m_recordingFinished;

    bool m_muted;
    qreal m_volume;

    AVCaptureMovieFileOutput *m_movieOutput;
    AVFMediaRecorderDelegate *m_recorderDelagate;
    AVFStorageLocation m_storageLocation;
};

QT_END_NAMESPACE

#endif
