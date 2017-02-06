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

#ifndef QDECLARATIVECAMERA_H
#define QDECLARATIVECAMERA_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qdeclarativecameracapture_p.h"
#include "qdeclarativecamerarecorder_p.h"

#include <qcamera.h>
#include <qcamerainfo.h>
#include <qcameraimageprocessing.h>
#include <qcameraimagecapture.h>

#include <QtCore/qbasictimer.h>
#include <QtCore/qdatetime.h>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqml.h>
#include <QtQml/qjsvalue.h>

QT_BEGIN_NAMESPACE

class QDeclarativeCameraExposure;
class QDeclarativeCameraFocus;
class QDeclarativeCameraFlash;
class QDeclarativeCameraImageProcessing;
class QDeclarativeMediaMetaData;
class QDeclarativeCameraViewfinder;

class QDeclarativeCamera : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QString deviceId READ deviceId WRITE setDeviceId NOTIFY deviceIdChanged REVISION 1)
    Q_PROPERTY(Position position READ position WRITE setPosition NOTIFY positionChanged REVISION 1)
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayNameChanged REVISION 1)
    Q_PROPERTY(int orientation READ orientation NOTIFY orientationChanged REVISION 1)

    Q_PROPERTY(CaptureMode captureMode READ captureMode WRITE setCaptureMode NOTIFY captureModeChanged)
    Q_PROPERTY(State cameraState READ cameraState WRITE setCameraState NOTIFY cameraStateChanged)
    Q_PROPERTY(Status cameraStatus READ cameraStatus NOTIFY cameraStatusChanged)
    Q_PROPERTY(LockStatus lockStatus READ lockStatus NOTIFY lockStatusChanged)
    Q_PROPERTY(Error errorCode READ errorCode NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

    Q_PROPERTY(Availability availability READ availability NOTIFY availabilityChanged)

    Q_PROPERTY(qreal opticalZoom READ opticalZoom WRITE setOpticalZoom NOTIFY opticalZoomChanged)
    Q_PROPERTY(qreal maximumOpticalZoom READ maximumOpticalZoom NOTIFY maximumOpticalZoomChanged)
    Q_PROPERTY(qreal digitalZoom READ digitalZoom WRITE setDigitalZoom NOTIFY digitalZoomChanged)
    Q_PROPERTY(qreal maximumDigitalZoom READ maximumDigitalZoom NOTIFY maximumDigitalZoomChanged)

    Q_PROPERTY(QObject *mediaObject READ mediaObject NOTIFY mediaObjectChanged SCRIPTABLE false DESIGNABLE false)
    Q_PROPERTY(QDeclarativeCameraCapture* imageCapture READ imageCapture CONSTANT)
    Q_PROPERTY(QDeclarativeCameraRecorder* videoRecorder READ videoRecorder CONSTANT)
    Q_PROPERTY(QDeclarativeCameraExposure* exposure READ exposure CONSTANT)
    Q_PROPERTY(QDeclarativeCameraFlash* flash READ flash CONSTANT)
    Q_PROPERTY(QDeclarativeCameraFocus* focus READ focus CONSTANT)
    Q_PROPERTY(QDeclarativeCameraImageProcessing* imageProcessing READ imageProcessing CONSTANT)
    Q_PROPERTY(QDeclarativeMediaMetaData *metaData READ metaData CONSTANT REVISION 1)
    Q_PROPERTY(QDeclarativeCameraViewfinder *viewfinder READ viewfinder CONSTANT REVISION 1)

    Q_ENUMS(Position)
    Q_ENUMS(CaptureMode)
    Q_ENUMS(State)
    Q_ENUMS(Status)
    Q_ENUMS(LockStatus)
    Q_ENUMS(Error)

    Q_ENUMS(FlashMode)
    Q_ENUMS(ExposureMode)
    Q_ENUMS(MeteringMode)

    Q_ENUMS(FocusMode)
    Q_ENUMS(FocusPointMode)
    Q_ENUMS(FocusAreaStatus)
    Q_ENUMS(Availability)

public:
    enum Position {
        UnspecifiedPosition = QCamera::UnspecifiedPosition,
        BackFace = QCamera::BackFace,
        FrontFace = QCamera::FrontFace
    };

    enum CaptureMode {
        CaptureViewfinder = QCamera::CaptureViewfinder,
        CaptureStillImage = QCamera::CaptureStillImage,
        CaptureVideo = QCamera::CaptureVideo
    };

    enum State
    {
        ActiveState = QCamera::ActiveState,
        LoadedState = QCamera::LoadedState,
        UnloadedState = QCamera::UnloadedState
    };

    enum Status
    {
        UnavailableStatus = QCamera::UnavailableStatus,
        UnloadedStatus = QCamera::UnloadedStatus,
        LoadingStatus = QCamera::LoadingStatus,
        UnloadingStatus = QCamera::UnloadingStatus,
        LoadedStatus = QCamera::LoadedStatus,
        StandbyStatus = QCamera::StandbyStatus,
        StartingStatus = QCamera::StartingStatus,
        StoppingStatus = QCamera::StoppingStatus,
        ActiveStatus = QCamera::ActiveStatus
    };

    enum LockStatus
    {
        Unlocked = QCamera::Unlocked,
        Searching = QCamera::Searching,
        Locked = QCamera::Locked
    };

    enum Error
    {
        NoError = QCamera::NoError,
        CameraError = QCamera::CameraError,
        InvalidRequestError = QCamera::InvalidRequestError,
        ServiceMissingError = QCamera::ServiceMissingError,
        NotSupportedFeatureError = QCamera::NotSupportedFeatureError
    };

    enum FlashMode {
        FlashAuto = QCameraExposure::FlashAuto,
        FlashOff = QCameraExposure::FlashOff,
        FlashOn = QCameraExposure::FlashOn,
        FlashRedEyeReduction = QCameraExposure::FlashRedEyeReduction,
        FlashFill = QCameraExposure::FlashFill,
        FlashTorch = QCameraExposure::FlashTorch,
        FlashVideoLight = QCameraExposure::FlashVideoLight,
        FlashSlowSyncFrontCurtain = QCameraExposure::FlashSlowSyncFrontCurtain,
        FlashSlowSyncRearCurtain = QCameraExposure::FlashSlowSyncRearCurtain,
        FlashManual = QCameraExposure::FlashManual
    };

    enum ExposureMode {
        ExposureAuto = QCameraExposure::ExposureAuto,
        ExposureManual = QCameraExposure::ExposureManual,
        ExposurePortrait = QCameraExposure::ExposurePortrait,
        ExposureNight = QCameraExposure::ExposureNight,
        ExposureBacklight = QCameraExposure::ExposureBacklight,
        ExposureSpotlight = QCameraExposure::ExposureSpotlight,
        ExposureSports = QCameraExposure::ExposureSports,
        ExposureSnow = QCameraExposure::ExposureSnow,
        ExposureBeach = QCameraExposure::ExposureBeach,
        ExposureLargeAperture = QCameraExposure::ExposureLargeAperture,
        ExposureSmallAperture = QCameraExposure::ExposureSmallAperture,
        ExposureAction = QCameraExposure::ExposureAction,
        ExposureLandscape = QCameraExposure::ExposureLandscape,
        ExposureNightPortrait = QCameraExposure::ExposureNightPortrait,
        ExposureTheatre = QCameraExposure::ExposureTheatre,
        ExposureSunset = QCameraExposure::ExposureSunset,
        ExposureSteadyPhoto = QCameraExposure::ExposureSteadyPhoto,
        ExposureFireworks = QCameraExposure::ExposureFireworks,
        ExposureParty = QCameraExposure::ExposureParty,
        ExposureCandlelight = QCameraExposure::ExposureCandlelight,
        ExposureBarcode = QCameraExposure::ExposureBarcode,
        ExposureModeVendor = QCameraExposure::ExposureModeVendor
    };

    enum MeteringMode {
        MeteringMatrix = QCameraExposure::MeteringMatrix,
        MeteringAverage = QCameraExposure::MeteringAverage,
        MeteringSpot = QCameraExposure::MeteringSpot
    };

    enum FocusMode {
        FocusManual = QCameraFocus::ManualFocus,
        FocusHyperfocal = QCameraFocus::HyperfocalFocus,
        FocusInfinity = QCameraFocus::InfinityFocus,
        FocusAuto = QCameraFocus::AutoFocus,
        FocusContinuous = QCameraFocus::ContinuousFocus,
        FocusMacro = QCameraFocus::MacroFocus
    };

    enum FocusPointMode {
        FocusPointAuto = QCameraFocus::FocusPointAuto,
        FocusPointCenter = QCameraFocus::FocusPointCenter,
        FocusPointFaceDetection = QCameraFocus::FocusPointFaceDetection,
        FocusPointCustom = QCameraFocus::FocusPointCustom
    };

    enum FocusAreaStatus {
        FocusAreaUnused = QCameraFocusZone::Unused,
        FocusAreaSelected = QCameraFocusZone::Selected,
        FocusAreaFocused = QCameraFocusZone::Focused
    };

    enum Availability {
        Available = QMultimedia::Available,
        Busy = QMultimedia::Busy,
        Unavailable = QMultimedia::ServiceMissing,
        ResourceMissing = QMultimedia::ResourceError
    };

    QDeclarativeCamera(QObject *parent = 0);
    ~QDeclarativeCamera();

    QObject *mediaObject() { return m_camera; }

    QDeclarativeCameraCapture *imageCapture() { return m_imageCapture; }
    QDeclarativeCameraRecorder *videoRecorder() { return m_videoRecorder; }
    QDeclarativeCameraExposure *exposure() { return m_exposure; }
    QDeclarativeCameraFlash *flash() { return m_flash; }
    QDeclarativeCameraFocus *focus() { return m_focus; }
    QDeclarativeCameraImageProcessing *imageProcessing() { return m_imageProcessing; }
    QDeclarativeCameraViewfinder *viewfinder();

    QDeclarativeMediaMetaData *metaData();

    QString deviceId() const;
    void setDeviceId(const QString &name);

    Position position() const;
    void setPosition(Position position);

    QString displayName() const;
    int orientation() const;

    CaptureMode captureMode() const;
    State cameraState() const;
    Status cameraStatus() const;

    Error errorCode() const;
    QString errorString() const;

    LockStatus lockStatus() const;

    qreal maximumOpticalZoom() const;
    qreal maximumDigitalZoom() const;

    qreal opticalZoom() const;
    qreal digitalZoom() const;

    Availability availability() const;

public Q_SLOTS:
    void setCaptureMode(CaptureMode mode);

    void start();
    void stop();

    void setCameraState(State state);

    void searchAndLock();
    void unlock();

    void setOpticalZoom(qreal);
    void setDigitalZoom(qreal);

    Q_REVISION(2) QJSValue supportedViewfinderResolutions(qreal minimumFrameRate = 0.0,
                                                          qreal maximumFrameRate = 0.0);

    Q_REVISION(2) QJSValue supportedViewfinderFrameRateRanges(const QJSValue &resolution = QJSValue());

Q_SIGNALS:
    void errorChanged();
    void error(QDeclarativeCamera::Error errorCode, const QString &errorString);

    Q_REVISION(1) void deviceIdChanged();
    Q_REVISION(1) void positionChanged();
    Q_REVISION(1) void displayNameChanged();
    Q_REVISION(1) void orientationChanged();

    void captureModeChanged();
    void cameraStateChanged(QDeclarativeCamera::State);
    void cameraStatusChanged();

    void lockStatusChanged();

    void opticalZoomChanged(qreal);
    void digitalZoomChanged(qreal);
    void maximumOpticalZoomChanged(qreal);
    void maximumDigitalZoomChanged(qreal);

    void mediaObjectChanged();

    void availabilityChanged(Availability availability);

private Q_SLOTS:
    void _q_updateState(QCamera::State);
    void _q_error(QCamera::Error);
    void _q_availabilityChanged(QMultimedia::AvailabilityStatus);

protected:
    void classBegin();
    void componentComplete();

private:
    Q_DISABLE_COPY(QDeclarativeCamera)
    void setupDevice(const QString &deviceName);

    QCamera *m_camera;
    QCameraInfo m_currentCameraInfo;

    QDeclarativeCameraCapture *m_imageCapture;
    QDeclarativeCameraRecorder *m_videoRecorder;
    QDeclarativeCameraExposure *m_exposure;
    QDeclarativeCameraFlash *m_flash;
    QDeclarativeCameraFocus *m_focus;
    QDeclarativeCameraImageProcessing *m_imageProcessing;
    QDeclarativeMediaMetaData *m_metaData;
    QDeclarativeCameraViewfinder *m_viewfinder;

    State m_pendingState;
    bool m_componentComplete;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QT_PREPEND_NAMESPACE(QDeclarativeCamera))

#endif
