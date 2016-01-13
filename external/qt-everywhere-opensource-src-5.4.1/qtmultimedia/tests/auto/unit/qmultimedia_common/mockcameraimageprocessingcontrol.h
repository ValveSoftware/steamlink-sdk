/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef MOCKCAMERAIMAGEPROCESSINGCONTROL_H
#define MOCKCAMERAIMAGEPROCESSINGCONTROL_H

#include "qcameraimageprocessingcontrol.h"

class MockImageProcessingControl : public QCameraImageProcessingControl
{
    Q_OBJECT
public:
    MockImageProcessingControl(QObject *parent = 0)
        : QCameraImageProcessingControl(parent)
    {
        m_supportedWhiteBalance.insert(QCameraImageProcessing::WhiteBalanceAuto);
    }

    QCameraImageProcessing::WhiteBalanceMode whiteBalanceMode() const
    {
        return m_whiteBalanceMode;
    }
    void setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceMode mode)
    {
        m_whiteBalanceMode = mode;
    }

    bool isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceMode mode) const
    {
        return m_supportedWhiteBalance.contains(mode);
    }

    void setSupportedWhiteBalanceModes(QSet<QCameraImageProcessing::WhiteBalanceMode> modes)
    {
        m_supportedWhiteBalance = modes;
    }

    bool isParameterSupported(ProcessingParameter parameter) const
    {
        switch (parameter)
        {
        case ContrastAdjustment:
        case BrightnessAdjustment:
        case SharpeningAdjustment:
        case SaturationAdjustment:
        case DenoisingAdjustment:
        case ColorTemperature:
        case WhiteBalancePreset:
            return true;
        default :
            return false;
        }
    }

    bool isParameterValueSupported(ProcessingParameter parameter, const QVariant &value) const
    {
        if (parameter != WhiteBalancePreset)
            return false;

        return m_supportedWhiteBalance.contains(value.value<QCameraImageProcessing::WhiteBalanceMode>());
    }

    QVariant parameter(ProcessingParameter parameter) const
    {
        switch (parameter) {
        case ContrastAdjustment:
            return m_contrast;
        case SaturationAdjustment:
            return m_saturation;
        case BrightnessAdjustment:
            return m_brightness;
        case SharpeningAdjustment:
            return m_sharpeningLevel;
        case DenoisingAdjustment:
            return m_denoising;
        case ColorTemperature:
            return m_manualWhiteBalance;
        case WhiteBalancePreset:
            return QVariant::fromValue<QCameraImageProcessing::WhiteBalanceMode>(m_whiteBalanceMode);
        default:
            return QVariant();
        }
    }
    void setParameter(ProcessingParameter parameter, const QVariant &value)
    {
        switch (parameter) {
        case ContrastAdjustment:
            m_contrast = value;
            break;
        case SaturationAdjustment:
            m_saturation = value;
            break;
        case BrightnessAdjustment:
            m_brightness = value;
            break;
        case SharpeningAdjustment:
            m_sharpeningLevel = value;
            break;
        case DenoisingAdjustment:
            m_denoising = value;
            break;
        case ColorTemperature:
            m_manualWhiteBalance = value;
            break;
        case WhiteBalancePreset:
            m_whiteBalanceMode = value.value<QCameraImageProcessing::WhiteBalanceMode>();
            break;
        default:
            break;
        }
    }


private:
    QCameraImageProcessing::WhiteBalanceMode m_whiteBalanceMode;
    QSet<QCameraImageProcessing::WhiteBalanceMode> m_supportedWhiteBalance;
    QVariant m_manualWhiteBalance;
    QVariant m_contrast;
    QVariant m_sharpeningLevel;
    QVariant m_saturation;
    QVariant m_brightness;
    QVariant m_denoising;
};

#endif // MOCKCAMERAIMAGEPROCESSINGCONTROL_H
