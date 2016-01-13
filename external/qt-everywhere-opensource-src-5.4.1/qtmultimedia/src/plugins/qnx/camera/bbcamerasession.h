/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
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
#ifndef BBCAMERASESSION_H
#define BBCAMERASESSION_H

#include "bbmediastoragelocation.h"

#include <QCamera>
#include <QCameraImageCapture>
#include <QCameraViewfinderSettingsControl>
#include <QElapsedTimer>
#include <QMediaRecorder>
#include <QMutex>
#include <QObject>
#include <QPointer>

#include <camera/camera_api.h>

QT_BEGIN_NAMESPACE

class BbCameraOrientationHandler;
class WindowGrabber;

class BbCameraSession : public QObject
{
    Q_OBJECT
public:
    explicit BbCameraSession(QObject *parent = 0);
    ~BbCameraSession();

    camera_handle_t handle() const;

    // camera control
    QCamera::State state() const;
    void setState(QCamera::State state);
    QCamera::Status status() const;
    QCamera::CaptureModes captureMode() const;
    void setCaptureMode(QCamera::CaptureModes);
    bool isCaptureModeSupported(QCamera::CaptureModes mode) const;

    // video device selector control
    static QByteArray cameraIdentifierFront();
    static QByteArray cameraIdentifierRear();
    static QByteArray cameraIdentifierDesktop();

    void setDevice(const QByteArray &device);
    QByteArray device() const;

    // video renderer control
    QAbstractVideoSurface *surface() const;
    void setSurface(QAbstractVideoSurface *surface);

    // image capture control
    bool isReadyForCapture() const;
    QCameraImageCapture::DriveMode driveMode() const;
    void setDriveMode(QCameraImageCapture::DriveMode mode);
    int capture(const QString &fileName);
    void cancelCapture();

    // capture destination control
    bool isCaptureDestinationSupported(QCameraImageCapture::CaptureDestinations destination) const;
    QCameraImageCapture::CaptureDestinations captureDestination() const;
    void setCaptureDestination(QCameraImageCapture::CaptureDestinations destination);

    // image encoder control
    QList<QSize> supportedResolutions(const QImageEncoderSettings &settings, bool *continuous) const;
    QImageEncoderSettings imageSettings() const;
    void setImageSettings(const QImageEncoderSettings &settings);

    // media recorder control
    QUrl outputLocation() const;
    bool setOutputLocation(const QUrl &location);
    QMediaRecorder::State videoState() const;
    void setVideoState(QMediaRecorder::State state);
    QMediaRecorder::Status videoStatus() const;
    qint64 duration() const;
    void applyVideoSettings();

    // video encoder settings control
    QList<QSize> supportedResolutions(const QVideoEncoderSettings &settings, bool *continuous) const;
    QList<qreal> supportedFrameRates(const QVideoEncoderSettings &settings, bool *continuous) const;
    QVideoEncoderSettings videoSettings() const;
    void setVideoSettings(const QVideoEncoderSettings &settings);

    // audio encoder settings control
    QAudioEncoderSettings audioSettings() const;
    void setAudioSettings(const QAudioEncoderSettings &settings);

Q_SIGNALS:
    // camera control
    void statusChanged(QCamera::Status);
    void stateChanged(QCamera::State);
    void error(int error, const QString &errorString);
    void captureModeChanged(QCamera::CaptureModes);

    // image capture control
    void readyForCaptureChanged(bool);
    void imageExposed(int id);
    void imageCaptured(int id, const QImage &preview);
    void imageMetadataAvailable(int id, const QString &key, const QVariant &value);
    void imageAvailable(int id, const QVideoFrame &buffer);
    void imageSaved(int id, const QString &fileName);
    void imageCaptureError(int id, int error, const QString &errorString);

    // capture destination control
    void captureDestinationChanged(QCameraImageCapture::CaptureDestinations destination);

    // media recorder control
    void videoStateChanged(QMediaRecorder::State state);
    void videoStatusChanged(QMediaRecorder::Status status);
    void durationChanged(qint64 duration);
    void actualLocationChanged(const QUrl &location);
    void videoError(int error, const QString &errorString);

    void cameraOpened();
    void focusStatusChanged(int status);

private slots:
    void updateReadyForCapture();
    void imageCaptured(int, const QImage&, const QString&);
    void handleVideoRecordingPaused();
    void handleVideoRecordingResumed();
    void deviceOrientationChanged(int);
    void handleCameraPowerUp();
    void viewfinderFrameGrabbed(const QImage &image);
    void applyConfiguration();

private:
    bool openCamera();
    void closeCamera();
    bool startViewFinder();
    void stopViewFinder();
    bool startVideoRecording();
    void stopVideoRecording();

    bool isCaptureModeSupported(camera_handle_t handle, QCamera::CaptureModes mode) const;
    QList<QSize> supportedResolutions(QCamera::CaptureMode mode) const;
    QList<QSize> supportedViewfinderResolutions(QCamera::CaptureMode mode) const;
    QSize currentViewfinderResolution(QCamera::CaptureMode mode) const;

    quint32 m_nativeCameraOrientation;
    BbCameraOrientationHandler* m_orientationHandler;

    QCamera::Status m_status;
    QCamera::State m_state;
    QCamera::CaptureModes m_captureMode;

    QByteArray m_device;
    bool m_previewIsVideo;

    QPointer<QAbstractVideoSurface> m_surface;
    QMutex m_surfaceMutex;

    QCameraImageCapture::DriveMode m_captureImageDriveMode;
    int m_lastImageCaptureId;
    QCameraImageCapture::CaptureDestinations m_captureDestination;

    QImageEncoderSettings m_imageEncoderSettings;

    QString m_videoOutputLocation;
    QMediaRecorder::State m_videoState;
    QMediaRecorder::Status m_videoStatus;
    QElapsedTimer m_videoRecordingDuration;

    QVideoEncoderSettings m_videoEncoderSettings;
    QAudioEncoderSettings m_audioEncoderSettings;

    BbMediaStorageLocation m_mediaStorageLocation;

    camera_handle_t m_handle;

    WindowGrabber* m_windowGrabber;
};

QDebug operator<<(QDebug debug, camera_error_t error);

QT_END_NAMESPACE

#endif
