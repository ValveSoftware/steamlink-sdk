/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#include "bbcamerasession.h"

#include "bbcameraorientationhandler.h"
#include "bbcameraviewfindersettingscontrol.h"
#include "windowgrabber.h"

#include <QAbstractVideoSurface>
#include <QBuffer>
#include <QDebug>
#include <QImage>
#include <QUrl>
#include <QVideoSurfaceFormat>
#include <qmath.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

static QString errorToString(camera_error_t error)
{
    switch (error) {
    case CAMERA_EOK:
        return QLatin1String("No error");
    case CAMERA_EAGAIN:
        return QLatin1String("Camera unavailable");
    case CAMERA_EINVAL:
        return QLatin1String("Invalid argument");
    case CAMERA_ENODEV:
        return QLatin1String("Camera not found");
    case CAMERA_EMFILE:
        return QLatin1String("File table overflow");
    case CAMERA_EBADF:
        return QLatin1String("Invalid handle passed");
    case CAMERA_EACCESS:
        return QLatin1String("No permission");
    case CAMERA_EBADR:
        return QLatin1String("Invalid file descriptor");
    case CAMERA_ENOENT:
        return QLatin1String("File or directory does not exists");
    case CAMERA_ENOMEM:
        return QLatin1String("Memory allocation failed");
    case CAMERA_EOPNOTSUPP:
        return QLatin1String("Operation not supported");
    case CAMERA_ETIMEDOUT:
        return QLatin1String("Communication timeout");
    case CAMERA_EALREADY:
        return QLatin1String("Operation already in progress");
    case CAMERA_EUNINIT:
        return QLatin1String("Camera library not initialized");
    case CAMERA_EREGFAULT:
        return QLatin1String("Callback registration failed");
    case CAMERA_EMICINUSE:
        return QLatin1String("Microphone in use already");
    case CAMERA_ENODATA:
        return QLatin1String("Data does not exist");
    case CAMERA_EBUSY:
        return QLatin1String("Camera busy");
    case CAMERA_EDESKTOPCAMERAINUSE:
        return QLatin1String("Desktop camera in use already");
    case CAMERA_ENOSPC:
        return QLatin1String("Disk is full");
    case CAMERA_EPOWERDOWN:
        return QLatin1String("Camera in power down state");
    case CAMERA_3ALOCKED:
        return QLatin1String("3A have been locked");
//  case CAMERA_EVIEWFINDERFROZEN: // not yet available in 10.2 NDK
//      return QLatin1String("Freeze flag set");
    default:
        return QLatin1String("Unknown error");
    }
}

QDebug operator<<(QDebug debug, camera_error_t error)
{
    debug.nospace() << errorToString(error);
    return debug.space();
}

BbCameraSession::BbCameraSession(QObject *parent)
    : QObject(parent)
    , m_nativeCameraOrientation(0)
    , m_orientationHandler(new BbCameraOrientationHandler(this))
    , m_status(QCamera::UnloadedStatus)
    , m_state(QCamera::UnloadedState)
    , m_captureMode(QCamera::CaptureStillImage)
    , m_device("bb:RearCamera")
    , m_previewIsVideo(true)
    , m_surface(0)
    , m_captureImageDriveMode(QCameraImageCapture::SingleImageCapture)
    , m_lastImageCaptureId(0)
    , m_captureDestination(QCameraImageCapture::CaptureToFile)
    , m_videoState(QMediaRecorder::StoppedState)
    , m_videoStatus(QMediaRecorder::LoadedStatus)
    , m_handle(CAMERA_HANDLE_INVALID)
    , m_windowGrabber(new WindowGrabber(this))
{
    connect(this, SIGNAL(statusChanged(QCamera::Status)), SLOT(updateReadyForCapture()));
    connect(this, SIGNAL(captureModeChanged(QCamera::CaptureModes)), SLOT(updateReadyForCapture()));
    connect(m_orientationHandler, SIGNAL(orientationChanged(int)), SLOT(deviceOrientationChanged(int)));

    connect(m_windowGrabber, SIGNAL(frameGrabbed(QImage, int)), SLOT(viewfinderFrameGrabbed(QImage)));
}

BbCameraSession::~BbCameraSession()
{
    stopViewFinder();
    closeCamera();
}

camera_handle_t BbCameraSession::handle() const
{
    return m_handle;
}

QCamera::State BbCameraSession::state() const
{
    return m_state;
}

void BbCameraSession::setState(QCamera::State state)
{
    if (m_state == state)
        return;

    const QCamera::State previousState = m_state;

    if (previousState == QCamera::UnloadedState) {
        if (state == QCamera::LoadedState) {
            if (openCamera()) {
                m_state = state;
            }
        } else if (state == QCamera::ActiveState) {
            if (openCamera()) {
                QMetaObject::invokeMethod(this, "applyConfiguration", Qt::QueuedConnection);
                m_state = state;
            }
        }
    } else if (previousState == QCamera::LoadedState) {
        if (state == QCamera::UnloadedState) {
            closeCamera();
            m_state = state;
        } else if (state == QCamera::ActiveState) {
            QMetaObject::invokeMethod(this, "applyConfiguration", Qt::QueuedConnection);
            m_state = state;
        }
    } else if (previousState == QCamera::ActiveState) {
        if (state == QCamera::LoadedState) {
            stopViewFinder();
            m_state = state;
        } else if (state == QCamera::UnloadedState) {
            stopViewFinder();
            closeCamera();
            m_state = state;
        }
    }

    if (m_state != previousState)
        emit stateChanged(m_state);
}

QCamera::Status BbCameraSession::status() const
{
    return m_status;
}

QCamera::CaptureModes BbCameraSession::captureMode() const
{
    return m_captureMode;
}

void BbCameraSession::setCaptureMode(QCamera::CaptureModes captureMode)
{
    if (m_captureMode == captureMode)
        return;

    m_captureMode = captureMode;
    emit captureModeChanged(m_captureMode);
}

bool BbCameraSession::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    if (m_handle == CAMERA_HANDLE_INVALID) {
        // the camera has not been loaded yet via QCamera::load(), so
        // we open it temporarily to peek for the supported capture modes

        camera_unit_t unit = CAMERA_UNIT_REAR;
        if (m_device == cameraIdentifierFront())
            unit = CAMERA_UNIT_FRONT;
        else if (m_device == cameraIdentifierRear())
            unit = CAMERA_UNIT_REAR;
        else if (m_device == cameraIdentifierDesktop())
            unit = CAMERA_UNIT_DESKTOP;

        camera_handle_t handle;
        const camera_error_t result = camera_open(unit, CAMERA_MODE_RW, &handle);
        if (result != CAMERA_EOK)
            return true;

        const bool supported = isCaptureModeSupported(handle, mode);

        camera_close(handle);

        return supported;
    } else {
        return isCaptureModeSupported(m_handle, mode);
    }
}

QByteArray BbCameraSession::cameraIdentifierFront()
{
    return "bb:FrontCamera";
}

QByteArray BbCameraSession::cameraIdentifierRear()
{
    return "bb:RearCamera";
}

QByteArray BbCameraSession::cameraIdentifierDesktop()
{
    return "bb:DesktopCamera";
}

void BbCameraSession::setDevice(const QByteArray &device)
{
    m_device = device;
}

QByteArray BbCameraSession::device() const
{
    return m_device;
}

QAbstractVideoSurface* BbCameraSession::surface() const
{
    return m_surface;
}

void BbCameraSession::setSurface(QAbstractVideoSurface *surface)
{
    QMutexLocker locker(&m_surfaceMutex);

    if (m_surface == surface)
        return;

    m_surface = surface;
}

bool BbCameraSession::isReadyForCapture() const
{
    if (m_captureMode & QCamera::CaptureStillImage)
        return (m_status == QCamera::ActiveStatus);

    if (m_captureMode & QCamera::CaptureVideo)
        return (m_status == QCamera::ActiveStatus);

    return false;
}

QCameraImageCapture::DriveMode BbCameraSession::driveMode() const
{
    return m_captureImageDriveMode;
}

void BbCameraSession::setDriveMode(QCameraImageCapture::DriveMode mode)
{
    m_captureImageDriveMode = mode;
}

/**
 * A helper structure that keeps context data for image capture callbacks.
 */
struct ImageCaptureData
{
    int requestId;
    QString fileName;
    BbCameraSession *session;
};

static void imageCaptureShutterCallback(camera_handle_t handle, void *context)
{
    Q_UNUSED(handle)

    const ImageCaptureData *data = static_cast<ImageCaptureData*>(context);

    // We are inside a worker thread here, so emit imageExposed inside the main thread
    QMetaObject::invokeMethod(data->session, "imageExposed", Qt::QueuedConnection,
                              Q_ARG(int, data->requestId));
}

static void imageCaptureImageCallback(camera_handle_t handle, camera_buffer_t *buffer, void *context)
{
    Q_UNUSED(handle)

    QScopedPointer<ImageCaptureData> data(static_cast<ImageCaptureData*>(context));

    if (buffer->frametype != CAMERA_FRAMETYPE_JPEG) {
        // We are inside a worker thread here, so emit error signal inside the main thread
        QMetaObject::invokeMethod(data->session, "imageCaptureError", Qt::QueuedConnection,
                                  Q_ARG(int, data->requestId),
                                  Q_ARG(QCameraImageCapture::Error, QCameraImageCapture::FormatError),
                                  Q_ARG(QString, BbCameraSession::tr("Camera provides image in unsupported format")));
        return;
    }

    const QByteArray rawData((const char*)buffer->framebuf, buffer->framedesc.jpeg.bufsize);

    QImage image;
    const bool ok = image.loadFromData(rawData, "JPG");
    if (!ok) {
        const QString errorMessage = BbCameraSession::tr("Could not load JPEG data from frame");
        // We are inside a worker thread here, so emit error signal inside the main thread
        QMetaObject::invokeMethod(data->session, "imageCaptureError", Qt::QueuedConnection,
                                  Q_ARG(int, data->requestId),
                                  Q_ARG(QCameraImageCapture::Error, QCameraImageCapture::FormatError),
                                  Q_ARG(QString, errorMessage));
        return;
    }


    // We are inside a worker thread here, so invoke imageCaptured inside the main thread
    QMetaObject::invokeMethod(data->session, "imageCaptured", Qt::QueuedConnection,
                              Q_ARG(int, data->requestId),
                              Q_ARG(QImage, image),
                              Q_ARG(QString, data->fileName));
}

int BbCameraSession::capture(const QString &fileName)
{
    m_lastImageCaptureId++;

    if (!isReadyForCapture()) {
        emit imageCaptureError(m_lastImageCaptureId, QCameraImageCapture::NotReadyError, tr("Camera not ready"));
        return m_lastImageCaptureId;
    }

    if (m_captureImageDriveMode == QCameraImageCapture::SingleImageCapture) {
        // prepare context object for callback
        ImageCaptureData *context = new ImageCaptureData;
        context->requestId = m_lastImageCaptureId;
        context->fileName = fileName;
        context->session = this;

        const camera_error_t result = camera_take_photo(m_handle,
                                                        imageCaptureShutterCallback,
                                                        0,
                                                        0,
                                                        imageCaptureImageCallback,
                                                        context, false);

        if (result != CAMERA_EOK)
            qWarning() << "Unable to take photo:" << result;
    } else {
        // TODO: implement burst mode when available in Qt API
    }

    return m_lastImageCaptureId;
}

void BbCameraSession::cancelCapture()
{
    // BB10 API doesn't provide functionality for that
}

bool BbCameraSession::isCaptureDestinationSupported(QCameraImageCapture::CaptureDestinations destination) const
{
    // capture to buffer, file and both are supported.
    return destination & (QCameraImageCapture::CaptureToFile | QCameraImageCapture::CaptureToBuffer);
}

QCameraImageCapture::CaptureDestinations BbCameraSession::captureDestination() const
{
    return m_captureDestination;
}

void BbCameraSession::setCaptureDestination(QCameraImageCapture::CaptureDestinations destination)
{
    if (m_captureDestination != destination) {
        m_captureDestination = destination;
        emit captureDestinationChanged(m_captureDestination);
    }
}

QList<QSize> BbCameraSession::supportedResolutions(const QImageEncoderSettings&, bool *continuous) const
{
    if (continuous)
        *continuous = false;

    if (m_status == QCamera::UnloadedStatus)
        return QList<QSize>();

    if (m_captureMode & QCamera::CaptureStillImage) {
        return supportedResolutions(QCamera::CaptureStillImage);
    } else if (m_captureMode & QCamera::CaptureVideo) {
        return supportedResolutions(QCamera::CaptureVideo);
    }

    return QList<QSize>();
}

QImageEncoderSettings BbCameraSession::imageSettings() const
{
    return m_imageEncoderSettings;
}

void BbCameraSession::setImageSettings(const QImageEncoderSettings &settings)
{
    m_imageEncoderSettings = settings;
    if (m_imageEncoderSettings.codec().isEmpty())
        m_imageEncoderSettings.setCodec(QLatin1String("jpeg"));
}

QUrl BbCameraSession::outputLocation() const
{
    return QUrl::fromLocalFile(m_videoOutputLocation);
}

bool BbCameraSession::setOutputLocation(const QUrl &location)
{
    m_videoOutputLocation = location.toLocalFile();

    return true;
}

QMediaRecorder::State BbCameraSession::videoState() const
{
    return m_videoState;
}

void BbCameraSession::setVideoState(QMediaRecorder::State state)
{
    if (m_videoState == state)
        return;

    const QMediaRecorder::State previousState = m_videoState;

    if (previousState == QMediaRecorder::StoppedState) {
        if (state == QMediaRecorder::RecordingState) {
            if (startVideoRecording()) {
                m_videoState = state;
            }
        } else if (state == QMediaRecorder::PausedState) {
            // do nothing
        }
    } else if (previousState == QMediaRecorder::RecordingState) {
        if (state == QMediaRecorder::StoppedState) {
            stopVideoRecording();
            m_videoState = state;
        } else if (state == QMediaRecorder::PausedState) {
            //TODO: (pause) not supported by BB10 API yet
        }
    } else if (previousState == QMediaRecorder::PausedState) {
        if (state == QMediaRecorder::StoppedState) {
            stopVideoRecording();
            m_videoState = state;
        } else if (state == QMediaRecorder::RecordingState) {
            //TODO: (resume) not supported by BB10 API yet
        }
    }

    emit videoStateChanged(m_videoState);
}

QMediaRecorder::Status BbCameraSession::videoStatus() const
{
    return m_videoStatus;
}

qint64 BbCameraSession::duration() const
{
    return (m_videoRecordingDuration.isValid() ? m_videoRecordingDuration.elapsed() : 0);
}

void BbCameraSession::applyVideoSettings()
{
    if (m_handle == CAMERA_HANDLE_INVALID)
        return;

    // apply viewfinder configuration
    const QList<QSize> videoOutputResolutions = supportedResolutions(QCamera::CaptureVideo);

    if (!m_videoEncoderSettings.resolution().isValid() || !videoOutputResolutions.contains(m_videoEncoderSettings.resolution()))
        m_videoEncoderSettings.setResolution(videoOutputResolutions.first());

    QSize viewfinderResolution;

    if (m_previewIsVideo) {
        // The viewfinder is responsible for encoding the video frames, so the resolutions must match.
        viewfinderResolution = m_videoEncoderSettings.resolution();
    } else {
        // The frames are encoded separately from the viewfinder, so only the aspect ratio must match.
        const QSize videoResolution = m_videoEncoderSettings.resolution();
        const qreal aspectRatio = static_cast<qreal>(videoResolution.width())/static_cast<qreal>(videoResolution.height());

        QList<QSize> sizes = supportedViewfinderResolutions(QCamera::CaptureVideo);
        std::reverse(sizes.begin(), sizes.end()); // use smallest possible resolution
        for (const QSize &size : qAsConst(sizes)) {
            // search for viewfinder resolution with the same aspect ratio
            if (qFuzzyCompare(aspectRatio, (static_cast<qreal>(size.width())/static_cast<qreal>(size.height())))) {
                viewfinderResolution = size;
                break;
            }
        }
    }

    Q_ASSERT(viewfinderResolution.isValid());

    const QByteArray windowId = QString().sprintf("qcamera_vf_%p", this).toLatin1();
    m_windowGrabber->setWindowId(windowId);

    const QByteArray windowGroupId = m_windowGrabber->windowGroupId();

    const int rotationAngle = (360 - m_nativeCameraOrientation);

    camera_error_t result = CAMERA_EOK;
    result = camera_set_videovf_property(m_handle,
                                         CAMERA_IMGPROP_WIN_GROUPID, windowGroupId.data(),
                                         CAMERA_IMGPROP_WIN_ID, windowId.data(),
                                         CAMERA_IMGPROP_WIDTH, viewfinderResolution.width(),
                                         CAMERA_IMGPROP_HEIGHT, viewfinderResolution.height(),
                                         CAMERA_IMGPROP_ROTATION, rotationAngle);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to apply video viewfinder settings:" << result;
        return;
    }

    const QSize resolution = m_videoEncoderSettings.resolution();

    QString videoCodec = m_videoEncoderSettings.codec();
    if (videoCodec.isEmpty())
        videoCodec = QLatin1String("h264");

    camera_videocodec_t cameraVideoCodec = CAMERA_VIDEOCODEC_H264;
    if (videoCodec == QLatin1String("none"))
        cameraVideoCodec = CAMERA_VIDEOCODEC_NONE;
    else if (videoCodec == QLatin1String("avc1"))
        cameraVideoCodec = CAMERA_VIDEOCODEC_AVC1;
    else if (videoCodec == QLatin1String("h264"))
        cameraVideoCodec = CAMERA_VIDEOCODEC_H264;

    qreal frameRate = m_videoEncoderSettings.frameRate();
    if (frameRate == 0) {
        const QList<qreal> frameRates = supportedFrameRates(QVideoEncoderSettings(), 0);
        if (!frameRates.isEmpty())
            frameRate = frameRates.last();
    }

    QString audioCodec = m_audioEncoderSettings.codec();
    if (audioCodec.isEmpty())
        audioCodec = QLatin1String("aac");

    camera_audiocodec_t cameraAudioCodec = CAMERA_AUDIOCODEC_AAC;
    if (audioCodec == QLatin1String("none"))
        cameraAudioCodec = CAMERA_AUDIOCODEC_NONE;
    else if (audioCodec == QLatin1String("aac"))
        cameraAudioCodec = CAMERA_AUDIOCODEC_AAC;
    else if (audioCodec == QLatin1String("raw"))
        cameraAudioCodec = CAMERA_AUDIOCODEC_RAW;

    result = camera_set_video_property(m_handle,
                                       CAMERA_IMGPROP_WIDTH, resolution.width(),
                                       CAMERA_IMGPROP_HEIGHT, resolution.height(),
                                       CAMERA_IMGPROP_ROTATION, rotationAngle,
                                       CAMERA_IMGPROP_VIDEOCODEC, cameraVideoCodec,
                                       CAMERA_IMGPROP_AUDIOCODEC, cameraAudioCodec);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to apply video settings:" << result;
        emit videoError(QMediaRecorder::ResourceError, tr("Unable to apply video settings"));
    }
}

QList<QSize> BbCameraSession::supportedResolutions(const QVideoEncoderSettings &settings, bool *continuous) const
{
    Q_UNUSED(settings);

    if (continuous)
        *continuous = false;

    return supportedResolutions(QCamera::CaptureVideo);
}

QList<qreal> BbCameraSession::supportedFrameRates(const QVideoEncoderSettings &settings, bool *continuous) const
{
    Q_UNUSED(settings);

    if (m_handle == CAMERA_HANDLE_INVALID)
        return QList<qreal>();

    int supported = 0;
    double rates[20];
    bool maxmin = false;

    /**
     * Since in current version of the BB10 platform the video viewfinder encodes the video frames, we use
     * the values as returned by camera_get_video_vf_framerates().
     */
    const camera_error_t result = camera_get_video_vf_framerates(m_handle, 20, &supported, rates, &maxmin);
    if (result != CAMERA_EOK) {
        qWarning() << "Unable to retrieve supported viewfinder framerates:" << result;
        return QList<qreal>();
    }

    QList<qreal> frameRates;
    for (int i = 0; i < supported; ++i)
        frameRates << rates[i];

    if (continuous)
        *continuous = maxmin;

    return frameRates;
}

QVideoEncoderSettings BbCameraSession::videoSettings() const
{
    return m_videoEncoderSettings;
}

void BbCameraSession::setVideoSettings(const QVideoEncoderSettings &settings)
{
    m_videoEncoderSettings = settings;
}

QAudioEncoderSettings BbCameraSession::audioSettings() const
{
    return m_audioEncoderSettings;
}

void BbCameraSession::setAudioSettings(const QAudioEncoderSettings &settings)
{
    m_audioEncoderSettings = settings;
}

void BbCameraSession::updateReadyForCapture()
{
    emit readyForCaptureChanged(isReadyForCapture());
}

void BbCameraSession::imageCaptured(int requestId, const QImage &rawImage, const QString &fileName)
{
    QTransform transform;

    // subtract out the native rotation
    transform.rotate(m_nativeCameraOrientation);

    // subtract out the current device orientation
    if (m_device == cameraIdentifierRear())
        transform.rotate(360 - m_orientationHandler->orientation());
    else
        transform.rotate(m_orientationHandler->orientation());

    const QImage image = rawImage.transformed(transform);

    // Generate snap preview as downscaled image
    {
        QSize previewSize = image.size();
        int downScaleSteps = 0;
        while (previewSize.width() > 800 && downScaleSteps < 8) {
            previewSize.rwidth() /= 2;
            previewSize.rheight() /= 2;
            downScaleSteps++;
        }

        const QImage snapPreview = image.scaled(previewSize);

        emit imageCaptured(requestId, snapPreview);
    }

    if (m_captureDestination & QCameraImageCapture::CaptureToBuffer) {
        QVideoFrame frame(image);

        emit imageAvailable(requestId, frame);
    }

    if (m_captureDestination & QCameraImageCapture::CaptureToFile) {
        const QString actualFileName = m_mediaStorageLocation.generateFileName(fileName,
                                                                               QCamera::CaptureStillImage,
                                                                               QLatin1String("IMG_"),
                                                                               QLatin1String("jpg"));

        QFile file(actualFileName);
        if (file.open(QFile::WriteOnly)) {
            if (image.save(&file, "JPG")) {
                emit imageSaved(requestId, actualFileName);
            } else {
                emit imageCaptureError(requestId, QCameraImageCapture::OutOfSpaceError, file.errorString());
            }
        } else {
            const QString errorMessage = tr("Could not open destination file:\n%1").arg(actualFileName);
            emit imageCaptureError(requestId, QCameraImageCapture::ResourceError, errorMessage);
        }
    }
}

void BbCameraSession::handleVideoRecordingPaused()
{
    //TODO: implement once BB10 API supports pausing a video
}

void BbCameraSession::handleVideoRecordingResumed()
{
    if (m_videoStatus == QMediaRecorder::StartingStatus) {
        m_videoStatus = QMediaRecorder::RecordingStatus;
        emit videoStatusChanged(m_videoStatus);

        m_videoRecordingDuration.restart();
    }
}

void BbCameraSession::deviceOrientationChanged(int angle)
{
    if (m_handle != CAMERA_HANDLE_INVALID)
        camera_set_device_orientation(m_handle, angle);
}

void BbCameraSession::handleCameraPowerUp()
{
    stopViewFinder();
    startViewFinder();
}

void BbCameraSession::viewfinderFrameGrabbed(const QImage &image)
{
    QTransform transform;

    // subtract out the native rotation
    transform.rotate(m_nativeCameraOrientation);

    // subtract out the current device orientation
    if (m_device == cameraIdentifierRear())
        transform.rotate(360 - m_orientationHandler->viewfinderOrientation());
    else
        transform.rotate(m_orientationHandler->viewfinderOrientation());

    QImage frame = image.copy().transformed(transform);

    QMutexLocker locker(&m_surfaceMutex);
    if (m_surface) {
        if (frame.size() != m_surface->surfaceFormat().frameSize()) {
            m_surface->stop();
            m_surface->start(QVideoSurfaceFormat(frame.size(), QVideoFrame::Format_ARGB32));
        }

        QVideoFrame videoFrame(frame);

        m_surface->present(videoFrame);
    }
}

bool BbCameraSession::openCamera()
{
    if (m_handle != CAMERA_HANDLE_INVALID) // camera is already open
        return true;

    m_status = QCamera::LoadingStatus;
    emit statusChanged(m_status);

    camera_unit_t unit = CAMERA_UNIT_REAR;
    if (m_device == cameraIdentifierFront())
        unit = CAMERA_UNIT_FRONT;
    else if (m_device == cameraIdentifierRear())
        unit = CAMERA_UNIT_REAR;
    else if (m_device == cameraIdentifierDesktop())
        unit = CAMERA_UNIT_DESKTOP;

    camera_error_t result = camera_open(unit, CAMERA_MODE_RW, &m_handle);
    if (result != CAMERA_EOK) {
        m_handle = CAMERA_HANDLE_INVALID;
        m_status = QCamera::UnloadedStatus;
        emit statusChanged(m_status);

        qWarning() << "Unable to open camera:" << result;
        emit error(QCamera::CameraError, tr("Unable to open camera"));
        return false;
    }

    result = camera_get_native_orientation(m_handle, &m_nativeCameraOrientation);
    if (result != CAMERA_EOK) {
        qWarning() << "Unable to retrieve native camera orientation:" << result;
        emit error(QCamera::CameraError, tr("Unable to retrieve native camera orientation"));
        return false;
    }

    m_previewIsVideo = camera_has_feature(m_handle, CAMERA_FEATURE_PREVIEWISVIDEO);

    m_status = QCamera::LoadedStatus;
    emit statusChanged(m_status);

    emit cameraOpened();

    return true;
}

void BbCameraSession::closeCamera()
{
    if (m_handle == CAMERA_HANDLE_INVALID) // camera is closed already
        return;

    m_status = QCamera::UnloadingStatus;
    emit statusChanged(m_status);

    const camera_error_t result = camera_close(m_handle);
    if (result != CAMERA_EOK) {
        m_status = QCamera::LoadedStatus;
        emit statusChanged(m_status);

        qWarning() << "Unable to close camera:" << result;
        emit error(QCamera::CameraError, tr("Unable to close camera"));
        return;
    }

    m_handle = CAMERA_HANDLE_INVALID;

    m_status = QCamera::UnloadedStatus;
    emit statusChanged(m_status);
}

static void viewFinderStatusCallback(camera_handle_t handle, camera_devstatus_t status, uint16_t value, void *context)
{
    Q_UNUSED(handle)

    if (status == CAMERA_STATUS_FOCUS_CHANGE) {
        BbCameraSession *session = static_cast<BbCameraSession*>(context);
        QMetaObject::invokeMethod(session, "focusStatusChanged", Qt::QueuedConnection, Q_ARG(int, value));
        return;
    } else if (status == CAMERA_STATUS_POWERUP) {
        BbCameraSession *session = static_cast<BbCameraSession*>(context);
        QMetaObject::invokeMethod(session, "handleCameraPowerUp", Qt::QueuedConnection);
    }
}

bool BbCameraSession::startViewFinder()
{
    m_status = QCamera::StartingStatus;
    emit statusChanged(m_status);

    QSize viewfinderResolution;
    camera_error_t result = CAMERA_EOK;
    if (m_captureMode & QCamera::CaptureStillImage) {
        result = camera_start_photo_viewfinder(m_handle, 0, viewFinderStatusCallback, this);
        viewfinderResolution = currentViewfinderResolution(QCamera::CaptureStillImage);
    } else if (m_captureMode & QCamera::CaptureVideo) {
        result = camera_start_video_viewfinder(m_handle, 0, viewFinderStatusCallback, this);
        viewfinderResolution = currentViewfinderResolution(QCamera::CaptureVideo);
    }

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to start viewfinder:" << result;
        return false;
    }

    const int angle = m_orientationHandler->viewfinderOrientation();

    const QSize rotatedSize = ((angle == 0 || angle == 180) ? viewfinderResolution
                                                            : viewfinderResolution.transposed());

    m_surfaceMutex.lock();
    if (m_surface) {
        const bool ok = m_surface->start(QVideoSurfaceFormat(rotatedSize, QVideoFrame::Format_ARGB32));
        if (!ok)
            qWarning() << "Unable to start camera viewfinder surface";
    }
    m_surfaceMutex.unlock();

    m_status = QCamera::ActiveStatus;
    emit statusChanged(m_status);

    return true;
}

void BbCameraSession::stopViewFinder()
{
    m_windowGrabber->stop();

    m_status = QCamera::StoppingStatus;
    emit statusChanged(m_status);

    m_surfaceMutex.lock();
    if (m_surface) {
        m_surface->stop();
    }
    m_surfaceMutex.unlock();

    camera_error_t result = CAMERA_EOK;
    if (m_captureMode & QCamera::CaptureStillImage)
        result = camera_stop_photo_viewfinder(m_handle);
    else if (m_captureMode & QCamera::CaptureVideo)
        result = camera_stop_video_viewfinder(m_handle);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to stop viewfinder:" << result;
        return;
    }

    m_status = QCamera::LoadedStatus;
    emit statusChanged(m_status);
}

void BbCameraSession::applyConfiguration()
{
    if (m_captureMode & QCamera::CaptureStillImage) {
        const QList<QSize> photoOutputResolutions = supportedResolutions(QCamera::CaptureStillImage);

        if (!m_imageEncoderSettings.resolution().isValid() || !photoOutputResolutions.contains(m_imageEncoderSettings.resolution()))
            m_imageEncoderSettings.setResolution(photoOutputResolutions.first());

        const QSize photoResolution = m_imageEncoderSettings.resolution();
        const qreal aspectRatio = static_cast<qreal>(photoResolution.width())/static_cast<qreal>(photoResolution.height());

        // apply viewfinder configuration
        QSize viewfinderResolution;
        QList<QSize> sizes = supportedViewfinderResolutions(QCamera::CaptureStillImage);
        std::reverse(sizes.begin(), sizes.end()); // use smallest possible resolution
        for (const QSize &size : qAsConst(sizes)) {
            // search for viewfinder resolution with the same aspect ratio
            if (qFuzzyCompare(aspectRatio, (static_cast<qreal>(size.width())/static_cast<qreal>(size.height())))) {
                viewfinderResolution = size;
                break;
            }
        }

        Q_ASSERT(viewfinderResolution.isValid());

        const QByteArray windowId = QString().sprintf("qcamera_vf_%p", this).toLatin1();
        m_windowGrabber->setWindowId(windowId);

        const QByteArray windowGroupId = m_windowGrabber->windowGroupId();

        camera_error_t result = camera_set_photovf_property(m_handle,
                                                            CAMERA_IMGPROP_WIN_GROUPID, windowGroupId.data(),
                                                            CAMERA_IMGPROP_WIN_ID, windowId.data(),
                                                            CAMERA_IMGPROP_WIDTH, viewfinderResolution.width(),
                                                            CAMERA_IMGPROP_HEIGHT, viewfinderResolution.height(),
                                                            CAMERA_IMGPROP_FORMAT, CAMERA_FRAMETYPE_NV12,
                                                            CAMERA_IMGPROP_ROTATION, 360 - m_nativeCameraOrientation);

        if (result != CAMERA_EOK) {
            qWarning() << "Unable to apply photo viewfinder settings:" << result;
            return;
        }


        int jpegQuality = 100;
        switch (m_imageEncoderSettings.quality()) {
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

        // apply photo configuration
        result = camera_set_photo_property(m_handle,
                                           CAMERA_IMGPROP_WIDTH, photoResolution.width(),
                                           CAMERA_IMGPROP_HEIGHT, photoResolution.height(),
                                           CAMERA_IMGPROP_JPEGQFACTOR, jpegQuality,
                                           CAMERA_IMGPROP_ROTATION, 360 - m_nativeCameraOrientation);

        if (result != CAMERA_EOK) {
            qWarning() << "Unable to apply photo settings:" << result;
            return;
        }

    } else if (m_captureMode & QCamera::CaptureVideo) {
        applyVideoSettings();
    }

    startViewFinder();
}

static void videoRecordingStatusCallback(camera_handle_t handle, camera_devstatus_t status, uint16_t value, void *context)
{
    Q_UNUSED(handle)
    Q_UNUSED(value)

    if (status == CAMERA_STATUS_VIDEO_PAUSE) {
        BbCameraSession *session = static_cast<BbCameraSession*>(context);
        QMetaObject::invokeMethod(session, "handleVideoRecordingPaused", Qt::QueuedConnection);
    } else if (status == CAMERA_STATUS_VIDEO_RESUME) {
        BbCameraSession *session = static_cast<BbCameraSession*>(context);
        QMetaObject::invokeMethod(session, "handleVideoRecordingResumed", Qt::QueuedConnection);
    }
}

bool BbCameraSession::startVideoRecording()
{
    m_videoRecordingDuration.invalidate();

    m_videoStatus = QMediaRecorder::StartingStatus;
    emit videoStatusChanged(m_videoStatus);

    if (m_videoOutputLocation.isEmpty())
        m_videoOutputLocation = m_mediaStorageLocation.generateFileName(QLatin1String("VID_"), m_mediaStorageLocation.defaultDir(QCamera::CaptureVideo), QLatin1String("mp4"));

    emit actualLocationChanged(m_videoOutputLocation);

    const camera_error_t result = camera_start_video(m_handle, QFile::encodeName(m_videoOutputLocation), 0, videoRecordingStatusCallback, this);
    if (result != CAMERA_EOK) {
        m_videoStatus = QMediaRecorder::LoadedStatus;
        emit videoStatusChanged(m_videoStatus);

        emit videoError(QMediaRecorder::ResourceError, tr("Unable to start video recording"));
        return false;
    }

    return true;
}

void BbCameraSession::stopVideoRecording()
{
    m_videoStatus = QMediaRecorder::FinalizingStatus;
    emit videoStatusChanged(m_videoStatus);

    const camera_error_t result = camera_stop_video(m_handle);
    if (result != CAMERA_EOK) {
        emit videoError(QMediaRecorder::ResourceError, tr("Unable to stop video recording"));
    }

    m_videoStatus = QMediaRecorder::LoadedStatus;
    emit videoStatusChanged(m_videoStatus);

    m_videoRecordingDuration.invalidate();
}

bool BbCameraSession::isCaptureModeSupported(camera_handle_t handle, QCamera::CaptureModes mode) const
{
    if (mode & QCamera::CaptureStillImage)
        return camera_has_feature(handle, CAMERA_FEATURE_PHOTO);

    if (mode & QCamera::CaptureVideo)
        return camera_has_feature(handle, CAMERA_FEATURE_VIDEO);

    return false;
}

QList<QSize> BbCameraSession::supportedResolutions(QCamera::CaptureMode mode) const
{
    Q_ASSERT(m_handle != CAMERA_HANDLE_INVALID);

    QList<QSize> list;

    camera_error_t result = CAMERA_EOK;
    camera_res_t resolutions[20];
    unsigned int supported = 0;

    if (mode == QCamera::CaptureStillImage)
        result = camera_get_photo_output_resolutions(m_handle, CAMERA_FRAMETYPE_JPEG, 20, &supported, resolutions);
    else if (mode == QCamera::CaptureVideo)
        result = camera_get_video_output_resolutions(m_handle, 20, &supported, resolutions);

    if (result != CAMERA_EOK)
        return list;

    for (unsigned int i = 0; i < supported; ++i)
        list << QSize(resolutions[i].width, resolutions[i].height);

    return list;
}

QList<QSize> BbCameraSession::supportedViewfinderResolutions(QCamera::CaptureMode mode) const
{
    Q_ASSERT(m_handle != CAMERA_HANDLE_INVALID);

    QList<QSize> list;

    camera_error_t result = CAMERA_EOK;
    camera_res_t resolutions[20];
    unsigned int supported = 0;

    if (mode == QCamera::CaptureStillImage)
        result = camera_get_photo_vf_resolutions(m_handle, 20, &supported, resolutions);
    else if (mode == QCamera::CaptureVideo)
        result = camera_get_video_vf_resolutions(m_handle, 20, &supported, resolutions);

    if (result != CAMERA_EOK)
        return list;

    for (unsigned int i = 0; i < supported; ++i)
        list << QSize(resolutions[i].width, resolutions[i].height);

    return list;
}

QSize BbCameraSession::currentViewfinderResolution(QCamera::CaptureMode mode) const
{
    Q_ASSERT(m_handle != CAMERA_HANDLE_INVALID);

    camera_error_t result = CAMERA_EOK;
    int width = 0;
    int height = 0;

    if (mode == QCamera::CaptureStillImage)
        result = camera_get_photovf_property(m_handle, CAMERA_IMGPROP_WIDTH, &width,
                                                       CAMERA_IMGPROP_HEIGHT, &height);
    else if (mode == QCamera::CaptureVideo)
        result = camera_get_videovf_property(m_handle, CAMERA_IMGPROP_WIDTH, &width,
                                                       CAMERA_IMGPROP_HEIGHT, &height);

    if (result != CAMERA_EOK)
        return QSize();

    return QSize(width, height);
}

QT_END_NAMESPACE
