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
#include "qdeclarativecameraimageprocessing_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype CameraImageProcessing
    \instantiates QDeclarativeCameraImageProcessing
    \inqmlmodule QtMultimedia
    \brief An interface for camera capture related settings.
    \ingroup multimedia_qml
    \ingroup camera_qml

    CameraImageProcessing provides control over post-processing
    done by the camera middleware, including white balance adjustments,
    contrast, saturation, sharpening, and denoising

    It should not be constructed separately, instead the
    \c imageProcessing property of a \l Camera should be used.

    \qml

    Camera {
        id: camera

        imageProcessing {
            whiteBalanceMode: Camera.WhiteBalanceTungsten
            contrast: 0.66
            saturation: -0.5
        }
    }

    \endqml


*/
/*!
    \class QDeclarativeCameraImageProcessing
    \internal
    \brief The CameraCapture provides an interface for camera capture related settings
*/


QDeclarativeCameraImageProcessing::QDeclarativeCameraImageProcessing(QCamera *camera, QObject *parent) :
    QObject(parent)
{
    m_imageProcessing = camera->imageProcessing();
}

QDeclarativeCameraImageProcessing::~QDeclarativeCameraImageProcessing()
{
}

/*!
    \qmlproperty enumeration QtMultimedia::CameraImageProcessing::whiteBalanceMode

    \table
    \header \li Value \li Description
    \row \li WhiteBalanceManual       \li Manual white balance. In this mode the manual white balance property value is used.
    \row \li WhiteBalanceAuto         \li Auto white balance mode.
    \row \li WhiteBalanceSunlight     \li Sunlight white balance mode.
    \row \li WhiteBalanceCloudy       \li Cloudy white balance mode.
    \row \li WhiteBalanceShade        \li Shade white balance mode.
    \row \li WhiteBalanceTungsten     \li Tungsten white balance mode.
    \row \li WhiteBalanceFluorescent  \li Fluorescent white balance mode.
    \row \li WhiteBalanceFlash        \li Flash white balance mode.
    \row \li WhiteBalanceSunset       \li Sunset white balance mode.
    \row \li WhiteBalanceVendor       \li Vendor defined white balance mode.
    \endtable

    \sa manualWhiteBalance
*/
QDeclarativeCameraImageProcessing::WhiteBalanceMode QDeclarativeCameraImageProcessing::whiteBalanceMode() const
{
    return WhiteBalanceMode(m_imageProcessing->whiteBalanceMode());
}

void QDeclarativeCameraImageProcessing::setWhiteBalanceMode(QDeclarativeCameraImageProcessing::WhiteBalanceMode mode) const
{
    if (whiteBalanceMode() != mode) {
        m_imageProcessing->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceMode(mode));
        emit whiteBalanceModeChanged(whiteBalanceMode());
    }
}

/*!
    \qmlproperty qreal QtMultimedia::CameraImageProcessing::manualWhiteBalance

    The color temperature used when in manual white balance mode (WhiteBalanceManual).
    The units are Kelvin.

    \sa whiteBalanceMode
*/
qreal QDeclarativeCameraImageProcessing::manualWhiteBalance() const
{
    return m_imageProcessing->manualWhiteBalance();
}

void QDeclarativeCameraImageProcessing::setManualWhiteBalance(qreal colorTemp) const
{
    if (manualWhiteBalance() != colorTemp) {
        m_imageProcessing->setManualWhiteBalance(colorTemp);
        emit manualWhiteBalanceChanged(manualWhiteBalance());
    }
}

/*!
    \qmlproperty qreal QtMultimedia::CameraImageProcessing::brightness

    Image brightness adjustment.
    Valid brightness adjustment values range between -1.0 and 1.0, with a default of 0.

    \since 5.7
*/
qreal QDeclarativeCameraImageProcessing::brightness() const
{
    return m_imageProcessing->brightness();
}

void QDeclarativeCameraImageProcessing::setBrightness(qreal value)
{
    if (value != brightness()) {
        m_imageProcessing->setBrightness(value);
        emit brightnessChanged(brightness());
    }
}

/*!
    \qmlproperty qreal QtMultimedia::CameraImageProcessing::contrast

    Image contrast adjustment.
    Valid contrast adjustment values range between -1.0 and 1.0, with a default of 0.
*/
qreal QDeclarativeCameraImageProcessing::contrast() const
{
    return m_imageProcessing->contrast();
}

void QDeclarativeCameraImageProcessing::setContrast(qreal value)
{
    if (value != contrast()) {
        m_imageProcessing->setContrast(value);
        emit contrastChanged(contrast());
    }
}

/*!
    \qmlproperty qreal QtMultimedia::CameraImageProcessing::saturation

    Image saturation adjustment.
    Valid saturation adjustment values range between -1.0 and 1.0, the default is 0.
*/
qreal QDeclarativeCameraImageProcessing::saturation() const
{
    return m_imageProcessing->saturation();
}

void QDeclarativeCameraImageProcessing::setSaturation(qreal value)
{
    if (value != saturation()) {
        m_imageProcessing->setSaturation(value);
        emit saturationChanged(saturation());
    }
}

/*!
    \qmlproperty qreal QtMultimedia::CameraImageProcessing::sharpeningLevel

    Adjustment of sharpening level applied to image.

    Valid sharpening level values range between -1.0 for for sharpening disabled,
    0 for default sharpening level and 1.0 for maximum sharpening applied.
*/
qreal QDeclarativeCameraImageProcessing::sharpeningLevel() const
{
    return m_imageProcessing->sharpeningLevel();
}

void QDeclarativeCameraImageProcessing::setSharpeningLevel(qreal value)
{
    if (value != sharpeningLevel()) {
        m_imageProcessing->setSharpeningLevel(value);
        emit sharpeningLevelChanged(sharpeningLevel());
    }
}

/*!
    \qmlproperty qreal QtMultimedia::CameraImageProcessing::denoisingLevel

    Adjustment of denoising applied to image.

    Valid denoising level values range between -1.0 for for denoising disabled,
    0 for default denoising level and 1.0 for maximum denoising applied.
*/
qreal QDeclarativeCameraImageProcessing::denoisingLevel() const
{
    return m_imageProcessing->denoisingLevel();
}

void QDeclarativeCameraImageProcessing::setDenoisingLevel(qreal value)
{
    if (value != denoisingLevel()) {
        m_imageProcessing->setDenoisingLevel(value);
        emit denoisingLevelChanged(denoisingLevel());
    }
}

/*!
    \qmlproperty enumeration QtMultimedia::CameraImageProcessing::colorFilter

    This property holds which color filter if any will be applied to image data captured by the camera.

    It can be one of:

    \table
    \row \li CameraImageProcessing.ColorFilterNone       \li No filter is applied to images.
    \row \li CameraImageProcessing.ColorFilterGrayscale  \li A grayscale filter.
    \row \li CameraImageProcessing.ColorFilterNegative   \li A negative filter.
    \row \li CameraImageProcessing.ColorFilterSolarize   \li A solarize filter.
    \row \li CameraImageProcessing.ColorFilterSepia      \li A sepia filter.
    \row \li CameraImageProcessing.ColorFilterPosterize  \li A posterize filter.
    \row \li CameraImageProcessing.ColorFilterWhiteboard \li A whiteboard filter.
    \row \li CameraImageProcessing.ColorFilterBlackboard \li A blackboard filter.
    \row \li CameraImageProcessing.ColorFilterAqua       \li An aqua filter.
    \row \li CameraImageProcessing.ColorFilterVendor     \li The base value for vendor defined filters.
    \endtable

    \since 5.5
*/

QDeclarativeCameraImageProcessing::ColorFilter QDeclarativeCameraImageProcessing::colorFilter() const
{
    return ColorFilter(m_imageProcessing->colorFilter());
}

void QDeclarativeCameraImageProcessing::setColorFilter(ColorFilter filter)
{
    if (this->colorFilter() != filter) {
        m_imageProcessing->setColorFilter(QCameraImageProcessing::ColorFilter(filter));
        emit colorFilterChanged();
    }
}

/*!
    \qmlsignal QtMultimedia::Camera::whiteBalanceModeChanged(Camera::WhiteBalanceMode)
    This signal is emitted when the \c whiteBalanceMode property is changed.

    The corresponding handler is \c onWhiteBalanceModeChanged.
*/

/*!
    \qmlsignal QtMultimedia::Camera::manualWhiteBalanceChanged(qreal)
    This signal is emitted when the \c manualWhiteBalance property is changed.

    The corresponding handler is \c onManualWhiteBalanceChanged.
*/

QT_END_NAMESPACE

#include "moc_qdeclarativecameraimageprocessing_p.cpp"
