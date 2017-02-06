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

#include "qandroidcamerasession.h"

#include "androidcamera.h"
#include "androidmultimediautils.h"
#include "qandroidvideooutput.h"
#include "qandroidmediavideoprobecontrol.h"
#include "qandroidmultimediautils.h"
#include <QtConcurrent/qtconcurrentrun.h>
#include <qfile.h>
#include <qguiapplication.h>
#include <qdebug.h>
#include <qvideoframe.h>
#include <private/qmemoryvideobuffer_p.h>
#include <private/qvideoframe_p.h>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QList<AndroidCameraInfo>, g_availableCameras)

QAndroidCameraSession::QAndroidCameraSession(QObject *parent)
    : QObject(parent)
    , m_selectedCamera(0)
    , m_camera(0)
    , m_nativeOrientation(0)
    , m_videoOutput(0)
    , m_captureMode(QCamera::CaptureViewfinder)
    , m_state(QCamera::UnloadedState)
    , m_savedState(-1)
    , m_status(QCamera::UnloadedStatus)
    , m_previewStarted(false)
    , m_captureDestination(QCameraImageCapture::CaptureToFile)
    , m_captureImageDriveMode(QCameraImageCapture::SingleImageCapture)
    , m_lastImageCaptureId(0)
    , m_readyForCapture(false)
    , m_captureCanceled(false)
    , m_currentImageCaptureId(-1)
    , m_previewCallback(0)
{
    m_mediaStorageLocation.addStorageLocation(
                QMediaStorageLocation::Pictures,
                AndroidMultimediaUtils::getDefaultMediaDirectory(AndroidMultimediaUtils::DCIM));

    if (qApp) {
        connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)),
                this, SLOT(onApplicationStateChanged(Qt::ApplicationState)));
    }
}

QAndroidCameraSession::~QAndroidCameraSession()
{
    close();
}

void QAndroidCameraSession::setCaptureMode(QCamera::CaptureModes mode)
{
    if (m_captureMode == mode || !isCaptureModeSupported(mode))
        return;

    m_captureMode = mode;
    emit captureModeChanged(m_captureMode);

    if (m_previewStarted && m_captureMode.testFlag(QCamera::CaptureStillImage))
        applyViewfinderSettings(m_actualImageSettings.resolution());
}

bool QAndroidCameraSession::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    if (mode & (QCamera::CaptureStillImage & QCamera::CaptureVideo))
        return false;

    return true;
}

void QAndroidCameraSession::setState(QCamera::State state)
{
    // If the application is inactive, the camera shouldn't be started. Save the desired state
    // instead and it will be set when the application becomes active.
    if (qApp->applicationState() != Qt::ApplicationActive) {
        m_savedState = state;
        return;
    }

    if (m_state == state)
        return;

    switch (state) {
    case QCamera::UnloadedState:
        close();
        break;
    case QCamera::LoadedState:
    case QCamera::ActiveState:
        if (!m_camera && !open()) {
            emit error(QCamera::CameraError, QStringLiteral("Failed to open camera"));
            return;
        }
        if (state == QCamera::ActiveState) {
            if (!startPreview())
                return;
        } else if (state == QCamera::LoadedState) {
            stopPreview();
        }
        break;
    }

     m_state = state;
     emit stateChanged(m_state);
}

void QAndroidCameraSession::updateAvailableCameras()
{
    g_availableCameras->clear();

    const int numCameras = AndroidCamera::getNumberOfCameras();
    for (int i = 0; i < numCameras; ++i) {
        AndroidCameraInfo info;
        AndroidCamera::getCameraInfo(i, &info);

        if (!info.name.isNull())
            g_availableCameras->append(info);
    }
}

const QList<AndroidCameraInfo> &QAndroidCameraSession::availableCameras()
{
    if (g_availableCameras->isEmpty())
        updateAvailableCameras();

    return *g_availableCameras;
}

bool QAndroidCameraSession::open()
{
    close();

    m_status = QCamera::LoadingStatus;
    emit statusChanged(m_status);

    m_camera = AndroidCamera::open(m_selectedCamera);

    if (m_camera) {
        connect(m_camera, SIGNAL(pictureExposed()), this, SLOT(onCameraPictureExposed()));
        connect(m_camera, SIGNAL(lastPreviewFrameFetched(QVideoFrame)),
                this, SLOT(onLastPreviewFrameFetched(QVideoFrame)),
                Qt::DirectConnection);
        connect(m_camera, SIGNAL(newPreviewFrame(QVideoFrame)),
                this, SLOT(onNewPreviewFrame(QVideoFrame)),
                Qt::DirectConnection);
        connect(m_camera, SIGNAL(pictureCaptured(QByteArray)), this, SLOT(onCameraPictureCaptured(QByteArray)));
        connect(m_camera, SIGNAL(previewStarted()), this, SLOT(onCameraPreviewStarted()));
        connect(m_camera, SIGNAL(previewStopped()), this, SLOT(onCameraPreviewStopped()));
        connect(m_camera, &AndroidCamera::previewFailedToStart, this, &QAndroidCameraSession::onCameraPreviewFailedToStart);
        connect(m_camera, &AndroidCamera::takePictureFailed, this, &QAndroidCameraSession::onCameraTakePictureFailed);

        m_nativeOrientation = m_camera->getNativeOrientation();

        m_status = QCamera::LoadedStatus;

        if (m_camera->getPreviewFormat() != AndroidCamera::NV21)
            m_camera->setPreviewFormat(AndroidCamera::NV21);

        m_camera->notifyNewFrames(m_videoProbes.count() || m_previewCallback);

        emit opened();
    } else {
        m_status = QCamera::UnavailableStatus;
    }

    emit statusChanged(m_status);

    return m_camera != 0;
}

void QAndroidCameraSession::close()
{
    if (!m_camera)
        return;

    stopPreview();

    m_status = QCamera::UnloadingStatus;
    emit statusChanged(m_status);

    m_readyForCapture = false;
    m_currentImageCaptureId = -1;
    m_currentImageCaptureFileName.clear();
    m_actualImageSettings = m_requestedImageSettings;
    m_actualViewfinderSettings = m_requestedViewfinderSettings;

    m_camera->release();
    delete m_camera;
    m_camera = 0;

    m_status = QCamera::UnloadedStatus;
    emit statusChanged(m_status);
}

void QAndroidCameraSession::setVideoOutput(QAndroidVideoOutput *output)
{
    if (m_videoOutput) {
        m_videoOutput->stop();
        m_videoOutput->reset();
    }

    if (output) {
        m_videoOutput = output;
        if (m_videoOutput->isReady())
            onVideoOutputReady(true);
        else
            connect(m_videoOutput, SIGNAL(readyChanged(bool)), this, SLOT(onVideoOutputReady(bool)));
    } else {
        m_videoOutput = 0;
    }
}

void QAndroidCameraSession::setViewfinderSettings(const QCameraViewfinderSettings &settings)
{
    if (m_requestedViewfinderSettings == settings)
        return;

    m_requestedViewfinderSettings = m_actualViewfinderSettings = settings;

    if (m_readyForCapture)
        applyViewfinderSettings();
}

void QAndroidCameraSession::applyViewfinderSettings(const QSize &captureSize, bool restartPreview)
{
    if (!m_camera)
        return;

    const QSize currentViewfinderResolution = m_camera->previewSize();
    const AndroidCamera::ImageFormat currentPreviewFormat = m_camera->getPreviewFormat();
    const AndroidCamera::FpsRange currentFpsRange = m_camera->getPreviewFpsRange();

    // -- adjust resolution
    QSize adjustedViewfinderResolution;
    const bool validCaptureSize = captureSize.width() > 0 && captureSize.height() > 0;
    if (m_captureMode.testFlag(QCamera::CaptureVideo)
            && validCaptureSize
            && m_camera->getPreferredPreviewSizeForVideo().isEmpty()) {
        // According to the Android doc, if getPreferredPreviewSizeForVideo() returns null, it means
        // the preview size cannot be different from the capture size
        adjustedViewfinderResolution = captureSize;
    } else {
        qreal captureAspectRatio = 0;
        if (validCaptureSize)
            captureAspectRatio = qreal(captureSize.width()) / qreal(captureSize.height());

        const QList<QSize> previewSizes = m_camera->getSupportedPreviewSizes();

        const QSize vfRes = m_requestedViewfinderSettings.resolution();
        if (vfRes.width() > 0 && vfRes.height() > 0
                && (!validCaptureSize || qAbs(captureAspectRatio - (qreal(vfRes.width()) / vfRes.height())) < 0.01)
                && previewSizes.contains(vfRes)) {
            adjustedViewfinderResolution = vfRes;
        } else if (validCaptureSize) {
            // search for viewfinder resolution with the same aspect ratio
            qreal minAspectDiff = 1;
            QSize closestResolution;
            for (int i = previewSizes.count() - 1; i >= 0; --i) {
                const QSize &size = previewSizes.at(i);
                const qreal sizeAspect = qreal(size.width()) / size.height();
                if (qFuzzyCompare(captureAspectRatio, sizeAspect)) {
                    adjustedViewfinderResolution = size;
                    break;
                } else if (minAspectDiff > qAbs(sizeAspect - captureAspectRatio)) {
                    closestResolution = size;
                    minAspectDiff = qAbs(sizeAspect - captureAspectRatio);
                }
            }
            if (!adjustedViewfinderResolution.isValid()) {
                qWarning("Cannot find a viewfinder resolution matching the capture aspect ratio.");
                if (closestResolution.isValid()) {
                    adjustedViewfinderResolution = closestResolution;
                    qWarning("Using closest viewfinder resolution.");
                } else {
                    return;
                }
            }
        } else {
            adjustedViewfinderResolution = previewSizes.last();
        }
    }
    m_actualViewfinderSettings.setResolution(adjustedViewfinderResolution);

    // -- adjust pixel format

    AndroidCamera::ImageFormat adjustedPreviewFormat = AndroidCamera::NV21;
    if (m_requestedViewfinderSettings.pixelFormat() != QVideoFrame::Format_Invalid) {
        const AndroidCamera::ImageFormat f = AndroidImageFormatFromQtPixelFormat(m_requestedViewfinderSettings.pixelFormat());
        if (f == AndroidCamera::UnknownImageFormat || !m_camera->getSupportedPreviewFormats().contains(f))
            qWarning("Unsupported viewfinder pixel format");
        else
            adjustedPreviewFormat = f;
    }
    m_actualViewfinderSettings.setPixelFormat(QtPixelFormatFromAndroidImageFormat(adjustedPreviewFormat));

    // -- adjust FPS

    AndroidCamera::FpsRange adjustedFps = currentFpsRange;
    const AndroidCamera::FpsRange requestedFpsRange = AndroidCamera::FpsRange::makeFromQReal(m_requestedViewfinderSettings.minimumFrameRate(),
                                                                                             m_requestedViewfinderSettings.maximumFrameRate());
    if (requestedFpsRange.min > 0 || requestedFpsRange.max > 0) {
        int minDist = INT_MAX;
        const QList<AndroidCamera::FpsRange> supportedFpsRanges = m_camera->getSupportedPreviewFpsRange();
        auto it = supportedFpsRanges.rbegin(), end = supportedFpsRanges.rend();
        for (; it != end; ++it) {
            int dist = (requestedFpsRange.min > 0 ? qAbs(requestedFpsRange.min - it->min) : 0)
                       + (requestedFpsRange.max > 0 ? qAbs(requestedFpsRange.max - it->max) : 0);
            if (dist < minDist) {
                minDist = dist;
                adjustedFps = *it;
                if (minDist == 0)
                    break; // exact match
            }
        }
    }
    m_actualViewfinderSettings.setMinimumFrameRate(adjustedFps.getMinReal());
    m_actualViewfinderSettings.setMaximumFrameRate(adjustedFps.getMaxReal());

    // -- Set values on camera

    if (currentViewfinderResolution != adjustedViewfinderResolution
            || currentPreviewFormat != adjustedPreviewFormat
            || currentFpsRange.min != adjustedFps.min
            || currentFpsRange.max != adjustedFps.max) {

        if (m_videoOutput)
            m_videoOutput->setVideoSize(adjustedViewfinderResolution);

        // if preview is started, we have to stop it first before changing its size
        if (m_previewStarted && restartPreview)
            m_camera->stopPreview();

        m_camera->setPreviewSize(adjustedViewfinderResolution);
        m_camera->setPreviewFormat(adjustedPreviewFormat);
        m_camera->setPreviewFpsRange(adjustedFps);

        // restart preview
        if (m_previewStarted && restartPreview)
            m_camera->startPreview();
    }
}

QList<QSize> QAndroidCameraSession::getSupportedPreviewSizes() const
{
    return m_camera ? m_camera->getSupportedPreviewSizes() : QList<QSize>();
}

QList<QVideoFrame::PixelFormat> QAndroidCameraSession::getSupportedPixelFormats() const
{
    QList<QVideoFrame::PixelFormat> formats;

    if (!m_camera)
        return formats;

    const QList<AndroidCamera::ImageFormat> nativeFormats = m_camera->getSupportedPreviewFormats();

    formats.reserve(nativeFormats.size());

    for (AndroidCamera::ImageFormat nativeFormat : nativeFormats) {
        QVideoFrame::PixelFormat format = QtPixelFormatFromAndroidImageFormat(nativeFormat);
        if (format != QVideoFrame::Format_Invalid)
            formats.append(format);
    }

    return formats;
}

QList<AndroidCamera::FpsRange> QAndroidCameraSession::getSupportedPreviewFpsRange() const
{
    return m_camera ? m_camera->getSupportedPreviewFpsRange() : QList<AndroidCamera::FpsRange>();
}

bool QAndroidCameraSession::startPreview()
{
    if (!m_camera)
        return false;

    if (!m_videoOutput) {
        Q_EMIT error(QCamera::InvalidRequestError, tr("Camera cannot be started without a viewfinder."));
        return false;
    }

    if (m_previewStarted)
        return true;

    if (!m_videoOutput->isReady())
        return true; // delay starting until the video output is ready

    Q_ASSERT(m_videoOutput->surfaceTexture() || m_videoOutput->surfaceHolder());

    if ((m_videoOutput->surfaceTexture() && !m_camera->setPreviewTexture(m_videoOutput->surfaceTexture()))
            || (m_videoOutput->surfaceHolder() && !m_camera->setPreviewDisplay(m_videoOutput->surfaceHolder())))
        return false;

    m_status = QCamera::StartingStatus;
    emit statusChanged(m_status);

    applyImageSettings();
    applyViewfinderSettings(m_captureMode.testFlag(QCamera::CaptureStillImage) ? m_actualImageSettings.resolution()
                                                                               : QSize());

    AndroidMultimediaUtils::enableOrientationListener(true);

    m_camera->startPreview();
    m_previewStarted = true;

    return true;
}

void QAndroidCameraSession::stopPreview()
{
    if (!m_camera || !m_previewStarted)
        return;

    m_status = QCamera::StoppingStatus;
    emit statusChanged(m_status);

    AndroidMultimediaUtils::enableOrientationListener(false);

    m_camera->stopPreview();
    m_camera->setPreviewSize(QSize());
    m_camera->setPreviewTexture(0);
    m_camera->setPreviewDisplay(0);

    if (m_videoOutput) {
        m_videoOutput->stop();
        m_videoOutput->reset();
    }
    m_previewStarted = false;
}

void QAndroidCameraSession::setImageSettings(const QImageEncoderSettings &settings)
{
    if (m_requestedImageSettings == settings)
        return;

    m_requestedImageSettings = m_actualImageSettings = settings;

    applyImageSettings();

    if (m_readyForCapture && m_captureMode.testFlag(QCamera::CaptureStillImage))
        applyViewfinderSettings(m_actualImageSettings.resolution());
}

int QAndroidCameraSession::currentCameraRotation() const
{
    if (!m_camera)
        return 0;

    // subtract natural camera orientation and physical device orientation
    int rotation = 0;
    int deviceOrientation = (AndroidMultimediaUtils::getDeviceOrientation() + 45) / 90 * 90;
    if (m_camera->getFacing() == AndroidCamera::CameraFacingFront)
        rotation = (m_nativeOrientation - deviceOrientation + 360) % 360;
    else // back-facing camera
        rotation = (m_nativeOrientation + deviceOrientation) % 360;

    return rotation;
}

void QAndroidCameraSession::addProbe(QAndroidMediaVideoProbeControl *probe)
{
    m_videoProbesMutex.lock();
    if (probe)
        m_videoProbes << probe;
    if (m_camera)
        m_camera->notifyNewFrames(m_videoProbes.count() || m_previewCallback);
    m_videoProbesMutex.unlock();
}

void QAndroidCameraSession::removeProbe(QAndroidMediaVideoProbeControl *probe)
{
    m_videoProbesMutex.lock();
    m_videoProbes.remove(probe);
    if (m_camera)
        m_camera->notifyNewFrames(m_videoProbes.count() || m_previewCallback);
    m_videoProbesMutex.unlock();
}

void QAndroidCameraSession::setPreviewFormat(AndroidCamera::ImageFormat format)
{
    if (format == AndroidCamera::UnknownImageFormat)
        return;

    m_camera->setPreviewFormat(format);
}

void QAndroidCameraSession::setPreviewCallback(PreviewCallback *callback)
{
    m_videoProbesMutex.lock();
    m_previewCallback = callback;
    if (m_camera)
        m_camera->notifyNewFrames(m_videoProbes.count() || m_previewCallback);
    m_videoProbesMutex.unlock();
}

void QAndroidCameraSession::applyImageSettings()
{
    if (!m_camera)
        return;

    if (m_actualImageSettings.codec().isEmpty())
        m_actualImageSettings.setCodec(QLatin1String("jpeg"));

    const QSize requestedResolution = m_requestedImageSettings.resolution();
    const QList<QSize> supportedResolutions = m_camera->getSupportedPictureSizes();
    if (!requestedResolution.isValid()) {
        // if the viewfinder resolution is explicitly set, pick the highest available capture
        // resolution with the same aspect ratio
        if (m_requestedViewfinderSettings.resolution().isValid()) {
            const QSize vfResolution = m_actualViewfinderSettings.resolution();
            const qreal vfAspectRatio = qreal(vfResolution.width()) / vfResolution.height();

            auto it = supportedResolutions.rbegin(), end = supportedResolutions.rend();
            for (; it != end; ++it) {
                if (qAbs(vfAspectRatio - (qreal(it->width()) / it->height())) < 0.01) {
                    m_actualImageSettings.setResolution(*it);
                    break;
                }
            }
        } else {
            // otherwise, use the highest supported one
            m_actualImageSettings.setResolution(supportedResolutions.last());
        }
    } else if (!supportedResolutions.contains(requestedResolution)) {
        // if the requested resolution is not supported, find the closest one
        int reqPixelCount = requestedResolution.width() * requestedResolution.height();
        QList<int> supportedPixelCounts;
        for (int i = 0; i < supportedResolutions.size(); ++i) {
            const QSize &s = supportedResolutions.at(i);
            supportedPixelCounts.append(s.width() * s.height());
        }
        int closestIndex = qt_findClosestValue(supportedPixelCounts, reqPixelCount);
        m_actualImageSettings.setResolution(supportedResolutions.at(closestIndex));
    }
    m_camera->setPictureSize(m_actualImageSettings.resolution());

    int jpegQuality = 100;
    switch (m_requestedImageSettings.quality()) {
    case QMultimedia::VeryLowQuality:
        jpegQuality = 20;
        break;
    case QMultimedia::LowQuality:
        jpegQuality = 40;
        break;
    case QMultimedia::NormalQuality:
        jpegQuality = 60;
        break;
    case QMultimedia::HighQuality:
        jpegQuality = 80;
        break;
    case QMultimedia::VeryHighQuality:
        jpegQuality = 100;
        break;
    }
    m_camera->setJpegQuality(jpegQuality);
}

bool QAndroidCameraSession::isCaptureDestinationSupported(QCameraImageCapture::CaptureDestinations destination) const
{
    return destination & (QCameraImageCapture::CaptureToFile | QCameraImageCapture::CaptureToBuffer);
}

QCameraImageCapture::CaptureDestinations QAndroidCameraSession::captureDestination() const
{
    return m_captureDestination;
}

void QAndroidCameraSession::setCaptureDestination(QCameraImageCapture::CaptureDestinations destination)
{
    if (m_captureDestination != destination) {
        m_captureDestination = destination;
        emit captureDestinationChanged(m_captureDestination);
    }
}

bool QAndroidCameraSession::isReadyForCapture() const
{
    return m_status == QCamera::ActiveStatus && m_readyForCapture;
}

void QAndroidCameraSession::setReadyForCapture(bool ready)
{
    if (m_readyForCapture == ready)
        return;

    m_readyForCapture = ready;
    emit readyForCaptureChanged(ready);
}

QCameraImageCapture::DriveMode QAndroidCameraSession::driveMode() const
{
    return m_captureImageDriveMode;
}

void QAndroidCameraSession::setDriveMode(QCameraImageCapture::DriveMode mode)
{
    m_captureImageDriveMode = mode;
}

int QAndroidCameraSession::capture(const QString &fileName)
{
    ++m_lastImageCaptureId;

    if (!isReadyForCapture()) {
        emit imageCaptureError(m_lastImageCaptureId, QCameraImageCapture::NotReadyError,
                               tr("Camera not ready"));
        return m_lastImageCaptureId;
    }

    if (m_captureImageDriveMode == QCameraImageCapture::SingleImageCapture) {
        setReadyForCapture(false);

        m_currentImageCaptureId = m_lastImageCaptureId;
        m_currentImageCaptureFileName = fileName;

        applyImageSettings();
        applyViewfinderSettings(m_actualImageSettings.resolution());

        // adjust picture rotation depending on the device orientation
        m_camera->setRotation(currentCameraRotation());

        m_camera->takePicture();
    } else {
        //: Drive mode is the camera's shutter mode, for example single shot, continuos exposure, etc.
        emit imageCaptureError(m_lastImageCaptureId, QCameraImageCapture::NotSupportedFeatureError,
                               tr("Drive mode not supported"));
    }

    return m_lastImageCaptureId;
}

void QAndroidCameraSession::cancelCapture()
{
    if (m_readyForCapture)
        return;

    m_captureCanceled = true;
}

void QAndroidCameraSession::onCameraTakePictureFailed()
{
    emit imageCaptureError(m_currentImageCaptureId, QCameraImageCapture::ResourceError,
                           tr("Failed to capture image"));

    // Preview needs to be restarted and the preview call back must be setup again
    m_camera->startPreview();
}

void QAndroidCameraSession::onCameraPictureExposed()
{
    if (m_captureCanceled)
        return;

    emit imageExposed(m_currentImageCaptureId);
    m_camera->fetchLastPreviewFrame();
}

void QAndroidCameraSession::onLastPreviewFrameFetched(const QVideoFrame &frame)
{
    if (m_captureCanceled)
        return;

    QtConcurrent::run(this, &QAndroidCameraSession::processPreviewImage,
                      m_currentImageCaptureId,
                      frame,
                      m_camera->getRotation());
}

void QAndroidCameraSession::processPreviewImage(int id, const QVideoFrame &frame, int rotation)
{
    // Preview display of front-facing cameras is flipped horizontally, but the frame data
    // we get here is not. Flip it ourselves if the camera is front-facing to match what the user
    // sees on the viewfinder.
    QTransform transform;
    if (m_camera->getFacing() == AndroidCamera::CameraFacingFront)
        transform.scale(-1, 1);
    transform.rotate(rotation);

    emit imageCaptured(id, qt_imageFromVideoFrame(frame).transformed(transform));
}

void QAndroidCameraSession::onNewPreviewFrame(const QVideoFrame &frame)
{
    m_videoProbesMutex.lock();

    for (QAndroidMediaVideoProbeControl *probe : qAsConst(m_videoProbes))
        probe->newFrameProbed(frame);

    if (m_previewCallback)
        m_previewCallback->onFrameAvailable(frame);

    m_videoProbesMutex.unlock();
}

void QAndroidCameraSession::onCameraPictureCaptured(const QByteArray &data)
{
    if (!m_captureCanceled) {
        // Loading and saving the captured image can be slow, do it in a separate thread
        QtConcurrent::run(this, &QAndroidCameraSession::processCapturedImage,
                          m_currentImageCaptureId,
                          data,
                          m_actualImageSettings.resolution(),
                          m_captureDestination,
                          m_currentImageCaptureFileName);
    }

    m_captureCanceled = false;

    // Preview needs to be restarted after taking a picture
    m_camera->startPreview();
}

void QAndroidCameraSession::onCameraPreviewStarted()
{
    if (m_status == QCamera::StartingStatus) {
        m_status = QCamera::ActiveStatus;
        emit statusChanged(m_status);
    }

    setReadyForCapture(true);
}

void QAndroidCameraSession::onCameraPreviewFailedToStart()
{
    if (m_status == QCamera::StartingStatus) {
        Q_EMIT error(QCamera::CameraError, tr("Camera preview failed to start."));

        AndroidMultimediaUtils::enableOrientationListener(false);
        m_camera->setPreviewSize(QSize());
        m_camera->setPreviewTexture(0);
        if (m_videoOutput) {
            m_videoOutput->stop();
            m_videoOutput->reset();
        }
        m_previewStarted = false;

        m_status = QCamera::LoadedStatus;
        emit statusChanged(m_status);

        setReadyForCapture(false);
    }
}

void QAndroidCameraSession::onCameraPreviewStopped()
{
    if (m_status == QCamera::StoppingStatus) {
        m_status = QCamera::LoadedStatus;
        emit statusChanged(m_status);
    }

    setReadyForCapture(false);
}

void QAndroidCameraSession::processCapturedImage(int id,
                                                 const QByteArray &data,
                                                 const QSize &resolution,
                                                 QCameraImageCapture::CaptureDestinations dest,
                                                 const QString &fileName)
{


    if (dest & QCameraImageCapture::CaptureToFile) {
        const QString actualFileName = m_mediaStorageLocation.generateFileName(fileName,
                                                                               QMediaStorageLocation::Pictures,
                                                                               QLatin1String("IMG_"),
                                                                               QLatin1String("jpg"));

        QFile file(actualFileName);
        if (file.open(QFile::WriteOnly)) {
            if (file.write(data) == data.size()) {
                // if the picture is saved into the standard picture location, register it
                // with the Android media scanner so it appears immediately in apps
                // such as the gallery.
                QString standardLoc = AndroidMultimediaUtils::getDefaultMediaDirectory(AndroidMultimediaUtils::DCIM);
                if (actualFileName.startsWith(standardLoc))
                    AndroidMultimediaUtils::registerMediaFile(actualFileName);

                emit imageSaved(id, actualFileName);
            } else {
                emit imageCaptureError(id, QCameraImageCapture::OutOfSpaceError, file.errorString());
            }
        } else {
            const QString errorMessage = tr("Could not open destination file: %1").arg(actualFileName);
            emit imageCaptureError(id, QCameraImageCapture::ResourceError, errorMessage);
        }
    }

    if (dest & QCameraImageCapture::CaptureToBuffer) {
        QVideoFrame frame(new QMemoryVideoBuffer(data, -1), resolution, QVideoFrame::Format_Jpeg);
        emit imageAvailable(id, frame);
    }
}

QVideoFrame::PixelFormat QAndroidCameraSession::QtPixelFormatFromAndroidImageFormat(AndroidCamera::ImageFormat format)
{
    switch (format) {
    case AndroidCamera::RGB565:
        return QVideoFrame::Format_RGB565;
    case AndroidCamera::NV21:
        return QVideoFrame::Format_NV21;
    case AndroidCamera::YUY2:
        return QVideoFrame::Format_YUYV;
    case AndroidCamera::JPEG:
        return QVideoFrame::Format_Jpeg;
    case AndroidCamera::YV12:
        return QVideoFrame::Format_YV12;
    default:
        return QVideoFrame::Format_Invalid;
    }
}

AndroidCamera::ImageFormat QAndroidCameraSession::AndroidImageFormatFromQtPixelFormat(QVideoFrame::PixelFormat format)
{
    switch (format) {
    case QVideoFrame::Format_RGB565:
        return AndroidCamera::RGB565;
    case QVideoFrame::Format_NV21:
        return AndroidCamera::NV21;
    case QVideoFrame::Format_YUYV:
        return AndroidCamera::YUY2;
    case QVideoFrame::Format_Jpeg:
        return AndroidCamera::JPEG;
    case QVideoFrame::Format_YV12:
        return AndroidCamera::YV12;
    default:
        return AndroidCamera::UnknownImageFormat;
    }
}

void QAndroidCameraSession::onVideoOutputReady(bool ready)
{
    if (ready && m_state == QCamera::ActiveState)
        startPreview();
}

void QAndroidCameraSession::onApplicationStateChanged(Qt::ApplicationState state)
{
    switch (state) {
    case Qt::ApplicationInactive:
        if (m_state != QCamera::UnloadedState) {
            m_savedState = m_state;
            close();
            m_state = QCamera::UnloadedState;
            emit stateChanged(m_state);
        }
        break;
    case Qt::ApplicationActive:
        if (m_savedState != -1) {
            setState(QCamera::State(m_savedState));
            m_savedState = -1;
        }
        break;
    default:
        break;
    }
}

QT_END_NAMESPACE
