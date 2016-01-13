/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qdeclarativecamera_p.h"
#include "qdeclarativecameraexposure_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype CameraExposure
    \instantiates QDeclarativeCameraExposure
    \brief An interface for exposure related camera settings.
    \ingroup multimedia_qml
    \ingroup camera_qml
    \inqmlmodule QtMultimedia

    This type is part of the \b{QtMultimedia 5.0} module.

    CameraExposure allows you to adjust exposure related settings
    like aperture and shutter speed, metering and ISO speed.

    It should not be constructed separately, instead the
    \c exposure property of the a \l Camera should be used.

    \qml
    import QtQuick 2.0
    import QtMultimedia 5.0

    Camera {
        id: camera

        exposure.exposureCompensation: -1.0
        exposure.exposureMode: Camera.ExposurePortrait
    }

    \endqml

    Several settings have both an automatic and a manual mode.  In
    the automatic modes the camera software itself will decide what
    a reasonable setting is, but in most cases these settings can
    be overridden with a specific manual setting.

    For example, to select automatic shutter speed selection:

    \code
        camera.exposure.setAutoShutterSpeed()
    \endcode

    Or for a specific shutter speed:

    \code
        camera.exposure.manualShutterSpeed = 0.01 // 10ms
    \endcode

    You can only choose one or the other mode.
*/

/*!
    \internal
    \class QDeclarativeCameraExposure
    \brief The CameraExposure provides interface for exposure related camera settings.

*/

/*!
    Construct a declarative camera exposure object using \a parent object.
 */
QDeclarativeCameraExposure::QDeclarativeCameraExposure(QCamera *camera, QObject *parent) :
    QObject(parent)
{
    m_exposure = camera->exposure();

    connect(m_exposure, SIGNAL(isoSensitivityChanged(int)), this, SIGNAL(isoSensitivityChanged(int)));
    connect(m_exposure, SIGNAL(apertureChanged(qreal)), this, SIGNAL(apertureChanged(qreal)));
    connect(m_exposure, SIGNAL(shutterSpeedChanged(qreal)), this, SIGNAL(shutterSpeedChanged(qreal)));

    connect(m_exposure, SIGNAL(exposureCompensationChanged(qreal)), this, SIGNAL(exposureCompensationChanged(qreal)));
}

QDeclarativeCameraExposure::~QDeclarativeCameraExposure()
{
}
/*!
    \property QDeclarativeCameraExposure::exposureCompensation

    This property holds the adjustment value for the automatically calculated exposure. The value is in EV units.
 */
/*!
    \qmlproperty real QtMultimedia::CameraExposure::exposureCompensation

    This property holds the adjustment value for the automatically calculated exposure.  The value is
    in EV units.
 */
qreal QDeclarativeCameraExposure::exposureCompensation() const
{
    return m_exposure->exposureCompensation();
}

void QDeclarativeCameraExposure::setExposureCompensation(qreal ev)
{
    m_exposure->setExposureCompensation(ev);
}
/*!
    \property QDeclarativeCameraExposure::iso

    This property holds the sensor's ISO sensitivity value.
 */
/*!
    \qmlproperty int QtMultimedia::CameraExposure::iso

    This property holds the sensor's ISO sensitivity value.
 */
int QDeclarativeCameraExposure::isoSensitivity() const
{
    return m_exposure->isoSensitivity();
}
/*!
    \property QDeclarativeCameraExposure::shutterSpeed

    This property holds the camera's shutter speed value in seconds.
    To affect the shutter speed you can use the \l manualShutterSpeed
    property and \l setAutoShutterSpeed().

*/
/*!
    \qmlproperty real QtMultimedia::CameraExposure::shutterSpeed

    This property holds the camera's current shutter speed value in seconds.
    To affect the shutter speed you can use the \l manualShutterSpeed
    property and \l setAutoShutterSpeed().

*/
qreal QDeclarativeCameraExposure::shutterSpeed() const
{
    return m_exposure->shutterSpeed();
}
/*!
    \property QDeclarativeCameraExposure::aperture

    This property holds the current lens aperture as an F number (the ratio of the focal length to effective aperture diameter).

    \sa manualAperture, setAutoAperture()
*/
/*!
    \qmlproperty real QtMultimedia::CameraExposure::aperture

    This property holds the current lens aperture as an F number (the ratio of
    the focal length to effective aperture diameter).

    \sa manualAperture, setAutoAperture()
*/
qreal QDeclarativeCameraExposure::aperture() const
{
    return m_exposure->aperture();
}
/*!
    \property QDeclarativeCameraExposure::manualIso

    This property holds the ISO settings for capturing photos.

    If the value is negative, the camera will
    automatically determine an appropriate value.

    \sa iso, setAutoIsoSensitivity()
*/
/*!
    \qmlproperty real QtMultimedia::CameraExposure::manualIso

    This property holds the ISO settings for capturing photos.

    If a negative value is specified, the camera will
    automatically determine an appropriate value.

    \sa iso, setAutoIsoSensitivity()
*/

int QDeclarativeCameraExposure::manualIsoSensitivity() const
{
    return m_manualIso;
}

void QDeclarativeCameraExposure::setManualIsoSensitivity(int iso)
{
    m_manualIso = iso;
    if (iso > 0)
        m_exposure->setManualIsoSensitivity(iso);
    else
        m_exposure->setAutoIsoSensitivity();

    emit manualIsoSensitivityChanged(iso);
}
/*!
    \property QDeclarativeCameraExposure::manualShutterSpeed

    This property holds the shutter speed value (in seconds).
    If the value is less than zero, the camera automatically
    determines an appropriate shutter speed.

    \l shutterSpeed, setAutoShutterSpeed()
*/
/*!
    \qmlproperty real QtMultimedia::CameraExposure::manualShutterSpeed

    This property holds the shutter speed value (in seconds).
    If the value is less than zero, the camera automatically
    determines an appropriate shutter speed.

    \l shutterSpeed, setAutoShutterSpeed()
*/
qreal QDeclarativeCameraExposure::manualShutterSpeed() const
{
    return m_manualShutterSpeed;
}

void QDeclarativeCameraExposure::setManualShutterSpeed(qreal speed)
{
    m_manualShutterSpeed = speed;
    if (speed > 0)
        m_exposure->setManualShutterSpeed(speed);
    else
        m_exposure->setAutoShutterSpeed();

    emit manualShutterSpeedChanged(speed);
}
/*!
    \property QDeclarativeCameraExposure::manualAperture

    This property holds aperture (F number) value
    for capturing photos.

    If the value is less than zero,
    the camera automatically determines an appropriate aperture value.

    \l aperture, setAutoAperture()
*/
/*!
    \qmlproperty real QtMultimedia::CameraExposure::manualAperture

    This property holds the aperture (F number) value
    for capturing photos.

    If the value is less than zero, the camera automatically
    determines an appropriate aperture value.

    \l aperture, setAutoAperture()
*/
qreal QDeclarativeCameraExposure::manualAperture() const
{
    return m_manualAperture;
}

void QDeclarativeCameraExposure::setManualAperture(qreal aperture)
{
    m_manualAperture = aperture;
    if (aperture > 0)
        m_exposure->setManualAperture(aperture);
    else
        m_exposure->setAutoAperture();

    emit manualApertureChanged(aperture);
}

/*!
    \qmlmethod QtMultimedia::CameraExposure::setAutoAperture()
  Turn on auto aperture selection. The manual aperture value is reset to -1.0
 */
void QDeclarativeCameraExposure::setAutoAperture()
{
    setManualAperture(-1.0);
}

/*!
    \qmlmethod QtMultimedia::CameraExposure::setAutoShutterSpeed()
  Turn on auto shutter speed selection. The manual shutter speed value is reset to -1.0
 */
void QDeclarativeCameraExposure::setAutoShutterSpeed()
{
    setManualShutterSpeed(-1.0);
}

/*!
    \qmlmethod QtMultimedia::CameraExposure::setAutoIsoSensitivity()
  Turn on auto ISO sensitivity selection. The manual ISO value is reset to -1.
 */
void QDeclarativeCameraExposure::setAutoIsoSensitivity()
{
    setManualIsoSensitivity(-1);
}
/*!
    \property QDeclarativeCameraExposure::exposureMode

    This property holds the camera exposure mode. The mode can one of the values in \l QCameraExposure::ExposureMode.
*/
/*!
    \qmlproperty enumeration QtMultimedia::CameraExposure::exposureMode

    This property holds the camera exposure mode.

    The mode can be one of the following:

    \table
    \header \li Value \li Description
    \row \li Camera.ExposureManual        \li Manual mode.
    \row \li Camera.ExposureAuto          \li Automatic mode.
    \row \li Camera.ExposureNight         \li Night mode.
    \row \li Camera.ExposureBacklight     \li Backlight exposure mode.
    \row \li Camera.ExposureSpotlight     \li Spotlight exposure mode.
    \row \li Camera.ExposureSports        \li Spots exposure mode.
    \row \li Camera.ExposureSnow          \li Snow exposure mode.
    \row \li Camera.ExposureBeach         \li Beach exposure mode.
    \row \li Camera.ExposureLargeAperture \li Use larger aperture with small depth of field.
    \row \li Camera.ExposureSmallAperture \li Use smaller aperture.
    \row \li Camera.ExposurePortrait      \li Portrait exposure mode.
    \row \li Camera.ExposureModeVendor    \li The base value for device specific exposure modes.
    \endtable
*/

QDeclarativeCameraExposure::ExposureMode QDeclarativeCameraExposure::exposureMode() const
{
    return QDeclarativeCameraExposure::ExposureMode(m_exposure->exposureMode());
}

void QDeclarativeCameraExposure::setExposureMode(QDeclarativeCameraExposure::ExposureMode mode)
{
    if (exposureMode() != mode) {
        m_exposure->setExposureMode(QCameraExposure::ExposureMode(mode));
        emit exposureModeChanged(exposureMode());
    }
}
/*!
    \property QDeclarativeCameraExposure::spotMeteringPoint

    This property holds the relative frame coordinates of the point to use
    for exposure metering. This point is only used in spot metering mode, and it
    typically defaults to the center \c (0.5, 0.5).
 */
/*!
    \qmlproperty QPointF QtMultimedia::CameraExposure::spotMeteringPoint

    The property holds the frame coordinates of the point to use for exposure metering.
    This point is only used in spot metering mode, and it typically defaults
    to the center \c (0.5, 0.5).
 */

QPointF QDeclarativeCameraExposure::spotMeteringPoint() const
{
    return m_exposure->spotMeteringPoint();
}

void QDeclarativeCameraExposure::setSpotMeteringPoint(const QPointF &point)
{
    QPointF oldPoint(spotMeteringPoint());
    m_exposure->setSpotMeteringPoint(point);

    if (oldPoint != spotMeteringPoint())
        emit spotMeteringPointChanged(spotMeteringPoint());
}
/*!
    \property QDeclarativeCameraExposure::meteringMode

    This property holds the camera metering mode (how exposure is balanced).
    The mode can be one of the constants in \l QCameraExposure::MeteringMode.
*/
/*!
    \qmlproperty enumeration QtMultimedia::CameraExposure::meteringMode

    This property holds the camera metering mode (how exposure is balanced).

    The mode can be one of the following:

    \table
    \header \li Value \li Description
    \row \li Camera.MeteringMatrix       \li A matrix of sample points is used to measure exposure.
    \row \li Camera.MeteringAverage      \li An average is used to measure exposure.
    \row \li Camera.MeteringSpot         \li A specific location (\l spotMeteringPoint) is used to measure exposure.
    \endtable
*/
QDeclarativeCameraExposure::MeteringMode QDeclarativeCameraExposure::meteringMode() const
{
    return QDeclarativeCameraExposure::MeteringMode(m_exposure->meteringMode());
}

void QDeclarativeCameraExposure::setMeteringMode(QDeclarativeCameraExposure::MeteringMode mode)
{
    QDeclarativeCameraExposure::MeteringMode oldMode = meteringMode();
    m_exposure->setMeteringMode(QCameraExposure::MeteringMode(mode));
    if (oldMode != meteringMode())
        emit meteringModeChanged(meteringMode());
}

QT_END_NAMESPACE

#include "moc_qdeclarativecameraexposure_p.cpp"
