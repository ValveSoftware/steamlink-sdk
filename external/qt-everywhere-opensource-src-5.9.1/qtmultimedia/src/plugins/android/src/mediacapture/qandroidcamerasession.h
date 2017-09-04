/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Ruslan Baratov
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

#ifndef QANDROIDCAMERASESSION_H
#define QANDROIDCAMERASESSION_H

#include <qcamera.h>
#include <qmediaencodersettings.h>
#include <QCameraImageCapture>
#include <QSet>
#include <QMutex>
#include <private/qmediastoragelocation_p.h>
#include "androidcamera.h"

QT_BEGIN_NAMESPACE

class QAndroidVideoOutput;
class QAndroidMediaVideoProbeControl;

class QAndroidCameraSession : public QObject
{
    Q_OBJECT
public:
    explicit QAndroidCameraSession(QObject *parent = 0);
    ~QAndroidCameraSession();

    static const QList<AndroidCameraInfo> &availableCameras();

    void setSelectedCamera(int cameraId) { m_selectedCamera = cameraId; }
    AndroidCamera *camera() const { return m_camera; }

    QCamera::State state() const { return m_state; }
    void setState(QCamera::State state);

    QCamera::Status status() const { return m_status; }

    QCamera::CaptureModes captureMode() const { return m_captureMode; }
    void setCaptureMode(QCamera::CaptureModes mode);
    bool isCaptureModeSupported(QCamera::CaptureModes mode) const;

    QCameraViewfinderSettings viewfinderSettings() const { return m_actualViewfinderSettings; }
    void setViewfinderSettings(const QCameraViewfinderSettings &settings);
    void applyViewfinderSettings(const QSize &captureSize = QSize(), bool restartPreview = true);

    QAndroidVideoOutput *videoOutput() const { return m_videoOutput; }
    void setVideoOutput(QAndroidVideoOutput *output);

    QList<QSize> getSupportedPreviewSizes() const;
    QList<QVideoFrame::PixelFormat> getSupportedPixelFormats() const;
    QList<AndroidCamera::FpsRange> getSupportedPreviewFpsRange() const;

    QImageEncoderSettings imageSettings() const { return m_actualImageSettings; }
    void setImageSettings(const QImageEncoderSettings &settings);

    bool isCaptureDestinationSupported(QCameraImageCapture::CaptureDestinations destination) const;
    QCameraImageCapture::CaptureDestinations captureDestination() const;
    void setCaptureDestination(QCameraImageCapture::CaptureDestinations destination);

    bool isReadyForCapture() const;
    void setReadyForCapture(bool ready);
    QCameraImageCapture::DriveMode driveMode() const;
    void setDriveMode(QCameraImageCapture::DriveMode mode);
    int capture(const QString &fileName);
    void cancelCapture();

    int currentCameraRotation() const;

    void addProbe(QAndroidMediaVideoProbeControl *probe);
    void removeProbe(QAndroidMediaVideoProbeControl *probe);

    void setPreviewFormat(AndroidCamera::ImageFormat format);

    struct PreviewCallback
    {
        virtual void onFrameAvailable(const QVideoFrame &frame) = 0;
    };
    void setPreviewCallback(PreviewCallback *callback);

Q_SIGNALS:
    void statusChanged(QCamera::Status status);
    void stateChanged(QCamera::State);
    void error(int error, const QString &errorString);
    void captureModeChanged(QCamera::CaptureModes);
    void opened();

    void captureDestinationChanged(QCameraImageCapture::CaptureDestinations destination);

    void readyForCaptureChanged(bool);
    void imageExposed(int id);
    void imageCaptured(int id, const QImage &preview);
    void imageMetadataAvailable(int id, const QString &key, const QVariant &value);
    void imageAvailable(int id, const QVideoFrame &buffer);
    void imageSaved(int id, const QString &fileName);
    void imageCaptureError(int id, int error, const QString &errorString);

private Q_SLOTS:
    void onVideoOutputReady(bool ready);

    void onApplicationStateChanged(Qt::ApplicationState state);

    void onCameraTakePictureFailed();
    void onCameraPictureExposed();
    void onCameraPictureCaptured(const QByteArray &data);
    void onLastPreviewFrameFetched(const QVideoFrame &frame);
    void onNewPreviewFrame(const QVideoFrame &frame);
    void onCameraPreviewStarted();
    void onCameraPreviewFailedToStart();
    void onCameraPreviewStopped();

private:
    static void updateAvailableCameras();

    bool open();
    void close();

    bool startPreview();
    void stopPreview();

    void applyImageSettings();

    void processPreviewImage(int id, const QVideoFrame &frame, int rotation);
    void processCapturedImage(int id,
                              const QByteArray &data,
                              const QSize &resolution,
                              QCameraImageCapture::CaptureDestinations dest,
                              const QString &fileName);

    static QVideoFrame::PixelFormat QtPixelFormatFromAndroidImageFormat(AndroidCamera::ImageFormat);
    static AndroidCamera::ImageFormat AndroidImageFormatFromQtPixelFormat(QVideoFrame::PixelFormat);

    int m_selectedCamera;
    AndroidCamera *m_camera;
    int m_nativeOrientation;
    QAndroidVideoOutput *m_videoOutput;

    QCamera::CaptureModes m_captureMode;
    QCamera::State m_state;
    int m_savedState;
    QCamera::Status m_status;
    bool m_previewStarted;

    QCameraViewfinderSettings m_requestedViewfinderSettings;
    QCameraViewfinderSettings m_actualViewfinderSettings;

    QImageEncoderSettings m_requestedImageSettings;
    QImageEncoderSettings m_actualImageSettings;
    QCameraImageCapture::CaptureDestinations m_captureDestination;
    QCameraImageCapture::DriveMode m_captureImageDriveMode;
    int m_lastImageCaptureId;
    bool m_readyForCapture;
    bool m_captureCanceled;
    int m_currentImageCaptureId;
    QString m_currentImageCaptureFileName;

    QMediaStorageLocation m_mediaStorageLocation;

    QSet<QAndroidMediaVideoProbeControl *> m_videoProbes;
    QMutex m_videoProbesMutex;
    PreviewCallback *m_previewCallback;
};

QT_END_NAMESPACE

#endif // QANDROIDCAMERASESSION_H
