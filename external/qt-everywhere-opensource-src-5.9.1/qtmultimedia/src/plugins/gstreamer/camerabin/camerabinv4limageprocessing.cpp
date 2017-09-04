/****************************************************************************
**
** Copyright (C) 2016 Denis Shienkov <denis.shienkov@gmail.com>
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

#include "camerabinv4limageprocessing.h"
#include "camerabinsession.h"

#include <QDebug>

#include <private/qcore_unix_p.h>
#include <linux/videodev2.h>

QT_BEGIN_NAMESPACE

CameraBinV4LImageProcessing::CameraBinV4LImageProcessing(CameraBinSession *session)
    : QCameraImageProcessingControl(session)
    , m_session(session)
{
}

CameraBinV4LImageProcessing::~CameraBinV4LImageProcessing()
{
}

bool CameraBinV4LImageProcessing::isParameterSupported(
        ProcessingParameter parameter) const
{
    return m_parametersInfo.contains(parameter);
}

bool CameraBinV4LImageProcessing::isParameterValueSupported(
        ProcessingParameter parameter, const QVariant &value) const
{
    QMap<ProcessingParameter, SourceParameterValueInfo>::const_iterator sourceValueInfo =
            m_parametersInfo.constFind(parameter);
    if (sourceValueInfo == m_parametersInfo.constEnd())
        return false;

    switch (parameter) {

    case QCameraImageProcessingControl::WhiteBalancePreset: {
        const QCameraImageProcessing::WhiteBalanceMode checkedValue =
                value.value<QCameraImageProcessing::WhiteBalanceMode>();
        const QCameraImageProcessing::WhiteBalanceMode firstAllowedValue =
                (*sourceValueInfo).minimumValue ? QCameraImageProcessing::WhiteBalanceAuto
                                                : QCameraImageProcessing::WhiteBalanceManual;
        const QCameraImageProcessing::WhiteBalanceMode secondAllowedValue =
                (*sourceValueInfo).maximumValue ? QCameraImageProcessing::WhiteBalanceAuto
                                                : QCameraImageProcessing::WhiteBalanceManual;
        if (checkedValue != firstAllowedValue
                && checkedValue != secondAllowedValue) {
            return false;
        }
    }
        break;

    case QCameraImageProcessingControl::ColorTemperature: {
        const qint32 checkedValue = value.toInt();
        if (checkedValue < (*sourceValueInfo).minimumValue
                || checkedValue > (*sourceValueInfo).maximumValue) {
            return false;
        }
    }
        break;

    case QCameraImageProcessingControl::ContrastAdjustment: // falling back
    case QCameraImageProcessingControl::SaturationAdjustment: // falling back
    case QCameraImageProcessingControl::BrightnessAdjustment: // falling back
    case QCameraImageProcessingControl::SharpeningAdjustment: {
        const qint32 sourceValue = sourceImageProcessingParameterValue(
                    value.toReal(), (*sourceValueInfo));
        if (sourceValue < (*sourceValueInfo).minimumValue
                || sourceValue > (*sourceValueInfo).maximumValue) {
            return false;
        }
    }
        break;

    default:
        return false;
    }

    return true;
}

QVariant CameraBinV4LImageProcessing::parameter(
        ProcessingParameter parameter) const
{
    QMap<ProcessingParameter, SourceParameterValueInfo>::const_iterator sourceValueInfo =
            m_parametersInfo.constFind(parameter);
    if (sourceValueInfo == m_parametersInfo.constEnd()) {
        qWarning() << "Unable to get the parameter value: the parameter is not supported.";
        return QVariant();
    }

    const QString deviceName = m_session->device();
    const int fd = qt_safe_open(deviceName.toLocal8Bit().constData(), O_RDONLY);
    if (fd == -1) {
        qWarning() << "Unable to open the camera" << deviceName
                   << "for read to get the parameter value:" << qt_error_string(errno);
        return QVariant();
    }

    struct v4l2_control control;
    ::memset(&control, 0, sizeof(control));
    control.id = (*sourceValueInfo).cid;

    const bool ret = (::ioctl(fd, VIDIOC_G_CTRL, &control) == 0);

    qt_safe_close(fd);

    if (!ret) {
        qWarning() << "Unable to get the parameter value:" << qt_error_string(errno);
        return QVariant();
    }

    switch (parameter) {

    case QCameraImageProcessingControl::WhiteBalancePreset:
        return QVariant::fromValue<QCameraImageProcessing::WhiteBalanceMode>(
                    control.value ? QCameraImageProcessing::WhiteBalanceAuto
                                  : QCameraImageProcessing::WhiteBalanceManual);

    case QCameraImageProcessingControl::ColorTemperature:
        return QVariant::fromValue<qint32>(control.value);

    case QCameraImageProcessingControl::ContrastAdjustment: // falling back
    case QCameraImageProcessingControl::SaturationAdjustment: // falling back
    case QCameraImageProcessingControl::BrightnessAdjustment: // falling back
    case QCameraImageProcessingControl::SharpeningAdjustment: {
        return scaledImageProcessingParameterValue(
                    control.value, (*sourceValueInfo));
    }

    default:
        return QVariant();
    }
}

void CameraBinV4LImageProcessing::setParameter(
        ProcessingParameter parameter, const QVariant &value)
{
    QMap<ProcessingParameter, SourceParameterValueInfo>::const_iterator sourceValueInfo =
            m_parametersInfo.constFind(parameter);
    if (sourceValueInfo == m_parametersInfo.constEnd()) {
        qWarning() << "Unable to set the parameter value: the parameter is not supported.";
        return;
    }

    const QString deviceName = m_session->device();
    const int fd = qt_safe_open(deviceName.toLocal8Bit().constData(), O_WRONLY);
    if (fd == -1) {
        qWarning() << "Unable to open the camera" << deviceName
                   << "for write to set the parameter value:" << qt_error_string(errno);
        return;
    }

    struct v4l2_control control;
    ::memset(&control, 0, sizeof(control));
    control.id = (*sourceValueInfo).cid;

    switch (parameter) {

    case QCameraImageProcessingControl::WhiteBalancePreset: {
        const QCameraImageProcessing::WhiteBalanceMode m =
                value.value<QCameraImageProcessing::WhiteBalanceMode>();
        if (m != QCameraImageProcessing::WhiteBalanceAuto
                && m != QCameraImageProcessing::WhiteBalanceManual) {
            qt_safe_close(fd);
            return;
        }

        control.value = (m == QCameraImageProcessing::WhiteBalanceAuto) ? true : false;
    }
        break;

    case QCameraImageProcessingControl::ColorTemperature:
        control.value = value.toInt();
        break;

    case QCameraImageProcessingControl::ContrastAdjustment: // falling back
    case QCameraImageProcessingControl::SaturationAdjustment: // falling back
    case QCameraImageProcessingControl::BrightnessAdjustment: // falling back
    case QCameraImageProcessingControl::SharpeningAdjustment:
        control.value = sourceImageProcessingParameterValue(
                    value.toReal(), (*sourceValueInfo));
        break;

    default:
        qt_safe_close(fd);
        return;
    }

    if (::ioctl(fd, VIDIOC_S_CTRL, &control) != 0)
        qWarning() << "Unable to set the parameter value:" << qt_error_string(errno);

    qt_safe_close(fd);
}

void CameraBinV4LImageProcessing::updateParametersInfo(
        QCamera::Status cameraStatus)
{
    if (cameraStatus == QCamera::UnloadedStatus)
        m_parametersInfo.clear();
    else if (cameraStatus == QCamera::LoadedStatus) {
        const QString deviceName = m_session->device();
        const int fd = qt_safe_open(deviceName.toLocal8Bit().constData(), O_RDONLY);
        if (fd == -1) {
            qWarning() << "Unable to open the camera" << deviceName
                       << "for read to query the parameter info:" << qt_error_string(errno);
            return;
        }

        static const struct SupportedParameterEntry {
            quint32 cid;
            QCameraImageProcessingControl::ProcessingParameter parameter;
        } supportedParametersEntries[] = {
            { V4L2_CID_AUTO_WHITE_BALANCE, QCameraImageProcessingControl::WhiteBalancePreset },
            { V4L2_CID_WHITE_BALANCE_TEMPERATURE, QCameraImageProcessingControl::ColorTemperature },
            { V4L2_CID_CONTRAST, QCameraImageProcessingControl::ContrastAdjustment },
            { V4L2_CID_SATURATION, QCameraImageProcessingControl::SaturationAdjustment },
            { V4L2_CID_BRIGHTNESS, QCameraImageProcessingControl::BrightnessAdjustment },
            { V4L2_CID_SHARPNESS, QCameraImageProcessingControl::SharpeningAdjustment }
        };

        for (int i = 0; i < int(sizeof(supportedParametersEntries) / sizeof(SupportedParameterEntry)); ++i) {
            struct v4l2_queryctrl queryControl;
            ::memset(&queryControl, 0, sizeof(queryControl));
            queryControl.id = supportedParametersEntries[i].cid;

            if (::ioctl(fd, VIDIOC_QUERYCTRL, &queryControl) != 0) {
                qWarning() << "Unable to query the parameter info:" << qt_error_string(errno);
                continue;
            }

            SourceParameterValueInfo sourceValueInfo;
            sourceValueInfo.cid = queryControl.id;
            sourceValueInfo.defaultValue = queryControl.default_value;
            sourceValueInfo.maximumValue = queryControl.maximum;
            sourceValueInfo.minimumValue = queryControl.minimum;

            m_parametersInfo.insert(supportedParametersEntries[i].parameter, sourceValueInfo);
        }

        qt_safe_close(fd);
    }
}

qreal CameraBinV4LImageProcessing::scaledImageProcessingParameterValue(
        qint32 sourceValue, const SourceParameterValueInfo &sourceValueInfo)
{
    if (sourceValue == sourceValueInfo.defaultValue) {
        return 0.0f;
    } else if (sourceValue < sourceValueInfo.defaultValue) {
        return ((sourceValue - sourceValueInfo.minimumValue)
                / qreal(sourceValueInfo.defaultValue - sourceValueInfo.minimumValue))
                + (-1.0f);
    } else {
        return ((sourceValue - sourceValueInfo.defaultValue)
                / qreal(sourceValueInfo.maximumValue - sourceValueInfo.defaultValue));
    }
}

qint32 CameraBinV4LImageProcessing::sourceImageProcessingParameterValue(
        qreal scaledValue, const SourceParameterValueInfo &valueRange)
{
    if (qFuzzyIsNull(scaledValue)) {
        return valueRange.defaultValue;
    } else if (scaledValue < 0.0f) {
        return ((scaledValue - (-1.0f)) * (valueRange.defaultValue - valueRange.minimumValue))
                + valueRange.minimumValue;
    } else {
        return (scaledValue * (valueRange.maximumValue - valueRange.defaultValue))
                + valueRange.defaultValue;
    }
}

QT_END_NAMESPACE
