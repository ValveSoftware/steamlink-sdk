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

#include "qandroidcameraimageprocessingcontrol.h"

#include "qandroidcamerasession.h"
#include "androidcamera.h"

QT_BEGIN_NAMESPACE

QAndroidCameraImageProcessingControl::QAndroidCameraImageProcessingControl(QAndroidCameraSession *session)
    : QCameraImageProcessingControl()
    , m_session(session)
    , m_whiteBalanceMode(QCameraImageProcessing::WhiteBalanceAuto)
{
    connect(m_session, SIGNAL(opened()),
            this, SLOT(onCameraOpened()));
}

bool QAndroidCameraImageProcessingControl::isParameterSupported(ProcessingParameter parameter) const
{
    return parameter == QCameraImageProcessingControl::WhiteBalancePreset
            && m_session->camera()
            && !m_supportedWhiteBalanceModes.isEmpty();
}

bool QAndroidCameraImageProcessingControl::isParameterValueSupported(ProcessingParameter parameter,
                                                                     const QVariant &value) const
{
    return parameter == QCameraImageProcessingControl::WhiteBalancePreset
            && m_session->camera()
            && m_supportedWhiteBalanceModes.contains(value.value<QCameraImageProcessing::WhiteBalanceMode>());
}

QVariant QAndroidCameraImageProcessingControl::parameter(ProcessingParameter parameter) const
{
    if (parameter != QCameraImageProcessingControl::WhiteBalancePreset)
        return QVariant();

    return QVariant::fromValue(m_whiteBalanceMode);
}

void QAndroidCameraImageProcessingControl::setParameter(ProcessingParameter parameter, const QVariant &value)
{
    if (parameter != QCameraImageProcessingControl::WhiteBalancePreset)
        return;

    QCameraImageProcessing::WhiteBalanceMode mode = value.value<QCameraImageProcessing::WhiteBalanceMode>();

    if (m_session->camera())
        setWhiteBalanceModeHelper(mode);
    else
        m_whiteBalanceMode = mode;
}

void QAndroidCameraImageProcessingControl::setWhiteBalanceModeHelper(QCameraImageProcessing::WhiteBalanceMode mode)
{
    QString wb = m_supportedWhiteBalanceModes.value(mode, QString());
    if (!wb.isEmpty()) {
        m_session->camera()->setWhiteBalance(wb);
        m_whiteBalanceMode = mode;
    }
}

void QAndroidCameraImageProcessingControl::onCameraOpened()
{
    m_supportedWhiteBalanceModes.clear();
    QStringList whiteBalanceModes = m_session->camera()->getSupportedWhiteBalance();
    for (int i = 0; i < whiteBalanceModes.size(); ++i) {
        const QString &wb = whiteBalanceModes.at(i);
        if (wb == QLatin1String("auto")) {
            m_supportedWhiteBalanceModes.insert(QCameraImageProcessing::WhiteBalanceAuto,
                                                QStringLiteral("auto"));
        } else if (wb == QLatin1String("cloudy-daylight")) {
            m_supportedWhiteBalanceModes.insert(QCameraImageProcessing::WhiteBalanceCloudy,
                                                QStringLiteral("cloudy-daylight"));
        } else if (wb == QLatin1String("daylight")) {
            m_supportedWhiteBalanceModes.insert(QCameraImageProcessing::WhiteBalanceSunlight,
                                                QStringLiteral("daylight"));
        } else if (wb == QLatin1String("fluorescent")) {
            m_supportedWhiteBalanceModes.insert(QCameraImageProcessing::WhiteBalanceFluorescent,
                                                QStringLiteral("fluorescent"));
        } else if (wb == QLatin1String("incandescent")) {
            m_supportedWhiteBalanceModes.insert(QCameraImageProcessing::WhiteBalanceTungsten,
                                                QStringLiteral("incandescent"));
        } else if (wb == QLatin1String("shade")) {
            m_supportedWhiteBalanceModes.insert(QCameraImageProcessing::WhiteBalanceShade,
                                                QStringLiteral("shade"));
        } else if (wb == QLatin1String("twilight")) {
            m_supportedWhiteBalanceModes.insert(QCameraImageProcessing::WhiteBalanceSunset,
                                                QStringLiteral("twilight"));
        } else if (wb == QLatin1String("warm-fluorescent")) {
            m_supportedWhiteBalanceModes.insert(QCameraImageProcessing::WhiteBalanceFlash,
                                                QStringLiteral("warm-fluorescent"));
        }
    }

    if (!m_supportedWhiteBalanceModes.contains(m_whiteBalanceMode))
        m_whiteBalanceMode = QCameraImageProcessing::WhiteBalanceAuto;

    setWhiteBalanceModeHelper(m_whiteBalanceMode);
}

QT_END_NAMESPACE
