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
