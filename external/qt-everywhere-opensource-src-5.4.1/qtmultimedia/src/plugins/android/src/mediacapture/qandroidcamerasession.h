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

    void setVideoPreview(QObject *videoOutput);
    void adjustViewfinderSize(const QSize &captureSize, bool restartPreview = true);

    QImageEncoderSettings imageSettings() const { return m_imageSettings; }
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

    void onCameraPictureExposed();
    void onCameraPreviewFetched(const QByteArray &preview);
    void onCameraFrameFetched(const QByteArray &frame);
    void onCameraPictureCaptured(const QByteArray &data);
    void onCameraPreviewStarted();
    void onCameraPreviewStopped();

private:
    static void updateAvailableCameras();

    bool open();
    void close();

    bool startPreview();
    void stopPreview();

    void applyImageSettings();
    void processPreviewImage(int id, const QByteArray &data, int rotation);
    QImage prepareImageFromPreviewData(const QByteArray &data, int rotation);
    void processCapturedImage(int id,
                              const QByteArray &data,
                              const QSize &resolution,
                              QCameraImageCapture::CaptureDestinations dest,
                              const QString &fileName);

    int m_selectedCamera;
    AndroidCamera *m_camera;
    int m_nativeOrientation;
    QAndroidVideoOutput *m_videoOutput;

    QCamera::CaptureModes m_captureMode;
    QCamera::State m_state;
    int m_savedState;
    QCamera::Status m_status;
    bool m_previewStarted;

    QImageEncoderSettings m_imageSettings;
    bool m_imageSettingsDirty;
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
};

QT_END_NAMESPACE

#endif // QANDROIDCAMERASESSION_H
