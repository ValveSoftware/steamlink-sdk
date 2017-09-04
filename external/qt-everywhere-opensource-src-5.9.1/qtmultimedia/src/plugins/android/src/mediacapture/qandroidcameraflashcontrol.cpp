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

#include "qandroidcameraflashcontrol.h"

#include "qandroidcamerasession.h"
#include "androidcamera.h"

QT_BEGIN_NAMESPACE

QAndroidCameraFlashControl::QAndroidCameraFlashControl(QAndroidCameraSession *session)
    : QCameraFlashControl()
    , m_session(session)
    , m_flashMode(QCameraExposure::FlashOff)
{
    connect(m_session, SIGNAL(opened()),
            this, SLOT(onCameraOpened()));
}

QCameraExposure::FlashModes QAndroidCameraFlashControl::flashMode() const
{
    return m_flashMode;
}

void QAndroidCameraFlashControl::setFlashMode(QCameraExposure::FlashModes mode)
{
    if (!m_session->camera()) {
        m_flashMode = mode;
        return;
    }

    if (!isFlashModeSupported(mode))
        return;

    // if torch was enabled, it first needs to be turned off before setting another mode
    if (m_flashMode == QCameraExposure::FlashVideoLight)
        m_session->camera()->setFlashMode(QLatin1String("off"));

    m_flashMode = mode;

    QString flashMode;
    if (mode.testFlag(QCameraExposure::FlashAuto))
        flashMode = QLatin1String("auto");
    else if (mode.testFlag(QCameraExposure::FlashOn))
        flashMode = QLatin1String("on");
    else if (mode.testFlag(QCameraExposure::FlashRedEyeReduction))
        flashMode = QLatin1String("red-eye");
    else if (mode.testFlag(QCameraExposure::FlashVideoLight))
        flashMode = QLatin1String("torch");
    else // FlashOff
        flashMode = QLatin1String("off");

    m_session->camera()->setFlashMode(flashMode);
}

bool QAndroidCameraFlashControl::isFlashModeSupported(QCameraExposure::FlashModes mode) const
{
    return m_session->camera() ? m_supportedFlashModes.contains(mode) : false;
}

bool QAndroidCameraFlashControl::isFlashReady() const
{
    // Android doesn't have an API for that
    return true;
}

void QAndroidCameraFlashControl::onCameraOpened()
{
    m_supportedFlashModes.clear();

    QStringList flashModes = m_session->camera()->getSupportedFlashModes();
    for (int i = 0; i < flashModes.size(); ++i) {
        const QString &flashMode = flashModes.at(i);
        if (flashMode == QLatin1String("off"))
            m_supportedFlashModes << QCameraExposure::FlashOff;
        else if (flashMode == QLatin1String("auto"))
            m_supportedFlashModes << QCameraExposure::FlashAuto;
        else if (flashMode == QLatin1String("on"))
            m_supportedFlashModes << QCameraExposure::FlashOn;
        else if (flashMode == QLatin1String("red-eye"))
            m_supportedFlashModes << QCameraExposure::FlashRedEyeReduction;
        else if (flashMode == QLatin1String("torch"))
            m_supportedFlashModes << QCameraExposure::FlashVideoLight;
    }

    if (!m_supportedFlashModes.contains(m_flashMode))
        m_flashMode = QCameraExposure::FlashOff;

    setFlashMode(m_flashMode);
}

QT_END_NAMESPACE
