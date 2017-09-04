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

#include <QDebug>
#include <QMediaService>

#include "qdeclarativetorch_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype Torch
    \instantiates QDeclarativeTorch
    \inqmlmodule QtMultimedia
    \brief Simple control over torch functionality

    \ingroup multimedia_qml

    In many cases the torch hardware is shared with camera flash functionality,
    and might be automatically controlled by the device.  You have control over
    the power level (of course, higher power levels are brighter but reduce
    battery life significantly).

    \qml

    Torch {
        power: 75       // 75% of full power
        enabled: true   // On
    }

    \endqml
*/
QDeclarativeTorch::QDeclarativeTorch(QObject *parent)
    : QObject(parent)
{
    m_camera = new QCamera(this);
    QMediaService *service = m_camera->service();

    m_exposure = service ? service->requestControl<QCameraExposureControl*>() : 0;
    m_flash = service ? service->requestControl<QCameraFlashControl*>() : 0;

    if (m_exposure)
        connect(m_exposure, SIGNAL(actualValueChanged(int)), SLOT(parameterChanged(int)));

    // XXX There's no signal for flash mode changed
}

QDeclarativeTorch::~QDeclarativeTorch()
{
}

/*!
    \qmlproperty bool QtMultimedia::Torch::enabled

    This property indicates whether the torch is enabled.  If the torch functionality is shared
    with the camera flash hardware, the camera will take priority
    over torch settings and the torch is disabled.
*/
/*!
    \property QDeclarativeTorch::enabled

    This property indicates whether the torch is enabled.  If the torch functionality
    is shared with the camera flash hardware, the camera will take
    priority and the torch is disabled.
 */
bool QDeclarativeTorch::enabled() const
{
    if (!m_flash)
        return false;

    return m_flash->flashMode() & QCameraExposure::FlashTorch;
}


/*!
    If \a on is true, attempt to turn on the torch.
    If the torch hardware is already in use by the camera, this will
    be ignored.
*/
void QDeclarativeTorch::setEnabled(bool on)
{
    if (!m_flash)
        return;

    QCameraExposure::FlashModes mode = m_flash->flashMode();

    if (mode & QCameraExposure::FlashTorch) {
        if (!on) {
            m_flash->setFlashMode(mode & ~QCameraExposure::FlashTorch);
            emit enabledChanged();
        }
    } else {
        if (on) {
            m_flash->setFlashMode(mode | QCameraExposure::FlashTorch);
            emit enabledChanged();
        }
    }
}

/*!
    \qmlproperty int QtMultimedia::Torch::power

    This property holds the current torch power setting, as a percentage of full power.

    In some cases this setting may change automatically as a result
    of temperature or battery conditions.
*/
/*!
    \property QDeclarativeTorch::power

    This property holds the current torch power settings, as a percentage of full power.
*/
int QDeclarativeTorch::power() const
{
    if (!m_exposure)
        return 0;

    return m_exposure->requestedValue(QCameraExposureControl::TorchPower).toInt();
}

/*!
    Sets the current torch power setting to \a power, as a percentage of
    full power.
*/
void QDeclarativeTorch::setPower(int power)
{
    if (!m_exposure)
        return;

    power = qBound(0, power, 100);
    if (this->power() != power)
        m_exposure->setValue(QCameraExposureControl::TorchPower, power);
}

/* Check for changes in flash power */
void QDeclarativeTorch::parameterChanged(int parameter)
{
    if (parameter == QCameraExposureControl::FlashPower) {
        emit powerChanged();
    }
}

QT_END_NAMESPACE

