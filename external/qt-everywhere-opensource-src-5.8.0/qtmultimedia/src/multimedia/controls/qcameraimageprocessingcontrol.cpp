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

#include <qcameraimageprocessingcontrol.h>
#include  "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

static void qRegisterCameraImageProcessingControlMetaTypes()
{
    qRegisterMetaType<QCameraImageProcessingControl::ProcessingParameter>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterCameraImageProcessingControlMetaTypes)

/*!
    \class QCameraImageProcessingControl
    \inmodule QtMultimedia


    \ingroup multimedia_control


    \brief The QCameraImageProcessingControl class provides an abstract class
    for controlling image processing parameters, like white balance,
    contrast, saturation, sharpening and denoising.

    The interface name of QCameraImageProcessingControl is \c org.qt-project.qt.cameraimageprocessingcontrol/5.0 as
    defined in QCameraImageProcessingControl_iid.

    Camera service may choose the parameters of image processing pipeline depending
    on sensor properties camera settings and capture parameters.

    This control allows to modify some parameters of image processing pipeline
    to achieve desired results.

    Parameters with the "Adjustment" suffix, like ContrastAdjustment, SaturationAdjustment etc
    allows to adjust the parameter values, selected by camera engine,
    while parameters like Contrast and Saturation overwrites them.

    For example setting the SharpeningAdjustment parameter to -0.1
    slightly reduces the amount of sharpening applied,
    while settings the Sharpening parameter to 0 disables sharpening at all.

    \sa QMediaService::requestControl(), QCamera
*/

/*!
    \macro QCameraImageProcessingControl_iid

    \c org.qt-project.qt.cameraimageprocessingcontrol/5.0

    Defines the interface name of the QCameraImageProcessingControl class.

    \relates QCameraImageProcessingControl
*/

/*!
    Constructs an image processing control object with \a parent.
*/

QCameraImageProcessingControl::QCameraImageProcessingControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destruct the image processing control object.
*/

QCameraImageProcessingControl::~QCameraImageProcessingControl()
{
}

/*!
    \fn bool QCameraImageProcessingControl::isParameterSupported(ProcessingParameter parameter) const

    Returns true if the camera supports adjusting image processing \a parameter.

    Usually the supported setting is static,
    but some parameters may not be available depending on other
    camera settings, like presets.
    In such case the currently supported parameters should be returned.
*/

/*!
    \fn bool QCameraImageProcessingControl::isParameterValueSupported(ProcessingParameter parameter, const QVariant &value) const

    Returns true if the camera supports setting the image processing \a parameter \a value.

    It's used only for parameters with a limited set of values, like WhiteBalancePreset.
*/


/*!
    \fn QCameraImageProcessingControl::parameter(ProcessingParameter parameter) const

    Returns the image processing \a parameter value.
*/

/*!
    \fn QCameraImageProcessingControl::setParameter(ProcessingParameter parameter, const QVariant &value)

    Sets the image processing \a parameter \a value.
    Passing the null or invalid QVariant value allows
    backend to choose the suitable parameter value.

    The valid values range depends on the parameter type.
    For WhiteBalancePreset the value should be one of QCameraImageProcessing::WhiteBalanceMode values;
    for Contrast, Saturation, Brightness, Sharpening and Denoising the value should be
    in [0..1.0] range with invalid QVariant value indicating the default parameter value;
    for ContrastAdjustment, SaturationAdjustment, BrightnessAdjustment,
    SharpeningAdjustment and DenoisingAdjustment the value should be
    in [-1.0..1.0] range with default 0.
*/

/*!
  \enum QCameraImageProcessingControl::ProcessingParameter

  \value WhiteBalancePreset
    The white balance preset.
  \value ColorTemperature
    Color temperature in K. This value is used when the manual white balance mode is selected.
  \value Contrast
    Image contrast.
  \value Saturation
    Image saturation.
  \value Brightness
    Image brightness.
  \value Sharpening
    Amount of sharpening applied.
  \value Denoising
    Amount of denoising applied.
  \value ContrastAdjustment
    Image contrast adjustment.
  \value SaturationAdjustment
    Image saturation adjustment.
  \value BrightnessAdjustment
    Image brightness adjustment.
  \value SharpeningAdjustment
    Adjustment of sharpening applied.
  \value DenoisingAdjustment
    Adjustment of denoising applied.
  \value ColorFilter
    Image filter applied.  Since 5.5
  \value ExtendedParameter
    The base value for platform specific extended parameters.
 */

#include "moc_qcameraimageprocessingcontrol.cpp"
QT_END_NAMESPACE

