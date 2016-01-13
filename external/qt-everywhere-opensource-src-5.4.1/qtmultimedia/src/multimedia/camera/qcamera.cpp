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


#include "qcamera_p.h"
#include "qmediaserviceprovider_p.h"

#include <qcamerainfo.h>
#include <qcameracontrol.h>
#include <qcameralockscontrol.h>
#include <qcameraexposurecontrol.h>
#include <qcamerafocuscontrol.h>
#include <qmediarecordercontrol.h>
#include <qcameraimageprocessingcontrol.h>
#include <qcameraimagecapturecontrol.h>
#include <qvideodeviceselectorcontrol.h>
#include <qcamerainfocontrol.h>

#include <QDebug>

QT_BEGIN_NAMESPACE

static void qRegisterCameraMetaTypes()
{
    qRegisterMetaType<QCamera::Error>("QCamera::Error");
    qRegisterMetaType<QCamera::State>("QCamera::State");
    qRegisterMetaType<QCamera::Status>("QCamera::Status");
    qRegisterMetaType<QCamera::CaptureModes>("QCamera::CaptureModes");
    qRegisterMetaType<QCamera::LockType>("QCamera::LockType");
    qRegisterMetaType<QCamera::LockStatus>("QCamera::LockStatus");
    qRegisterMetaType<QCamera::LockChangeReason>("QCamera::LockChangeReason");
    qRegisterMetaType<QCamera::Position>("QCamera::Position");
}

Q_CONSTRUCTOR_FUNCTION(qRegisterCameraMetaTypes)

/*!
    \class QCamera


    \brief The QCamera class provides interface for system camera devices.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_camera

    QCamera can be used with QCameraViewfinder for viewfinder display,
    QMediaRecorder for video recording and QCameraImageCapture for image taking.

    You can use QCameraInfo to list available cameras and choose which one to use.

    \snippet multimedia-snippets/camera.cpp Camera selection

    See the \l{Camera Overview}{camera overview} for more information.
*/

void QCameraPrivate::_q_error(int error, const QString &errorString)
{
    Q_Q(QCamera);

    this->error = QCamera::Error(error);
    this->errorString = errorString;

    emit q->error(this->error);
}

void QCameraPrivate::setState(QCamera::State newState)
{
    unsetError();

    if (!control) {
        _q_error(QCamera::ServiceMissingError, QCamera::tr("The camera service is missing"));
        return;
    }

    restartPending = false;
    control->setState(newState);
}

void QCameraPrivate::_q_updateState(QCamera::State newState)
{
    Q_Q(QCamera);

    //omit changins state to Loaded when the camera is temporarily
    //stopped to apply shanges
    if (restartPending)
        return;

    if (newState != state) {
        state = newState;
        emit q->stateChanged(state);
    }
}

void QCameraPrivate::_q_preparePropertyChange(int changeType)
{
    if (!control)
        return;

    QCamera::Status status = control->status();

    //all the changes are allowed until the camera is starting
    if (control->state() != QCamera::ActiveState)
        return;

    if (control->canChangeProperty(QCameraControl::PropertyChangeType(changeType), status))
        return;

    restartPending = true;
    control->setState(QCamera::LoadedState);
    QMetaObject::invokeMethod(q_ptr, "_q_restartCamera", Qt::QueuedConnection);
}

void QCameraPrivate::_q_restartCamera()
{
    if (restartPending) {
        restartPending = false;
        control->setState(QCamera::ActiveState);
    }
}

void QCameraPrivate::init()
{
    Q_Q(QCamera);
    provider = QMediaServiceProvider::defaultServiceProvider();
    initControls();
    cameraExposure = new QCameraExposure(q);
    cameraFocus = new QCameraFocus(q);
    imageProcessing = new QCameraImageProcessing(q);
}

void QCameraPrivate::initControls()
{
    Q_Q(QCamera);

    supportedLocks = 0;

    if (service) {
        control = qobject_cast<QCameraControl *>(service->requestControl(QCameraControl_iid));
        locksControl = qobject_cast<QCameraLocksControl *>(service->requestControl(QCameraLocksControl_iid));
        deviceControl = qobject_cast<QVideoDeviceSelectorControl*>(service->requestControl(QVideoDeviceSelectorControl_iid));
        infoControl = qobject_cast<QCameraInfoControl*>(service->requestControl(QCameraInfoControl_iid));

        if (control) {
            q->connect(control, SIGNAL(stateChanged(QCamera::State)), q, SLOT(_q_updateState(QCamera::State)));
            q->connect(control, SIGNAL(statusChanged(QCamera::Status)), q, SIGNAL(statusChanged(QCamera::Status)));
            q->connect(control, SIGNAL(captureModeChanged(QCamera::CaptureModes)),
                       q, SIGNAL(captureModeChanged(QCamera::CaptureModes)));
            q->connect(control, SIGNAL(error(int,QString)), q, SLOT(_q_error(int,QString)));

        }

        if (locksControl) {
            q->connect(locksControl, SIGNAL(lockStatusChanged(QCamera::LockType,QCamera::LockStatus,QCamera::LockChangeReason)),
                       q, SLOT(_q_updateLockStatus(QCamera::LockType,QCamera::LockStatus,QCamera::LockChangeReason)));
            supportedLocks = locksControl->supportedLocks();
        }

        error = QCamera::NoError;
    } else {
        control = 0;
        locksControl = 0;
        deviceControl = 0;
        infoControl = 0;

        error = QCamera::ServiceMissingError;
        errorString = QCamera::tr("The camera service is missing");
    }
}

void QCameraPrivate::clear()
{
    delete cameraExposure;
    delete cameraFocus;
    delete imageProcessing;

    if (service) {
        if (control)
            service->releaseControl(control);
        if (locksControl)
            service->releaseControl(locksControl);
        if (deviceControl)
            service->releaseControl(deviceControl);
        if (infoControl)
            service->releaseControl(infoControl);

        provider->releaseService(service);
    }

    cameraExposure = 0;
    cameraFocus = 0;
    imageProcessing = 0;
    control = 0;
    locksControl = 0;
    deviceControl = 0;
    infoControl = 0;
    service = 0;
    supportedLocks = 0;
}

void QCameraPrivate::updateLockStatus()
{
    Q_Q(QCamera);

    QCamera::LockStatus oldStatus = lockStatus;

    QMap<QCamera::LockStatus, int> lockStatusPriority;
    lockStatusPriority.insert(QCamera::Locked, 1);
    lockStatusPriority.insert(QCamera::Searching, 2);
    lockStatusPriority.insert(QCamera::Unlocked, 3);

    lockStatus = requestedLocks ? QCamera::Locked : QCamera::Unlocked;
    int priority = 0;

    QList<QCamera::LockStatus> lockStatuses;

    if (requestedLocks & QCamera::LockFocus)
        lockStatuses << q->lockStatus(QCamera::LockFocus);

    if (requestedLocks & QCamera::LockExposure)
        lockStatuses << q->lockStatus(QCamera::LockExposure);

    if (requestedLocks & QCamera::LockWhiteBalance)
        lockStatuses << q->lockStatus(QCamera::LockWhiteBalance);


    foreach (QCamera::LockStatus currentStatus, lockStatuses) {
        int currentPriority = lockStatusPriority.value(currentStatus, -1);
        if (currentPriority > priority) {
            priority = currentPriority;
            lockStatus = currentStatus;
        }
    }

    if (!supressLockChangedSignal && oldStatus != lockStatus) {
        emit q->lockStatusChanged(lockStatus, lockChangeReason);

        if (lockStatus == QCamera::Locked)
            emit q->locked();
        else if (lockStatus == QCamera::Unlocked && lockChangeReason == QCamera::LockFailed)
            emit q->lockFailed();
    }
/*
    qDebug() << "Requested locks:" << (requestedLocks & QCamera::LockExposure ? 'e' : ' ')
            << (requestedLocks & QCamera::LockFocus ? 'f' : ' ')
            << (requestedLocks & QCamera::LockWhiteBalance ? 'w' : ' ');
    qDebug() << "Lock status: f:" << q->lockStatus(QCamera::LockFocus)
             << " e:" << q->lockStatus(QCamera::LockExposure)
             << " w:" << q->lockStatus(QCamera::LockWhiteBalance)
             << " composite:" << lockStatus;
*/
}

void QCameraPrivate::_q_updateLockStatus(QCamera::LockType type, QCamera::LockStatus status, QCamera::LockChangeReason reason)
{
    Q_Q(QCamera);
    lockChangeReason = reason;
    updateLockStatus();
    emit q->lockStatusChanged(type, status, reason);
}


/*!
    Construct a QCamera with a \a parent.
*/

QCamera::QCamera(QObject *parent):
    QMediaObject(*new QCameraPrivate,
                 parent,
                 QMediaServiceProvider::defaultServiceProvider()->requestService(Q_MEDIASERVICE_CAMERA))
{
    Q_D(QCamera);
    d->init();

    // Select the default camera
    if (d->service != 0 && d->deviceControl)
        d->deviceControl->setSelectedDevice(d->deviceControl->defaultDevice());
}

/*!
    Construct a QCamera from \a deviceName and \a parent.

    If no camera with that \a deviceName exists, the camera object will
    be invalid.
*/

QCamera::QCamera(const QByteArray& deviceName, QObject *parent):
    QMediaObject(*new QCameraPrivate, parent,
                  QMediaServiceProvider::defaultServiceProvider()->requestService(Q_MEDIASERVICE_CAMERA,
                                                                                  QMediaServiceProviderHint(deviceName)))
{
    Q_D(QCamera);
    d->init();

    if (d->service != 0) {
        //pass device name to service
        if (d->deviceControl) {
            const QString name = QString::fromLatin1(deviceName);
            for (int i = 0; i < d->deviceControl->deviceCount(); i++) {
                if (d->deviceControl->deviceName(i) == name) {
                    d->deviceControl->setSelectedDevice(i);
                    break;
                }
            }
        }
    }
}

/*!
    \since 5.3

    Construct a QCamera from a camera description \a cameraInfo and \a parent.
*/

QCamera::QCamera(const QCameraInfo &cameraInfo, QObject *parent)
    : QMediaObject(*new QCameraPrivate,
                   parent,
                   QMediaServiceProvider::defaultServiceProvider()->requestService(Q_MEDIASERVICE_CAMERA,
                                                                                   QMediaServiceProviderHint(cameraInfo.deviceName().toLatin1())))
{
    Q_D(QCamera);
    d->init();

    if (d->service != 0 && d->deviceControl) {
        for (int i = 0; i < d->deviceControl->deviceCount(); i++) {
            if (d->deviceControl->deviceName(i) == cameraInfo.deviceName()) {
                d->deviceControl->setSelectedDevice(i);
                break;
            }
        }
    }
}

/*!
    \since 5.3

    Construct a QCamera which uses a hardware camera located a the specified \a position.

    For example on a mobile phone it can be used to easily choose between front-facing and
    back-facing cameras.

    If no camera is available at the specified \a position or if \a position is
    QCamera::UnspecifiedPosition, the default camera is used.
*/

QCamera::QCamera(QCamera::Position position, QObject *parent)
    : QMediaObject(*new QCameraPrivate,
                   parent,
                   QMediaServiceProvider::defaultServiceProvider()->requestService(Q_MEDIASERVICE_CAMERA, QMediaServiceProviderHint(position)))
{
    Q_D(QCamera);
    d->init();

    if (d->service != 0 && d->deviceControl) {
        bool selectDefault = true;

        if (d->infoControl && position != UnspecifiedPosition) {
            for (int i = 0; i < d->deviceControl->deviceCount(); i++) {
                if (d->infoControl->cameraPosition(d->deviceControl->deviceName(i)) == position) {
                    d->deviceControl->setSelectedDevice(i);
                    selectDefault = false;
                    break;
                }
            }
        }

        if (selectDefault)
            d->deviceControl->setSelectedDevice(d->deviceControl->defaultDevice());
    }
}

/*!
    Destroys the camera object.
*/

QCamera::~QCamera()
{
    Q_D(QCamera);
    d->clear();
}

/*!
    Returns the availability state of the camera service.
*/
QMultimedia::AvailabilityStatus QCamera::availability() const
{
    Q_D(const QCamera);
    if (d->control == NULL)
        return QMultimedia::ServiceMissing;

    if (d->deviceControl && d->deviceControl->deviceCount() == 0)
        return QMultimedia::ResourceError;

    if (d->error != QCamera::NoError)
        return QMultimedia::ResourceError;

    return QMediaObject::availability();
}


/*!
    Returns the camera exposure control object.
*/
QCameraExposure *QCamera::exposure() const
{
    return d_func()->cameraExposure;
}

/*!
    Returns the camera focus control object.
*/
QCameraFocus *QCamera::focus() const
{
    return d_func()->cameraFocus;
}

/*!
    Returns the camera image processing control object.
*/
QCameraImageProcessing *QCamera::imageProcessing() const
{
    return d_func()->imageProcessing;
}

/*!
  Sets the QVideoWidget based camera \a viewfinder.
  The previously set viewfinder is detached.
*/

// QVideoWidget is forward declared
void QCamera::setViewfinder(QVideoWidget *viewfinder)
{
    Q_D(QCamera);
    d->_q_preparePropertyChange(QCameraControl::Viewfinder);

    if (d->viewfinder)
        unbind(d->viewfinder);

    // We don't know (in this library) that QVideoWidget inherits QObject
    QObject *viewFinderObject = reinterpret_cast<QObject*>(viewfinder);

    d->viewfinder = viewFinderObject && bind(viewFinderObject) ? viewFinderObject : 0;
}

/*!
  Sets the QGraphicsVideoItem based camera \a viewfinder.
  The previously set viewfinder is detached.
*/
// QGraphicsVideoItem is forward declared
void QCamera::setViewfinder(QGraphicsVideoItem *viewfinder)
{
    Q_D(QCamera);
    d->_q_preparePropertyChange(QCameraControl::Viewfinder);

    if (d->viewfinder)
        unbind(d->viewfinder);

    // We don't know (in this library) that QGraphicsVideoItem (multiply) inherits QObject
    // but QObject inheritance depends on QObject coming first, so try this out.
    QObject *viewFinderObject = reinterpret_cast<QObject*>(viewfinder);

    d->viewfinder = viewFinderObject && bind(viewFinderObject) ? viewFinderObject : 0;
}

/*!
    Sets a video \a surface as the viewfinder of a camera.

    If a viewfinder has already been set on the camera the new surface
    will replace it.
*/

void QCamera::setViewfinder(QAbstractVideoSurface *surface)
{
    Q_D(QCamera);

    d->surfaceViewfinder.setVideoSurface(surface);

    if (d->viewfinder != &d->surfaceViewfinder) {
        if (d->viewfinder)
            unbind(d->viewfinder);

        d->viewfinder = 0;

        if (surface && bind(&d->surfaceViewfinder))
            d->viewfinder = &d->surfaceViewfinder;
    } else if (!surface) {
        //unbind the surfaceViewfinder if null surface is set
        unbind(&d->surfaceViewfinder);
        d->viewfinder = 0;
    }
}

/*!
    Returns the error state of the object.
*/

QCamera::Error QCamera::error() const
{
    return d_func()->error;
}

/*!
    Returns a string describing a camera's error state.
*/
QString QCamera::errorString() const
{
    return d_func()->errorString;
}


/*!
    Returns true if the capture \a mode is suported.
*/
bool QCamera::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    return d_func()->control ? d_func()->control->isCaptureModeSupported(mode) : false;
}

/*!
  \property QCamera::captureMode

  The type of media (video or still images),
  the camera is configured to capture.

  It's allowed to change capture mode in any camera state,
  but if the camera is currently active,
  chaging capture mode is likely to lead to camera status
  chaged to QCamera::LoadedStatus, QCamera::LoadingStatus,
  and when the camera is ready to QCamera::ActiveStatus.
*/

QCamera::CaptureModes QCamera::captureMode() const
{
    return d_func()->control ? d_func()->control->captureMode() : QCamera::CaptureStillImage;
}

void QCamera::setCaptureMode(QCamera::CaptureModes mode)
{
    Q_D(QCamera);

    if (mode != captureMode()) {
        if (d->control) {
            d->_q_preparePropertyChange(QCameraControl::CaptureMode);
            d->control->setCaptureMode(mode);
        }
    }
}


/*!
    Starts the camera.

    State is changed to QCamera::ActiveState if camera is started
    successfully, otherwise error() signal is emitted.

    While the camera state is changed to QCamera::ActiveState,
    starting the camera service can be asynchronous with the actual
    status reported with QCamera::status property.
*/
void QCamera::start()
{
    Q_D(QCamera);
    d->setState(QCamera::ActiveState);
}

/*!
    Stops the camera.
    The camera state is changed from QCamera::ActiveState to QCamera::LoadedState.
*/
void QCamera::stop()
{
    Q_D(QCamera);
    d->setState(QCamera::LoadedState);
}

/*!
    Open the camera device.
    The camera state is changed to QCamera::LoadedStatus.

    It's not necessary to explcitly load the camera,
    unless unless the application have to read the supported camera
    settings and change the default depending on the camera capabilities.

    In all the other cases it's possible to start the camera directly
    from unloaded state.
*/
void QCamera::load()
{
    Q_D(QCamera);
    d->setState(QCamera::LoadedState);
}

/*!
    Close the camera device and deallocate the related resources.
    The camera state is changed to QCamera::UnloadedStatus.
*/
void QCamera::unload()
{
    Q_D(QCamera);
    d->setState(QCamera::UnloadedState);
}

#if QT_DEPRECATED_SINCE(5, 3)
/*!
    Returns a list of camera device's available from the default service provider.
    \deprecated
    \sa QCameraInfo::availableCameras()
*/

QList<QByteArray> QCamera::availableDevices()
{
    return QMediaServiceProvider::defaultServiceProvider()->devices(QByteArray(Q_MEDIASERVICE_CAMERA));
}

/*!
    Returns the description of the \a device.
    \deprecated
    \sa QCameraInfo::availableCameras(), QCameraInfo::description()
*/

QString QCamera::deviceDescription(const QByteArray &device)
{
    return QMediaServiceProvider::defaultServiceProvider()->deviceDescription(QByteArray(Q_MEDIASERVICE_CAMERA), device);
}
#endif

QCamera::State QCamera::state() const
{
    return d_func()->state;
}

QCamera::Status QCamera::status() const
{
    if(d_func()->control)
        return (QCamera::Status)d_func()->control->status();

    return QCamera::UnavailableStatus;
}


/*!
    Returns the lock types, camera supports.
*/
QCamera::LockTypes QCamera::supportedLocks() const
{
    return d_func()->supportedLocks;
}

/*!
    Returns the requested lock types.
*/
QCamera::LockTypes QCamera::requestedLocks() const
{
    return d_func()->requestedLocks;
}

/*!
    Returns the status of requested camera settings locks.
*/
QCamera::LockStatus QCamera::lockStatus() const
{
    return d_func()->lockStatus;
}

/*!
    Returns the status of camera settings \a lock.
*/
QCamera::LockStatus QCamera::lockStatus(QCamera::LockType lockType) const
{
    const QCameraPrivate *d = d_func();

    if (!(lockType & d->supportedLocks))
        return lockType & d->requestedLocks ? QCamera::Locked : QCamera::Unlocked;

    if (!(lockType & d->requestedLocks))
        return QCamera::Unlocked;

    if (d->locksControl)
        return d->locksControl->lockStatus(lockType);

    return QCamera::Unlocked;
}

/*!
    \fn void QCamera::searchAndLock(QCamera::LockTypes locks)

    Locks the camera settings with the requested \a locks, including focusing in the single autofocus mode,
    exposure and white balance if the exposure and white balance modes are not manual.

    The camera settings are usually locked before taking one or multiple still images,
    in responce to the shutter button being half pressed.

    The QCamera::locked() signal is emitted when camera settings are successfully locked,
    otherwise QCamera::lockFailed() is emitted.

    QCamera also emits lockStatusChanged(QCamera::LockType, QCamera::LockStatus)
    on individual lock status changes and lockStatusChanged(QCamera::LockStatus) signal on composite status changes.

    Locking serves two roles: it initializes calculation of automatic parameter
    (focusing, calculating the correct exposure and white balance) and allows
    to keep some or all of those parameters during number of shots.

    If the camera doesn't support keeping one of parameters between shots, the related
    lock state changes to QCamera::Unlocked.

    It's also acceptable to relock already locked settings,
    depending on the lock parameter this initiates new focusing, exposure or white balance calculation.
 */
void QCamera::searchAndLock(QCamera::LockTypes locks)
{
    Q_D(QCamera);

    QCamera::LockStatus oldStatus = d->lockStatus;
    d->supressLockChangedSignal = true;

    d->requestedLocks |= locks;

    locks &= d->supportedLocks;

    if (d->locksControl)
        d->locksControl->searchAndLock(locks);

    d->supressLockChangedSignal = false;

    d->lockStatus = oldStatus;
    d->updateLockStatus();
}

/*!
    Lock all the supported camera settings.
 */
void QCamera::searchAndLock()
{
    searchAndLock(LockExposure | LockWhiteBalance | LockFocus);
}

/*!
    Unlocks the camera settings specified with \a locks or cancel the current locking if one is active.
 */
void QCamera::unlock(QCamera::LockTypes locks)
{
    Q_D(QCamera);

    QCamera::LockStatus oldStatus = d->lockStatus;
    d->supressLockChangedSignal = true;

    d->requestedLocks &= ~locks;

    locks &= d->supportedLocks;

    if (d->locksControl)
        d->locksControl->unlock(locks);

    d->supressLockChangedSignal = false;

    d->lockStatus = oldStatus;
    d->updateLockStatus();
}

/*!
    Unlock all the requested camera locks.
 */
void QCamera::unlock()
{
    unlock(d_func()->requestedLocks);
}


/*!
    \enum QCamera::State
    \value UnloadedState
           The initial camera state, with camera not loaded,
           the camera capabilities except of supported capture modes
           are unknown.

           While the supported settings are unknown in this state,
           it's allowed to set the camera capture settings like codec,
           resolution, or frame rate.

    \value LoadedState
           The camera is loaded and ready to be configured.

           In the Idle state it's allowed to query camera capabilities,
           set capture resolution, codecs, etc.

           The viewfinder is not active in the loaded state.

    \value ActiveState
           In the active state as soon as camera is started
           the viewfinder displays video frames and the
           camera is ready for capture.
*/


/*!
    \property QCamera::state
    \brief The current state of the camera object.
*/

/*!
    \enum QCamera::Status
    \value ActiveStatus
           The camera has been started and can produce data.
           The viewfinder displays video frames in active state.

           Depending on backend, changing some camera settings like
           capture mode, codecs or resolution in ActiveState may lead
           to changing the camera status to LoadedStatus and StartingStatus while
           the settings are applied and back to ActiveStatus when the camera is ready.

    \value StartingStatus
           The camera is starting in result of state transition to QCamera::ActiveState.
           The camera service is not ready to capture yet.

    \value StoppingStatus
           The camera is stopping in result of state transition from QCamera::ActiveState
           to QCamera::LoadedState or QCamera::UnloadedState.

    \value StandbyStatus
           The camera is in the power saving standby mode.
           The camera may come to the standby mode after some time of inactivity
           in the QCamera::LoadedState state.

    \value LoadedStatus
           The camera is loaded and ready to be configured.
           This status indicates the camera device is opened and
           it's possible to query for supported image and video capture settings,
           like resolution, framerate and codecs.

    \value LoadingStatus
           The camera device loading in result of state transition from
           QCamera::UnloadedState to QCamera::LoadedState or QCamera::ActiveState.

    \value UnloadingStatus
           The camera device is unloading in result of state transition from
           QCamera::LoadedState or QCamera::ActiveState to QCamera::UnloadedState.

    \value UnloadedStatus
           The initial camera status, with camera not loaded.
           The camera capabilities including supported capture settings may be unknown.

    \value UnavailableStatus
           The camera or camera backend is not available.
*/


/*!
    \property QCamera::status
    \brief The current status of the camera object.
*/


/*!
    \enum QCamera::CaptureMode
    \value CaptureViewfinder Camera is only configured to display viewfinder.
    \value CaptureStillImage Camera is configured for still frames capture.
    \value CaptureVideo  Camera is configured for video capture.
*/

/*!
    \enum QCamera::LockType

    \value NoLock
    \value LockExposure
        Lock camera exposure.
    \value LockWhiteBalance
        Lock the white balance.
    \value LockFocus
        Lock camera focus.
*/


/*!
    \property QCamera::lockStatus
    \brief The overall status for all the requested camera locks.
*/

/*!
    \fn void QCamera::locked()

    Signals all the requested camera settings are locked.
*/

/*!
    \fn void QCamera::lockFailed()

    Signals locking of at least one requested camera settings failed.
*/

/*!
    \fn QCamera::lockStatusChanged(QCamera::LockStatus status, QCamera::LockChangeReason reason)

    Signals the overall \a status for all the requested camera locks was changed with specified \a reason.
*/

/*!
    \fn QCamera::lockStatusChanged(QCamera::LockType lock, QCamera::LockStatus status, QCamera::LockChangeReason reason)
    Signals the \a lock \a status was changed with specified \a reason.
*/

/*!
  \enum QCamera::LockStatus
    \value Unlocked
        The application is not interested in camera settings value.
        The camera may keep this parameter without changes, this is common with camera focus,
        or adjust exposure and white balance constantly to keep the viewfinder image nice.

    \value Searching
        The application has requested the camera focus, exposure or white balance lock with
        QCamera::searchAndLock(). This state indicates the camera is focusing or calculating exposure and white balance.

    \value Locked
        The camera focus, exposure or white balance is locked.
        The camera is ready to capture, application may check the exposure parameters.

        The locked state usually means the requested parameter stays the same,
        except of the cases when the parameter is requested to be constantly updated.
        For example in continuous focusing mode, the focus is considered locked as long
        and the object is in focus, even while the actual focusing distance may be constantly changing.
*/

/*!
    \enum QCamera::LockChangeReason

    \value UserRequest
        The lock status changed in result of user request, usually to unlock camera settings.
    \value LockAcquired
        The lock status successfuly changed to QCamera::Locked.
    \value LockFailed
        The camera failed to acquire the requested lock in result of
        autofocus failure, exposure out of supported range, etc.
    \value LockLost
        The camera is not able to maintain the requested lock any more.
        Lock status is changed to QCamera::Unlocked.
    \value LockTemporaryLost
        The lock is lost, but the camera is working hard to reacquire it.
        This value may be used in continuous focusing mode,
        when the camera loses the focus, the focus lock state is changed to Qcamera::Searching
        with LockTemporaryLost reason.
*/

/*!
    \enum QCamera::Error

    \value  NoError      No errors have occurred.
    \value  CameraError  An error has occurred.
    \value  InvalidRequestError System resource doesn't support requested functionality.
    \value  ServiceMissingError No camera service available.
    \value  NotSupportedFeatureError The feature is not supported.
*/

/*!
    \fn void QCamera::error(QCamera::Error value)

    Signal emitted when error state changes to \a value.
*/

/*!
    \enum QCamera::Position
    \since 5.3

    This enum specifies the physical position of the camera on the system hardware.

    \value UnspecifiedPosition  The camera position is unspecified or unknown.

    \value BackFace  The camera is on the back face of the system hardware. For example on a
    mobile device, it means it is on the opposite side to that of the screen.

    \value FrontFace  The camera is on the front face of the system hardware. For example on a
    mobile device, it means it is on the same side as that of the screen. Viewfinder frames of
    front-facing cameras are mirrored horizontally, so the users can see themselves as looking
    into a mirror. Captured images or videos are not mirrored.

    \sa QCameraInfo::position()
*/

/*!
    \fn void QCamera::captureModeChanged(QCamera::CaptureModes mode)

    Signals the capture \a mode has changed.
*/

/*!
  \fn QCamera::stateChanged(QCamera::State state)

  Signals the camera \a state has changed.

  Usually the state changes is caused by calling
  load(), unload(), start() and stop(),
  but the state can also be changed change as a result of camera error.
*/

/*!
  \fn QCamera::statusChanged(QCamera::Status status)

  Signals the camera \a status has changed.

*/

QT_END_NAMESPACE

#include "moc_qcamera.cpp"
