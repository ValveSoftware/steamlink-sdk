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

#ifndef QDECLARATIVECAMERAEXPOSURE_H
#define QDECLARATIVECAMERAEXPOSURE_H

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

#include "qdeclarativecamera_p.h"
#include <qcamera.h>
#include <qcameraexposure.h>

QT_BEGIN_NAMESPACE

class QDeclarativeCamera;

class QDeclarativeCameraExposure : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal exposureCompensation READ exposureCompensation WRITE setExposureCompensation NOTIFY exposureCompensationChanged)

    Q_PROPERTY(int iso READ isoSensitivity NOTIFY isoSensitivityChanged)
    Q_PROPERTY(qreal shutterSpeed READ shutterSpeed NOTIFY shutterSpeedChanged)
    Q_PROPERTY(qreal aperture READ aperture NOTIFY apertureChanged)

    Q_PROPERTY(qreal manualShutterSpeed READ manualShutterSpeed WRITE setManualShutterSpeed NOTIFY manualShutterSpeedChanged)
    Q_PROPERTY(qreal manualAperture READ manualAperture WRITE setManualAperture NOTIFY manualApertureChanged)
    Q_PROPERTY(qreal manualIso READ manualIsoSensitivity WRITE setManualIsoSensitivity NOTIFY manualIsoSensitivityChanged)

    Q_PROPERTY(ExposureMode exposureMode READ exposureMode WRITE setExposureMode NOTIFY exposureModeChanged)

    Q_PROPERTY(QPointF spotMeteringPoint READ spotMeteringPoint WRITE setSpotMeteringPoint NOTIFY spotMeteringPointChanged)
    Q_PROPERTY(MeteringMode meteringMode READ meteringMode WRITE setMeteringMode NOTIFY meteringModeChanged)

    Q_ENUMS(ExposureMode)
    Q_ENUMS(MeteringMode)
public:
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

    ~QDeclarativeCameraExposure();

    ExposureMode exposureMode() const;
    qreal exposureCompensation() const;

    int isoSensitivity() const;
    qreal shutterSpeed() const;
    qreal aperture() const;

    int manualIsoSensitivity() const;
    qreal manualShutterSpeed() const;
    qreal manualAperture() const;

    QPointF spotMeteringPoint() const;
    void setSpotMeteringPoint(const QPointF &point);

    MeteringMode meteringMode() const;
    void setMeteringMode(MeteringMode mode);

public Q_SLOTS:
    void setExposureMode(ExposureMode);
    void setExposureCompensation(qreal ev);

    void setManualAperture(qreal);
    void setManualShutterSpeed(qreal);
    void setManualIsoSensitivity(int iso);

    void setAutoAperture();
    void setAutoShutterSpeed();
    void setAutoIsoSensitivity();

Q_SIGNALS:
    void isoSensitivityChanged(int);
    void apertureChanged(qreal);
    void shutterSpeedChanged(qreal);

    void manualIsoSensitivityChanged(int);
    void manualApertureChanged(qreal);
    void manualShutterSpeedChanged(qreal);

    void exposureCompensationChanged(qreal);
    void exposureModeChanged(ExposureMode);

    void meteringModeChanged(MeteringMode);
    void spotMeteringPointChanged(QPointF);

private:
    friend class QDeclarativeCamera;
    QDeclarativeCameraExposure(QCamera *camera, QObject *parent = 0);

    QCameraExposure *m_exposure;
    int m_manualIso;
    qreal m_manualAperture;
    qreal m_manualShutterSpeed;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QT_PREPEND_NAMESPACE(QDeclarativeCameraExposure))

#endif
