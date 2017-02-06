/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qdeclarativecamera_p.h"
#include "qdeclarativecameracapture_p.h"
#include "qdeclarativecamerapreviewprovider_p.h"

#include <qmetadatawritercontrol.h>

#include <QtCore/qurl.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype CameraCapture
    \instantiates QDeclarativeCameraCapture
    \brief An interface for capturing camera images
    \ingroup multimedia_qml
    \inqmlmodule QtMultimedia
    \ingroup camera_qml

    This type allows you to capture still images and be notified when they
    are available or saved to disk.  You can adjust the resolution of the captured
    image and where the saved image should go.

    CameraCapture is a child of a \l Camera (as the \c imageCapture property)
    and cannot be created directly.

    \qml
    Item {
        width: 640
        height: 360

        Camera {
            id: camera

            imageCapture {
                onImageCaptured: {
                    // Show the preview in an Image
                    photoPreview.source = preview
                }
            }
        }

        VideoOutput {
            source: camera
            focus : visible // to receive focus and capture key events when visible
            anchors.fill: parent

            MouseArea {
                anchors.fill: parent;
                onClicked: camera.imageCapture.capture();
            }
        }

        Image {
            id: photoPreview
        }
    }
    \endqml

*/

QDeclarativeCameraCapture::QDeclarativeCameraCapture(QCamera *camera, QObject *parent) :
    QObject(parent),
    m_camera(camera)
{
    m_capture = new QCameraImageCapture(camera, this);

    connect(m_capture, SIGNAL(readyForCaptureChanged(bool)), this, SIGNAL(readyForCaptureChanged(bool)));
    connect(m_capture, SIGNAL(imageExposed(int)), this, SIGNAL(imageExposed(int)));
    connect(m_capture, SIGNAL(imageCaptured(int,QImage)), this, SLOT(_q_imageCaptured(int,QImage)));
    connect(m_capture, SIGNAL(imageMetadataAvailable(int,QString,QVariant)), this,
            SLOT(_q_imageMetadataAvailable(int,QString,QVariant)));
    connect(m_capture, SIGNAL(imageSaved(int,QString)), this, SLOT(_q_imageSaved(int,QString)));
    connect(m_capture, SIGNAL(error(int,QCameraImageCapture::Error,QString)),
            this, SLOT(_q_captureFailed(int,QCameraImageCapture::Error,QString)));

    QMediaService *service = camera->service();
    m_metadataWriterControl = service ? service->requestControl<QMetaDataWriterControl*>() : 0;
}

QDeclarativeCameraCapture::~QDeclarativeCameraCapture()
{
}

/*!
    \property QDeclarativeCameraCapture::ready

    This property holds a bool value indicating whether the camera
    is ready to capture photos or not.

    Calling capture() while \e ready is \c false is not permitted and
    results in an error.
*/

/*!
    \qmlproperty bool QtMultimedia::CameraCapture::ready

    This property holds a bool value indicating whether the camera
    is ready to capture photos or not.

    Calling capture() while \e ready is \c false is not permitted and
    results in an error.
*/
bool QDeclarativeCameraCapture::isReadyForCapture() const
{
    return m_capture->isReadyForCapture();
}

/*!
    \qmlmethod QtMultimedia::CameraCapture::capture()

    Start image capture.  The \l imageCaptured and \l imageSaved signals will
    be emitted when the capture is complete.

    The image will be captured to the default system location, typically
    QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) for
    still imaged or QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
    for video.

    Camera saves all the capture parameters like exposure settings or
    image processing parameters, so changes to camera parameters after
    capture() is called do not affect previous capture requests.

    capture() returns the capture requestId parameter, used with
    imageExposed(), imageCaptured(), imageMetadataAvailable() and imageSaved() signals.

    \sa ready
*/
int QDeclarativeCameraCapture::capture()
{
    return m_capture->capture();
}

/*!
    \qmlmethod QtMultimedia::CameraCapture::captureToLocation(location)

    Start image capture to specified \a location.  The \l imageCaptured and \l imageSaved signals will
    be emitted when the capture is complete.

    CameraCapture::captureToLocation returns the capture requestId parameter, used with
    imageExposed(), imageCaptured(), imageMetadataAvailable() and imageSaved() signals.

    If the application is unable to write to the location specified by \c location
    the CameraCapture will emit an error. The most likely reasons for the application
    to be unable to write to a location is that the path is wrong and the location does not exists,
    or the application does not have write permission for that location.
*/
int QDeclarativeCameraCapture::captureToLocation(const QString &location)
{
    return m_capture->capture(location);
}

/*!
    \qmlmethod QtMultimedia::CameraCapture::cancelCapture()

    Cancel pending image capture requests.
*/

void QDeclarativeCameraCapture::cancelCapture()
{
    m_capture->cancelCapture();
}
/*!
    \property QDeclarativeCameraCapture::capturedImagePath

    This property holds the location of the last captured image.
*/
/*!
    \qmlproperty string QtMultimedia::CameraCapture::capturedImagePath

    This property holds the location of the last captured image.
*/
QString QDeclarativeCameraCapture::capturedImagePath() const
{
    return m_capturedImagePath;
}

void QDeclarativeCameraCapture::_q_imageCaptured(int id, const QImage &preview)
{
    QString previewId = QString("preview_%1").arg(id);
    QDeclarativeCameraPreviewProvider::registerPreview(previewId, preview);

    emit imageCaptured(id, QLatin1String("image://camera/")+previewId);
}

void QDeclarativeCameraCapture::_q_imageSaved(int id, const QString &fileName)
{
    m_capturedImagePath = fileName;
    emit imageSaved(id, fileName);
}

void QDeclarativeCameraCapture::_q_imageMetadataAvailable(int id, const QString &key, const QVariant &value)
{
    emit imageMetadataAvailable(id, key, value);
}


void QDeclarativeCameraCapture::_q_captureFailed(int id, QCameraImageCapture::Error error, const QString &message)
{
    Q_UNUSED(error);
    qWarning() << "QCameraImageCapture error:" << message;
    emit captureFailed(id, message);
}
/*!
    \property QDeclarativeCameraCapture::resolution

    This property holds the resolution/size of the image to be captured.
    If empty, the system chooses the appropriate resolution.
*/

/*!
    \qmlproperty size QtMultimedia::CameraCapture::resolution

    This property holds the resolution/size of the image to be captured.
    If empty, the system chooses the appropriate resolution.
*/

QSize QDeclarativeCameraCapture::resolution()
{
    return m_imageSettings.resolution();
}

void QDeclarativeCameraCapture::setResolution(const QSize &captureResolution)
{
    m_imageSettings = m_capture->encodingSettings();
    if (captureResolution != resolution()) {
        m_imageSettings.setResolution(captureResolution);
        m_capture->setEncodingSettings(m_imageSettings);
        emit resolutionChanged(captureResolution);
    }
}

QCameraImageCapture::Error QDeclarativeCameraCapture::error() const
{
    return m_capture->error();
}
/*!
    \property QDeclarativeCameraCapture::errorString

    This property holds the error message related to the last capture.
*/

/*!
    \qmlproperty string QtMultimedia::CameraCapture::errorString

    This property holds the error message related to the last capture.
*/
QString QDeclarativeCameraCapture::errorString() const
{
    return m_capture->errorString();
}

/*!
    \qmlmethod QtMultimedia::CameraCapture::setMetadata(key, value)


    Sets a particular metadata \a key to \a value for the subsequent image captures.

    \sa QMediaMetaData
*/
void QDeclarativeCameraCapture::setMetadata(const QString &key, const QVariant &value)
{
    if (m_metadataWriterControl)
        m_metadataWriterControl->setMetaData(key, value);
}

/*!
    \qmlsignal QtMultimedia::CameraCapture::captureFailed(requestId, message)

    This signal is emitted when an error occurs during capture with \a requestId.
    A descriptive message is available in \a message.

    The corresponding handler is \c onCaptureFailed.
*/

/*!
    \qmlsignal QtMultimedia::CameraCapture::imageCaptured(requestId, preview)

    This signal is emitted when an image with \a requestId has been captured
    but not yet saved to the filesystem.  The \a preview
    parameter can be used as the URL supplied to an \l Image.

    The corresponding handler is \c onImageCaptured.

    \sa imageSaved
*/

/*!
    \qmlsignal QtMultimedia::CameraCapture::imageSaved(requestId, path)

    This signal is emitted after the image with \a requestId has been written to the filesystem.
    The \a path is a local file path, not a URL.

    The corresponding handler is \c onImageSaved.

    \sa imageCaptured
*/


/*!
    \qmlsignal QtMultimedia::CameraCapture::imageMetadataAvailable(requestId, key, value)

    This signal is emitted when the image with \a requestId has new metadata
    available with the key \a key and value \a value.

    The corresponding handler is \c onImageMetadataAvailable.

    \sa imageCaptured
*/


QT_END_NAMESPACE

#include "moc_qdeclarativecameracapture_p.cpp"
