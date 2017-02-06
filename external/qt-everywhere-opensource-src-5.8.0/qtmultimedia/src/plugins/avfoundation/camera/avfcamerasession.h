/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef AVFCAMERASESSION_H
#define AVFCAMERASESSION_H

#include <QtCore/qmutex.h>
#include <QtMultimedia/qcamera.h>
#include <QVideoFrame>

#import <AVFoundation/AVFoundation.h>

@class AVFCameraSessionObserver;

QT_BEGIN_NAMESPACE

class AVFCameraControl;
class AVFCameraService;
class AVFCameraRendererControl;
class AVFMediaVideoProbeControl;

struct AVFCameraInfo
{
    AVFCameraInfo() : position(QCamera::UnspecifiedPosition), orientation(0)
    { }

    QByteArray deviceId;
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

    static int defaultCameraIndex();
    static const QList<AVFCameraInfo> &availableCameraDevices();
    static AVFCameraInfo cameraDeviceInfo(const QByteArray &device);
    AVFCameraInfo activeCameraInfo() const { return m_activeCameraInfo; }

    void setVideoOutput(AVFCameraRendererControl *output);
    AVCaptureSession *captureSession() const { return m_captureSession; }
    AVCaptureDevice *videoCaptureDevice() const;

    QCamera::State state() const;
    QCamera::State requestedState() const { return m_state; }
    bool isActive() const { return m_active; }

    void addProbe(AVFMediaVideoProbeControl *probe);
    void removeProbe(AVFMediaVideoProbeControl *probe);
    FourCharCode defaultCodec();

    AVCaptureDeviceInput *videoInput() const {return m_videoInput;}

public Q_SLOTS:
    void setState(QCamera::State state);

    void processRuntimeError();
    void processSessionStarted();
    void processSessionStopped();

    void onCaptureModeChanged(QCamera::CaptureModes mode);

    void onCameraFrameFetched(const QVideoFrame &frame);

Q_SIGNALS:
    void readyToConfigureConnections();
    void stateChanged(QCamera::State newState);
    void activeChanged(bool);
    void newViewfinderFrame(const QVideoFrame &frame);
    void error(int error, const QString &errorString);

private:
    static void updateCameraDevices();
    void attachVideoInputDevice();
    bool applyImageEncoderSettings();
    bool applyViewfinderSettings();

    static int m_defaultCameraIndex;
    static QList<AVFCameraInfo> m_cameraDevices;
    AVFCameraInfo m_activeCameraInfo;

    AVFCameraService *m_service;
    AVFCameraRendererControl *m_videoOutput;

    QCamera::State m_state;
    bool m_active;

    AVCaptureSession *m_captureSession;
    AVCaptureDeviceInput *m_videoInput;
    AVFCameraSessionObserver *m_observer;

    QSet<AVFMediaVideoProbeControl *> m_videoProbes;
    QMutex m_videoProbesMutex;

    FourCharCode m_defaultCodec;
};

QT_END_NAMESPACE

#endif
