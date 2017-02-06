/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MOCKCAMERAEXPOSURECONTROL_H
#define MOCKCAMERAEXPOSURECONTROL_H

#include "qcameraexposurecontrol.h"

class MockCameraExposureControl : public QCameraExposureControl
{
    Q_OBJECT
public:
    MockCameraExposureControl(QObject *parent = 0):
        QCameraExposureControl(parent),
        m_aperture(2.8),
        m_shutterSpeed(0.01),
        m_isoSensitivity(100),
        m_meteringMode(QCameraExposure::MeteringMatrix),
        m_exposureCompensation(0),
        m_exposureMode(QCameraExposure::ExposureAuto),
        m_flashMode(QCameraExposure::FlashAuto),
        m_spot(0.5, 0.5)
    {
        m_isoRanges << 100 << 200 << 400 << 800;
        m_apertureRanges << 2.8 << 4.0 << 5.6 << 8.0 << 11.0 << 16.0;
        m_shutterRanges << 0.001 << 0.01 << 0.1 << 1.0;
        m_exposureRanges << -2.0 << 2.0;

        QList<QCameraExposure::ExposureMode> exposureModes;
        exposureModes << QCameraExposure::ExposureAuto << QCameraExposure::ExposureManual << QCameraExposure::ExposureBacklight
                      << QCameraExposure::ExposureNight << QCameraExposure::ExposureSpotlight << QCameraExposure::ExposureSports
                      << QCameraExposure::ExposureSnow << QCameraExposure:: ExposureLargeAperture << QCameraExposure::ExposureSmallAperture
                      << QCameraExposure::ExposurePortrait << QCameraExposure::ExposureModeVendor << QCameraExposure::ExposureBeach;

        foreach (QCameraExposure::ExposureMode mode, exposureModes)
            m_exposureModes << QVariant::fromValue<QCameraExposure::ExposureMode>(mode);

        m_meteringModes << QVariant::fromValue<QCameraExposure::MeteringMode>(QCameraExposure::MeteringMatrix)
                        << QVariant::fromValue<QCameraExposure::MeteringMode>(QCameraExposure::MeteringSpot);
    }

    ~MockCameraExposureControl() {}

    bool isParameterSupported(ExposureParameter parameter) const
    {
        switch (parameter) {
        case QCameraExposureControl::ExposureMode:
        case QCameraExposureControl::MeteringMode:
        case QCameraExposureControl::ExposureCompensation:
        case QCameraExposureControl::ISO:
        case QCameraExposureControl::Aperture:
        case QCameraExposureControl::ShutterSpeed:
        case QCameraExposureControl::SpotMeteringPoint:
            return true;
        default:
            return false;
        }
    }

    QVariant requestedValue(ExposureParameter param) const
    {
        return m_requestedParameters.value(param);
    }

    QVariant actualValue(ExposureParameter param) const
    {
        switch (param) {
        case QCameraExposureControl::ExposureMode:
            return QVariant::fromValue<QCameraExposure::ExposureMode>(m_exposureMode);
        case QCameraExposureControl::MeteringMode:
            return QVariant::fromValue<QCameraExposure::MeteringMode>(m_meteringMode);
        case QCameraExposureControl::ExposureCompensation:
            return QVariant(m_exposureCompensation);
        case QCameraExposureControl::ISO:
            return QVariant(m_isoSensitivity);
        case QCameraExposureControl::Aperture:
            return QVariant(m_aperture);
        case QCameraExposureControl::ShutterSpeed:
            return QVariant(m_shutterSpeed);
        case QCameraExposureControl::SpotMeteringPoint:
            return QVariant(m_spot);
        default:
            return QVariant();
        }
    }

    QVariantList supportedParameterRange(ExposureParameter parameter, bool *continuous) const
    {
        *continuous = false;

        QVariantList res;
        switch (parameter) {
        case QCameraExposureControl::ExposureCompensation:
            *continuous = true;
            return m_exposureRanges;
        case QCameraExposureControl::ISO:
            return m_isoRanges;
        case QCameraExposureControl::Aperture:
            *continuous = true;
            return m_apertureRanges;
        case QCameraExposureControl::ShutterSpeed:
            *continuous = true;
            return m_shutterRanges;
        case QCameraExposureControl::ExposureMode:
            return m_exposureModes;
        case QCameraExposureControl::MeteringMode:
            return m_meteringModes;
        default:
            break;
        }

        return res;
    }

    // Added valueChanged  and parameterRangeChanged signal
    bool setValue(ExposureParameter param, const QVariant& value)
    {
        if (!isParameterSupported(param))
            return false;

        if (m_requestedParameters.value(param) != value) {
            m_requestedParameters.insert(param, value);
            emit requestedValueChanged(param);
        }

        switch (param) {
        case QCameraExposureControl::ExposureMode:
        {
            QCameraExposure::ExposureMode mode = value.value<QCameraExposure::ExposureMode>();
            if (mode != m_exposureMode && m_exposureModes.contains(value)) {
                m_exposureMode = mode;
                emit actualValueChanged(param);
            }
        }
            break;
        case QCameraExposureControl::MeteringMode:
        {
            QCameraExposure::MeteringMode mode = value.value<QCameraExposure::MeteringMode>();
            if (mode != m_meteringMode && m_meteringModes.contains(value)) {
                m_meteringMode = mode;
                emit actualValueChanged(param);
            }
        }
            break;
        case QCameraExposureControl::ExposureCompensation:
        {
            m_res.clear();
            m_res << -4.0 << 4.0;
            qreal exposureCompensationlocal = qBound<qreal>(-2.0, value.toReal(), 2.0);
            if (actualValue(param).toReal() !=  exposureCompensationlocal) {
                m_exposureCompensation = exposureCompensationlocal;
                emit actualValueChanged(param);
            }

            if (m_exposureRanges.last().toReal() != m_res.last().toReal()) {
                m_exposureRanges.clear();
                m_exposureRanges = m_res;
                emit parameterRangeChanged(param);
            }
        }
            break;
        case QCameraExposureControl::ISO:
        {
            m_res.clear();
            m_res << 20 << 50;
            qreal exposureCompensationlocal = 100*qRound(qBound(100, value.toInt(), 800)/100.0);
            if (actualValue(param).toReal() !=  exposureCompensationlocal) {
                m_isoSensitivity = exposureCompensationlocal;
                emit actualValueChanged(param);
            }

            if (m_isoRanges.last().toInt() != m_res.last().toInt()) {
                m_isoRanges.clear();
                m_isoRanges = m_res;
                emit parameterRangeChanged(param);
            }
        }
            break;
        case QCameraExposureControl::Aperture:
        {
            m_res.clear();
            m_res << 12.0 << 18.0 << 20.0;
            qreal exposureCompensationlocal = qBound<qreal>(2.8, value.toReal(), 16.0);
            if (actualValue(param).toReal() !=  exposureCompensationlocal) {
                m_aperture = exposureCompensationlocal;
                emit actualValueChanged(param);
            }

            if (m_apertureRanges.last().toReal() != m_res.last().toReal()) {
                m_apertureRanges.clear();
                m_apertureRanges = m_res;
                emit parameterRangeChanged(param);
            }
        }
            break;
        case QCameraExposureControl::ShutterSpeed:
        {
            m_res.clear();
            m_res << 0.12 << 1.0 << 2.0;
            qreal exposureCompensationlocal = qBound<qreal>(0.001, value.toReal(), 1.0);
            if (actualValue(param).toReal() !=  exposureCompensationlocal) {
                m_shutterSpeed = exposureCompensationlocal;
                emit actualValueChanged(param);
            }

            if (m_shutterRanges.last().toReal() != m_res.last().toReal()) {
                m_shutterRanges.clear();
                m_shutterRanges = m_res;
                emit parameterRangeChanged(param);
            }
        }
            break;

        case QCameraExposureControl::SpotMeteringPoint:
        {
            static QRectF valid(0, 0, 1, 1);
            if (valid.contains(value.toPointF())) {
                m_spot = value.toPointF();
                emit actualValueChanged(param);
                return true;
            }
            return false;
        }

        default:
            return false;
        }

        return true;
    }

private:
    qreal m_aperture;
    qreal m_shutterSpeed;
    int m_isoSensitivity;
    QCameraExposure::MeteringMode m_meteringMode;
    qreal m_exposureCompensation;
    QCameraExposure::ExposureMode m_exposureMode;
    QCameraExposure::FlashModes m_flashMode;
    QVariantList m_isoRanges,m_apertureRanges, m_shutterRanges, m_exposureRanges, m_res, m_exposureModes, m_meteringModes;
    QPointF m_spot;

    QMap<QCameraExposureControl::ExposureParameter, QVariant> m_requestedParameters;
};

#endif // MOCKCAMERAEXPOSURECONTROL_H
