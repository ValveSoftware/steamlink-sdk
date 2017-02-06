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

#include "qcameraimageprocessing.h"
#include "qmediaobject_p.h"

#include <qcameracontrol.h>
#include <qcameraexposurecontrol.h>
#include <qcamerafocuscontrol.h>
#include <qmediarecordercontrol.h>
#include <qcameraimageprocessingcontrol.h>
#include <qcameraimagecapturecontrol.h>
#include <qvideodeviceselectorcontrol.h>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

static void qRegisterCameraImageProcessingMetaTypes()
        {
            qRegisterMetaType<QCameraImageProcessing::WhiteBalanceMode>();
            qRegisterMetaType<QCameraImageProcessing::ColorFilter>();
        }

Q_CONSTRUCTOR_FUNCTION(qRegisterCameraImageProcessingMetaTypes)


/*!
    \class QCameraImageProcessing

    \brief The QCameraImageProcessing class provides an interface for
    image processing related camera settings.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_camera

    After capturing the data for a camera frame, the camera hardware and
    software performs various image processing tasks to produce a final
    image.  This includes compensating for ambient light color, reducing
    noise, as well as making some other adjustments to the image.

    You can retrieve this class from an instance of a \l QCamera object.

    For example, you can set the white balance (or color temperature) used
    for processing images:

    \snippet multimedia-snippets/camera.cpp Camera image whitebalance

    Or adjust the amount of denoising performed:

    \snippet multimedia-snippets/camera.cpp Camera image denoising

    In some cases changing these settings may result in a longer delay
    before an image is ready.

    For more information on image processing of camera frames, see \l {camera_image_processing}{Camera Image Processing}.

    \sa QCameraImageProcessingControl
*/

class QCameraImageProcessingFakeControl : public QCameraImageProcessingControl {
public:
    QCameraImageProcessingFakeControl(QObject *parent) :
        QCameraImageProcessingControl(parent)
    {}

    bool isParameterSupported(ProcessingParameter) const { return false; }
    bool isParameterValueSupported(ProcessingParameter, const QVariant &) const { return false; }
    QVariant parameter(ProcessingParameter) const { return QVariant(); }
    void setParameter(ProcessingParameter, const QVariant &) {}
};


class QCameraImageProcessingPrivate : public QMediaObjectPrivate
{
    Q_DECLARE_NON_CONST_PUBLIC(QCameraImageProcessing)
public:
    void initControls();

    QCameraImageProcessing *q_ptr;

    QCamera *camera;
    QCameraImageProcessingControl *imageControl;
    bool available;
};


void QCameraImageProcessingPrivate::initControls()
{
    imageControl = 0;

    QMediaService *service = camera->service();
    if (service)
        imageControl = qobject_cast<QCameraImageProcessingControl *>(service->requestControl(QCameraImageProcessingControl_iid));

    available = (imageControl != 0);

    if (!imageControl)
        imageControl = new QCameraImageProcessingFakeControl(q_ptr);
}

/*!
    Construct a QCameraImageProcessing for \a camera.
*/

QCameraImageProcessing::QCameraImageProcessing(QCamera *camera):
    QObject(camera), d_ptr(new QCameraImageProcessingPrivate)
{
    Q_D(QCameraImageProcessing);
    d->camera = camera;
    d->q_ptr = this;
    d->initControls();
}


/*!
    Destroys the camera focus object.
*/

QCameraImageProcessing::~QCameraImageProcessing()
{
    delete d_ptr;
}


/*!
    Returns true if image processing related settings are supported by this camera.
*/
bool QCameraImageProcessing::isAvailable() const
{
    return d_func()->available;
}


/*!
    Returns the white balance mode being used.
*/

QCameraImageProcessing::WhiteBalanceMode QCameraImageProcessing::whiteBalanceMode() const
{
    return d_func()->imageControl->parameter(QCameraImageProcessingControl::WhiteBalancePreset)
            .value<QCameraImageProcessing::WhiteBalanceMode>();
}

/*!
    Sets the white balance to \a mode.
*/

void QCameraImageProcessing::setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceMode mode)
{
    d_func()->imageControl->setParameter(
                QCameraImageProcessingControl::WhiteBalancePreset,
                QVariant::fromValue<QCameraImageProcessing::WhiteBalanceMode>(mode));
}

/*!
    Returns true if the white balance \a mode is supported.
*/

bool QCameraImageProcessing::isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceMode mode) const
{
    return d_func()->imageControl->isParameterValueSupported(
                QCameraImageProcessingControl::WhiteBalancePreset,
                QVariant::fromValue<QCameraImageProcessing::WhiteBalanceMode>(mode));

}

/*!
    Returns the current color temperature if the
    current white balance mode is \c WhiteBalanceManual.  For other modes the
    return value is undefined.
*/

qreal QCameraImageProcessing::manualWhiteBalance() const
{
    return d_func()->imageControl->parameter(QCameraImageProcessingControl::ColorTemperature).toReal();
}

/*!
    Sets manual white balance to \a colorTemperature.  This is used
    when whiteBalanceMode() is set to \c WhiteBalanceManual.  The units are Kelvin.
*/

void QCameraImageProcessing::setManualWhiteBalance(qreal colorTemperature)
{
    d_func()->imageControl->setParameter(
                QCameraImageProcessingControl::ColorTemperature,
                QVariant(colorTemperature));
}

/*!
    Returns the brightness adjustment setting.
 */
qreal QCameraImageProcessing::brightness() const
{
    return d_func()->imageControl->parameter(QCameraImageProcessingControl::BrightnessAdjustment).toReal();
}

/*!
    Set the brightness adjustment to \a value.

    Valid brightness adjustment values range between -1.0 and 1.0, with a default of 0.
 */
void QCameraImageProcessing::setBrightness(qreal value)
{
    d_func()->imageControl->setParameter(QCameraImageProcessingControl::BrightnessAdjustment,
                                         QVariant(value));
}

/*!
    Returns the contrast adjustment setting.
*/
qreal QCameraImageProcessing::contrast() const
{
    return d_func()->imageControl->parameter(QCameraImageProcessingControl::ContrastAdjustment).toReal();
}

/*!
    Set the contrast adjustment to \a value.

    Valid contrast adjustment values range between -1.0 and 1.0, with a default of 0.
*/
void QCameraImageProcessing::setContrast(qreal value)
{
    d_func()->imageControl->setParameter(QCameraImageProcessingControl::ContrastAdjustment,
                                         QVariant(value));
}

/*!
    Returns the saturation adjustment value.
*/
qreal QCameraImageProcessing::saturation() const
{
    return d_func()->imageControl->parameter(QCameraImageProcessingControl::SaturationAdjustment).toReal();
}

/*!
    Sets the saturation adjustment value to \a value.

    Valid saturation values range between -1.0 and 1.0, with a default of 0.
*/

void QCameraImageProcessing::setSaturation(qreal value)
{
    d_func()->imageControl->setParameter(QCameraImageProcessingControl::SaturationAdjustment,
                                         QVariant(value));
}

/*!
    Returns the sharpening adjustment level.
*/
qreal QCameraImageProcessing::sharpeningLevel() const
{
    return d_func()->imageControl->parameter(QCameraImageProcessingControl::SharpeningAdjustment).toReal();
}

/*!
    Sets the sharpening adjustment \a level.

    Valid sharpening values range between -1.0 and 1.0, with a default of 0.
*/

void QCameraImageProcessing::setSharpeningLevel(qreal level)
{
    d_func()->imageControl->setParameter(QCameraImageProcessingControl::SharpeningAdjustment,
                                         QVariant(level));
}

/*!
    Returns the denoising adjustment level.
*/
qreal QCameraImageProcessing::denoisingLevel() const
{
    return d_func()->imageControl->parameter(QCameraImageProcessingControl::DenoisingAdjustment).toReal();
}

/*!
    Sets the denoising adjustment \a level.

    Valid sharpening values range between -1.0 and 1.0, with a default of 0.

    If the parameter value is set to 0, the amount of denoising applied
    is selected by camera and depends on camera capabilities and settings.
    Changing value in -1.0..1.0 range adjusts the amount of denoising applied
    within the supported range.
*/
void QCameraImageProcessing::setDenoisingLevel(qreal level)
{
    d_func()->imageControl->setParameter(QCameraImageProcessingControl::DenoisingAdjustment,
                                         QVariant(level));
}

/*!
    \enum QCameraImageProcessing::WhiteBalanceMode

    \value WhiteBalanceAuto         Auto white balance mode.
    \value WhiteBalanceManual       Manual white balance. In this mode the white balance should be set with
                                    setManualWhiteBalance()
    \value WhiteBalanceSunlight     Sunlight white balance mode.
    \value WhiteBalanceCloudy       Cloudy white balance mode.
    \value WhiteBalanceShade        Shade white balance mode.
    \value WhiteBalanceTungsten     Tungsten (incandescent) white balance mode.
    \value WhiteBalanceFluorescent  Fluorescent white balance mode.
    \value WhiteBalanceFlash        Flash white balance mode.
    \value WhiteBalanceSunset       Sunset white balance mode.
    \value WhiteBalanceVendor       Base value for vendor defined white balance modes.
*/

/*!
    \enum QCameraImageProcessing::ColorFilter

    \value ColorFilterNone               No filter is applied to images.
    \value ColorFilterGrayscale          A grayscale filter.
    \value ColorFilterNegative           A negative filter.
    \value ColorFilterSolarize           A solarize filter.
    \value ColorFilterSepia              A sepia filter.
    \value ColorFilterPosterize          A posterize filter.
    \value ColorFilterWhiteboard         A whiteboard filter.
    \value ColorFilterBlackboard         A blackboard filter.
    \value ColorFilterAqua               An aqua filter.
    \value ColorFilterVendor             The base value for vendor defined filters.

    \since 5.5
*/

/*!
    Returns the color filter which will be applied to image data captured by the camera.

    \since 5.5
*/

QCameraImageProcessing::ColorFilter QCameraImageProcessing::colorFilter() const
{
    return d_func()->imageControl->parameter(QCameraImageProcessingControl::ColorFilter)
            .value<QCameraImageProcessing::ColorFilter>();
}


/*!
    Sets the color \a filter which will be applied to image data captured by the camera.

    \since 5.5
*/

void QCameraImageProcessing::setColorFilter(QCameraImageProcessing::ColorFilter filter)
{
    d_func()->imageControl->setParameter(
                QCameraImageProcessingControl::ColorFilter,
                QVariant::fromValue<QCameraImageProcessing::ColorFilter>(filter));
}

/*!
    Returns true if a color \a filter is supported.

    \since 5.5
*/

bool QCameraImageProcessing::isColorFilterSupported(QCameraImageProcessing::ColorFilter filter) const
{
    return d_func()->imageControl->isParameterValueSupported(
                QCameraImageProcessingControl::ColorFilter,
                QVariant::fromValue<QCameraImageProcessing::ColorFilter>(filter));

}


#include "moc_qcameraimageprocessing.cpp"
QT_END_NAMESPACE
