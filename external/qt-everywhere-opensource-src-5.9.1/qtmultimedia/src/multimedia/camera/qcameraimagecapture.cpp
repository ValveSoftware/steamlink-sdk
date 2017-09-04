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
#include <qcameraimagecapture.h>
#include <qcameraimagecapturecontrol.h>
#include <qmediaencodersettings.h>
#include <qcameracapturedestinationcontrol.h>
#include <qcameracapturebufferformatcontrol.h>

#include <qimageencodercontrol.h>
#include "qmediaobject_p.h"
#include <qmediaservice.h>
#include <qcamera.h>
#include <qcameracontrol.h>
#include <QtCore/qdebug.h>
#include <QtCore/qurl.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qmetaobject.h>

QT_BEGIN_NAMESPACE

/*!
    \class QCameraImageCapture
    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_camera


    \brief The QCameraImageCapture class is used for the recording of media content.

    The QCameraImageCapture class is a high level images recording class.
    It's not intended to be used alone but for accessing the media
    recording functions of other media objects, like QCamera.

    \snippet multimedia-snippets/camera.cpp Camera

    \snippet multimedia-snippets/camera.cpp Camera keys

    \sa QCamera
*/

/*!
    \enum QCameraImageCapture::CaptureDestination

    \value CaptureToFile  Capture the image to a file.
    \value CaptureToBuffer  Capture the image to a buffer for further processing.
*/

static void qRegisterCameraImageCaptureMetaTypes()
{
    qRegisterMetaType<QCameraImageCapture::Error>("QCameraImageCapture::Error");
    qRegisterMetaType<QCameraImageCapture::CaptureDestination>("QCameraImageCapture::CaptureDestination");
    qRegisterMetaType<QCameraImageCapture::CaptureDestinations>("QCameraImageCapture::CaptureDestinations");
}

Q_CONSTRUCTOR_FUNCTION(qRegisterCameraImageCaptureMetaTypes)


class QCameraImageCapturePrivate
{
    Q_DECLARE_NON_CONST_PUBLIC(QCameraImageCapture)
public:
    QCameraImageCapturePrivate();

    QMediaObject *mediaObject;

    QCameraImageCaptureControl *control;
    QImageEncoderControl *encoderControl;
    QCameraCaptureDestinationControl *captureDestinationControl;
    QCameraCaptureBufferFormatControl *bufferFormatControl;

    QCameraImageCapture::Error error;
    QString errorString;

    void _q_error(int id, int error, const QString &errorString);
    void _q_readyChanged(bool);
    void _q_serviceDestroyed();

    void unsetError() { error = QCameraImageCapture::NoError; errorString.clear(); }

    QCameraImageCapture *q_ptr;
};

QCameraImageCapturePrivate::QCameraImageCapturePrivate():
     mediaObject(0),
     control(0),
     encoderControl(0),
     captureDestinationControl(0),
     bufferFormatControl(0),
     error(QCameraImageCapture::NoError)
{
}

void QCameraImageCapturePrivate::_q_error(int id, int error, const QString &errorString)
{
    Q_Q(QCameraImageCapture);

    this->error = QCameraImageCapture::Error(error);
    this->errorString = errorString;

    emit q->error(id, this->error, errorString);
}

void QCameraImageCapturePrivate::_q_readyChanged(bool ready)
{
    Q_Q(QCameraImageCapture);
    emit q->readyForCaptureChanged(ready);
}

void QCameraImageCapturePrivate::_q_serviceDestroyed()
{
    mediaObject = 0;
    control = 0;
    encoderControl = 0;
    captureDestinationControl = 0;
    bufferFormatControl = 0;
}

/*!
    Constructs a media recorder which records the media produced by \a mediaObject.

    The \a parent is passed to QMediaObject.
*/

QCameraImageCapture::QCameraImageCapture(QMediaObject *mediaObject, QObject *parent):
    QObject(parent), d_ptr(new QCameraImageCapturePrivate)
{
    Q_D(QCameraImageCapture);

    d->q_ptr = this;

    if (mediaObject)
        mediaObject->bind(this);
}

/*!
    Destroys images capture object.
*/

QCameraImageCapture::~QCameraImageCapture()
{
    Q_D(QCameraImageCapture);

    if (d->mediaObject)
        d->mediaObject->unbind(this);

    delete d_ptr;
}

/*!
  \reimp
*/
QMediaObject *QCameraImageCapture::mediaObject() const
{
    return d_func()->mediaObject;
}

/*!
  \reimp
*/
bool QCameraImageCapture::setMediaObject(QMediaObject *mediaObject)
{
    Q_D(QCameraImageCapture);

    if (d->mediaObject) {
        if (d->control) {
            disconnect(d->control, SIGNAL(imageExposed(int)),
                       this, SIGNAL(imageExposed(int)));
            disconnect(d->control, SIGNAL(imageCaptured(int,QImage)),
                       this, SIGNAL(imageCaptured(int,QImage)));
            disconnect(d->control, SIGNAL(imageAvailable(int,QVideoFrame)),
                       this, SIGNAL(imageAvailable(int,QVideoFrame)));
            disconnect(d->control, SIGNAL(imageMetadataAvailable(int,QString,QVariant)),
                       this, SIGNAL(imageMetadataAvailable(int,QString,QVariant)));
            disconnect(d->control, SIGNAL(imageSaved(int,QString)),
                       this, SIGNAL(imageSaved(int,QString)));
            disconnect(d->control, SIGNAL(readyForCaptureChanged(bool)),
                       this, SLOT(_q_readyChanged(bool)));
            disconnect(d->control, SIGNAL(error(int,int,QString)),
                       this, SLOT(_q_error(int,int,QString)));

            if (d->captureDestinationControl) {
                disconnect(d->captureDestinationControl, SIGNAL(captureDestinationChanged(QCameraImageCapture::CaptureDestinations)),
                           this, SIGNAL(captureDestinationChanged(QCameraImageCapture::CaptureDestinations)));
            }

            if (d->bufferFormatControl) {
                disconnect(d->bufferFormatControl, SIGNAL(bufferFormatChanged(QVideoFrame::PixelFormat)),
                           this, SIGNAL(bufferFormatChanged(QVideoFrame::PixelFormat)));
            }

            QMediaService *service = d->mediaObject->service();
            service->releaseControl(d->control);
            if (d->encoderControl)
                service->releaseControl(d->encoderControl);
            if (d->captureDestinationControl)
                service->releaseControl(d->captureDestinationControl);
            if (d->bufferFormatControl)
                service->releaseControl(d->bufferFormatControl);

            disconnect(service, SIGNAL(destroyed()), this, SLOT(_q_serviceDestroyed()));
        }
    }

    d->mediaObject = mediaObject;

    if (d->mediaObject) {
        QMediaService *service = mediaObject->service();
        if (service) {
            d->control = qobject_cast<QCameraImageCaptureControl*>(service->requestControl(QCameraImageCaptureControl_iid));

            if (d->control) {
                d->encoderControl = qobject_cast<QImageEncoderControl *>(service->requestControl(QImageEncoderControl_iid));
                d->captureDestinationControl = qobject_cast<QCameraCaptureDestinationControl *>(
                    service->requestControl(QCameraCaptureDestinationControl_iid));
                d->bufferFormatControl = qobject_cast<QCameraCaptureBufferFormatControl *>(
                    service->requestControl(QCameraCaptureBufferFormatControl_iid));

                connect(d->control, SIGNAL(imageExposed(int)),
                        this, SIGNAL(imageExposed(int)));
                connect(d->control, SIGNAL(imageCaptured(int,QImage)),
                        this, SIGNAL(imageCaptured(int,QImage)));
                connect(d->control, SIGNAL(imageMetadataAvailable(int,QString,QVariant)),
                        this, SIGNAL(imageMetadataAvailable(int,QString,QVariant)));
                connect(d->control, SIGNAL(imageAvailable(int,QVideoFrame)),
                        this, SIGNAL(imageAvailable(int,QVideoFrame)));
                connect(d->control, SIGNAL(imageSaved(int,QString)),
                        this, SIGNAL(imageSaved(int,QString)));
                connect(d->control, SIGNAL(readyForCaptureChanged(bool)),
                        this, SLOT(_q_readyChanged(bool)));
                connect(d->control, SIGNAL(error(int,int,QString)),
                        this, SLOT(_q_error(int,int,QString)));

                if (d->captureDestinationControl) {
                    connect(d->captureDestinationControl, SIGNAL(captureDestinationChanged(QCameraImageCapture::CaptureDestinations)),
                            this, SIGNAL(captureDestinationChanged(QCameraImageCapture::CaptureDestinations)));
                }

                if (d->bufferFormatControl) {
                    connect(d->bufferFormatControl, SIGNAL(bufferFormatChanged(QVideoFrame::PixelFormat)),
                            this, SIGNAL(bufferFormatChanged(QVideoFrame::PixelFormat)));
                }

                connect(service, SIGNAL(destroyed()), this, SLOT(_q_serviceDestroyed()));

                return true;
            }
        }
    }

    // without QCameraImageCaptureControl discard the media object
    d->mediaObject = 0;
    d->control = 0;
    d->encoderControl = 0;
    d->captureDestinationControl = 0;
    d->bufferFormatControl = 0;

    return false;
}

/*!
    Returns true if the images capture service ready to use.
*/
bool QCameraImageCapture::isAvailable() const
{
    if (d_func()->control != NULL)
        return true;
    else
        return false;
}

/*!
    Returns the availability of this functionality.
*/
QMultimedia::AvailabilityStatus QCameraImageCapture::availability() const
{
    if (d_func()->control != NULL)
        return QMultimedia::Available;
    else
        return QMultimedia::ServiceMissing;
}

/*!
    Returns the current error state.

    \sa errorString()
*/

QCameraImageCapture::Error QCameraImageCapture::error() const
{
    return d_func()->error;
}

/*!
    Returns a string describing the current error state.

    \sa error()
*/

QString QCameraImageCapture::errorString() const
{
    return d_func()->errorString;
}


/*!
    Returns a list of supported image codecs.
*/
QStringList QCameraImageCapture::supportedImageCodecs() const
{
    return d_func()->encoderControl ?
           d_func()->encoderControl->supportedImageCodecs() : QStringList();
}

/*!
    Returns a description of an image \a codec.
*/
QString QCameraImageCapture::imageCodecDescription(const QString &codec) const
{
    return d_func()->encoderControl ?
           d_func()->encoderControl->imageCodecDescription(codec) : QString();
}

/*!
    Returns a list of resolutions images can be encoded at.

    If non null image \a settings parameter is passed,
    the returned list is reduced to resolution supported with partial settings like image codec or quality applied.

    If the encoder supports arbitrary resolutions within the supported range,
    *\a continuous is set to true, otherwise *\a continuous is set to false.

    \sa QImageEncoderSettings::resolution()
*/
QList<QSize> QCameraImageCapture::supportedResolutions(const QImageEncoderSettings &settings, bool *continuous) const
{
    if (continuous)
        *continuous = false;

    return d_func()->encoderControl ?
           d_func()->encoderControl->supportedResolutions(settings, continuous) : QList<QSize>();
}

/*!
    Returns the image encoder settings being used.

    \sa setEncodingSettings()
*/

QImageEncoderSettings QCameraImageCapture::encodingSettings() const
{
    return d_func()->encoderControl ?
           d_func()->encoderControl->imageSettings() : QImageEncoderSettings();
}

/*!
    Sets the image encoding \a settings.

    If some parameters are not specified, or null settings are passed,
    the encoder choose the default encoding parameters.

    \sa encodingSettings()
*/

void QCameraImageCapture::setEncodingSettings(const QImageEncoderSettings &settings)
{
    Q_D(QCameraImageCapture);

    if (d->encoderControl) {
        QCamera *camera = qobject_cast<QCamera*>(d->mediaObject);
        if (camera && camera->captureMode() == QCamera::CaptureStillImage) {
            QMetaObject::invokeMethod(camera,
                                      "_q_preparePropertyChange",
                                      Qt::DirectConnection,
                                      Q_ARG(int, QCameraControl::ImageEncodingSettings));
        }

        d->encoderControl->setImageSettings(settings);
    }
}

/*!
    Returns the list of supported buffer image capture formats.

    \sa bufferFormat(), setBufferFormat()
*/
QList<QVideoFrame::PixelFormat> QCameraImageCapture::supportedBufferFormats() const
{
    if (d_func()->bufferFormatControl)
        return d_func()->bufferFormatControl->supportedBufferFormats();
    else
        return QList<QVideoFrame::PixelFormat>();
}

/*!
    Returns the buffer image capture format being used.

    \sa supportedBufferFormats(), setBufferFormat()
*/
QVideoFrame::PixelFormat QCameraImageCapture::bufferFormat() const
{
    if (d_func()->bufferFormatControl)
        return d_func()->bufferFormatControl->bufferFormat();
    else
        return QVideoFrame::Format_Invalid;
}

/*!
    Sets the buffer image capture \a format to be used.

    \sa bufferFormat(), supportedBufferFormats(), captureDestination()
*/
void QCameraImageCapture::setBufferFormat(const QVideoFrame::PixelFormat format)
{
    if (d_func()->bufferFormatControl)
        d_func()->bufferFormatControl->setBufferFormat(format);
}

/*!
    Returns true if the image capture \a destination is supported; otherwise returns false.

    \sa captureDestination(), setCaptureDestination()
*/
bool QCameraImageCapture::isCaptureDestinationSupported(QCameraImageCapture::CaptureDestinations destination) const
{
    if (d_func()->captureDestinationControl)
        return d_func()->captureDestinationControl->isCaptureDestinationSupported(destination);
    else
        return destination == CaptureToFile;
}

/*!
    Returns the image capture destination being used.

    \sa isCaptureDestinationSupported(), setCaptureDestination()
*/
QCameraImageCapture::CaptureDestinations QCameraImageCapture::captureDestination() const
{
    if (d_func()->captureDestinationControl)
        return d_func()->captureDestinationControl->captureDestination();
    else
        return CaptureToFile;
}

/*!
    Sets the capture \a destination to be used.

    \sa isCaptureDestinationSupported(), captureDestination()
*/
void QCameraImageCapture::setCaptureDestination(QCameraImageCapture::CaptureDestinations destination)
{
    Q_D(QCameraImageCapture);

    if (d->captureDestinationControl)
        d->captureDestinationControl->setCaptureDestination(destination);
}

/*!
  \property QCameraImageCapture::readyForCapture
  \brief whether the service is ready to capture a an image immediately.

  Calling capture() while \e readyForCapture is \c false is not permitted and
  results in an error.
*/

bool QCameraImageCapture::isReadyForCapture() const
{
    if (d_func()->control)
        return d_func()->control->isReadyForCapture();
    else
        return false;
}

/*!
    \fn QCameraImageCapture::readyForCaptureChanged(bool ready)

    Signals that a camera's \a ready for capture state has changed.
*/


/*!
    Capture the image and save it to \a file.
    This operation is asynchronous in majority of cases,
    followed by signals QCameraImageCapture::imageExposed(),
    QCameraImageCapture::imageCaptured(), QCameraImageCapture::imageSaved()
    or QCameraImageCapture::error().

    If an empty \a file is passed, the camera backend choses
    the default location and naming scheme for photos on the system,
    if only file name without full path is specified, the image will be saved to
    the default directory, with a full path reported with imageCaptured() and imageSaved() signals.

    QCamera saves all the capture parameters like exposure settings or
    image processing parameters, so changes to camera parameters after
    capture() is called do not affect previous capture requests.

    QCameraImageCapture::capture returns the capture Id parameter, used with
    imageExposed(), imageCaptured() and imageSaved() signals.

    \sa isReadyForCapture()
*/
int QCameraImageCapture::capture(const QString &file)
{
    Q_D(QCameraImageCapture);

    d->unsetError();

    if (d->control) {
        return d->control->capture(file);
    } else {
        d->error = NotSupportedFeatureError;
        d->errorString = tr("Device does not support images capture.");

        emit error(-1, d->error, d->errorString);
    }

    return -1;
}

/*!
    Cancel incomplete capture requests.
    Already captured and queused for proicessing images may be discarded.
*/
void QCameraImageCapture::cancelCapture()
{
    Q_D(QCameraImageCapture);

    d->unsetError();

    if (d->control) {
        d->control->cancelCapture();
    } else {
        d->error = NotSupportedFeatureError;
        d->errorString = tr("Device does not support images capture.");

        emit error(-1, d->error, d->errorString);
    }
}


/*!
    \enum QCameraImageCapture::Error

    \value NoError         No Errors.
    \value NotReadyError   The service is not ready for capture yet.
    \value ResourceError   Device is not ready or not available.
    \value OutOfSpaceError No space left on device.
    \value NotSupportedFeatureError Device does not support stillimages capture.
    \value FormatError     Current format is not supported.
*/

/*!
    \enum QCameraImageCapture::DriveMode

    \value SingleImageCapture Drive mode is capturing a single picture.
*/

/*!
    \fn QCameraImageCapture::error(int id, QCameraImageCapture::Error error, const QString &errorString)

    Signals that the capture request \a id has failed with an \a error
    and \a errorString description.
*/

/*!
    \fn QCameraImageCapture::bufferFormatChanged(QVideoFrame::PixelFormat format)

    Signal emitted when the buffer \a format for the buffer image capture has changed.
*/

/*!
    \fn QCameraImageCapture::captureDestinationChanged(CaptureDestinations destination)

    Signal emitted when the capture \a destination has changed.
*/

/*!
    \fn QCameraImageCapture::imageExposed(int id)

    Signal emitted when the frame with request \a id was exposed.
*/

/*!
    \fn QCameraImageCapture::imageCaptured(int id, const QImage &preview);

    Signal emitted when the frame with request \a id was captured, but not processed and saved yet.
    Frame \a preview can be displayed to user.
*/

/*!
    \fn QCameraImageCapture::imageMetadataAvailable(int id, const QString &key, const QVariant &value)

    Signals that a metadata for an image with request \a id is available. Also
    includes the \a key and \a value of the metadata.

    This signal is emitted between imageExposed and imageSaved signals.
*/

/*!
    \fn QCameraImageCapture::imageAvailable(int id, const QVideoFrame &buffer)

    Signal emitted when the frame with request \a id is available as \a buffer.
*/

/*!
    \fn QCameraImageCapture::imageSaved(int id, const QString &fileName)

    Signal emitted when the frame with request \a id was saved to \a fileName.
*/


#include "moc_qcameraimagecapture.cpp"
QT_END_NAMESPACE

