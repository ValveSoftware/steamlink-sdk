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
#include "qdeclarativecamerapreviewprovider_p.h"

#include "qdeclarativecameraexposure_p.h"
#include "qdeclarativecameraflash_p.h"
#include "qdeclarativecamerafocus_p.h"
#include "qdeclarativecameraimageprocessing_p.h"
#include "qdeclarativecameraviewfinder_p.h"

#include "qdeclarativemediametadata_p.h"

#include <qmediaplayercontrol.h>
#include <qmediaservice.h>
#include <qvideorenderercontrol.h>
#include <qvideodeviceselectorcontrol.h>
#include <QtQml/qqmlinfo.h>

#include <QtCore/QTimer>
#include <QtGui/qevent.h>

QT_BEGIN_NAMESPACE

void QDeclarativeCamera::_q_error(QCamera::Error errorCode)
{
    emit error(Error(errorCode), errorString());
    emit errorChanged();
}

void QDeclarativeCamera::_q_updateState(QCamera::State state)
{
    emit cameraStateChanged(QDeclarativeCamera::State(state));
}

void QDeclarativeCamera::_q_availabilityChanged(QMultimedia::AvailabilityStatus availability)
{
    emit availabilityChanged(Availability(availability));
}

/*!
    \qmltype Camera
    \instantiates QDeclarativeCamera
    \brief Access viewfinder frames, and take photos and movies.
    \ingroup multimedia_qml
    \ingroup camera_qml
    \inqmlmodule QtMultimedia

    \inherits QtObject

    You can use \c Camera to capture images and movies from a camera, and manipulate
    the capture and processing settings that get applied to the images.  To display the
    viewfinder you can use \l VideoOutput with the Camera set as the source.

    \qml
    Item {
        width: 640
        height: 360

        Camera {
            id: camera

            imageProcessing.whiteBalanceMode: CameraImageProcessing.WhiteBalanceFlash

            exposure {
                exposureCompensation: -1.0
                exposureMode: Camera.ExposurePortrait
            }

            flash.mode: Camera.FlashRedEyeReduction

            imageCapture {
                onImageCaptured: {
                    photoPreview.source = preview  // Show the preview in an Image
                }
            }
        }

        VideoOutput {
            source: camera
            anchors.fill: parent
            focus : visible // to receive focus and capture key events when visible
        }

        Image {
            id: photoPreview
        }
    }
    \endqml

    If multiple cameras are available, you can select which one to use by setting the \l deviceId
    property to a value from
    \l{QtMultimedia::QtMultimedia::availableCameras}{QtMultimedia.availableCameras}.
    On a mobile device, you can conveniently switch between front-facing and back-facing cameras
    by setting the \l position property.

    The various settings and functionality of the Camera stack is spread
    across a few different child properties of Camera.

    \table
    \header \li Property \li Description
    \row \li \l {CameraCapture} {imageCapture}
         \li Methods and properties for capturing still images.
    \row \li \l {CameraRecorder} {videoRecording}
         \li Methods and properties for capturing movies.
    \row \li \l {CameraExposure} {exposure}
         \li Methods and properties for adjusting exposure (aperture, shutter speed etc).
    \row \li \l {CameraFocus} {focus}
         \li Methods and properties for adjusting focus and providing feedback on autofocus progress.
    \row \li \l {CameraFlash} {flash}
         \li Methods and properties for controlling the camera flash.
    \row \li \l {CameraImageProcessing} {imageProcessing}
         \li Methods and properties for adjusting camera image processing parameters.
    \endtable

    Basic camera state management, error reporting, and simple zoom properties are
    available in the Camera itself.  For integration with C++ code, the
    \l mediaObject property allows you to
    access the standard Qt Multimedia camera controls.

    Many of the camera settings may take some time to apply, and might be limited
    to certain supported values depending on the hardware.  Some camera settings may be
    set manually or automatically. These settings properties contain the current set value.
    For example, when autofocus is enabled the focus zones are exposed in the
    \l {CameraFocus}{focus} property.

    For additional information, read also the \l{Camera Overview}{camera overview}.
*/

/*!
    \class QDeclarativeCamera
    \internal
    \brief The QDeclarativeCamera class provides a camera item that you can add to a QQuickView.
*/

/*!
    Construct a declarative camera object using \a parent object.
 */
QDeclarativeCamera::QDeclarativeCamera(QObject *parent) :
    QObject(parent),
    m_camera(0),
    m_metaData(0),
    m_viewfinder(0),
    m_pendingState(ActiveState),
    m_componentComplete(false)
{
    m_camera = new QCamera;
    m_currentCameraInfo = QCameraInfo(*m_camera);

    m_imageCapture = new QDeclarativeCameraCapture(m_camera);
    m_videoRecorder = new QDeclarativeCameraRecorder(m_camera);
    m_exposure = new QDeclarativeCameraExposure(m_camera);
    m_flash = new QDeclarativeCameraFlash(m_camera);
    m_focus = new QDeclarativeCameraFocus(m_camera);
    m_imageProcessing = new QDeclarativeCameraImageProcessing(m_camera);

    connect(m_camera, SIGNAL(captureModeChanged(QCamera::CaptureModes)),
            this, SIGNAL(captureModeChanged()));
    connect(m_camera, SIGNAL(lockStatusChanged(QCamera::LockStatus,QCamera::LockChangeReason)),
            this, SIGNAL(lockStatusChanged()));
    connect(m_camera, &QCamera::stateChanged, this, &QDeclarativeCamera::_q_updateState);
    connect(m_camera, SIGNAL(statusChanged(QCamera::Status)), this, SIGNAL(cameraStatusChanged()));
    connect(m_camera, SIGNAL(error(QCamera::Error)), this, SLOT(_q_error(QCamera::Error)));
    connect(m_camera, SIGNAL(availabilityChanged(QMultimedia::AvailabilityStatus)),
            this, SLOT(_q_availabilityChanged(QMultimedia::AvailabilityStatus)));

    connect(m_camera->focus(), &QCameraFocus::opticalZoomChanged,
            this, &QDeclarativeCamera::opticalZoomChanged);
    connect(m_camera->focus(), &QCameraFocus::digitalZoomChanged,
            this, &QDeclarativeCamera::digitalZoomChanged);
    connect(m_camera->focus(), &QCameraFocus::maximumOpticalZoomChanged,
            this, &QDeclarativeCamera::maximumOpticalZoomChanged);
    connect(m_camera->focus(), &QCameraFocus::maximumDigitalZoomChanged,
            this, &QDeclarativeCamera::maximumDigitalZoomChanged);
}

/*! Destructor, clean up memory */
QDeclarativeCamera::~QDeclarativeCamera()
{
    m_camera->unload();

    // These must be deleted before QCamera
    delete m_imageCapture;
    delete m_videoRecorder;
    delete m_exposure;
    delete m_flash;
    delete m_focus;
    delete m_imageProcessing;
    delete m_metaData;
    delete m_viewfinder;

    delete m_camera;
}

void QDeclarativeCamera::classBegin()
{
}

void QDeclarativeCamera::componentComplete()
{
    m_componentComplete = true;
    setCameraState(m_pendingState);
}

/*!
    \qmlproperty string QtMultimedia::Camera::deviceId

    This property holds the unique identifier for the camera device being used. It may not be human-readable.

    You can get all available device IDs from \l{QtMultimedia::QtMultimedia::availableCameras}{QtMultimedia.availableCameras}.
    If no value is provided or if set to an empty string, the system's default camera will be used.

    If possible, \l cameraState, \l captureMode, \l digitalZoom and other camera parameters are
    preserved when changing the camera device.

    \sa displayName, position
    \since QtMultimedia 5.4
*/

QString QDeclarativeCamera::deviceId() const
{
    return m_currentCameraInfo.deviceName();
}

void QDeclarativeCamera::setDeviceId(const QString &name)
{
    if (name == m_currentCameraInfo.deviceName())
        return;

    setupDevice(name);
}

/*!
    \qmlproperty enumeration QtMultimedia::Camera::position

    This property holds the physical position of the camera on the hardware system.

    The position can be one of the following:

    \list
    \li \c Camera.UnspecifiedPosition - the camera position is unspecified or unknown.
    \li \c Camera.BackFace - the camera is on the back face of the system hardware. For example on a
        mobile device, it means it is on the opposite side to that of the screem.
    \li \c Camera.FrontFace - the camera is on the front face of the system hardware. For example on
        a mobile device, it means it is on the same side as that of the screen. Viewfinder frames of
        front-facing cameras are mirrored horizontally, so the users can see themselves as looking
        into a mirror. Captured images or videos are not mirrored.
    \endlist

    On a mobile device it can be used to easily choose between front-facing and back-facing cameras.
    If this property is set to \c Camera.UnspecifiedPosition, the system's default camera will be
    used.

    If possible, \l cameraState, \l captureMode, \l digitalZoom and other camera parameters are
    preserved when changing the camera device.

    \sa deviceId
    \since QtMultimedia 5.4
*/

QDeclarativeCamera::Position QDeclarativeCamera::position() const
{
    return QDeclarativeCamera::Position(m_currentCameraInfo.position());
}

void QDeclarativeCamera::setPosition(Position position)
{
    QCamera::Position pos = QCamera::Position(position);
    if (pos == m_currentCameraInfo.position())
        return;

    QString id;

    if (pos == QCamera::UnspecifiedPosition) {
        id = QCameraInfo::defaultCamera().deviceName();
    } else {
        QList<QCameraInfo> cameras = QCameraInfo::availableCameras(pos);
        if (!cameras.isEmpty())
            id = cameras.first().deviceName();
    }

    if (!id.isEmpty())
        setupDevice(id);
}

/*!
    \qmlproperty string QtMultimedia::Camera::displayName

    This property holds the human-readable name of the camera.

    You can use this property to display the name of the camera in a user interface.

    \readonly
    \sa deviceId
    \since QtMultimedia 5.4
*/

QString QDeclarativeCamera::displayName() const
{
    return m_currentCameraInfo.description();
}

/*!
    \qmlproperty int QtMultimedia::Camera::orientation

    This property holds the physical orientation of the camera sensor.

    The value is the orientation angle (clockwise, in steps of 90 degrees) of the camera sensor in
    relation to the display in its natural orientation.

    For example, suppose a mobile device which is naturally in portrait orientation. The back-facing
    camera is mounted in landscape. If the top side of the camera sensor is aligned with the right
    edge of the screen in natural orientation, \c orientation returns \c 270. If the top side of a
    front-facing camera sensor is aligned with the right edge of the screen, \c orientation
    returns \c 90.

    \readonly
    \sa VideoOutput::orientation
    \since QtMultimedia 5.4
*/

int QDeclarativeCamera::orientation() const
{
    return m_currentCameraInfo.orientation();
}

void QDeclarativeCamera::setupDevice(const QString &deviceName)
{
    QMediaService *service = m_camera->service();
    if (!service)
        return;

    QVideoDeviceSelectorControl *deviceControl = qobject_cast<QVideoDeviceSelectorControl*>(service->requestControl(QVideoDeviceSelectorControl_iid));
    if (!deviceControl)
        return;

    int deviceIndex = -1;

    if (deviceName.isEmpty()) {
        deviceIndex = deviceControl->defaultDevice();
    } else {
        for (int i = 0; i < deviceControl->deviceCount(); ++i) {
            if (deviceControl->deviceName(i) == deviceName) {
                deviceIndex = i;
                break;
            }
        }
    }

    if (deviceIndex == -1)
        return;

    State previousState = cameraState();
    setCameraState(UnloadedState);

    deviceControl->setSelectedDevice(deviceIndex);

    QCameraInfo oldCameraInfo = m_currentCameraInfo;
    m_currentCameraInfo = QCameraInfo(*m_camera);

    emit deviceIdChanged();
    if (oldCameraInfo.description() != m_currentCameraInfo.description())
        emit displayNameChanged();
    if (oldCameraInfo.position() != m_currentCameraInfo.position())
        emit positionChanged();
    if (oldCameraInfo.orientation() != m_currentCameraInfo.orientation())
        emit orientationChanged();

    setCameraState(previousState);
}

/*!
    Returns any camera error.
    \sa QDeclarativeCameraError::Error
*/
QDeclarativeCamera::Error QDeclarativeCamera::errorCode() const
{
    return QDeclarativeCamera::Error(m_camera->error());
}

/*!
    \qmlproperty string QtMultimedia::Camera::errorString

    This property holds the last error string, if any.

    \sa error, errorCode
*/
QString QDeclarativeCamera::errorString() const
{
    return m_camera->errorString();
}

/*!
    \qmlproperty enumeration QtMultimedia::Camera::availability

    This property holds the availability state of the camera.

    The availability states can be one of the following:

    \table
    \header \li Value \li Description
    \row \li Available
        \li The camera is available to use
    \row \li Busy
        \li The camera is busy at the moment as it is being used by another process.
    \row \li Unavailable
        \li The camera is not available to use (there may be no camera
           hardware)
    \row \li ResourceMissing
        \li The camera cannot be used because of missing resources.
         It may be possible to try again at a later time.
    \endtable
 */
QDeclarativeCamera::Availability QDeclarativeCamera::availability() const
{
    return Availability(m_camera->availability());
}


/*!
    \qmlproperty enumeration QtMultimedia::Camera::captureMode

    This property holds the camera capture mode, which can be one of the
    following:

    \table
    \header \li Value \li Description
    \row \li CaptureViewfinder
         \li Camera is only configured to display viewfinder.

    \row \li CaptureStillImage
         \li Prepares the Camera for capturing still images.

    \row \li CaptureVideo
         \li Prepares the Camera for capturing video.

    \endtable

    The default capture mode is \c CaptureStillImage.
*/
QDeclarativeCamera::CaptureMode QDeclarativeCamera::captureMode() const
{
    return QDeclarativeCamera::CaptureMode(int(m_camera->captureMode()));
}

void QDeclarativeCamera::setCaptureMode(QDeclarativeCamera::CaptureMode mode)
{
    m_camera->setCaptureMode(QCamera::CaptureModes(int(mode)));
}


/*!
    \qmlproperty enumeration QtMultimedia::Camera::cameraState

    This property holds the camera object's current state, which can be one of the following:

    \table
    \header \li Value \li Description
    \row \li UnloadedState
         \li The initial camera state, with the camera not loaded.
           The camera capabilities (with the exception of supported capture modes)
           are unknown. This state saves the most power, but takes the longest
           time to be ready for capture.

           While the supported settings are unknown in this state,
           you can still set the camera capture settings like codec,
           resolution, or frame rate.

    \row \li LoadedState
         \li The camera is loaded and ready to be configured.

           In the Idle state you can query camera capabilities,
           set capture resolution, codecs, and so on.

           The viewfinder is not active in the loaded state.

    \row \li ActiveState
          \li In the active state the viewfinder frames are available
             and the camera is ready for capture.
    \endtable

    The default camera state is ActiveState.
*/
QDeclarativeCamera::State QDeclarativeCamera::cameraState() const
{
    return m_componentComplete ? QDeclarativeCamera::State(m_camera->state()) : m_pendingState;
}

/*!
    \qmlproperty enumeration QtMultimedia::Camera::cameraStatus

    This property holds the camera object's current status, which can be one of the following:

    \table
    \header \li Value \li Description
    \row \li ActiveStatus
         \li The camera has been started and can produce data,
             viewfinder displays video frames.

             Depending on backend, changing camera settings such as
             capture mode, codecs, or resolution in ActiveState may lead
             to changing the status to LoadedStatus and StartingStatus while
             the settings are applied, and back to ActiveStatus when the camera is ready.

    \row \li StartingStatus
         \li The camera is starting as a result of state transition to Camera.ActiveState.
             The camera service is not ready to capture yet.

    \row \li StoppingStatus
         \li The camera is stopping as a result of state transition from Camera.ActiveState
             to Camera.LoadedState or Camera.UnloadedState.

    \row \li StandbyStatus
         \li The camera is in the power saving standby mode.
             The camera may enter standby mode after some time of inactivity
             in the Camera.LoadedState state.

    \row \li LoadedStatus
         \li The camera is loaded and ready to be configured.
             This status indicates the camera device is opened and
             it's possible to query for supported image and video capture settings
             such as resolution, frame rate, and codecs.

    \row \li LoadingStatus
         \li The camera device loading as a result of state transition from
             Camera.UnloadedState to Camera.LoadedState or Camera.ActiveState.

    \row \li UnloadingStatus
         \li The camera device is unloading as a result of state transition from
             Camera.LoadedState or Camera.ActiveState to Camera.UnloadedState.

    \row \li UnloadedStatus
         \li The initial camera status, with camera not loaded.
             The camera capabilities including supported capture settings may be unknown.

    \row \li UnavailableStatus
         \li The camera or camera backend is not available.

    \endtable
*/
QDeclarativeCamera::Status QDeclarativeCamera::cameraStatus() const
{
    return QDeclarativeCamera::Status(m_camera->status());
}

void QDeclarativeCamera::setCameraState(QDeclarativeCamera::State state)
{
    if (!m_componentComplete) {
        m_pendingState = state;
        return;
    }

    switch (state) {
    case QDeclarativeCamera::ActiveState:
        m_camera->start();
        break;
    case QDeclarativeCamera::UnloadedState:
        m_camera->unload();
        break;
    case QDeclarativeCamera::LoadedState:
        m_camera->load();
        break;
    }
}

/*!
    \qmlmethod QtMultimedia::Camera::start()

    Starts the camera.  Viewfinder frames will
    be available and image or movie capture will
    be possible.
*/
void QDeclarativeCamera::start()
{
    setCameraState(QDeclarativeCamera::ActiveState);
}

/*!
    \qmlmethod QtMultimedia::Camera::stop()

    Stops the camera, but leaves the camera
    stack loaded.
*/
void QDeclarativeCamera::stop()
{
    setCameraState(QDeclarativeCamera::LoadedState);
}


/*!
    \qmlproperty enumeration QtMultimedia::Camera::lockStatus

    This property holds the status of all the requested camera locks.

    The status can be one of the following values:

    \table
    \header \li Value \li Description
    \row \li Unlocked
        \li The application is not interested in camera settings value.
        The camera may keep this parameter without changes, which is common with camera focus,
        or adjust exposure and white balance constantly to keep the viewfinder image nice.

    \row \li Searching
        \li The application has requested the camera focus, exposure, or white balance lock with
        searchAndLock(). This state indicates the camera is focusing or calculating exposure and white balance.

    \row \li Locked
        \li The camera focus, exposure, or white balance is locked.
        The camera is ready to capture, and the application may check the exposure parameters.

        The locked state usually means the requested parameter stays the same,
        except in cases where the parameter is requested to be updated constantly.
        For example in continuous focusing mode, the focus is considered locked as long
        as the object is in focus, even while the actual focusing distance may be constantly changing.
    \endtable
*/
/*!
    \property QDeclarativeCamera::lockStatus

    This property holds the status of all the requested camera locks.

    The status can be one of the following:

    \table
    \header \li Value \li Description
    \row \li Unlocked
        \li The application is not interested in camera settings value.
        The camera may keep this parameter without changes, this is common with camera focus,
        or adjust exposure and white balance constantly to keep the viewfinder image nice.

    \row \li Searching
        \li The application has requested the camera focus, exposure or white balance lock with
        searchAndLock(). This state indicates the camera is focusing or calculating exposure and white balance.

    \row \li Locked
        \li The camera focus, exposure or white balance is locked.
        The camera is ready to capture, and the application may check the exposure parameters.

        The locked state usually means the requested parameter stays the same,
        except in the cases when the parameter is requested to be updated constantly.
        For example in continuous focusing mode, the focus is considered locked as long
        and the object is in focus, even while the actual focusing distance may be constantly changing.
    \endtable
*/
QDeclarativeCamera::LockStatus QDeclarativeCamera::lockStatus() const
{
    return QDeclarativeCamera::LockStatus(m_camera->lockStatus());
}

/*!
    \qmlmethod QtMultimedia::Camera::searchAndLock()

    Start focusing, exposure and white balance calculation.

    This is appropriate to call when the camera focus button is pressed
    (or on a camera capture button half-press).  If the camera supports
    autofocusing, information on the focus zones is available through
    the \l {CameraFocus}{focus} property.
*/
void QDeclarativeCamera::searchAndLock()
{
    m_camera->searchAndLock();
}

/*!
    \qmlmethod QtMultimedia::Camera::unlock()

    Unlock focus, exposure and white balance locks.
 */
void QDeclarativeCamera::unlock()
{
    m_camera->unlock();
}
/*!
    \property QDeclarativeCamera::maximumOpticalZoom

    This property holds the maximum optical zoom factor supported, or 1.0 if optical zoom is not supported.
*/
/*!
    \qmlproperty real QtMultimedia::Camera::maximumOpticalZoom

    This property holds the maximum optical zoom factor supported, or 1.0 if optical zoom is not supported.
*/
qreal QDeclarativeCamera::maximumOpticalZoom() const
{
    return m_camera->focus()->maximumOpticalZoom();
}
/*!
    \property  QDeclarativeCamera::maximumDigitalZoom

    This property holds the maximum digital zoom factor supported, or 1.0 if digital zoom is not supported.
*/
/*!
    \qmlproperty real QtMultimedia::Camera::maximumDigitalZoom

    This property holds the maximum digital zoom factor supported, or 1.0 if digital zoom is not supported.
*/
qreal QDeclarativeCamera::maximumDigitalZoom() const
{
    return m_camera->focus()->maximumDigitalZoom();
}
/*!
    \property QDeclarativeCamera::opticalZoom

    This property holds the current optical zoom factor.
*/

/*!
    \qmlproperty real QtMultimedia::Camera::opticalZoom

    This property holds the current optical zoom factor.
*/
qreal QDeclarativeCamera::opticalZoom() const
{
    return m_camera->focus()->opticalZoom();
}

void QDeclarativeCamera::setOpticalZoom(qreal value)
{
    m_camera->focus()->zoomTo(value, digitalZoom());
}
/*!
    \property   QDeclarativeCamera::digitalZoom

    This property holds the current digital zoom factor.
*/
/*!
    \qmlproperty real QtMultimedia::Camera::digitalZoom

    This property holds the current digital zoom factor.
*/
qreal QDeclarativeCamera::digitalZoom() const
{
    return m_camera->focus()->digitalZoom();
}

void QDeclarativeCamera::setDigitalZoom(qreal value)
{
    m_camera->focus()->zoomTo(opticalZoom(), value);
}

/*!
    \qmlproperty variant QtMultimedia::Camera::mediaObject

    This property holds the native media object for the camera.

    It can be used to get a pointer to a QCamera object in order to integrate with C++ code.

    \code
        QObject *qmlCamera; // The QML Camera object
        QCamera *camera = qvariant_cast<QCamera *>(qmlCamera->property("mediaObject"));
    \endcode

    \note This property is not accessible from QML.
*/

/*!
    \qmlproperty enumeration QtMultimedia::Camera::errorCode

    This property holds the last error code.

    \sa error, errorString
*/

/*!
    \qmlsignal QtMultimedia::Camera::error(errorCode, errorString)

    This signal is emitted when an error occurs. The enumeration value
    \a errorCode is one of the values defined below, and a descriptive string
    value is available in \a errorString.

    \table
    \header \li Value \li Description
    \row \li NoError \li No errors have occurred.
    \row \li CameraError \li An error has occurred.
    \row \li InvalidRequestError \li System resource doesn't support requested functionality.
    \row \li ServiceMissingError \li No camera service available.
    \row \li NotSupportedFeatureError \li The feature is not supported.
    \endtable

    The corresponding handler is \c onError.

    \sa errorCode, errorString
*/

/*!
    \qmlsignal Camera::lockStatusChanged()

    This signal is emitted when the lock status (focus, exposure etc) changes.
    This can happen when locking (e.g. autofocusing) is complete or has failed.

    The corresponding handler is \c onLockStatusChanged.
*/

/*!
    \qmlsignal Camera::cameraStateChanged(state)

    This signal is emitted when the camera state has changed to \a state.  Since the
    state changes may take some time to occur this signal may arrive sometime
    after the state change has been requested.

    The corresponding handler is \c onCameraStateChanged.
*/

/*!
    \qmlsignal Camera::opticalZoomChanged(zoom)

    This signal is emitted when the optical zoom setting has changed to \a zoom.

    The corresponding handler is \c onOpticalZoomChanged.
*/

/*!
    \qmlsignal Camera::digitalZoomChanged(zoom)

    This signal is emitted when the digital zoom setting has changed to \a zoom.

    The corresponding handler is \c onDigitalZoomChanged.
*/

/*!
    \qmlsignal Camera::maximumOpticalZoomChanged(zoom)

    This signal is emitted when the maximum optical zoom setting has
    changed to \a zoom.  This can occur when you change between video
    and still image capture modes, or the capture settings are changed.

    The corresponding handler is \c onMaximumOpticalZoomChanged.
*/

/*!
    \qmlsignal Camera::maximumDigitalZoomChanged(zoom)

    This signal is emitted when the maximum digital zoom setting has
    changed to \a zoom.  This can occur when you change between video
    and still image capture modes, or the capture settings are changed.

    The corresponding handler is \c onMaximumDigitalZoomChanged.
*/

/*!
    \qmlpropertygroup QtMultimedia::Camera::metaData
    \qmlproperty variant QtMultimedia::Camera::metaData.cameraManufacturer
    \qmlproperty variant QtMultimedia::Camera::metaData.cameraModel
    \qmlproperty variant QtMultimedia::Camera::metaData.event
    \qmlproperty variant QtMultimedia::Camera::metaData.subject
    \qmlproperty variant QtMultimedia::Camera::metaData.orientation
    \qmlproperty variant QtMultimedia::Camera::metaData.dateTimeOriginal
    \qmlproperty variant QtMultimedia::Camera::metaData.gpsLatitude
    \qmlproperty variant QtMultimedia::Camera::metaData.gpsLongitude
    \qmlproperty variant QtMultimedia::Camera::metaData.gpsAltitude
    \qmlproperty variant QtMultimedia::Camera::metaData.gpsTimestamp
    \qmlproperty variant QtMultimedia::Camera::metaData.gpsTrack
    \qmlproperty variant QtMultimedia::Camera::metaData.gpsSpeed
    \qmlproperty variant QtMultimedia::Camera::metaData.gpsImgDirection
    \qmlproperty variant QtMultimedia::Camera::metaData.gpsProcessingMethod

    These properties hold the meta data for the camera captures.

    \list
    \li \c metaData.cameraManufacturer holds the name of the manufacturer of the camera.
    \li \c metaData.cameraModel holds the name of the model of the camera.
    \li \c metaData.event holds the event during which the photo or video is to be captured.
    \li \c metaData.subject holds the name of the subject of the capture or recording.
    \li \c metaData.orientation holds the clockwise rotation of the camera at time of capture.
    \li \c metaData.dateTimeOriginal holds the initial time at which the photo or video is captured.
    \li \c metaData.gpsLatitude holds the latitude of the camera in decimal degrees at time of capture.
    \li \c metaData.gpsLongitude holds the longitude of the camera in decimal degrees at time of capture.
    \li \c metaData.gpsAltitude holds the altitude of the camera in meters at time of capture.
    \li \c metaData.gpsTimestamp holds the timestamp of the GPS position data.
    \li \c metaData.gpsTrack holds direction of movement of the camera at the time of
           capture. It is measured in degrees clockwise from north.
    \li \c metaData.gpsSpeed holds the velocity in kilometers per hour of the camera at time of capture.
    \li \c metaData.gpsImgDirection holds direction the camera is facing at the time of capture.
           It is measured in degrees clockwise from north.
    \li \c metaData.gpsProcessingMethod holds the name of the method for determining the GPS position.
    \endlist

    \sa {QMediaMetaData}
    \since 5.4
*/

QDeclarativeMediaMetaData *QDeclarativeCamera::metaData()
{
    if (!m_metaData)
        m_metaData = new QDeclarativeMediaMetaData(m_camera);
    return m_metaData;
}

/*!
    \qmlpropertygroup QtMultimedia::Camera::viewfinder
    \qmlproperty size QtMultimedia::Camera::viewfinder.resolution
    \qmlproperty real QtMultimedia::Camera::viewfinder.minimumFrameRate
    \qmlproperty real QtMultimedia::Camera::viewfinder.maximumFrameRate

    These properties hold the viewfinder settings.

    \c viewfinder.resolution holds the resolution of the camera viewfinder. If no
    resolution is given or if it is empty, the backend uses a default value.

    \c viewfinder.minimumFrameRate holds the minimum frame rate for the viewfinder in
    frames per second. If no value is given or if set to \c 0, the backend uses a default value.

    \c viewfinder.maximumFrameRate holds the maximum frame rate for the viewfinder in
    frames per second. If no value is given or if set to \c 0, the backend uses a default value.

    If \c viewfinder.minimumFrameRate is equal to \c viewfinder.maximumFrameRate, the frame rate is
    fixed. If not, the actual frame rate fluctuates between the two values.

    Changing the viewfinder settings while the camera is in the \c Camera.ActiveState state may
    cause the camera to be restarted.

    If the camera is used to capture videos or images, the viewfinder settings might be
    ignored if they conflict with the capture settings. You can check the actual viewfinder settings
    once the camera is in the \c Camera.ActiveStatus status.

    Supported values can be retrieved with supportedViewfinderResolutions() and
    supportedViewfinderFrameRateRanges().

    \since 5.4
 */

QDeclarativeCameraViewfinder *QDeclarativeCamera::viewfinder()
{
    if (!m_viewfinder)
        m_viewfinder = new QDeclarativeCameraViewfinder(m_camera);

    return m_viewfinder;
}

/*!
    \qmlmethod list<size> QtMultimedia::Camera::supportedViewfinderResolutions(real minimumFrameRate = undefined, real maximumFrameRate = undefined)

    Returns a list of supported viewfinder resolutions.

    If both optional parameters \a minimumFrameRate and \a maximumFrameRate are specified, the
    returned list is reduced to resolutions supported for the given frame rate range.

    The camera must be loaded before calling this function, otherwise the returned list
    is empty.

    \sa {QtMultimedia::Camera::viewfinder}{viewfinder}

    \since 5.5
*/
QJSValue QDeclarativeCamera::supportedViewfinderResolutions(qreal minimumFrameRate, qreal maximumFrameRate)
{
    QQmlEngine *engine = qmlEngine(this);

    QCameraViewfinderSettings settings;
    settings.setMinimumFrameRate(minimumFrameRate);
    settings.setMaximumFrameRate(maximumFrameRate);
    const QList<QSize> resolutions = m_camera->supportedViewfinderResolutions(settings);

    QJSValue supportedResolutions = engine->newArray(resolutions.count());
    int i = 0;
    for (const QSize &resolution : resolutions) {
        QJSValue size = engine->newObject();
        size.setProperty(QStringLiteral("width"), resolution.width());
        size.setProperty(QStringLiteral("height"), resolution.height());
        supportedResolutions.setProperty(i++, size);
    }

    return supportedResolutions;
}

/*!
    \qmlmethod list<object> QtMultimedia::Camera::supportedViewfinderFrameRateRanges(size resolution = undefined)

    Returns a list of supported viewfinder frame rate ranges.

    Each range object in the list has the \c minimumFrameRate and \c maximumFrameRate properties.

    If the optional parameter \a resolution is specified, the returned list is reduced to frame rate
    ranges supported for the given \a resolution.

    The camera must be loaded before calling this function, otherwise the returned list
    is empty.

    \sa {QtMultimedia::Camera::viewfinder}{viewfinder}

    \since 5.5
*/
QJSValue QDeclarativeCamera::supportedViewfinderFrameRateRanges(const QJSValue &resolution)
{
    QQmlEngine *engine = qmlEngine(this);

    QCameraViewfinderSettings settings;
    if (!resolution.isUndefined()) {
        QJSValue width = resolution.property(QStringLiteral("width"));
        QJSValue height = resolution.property(QStringLiteral("height"));
        if (width.isNumber() && height.isNumber())
            settings.setResolution(width.toInt(), height.toInt());
    }
    const QList<QCamera::FrameRateRange> frameRateRanges = m_camera->supportedViewfinderFrameRateRanges(settings);

    QJSValue supportedFrameRateRanges = engine->newArray(frameRateRanges.count());
    int i = 0;
    for (const QCamera::FrameRateRange &frameRateRange : frameRateRanges) {
        QJSValue range = engine->newObject();
        range.setProperty(QStringLiteral("minimumFrameRate"), frameRateRange.minimumFrameRate);
        range.setProperty(QStringLiteral("maximumFrameRate"), frameRateRange.maximumFrameRate);
        supportedFrameRateRanges.setProperty(i++, range);
    }

    return supportedFrameRateRanges;
}

QT_END_NAMESPACE

#include "moc_qdeclarativecamera_p.cpp"
