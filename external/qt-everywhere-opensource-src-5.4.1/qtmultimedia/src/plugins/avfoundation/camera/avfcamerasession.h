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

#ifndef AVFCAMERASESSION_H
#define AVFCAMERASESSION_H

#include <QtCore/qmutex.h>
#include <QtMultimedia/qcamera.h>

#import <AVFoundation/AVFoundation.h>

@class AVFCameraSessionObserver;

QT_BEGIN_NAMESPACE

class AVFCameraControl;
class AVFCameraService;
class AVFVideoRendererControl;

struct AVFCameraInfo
{
    AVFCameraInfo() : position(QCamera::UnspecifiedPosition), orientation(0)
    { }

    QString description;
    QCamera::Position position;
    int orientation;
};

class AVFCameraSession : public QObject
{
    Q_OBJECT
public:
    AVFCameraSession(AVFCameraService *service, QObject *parent = 0);
    ~AVFCameraSession();

    static const QByteArray &defaultCameraDevice();
    static const QList<QByteArray> &availableCameraDevices();
    static AVFCameraInfo cameraDeviceInfo(const QByteArray &device);

    void setVideoOutput(AVFVideoRendererControl *output);
    AVCaptureSession *captureSession() const { return m_captureSession; }
    AVCaptureDevice *videoCaptureDevice() const;

    QCamera::State state() const;
    QCamera::State requestedState() const { return m_state; }
    bool isActive() const { return m_active; }

public Q_SLOTS:
    void setState(QCamera::State state);

    void processRuntimeError();
    void processSessionStarted();
    void processSessionStopped();

Q_SIGNALS:
    void readyToConfigureConnections();
    void stateChanged(QCamera::State newState);
    void activeChanged(bool);
    void error(int error, const QString &errorString);

private:
    static void updateCameraDevices();
    void attachInputDevices();

    static QByteArray m_defaultCameraDevice;
    static QList<QByteArray> m_cameraDevices;
    static QMap<QByteArray, AVFCameraInfo> m_cameraInfo;

    AVFCameraService *m_service;
    AVFVideoRendererControl *m_videoOutput;

    QCamera::State m_state;
    bool m_active;

    AVCaptureSession *m_captureSession;
    AVCaptureDeviceInput *m_videoInput;
    AVCaptureDeviceInput *m_audioInput;
    AVFCameraSessionObserver *m_observer;
};

QT_END_NAMESPACE

#endif
