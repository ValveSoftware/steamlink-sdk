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

#include "camerabinexposure.h"
#include "camerabinsession.h"
#include <gst/interfaces/photography.h>

#include <QDebug>

#if !GST_CHECK_VERSION(1,0,0)
typedef GstSceneMode GstPhotographySceneMode;
#endif

QT_BEGIN_NAMESPACE

CameraBinExposure::CameraBinExposure(CameraBinSession *session)
    :QCameraExposureControl(session),
     m_session(session)
{
}

CameraBinExposure::~CameraBinExposure()
{
}

bool CameraBinExposure::isParameterSupported(ExposureParameter parameter) const
{
    switch (parameter) {
    case QCameraExposureControl::ExposureCompensation:
    case QCameraExposureControl::ISO:
    case QCameraExposureControl::Aperture:
    case QCameraExposureControl::ShutterSpeed:
        return true;
    default:
        return false;
    }
}

QVariantList CameraBinExposure::supportedParameterRange(ExposureParameter parameter,
                                                        bool *continuous) const
{
    if (continuous)
        *continuous = false;

    QVariantList res;
    switch (parameter) {
    case QCameraExposureControl::ExposureCompensation:
        if (continuous)
            *continuous = true;
        res << -2.0 << 2.0;
        break;
    case QCameraExposureControl::ISO:
        res << 100 << 200 << 400;
        break;
    case QCameraExposureControl::Aperture:
        res << 2.8;
        break;
    default:
        break;
    }

    return res;
}

QVariant CameraBinExposure::requestedValue(ExposureParameter parameter) const
{
    return m_requestedValues.value(parameter);
}

QVariant CameraBinExposure::actualValue(ExposureParameter parameter) const
{
    switch (parameter) {
    case QCameraExposureControl::ExposureCompensation:
    {
        gfloat ev;
        gst_photography_get_ev_compensation(m_session->photography(), &ev);
        return QVariant(ev);
    }
    case QCameraExposureControl::ISO:
    {
        guint isoSpeed = 0;
        gst_photography_get_iso_speed(m_session->photography(), &isoSpeed);
        return QVariant(isoSpeed);
    }
    case QCameraExposureControl::Aperture:
        return QVariant(2.8);
    case QCameraExposureControl::ShutterSpeed:
    {
        guint32 shutterSpeed = 0;
        gst_photography_get_exposure(m_session->photography(), &shutterSpeed);

        return QVariant(shutterSpeed/1000000.0);
    }
    case QCameraExposureControl::ExposureMode:
    {
        GstPhotographySceneMode sceneMode;
        gst_photography_get_scene_mode(m_session->photography(), &sceneMode);

        switch (sceneMode) {
        case GST_PHOTOGRAPHY_SCENE_MODE_PORTRAIT:
            return QVariant::fromValue(QCameraExposure::ExposurePortrait);
        case GST_PHOTOGRAPHY_SCENE_MODE_SPORT:
            return QVariant::fromValue(QCameraExposure::ExposureSports);
        case GST_PHOTOGRAPHY_SCENE_MODE_NIGHT:
            return QVariant::fromValue(QCameraExposure::ExposureNight);
        case GST_PHOTOGRAPHY_SCENE_MODE_MANUAL:
            return QVariant::fromValue(QCameraExposure::ExposureManual);
        case GST_PHOTOGRAPHY_SCENE_MODE_LANDSCAPE:
            return QVariant::fromValue(QCameraExposure::ExposureLandscape);
#if GST_CHECK_VERSION(1, 2, 0)
        case GST_PHOTOGRAPHY_SCENE_MODE_SNOW:
            return QVariant::fromValue(QCameraExposure::ExposureSnow);
        case GST_PHOTOGRAPHY_SCENE_MODE_BEACH:
            return QVariant::fromValue(QCameraExposure::ExposureBeach);
        case GST_PHOTOGRAPHY_SCENE_MODE_ACTION:
            return QVariant::fromValue(QCameraExposure::ExposureAction);
        case GST_PHOTOGRAPHY_SCENE_MODE_NIGHT_PORTRAIT:
            return QVariant::fromValue(QCameraExposure::ExposureNightPortrait);
        case GST_PHOTOGRAPHY_SCENE_MODE_THEATRE:
            return QVariant::fromValue(QCameraExposure::ExposureTheatre);
        case GST_PHOTOGRAPHY_SCENE_MODE_SUNSET:
            return QVariant::fromValue(QCameraExposure::ExposureSunset);
        case GST_PHOTOGRAPHY_SCENE_MODE_STEADY_PHOTO:
            return QVariant::fromValue(QCameraExposure::ExposureSteadyPhoto);
        case GST_PHOTOGRAPHY_SCENE_MODE_FIREWORKS:
            return QVariant::fromValue(QCameraExposure::ExposureFireworks);
        case GST_PHOTOGRAPHY_SCENE_MODE_PARTY:
            return QVariant::fromValue(QCameraExposure::ExposureParty);
        case GST_PHOTOGRAPHY_SCENE_MODE_CANDLELIGHT:
            return QVariant::fromValue(QCameraExposure::ExposureCandlelight);
        case GST_PHOTOGRAPHY_SCENE_MODE_BARCODE:
            return QVariant::fromValue(QCameraExposure::ExposureBarcode);
#endif
        //no direct mapping available so mapping to auto mode
        case GST_PHOTOGRAPHY_SCENE_MODE_CLOSEUP:
        case GST_PHOTOGRAPHY_SCENE_MODE_AUTO:
        default:
            return QVariant::fromValue(QCameraExposure::ExposureAuto);
        }
    }
    case QCameraExposureControl::MeteringMode:
        return QCameraExposure::MeteringMatrix;
    default:
        return QVariant();
    }
}

bool CameraBinExposure::setValue(ExposureParameter parameter, const QVariant& value)
{
    QVariant oldValue = actualValue(parameter);

    switch (parameter) {
    case QCameraExposureControl::ExposureCompensation:
        gst_photography_set_ev_compensation(m_session->photography(), value.toReal());
        break;
    case QCameraExposureControl::ISO:
        gst_photography_set_iso_speed(m_session->photography(), value.toInt());
        break;
    case QCameraExposureControl::Aperture:
        gst_photography_set_aperture(m_session->photography(), guint(value.toReal()*1000000));
        break;
    case QCameraExposureControl::ShutterSpeed:
        gst_photography_set_exposure(m_session->photography(), guint(value.toReal()*1000000));
        break;
    case QCameraExposureControl::ExposureMode:
    {
        QCameraExposure::ExposureMode mode = value.value<QCameraExposure::ExposureMode>();
        GstPhotographySceneMode sceneMode;

        gst_photography_get_scene_mode(m_session->photography(), &sceneMode);

        switch (mode) {
        case QCameraExposure::ExposureManual:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_MANUAL;
            break;
        case QCameraExposure::ExposurePortrait:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_PORTRAIT;
            break;
        case QCameraExposure::ExposureSports:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_SPORT;
            break;
        case QCameraExposure::ExposureNight:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_NIGHT;
            break;
        case QCameraExposure::ExposureAuto:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_AUTO;
            break;
        case QCameraExposure::ExposureLandscape:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_LANDSCAPE;
            break;
#if GST_CHECK_VERSION(1, 2, 0)
        case QCameraExposure::ExposureSnow:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_SNOW;
            break;
        case QCameraExposure::ExposureBeach:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_BEACH;
            break;
        case QCameraExposure::ExposureAction:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_ACTION;
            break;
        case QCameraExposure::ExposureNightPortrait:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_NIGHT_PORTRAIT;
            break;
        case QCameraExposure::ExposureTheatre:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_THEATRE;
            break;
        case QCameraExposure::ExposureSunset:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_SUNSET;
            break;
        case QCameraExposure::ExposureSteadyPhoto:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_STEADY_PHOTO;
            break;
        case QCameraExposure::ExposureFireworks:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_FIREWORKS;
            break;
        case QCameraExposure::ExposureParty:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_PARTY;
            break;
        case QCameraExposure::ExposureCandlelight:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_CANDLELIGHT;
            break;
        case QCameraExposure::ExposureBarcode:
            sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_BARCODE;
            break;
#endif
        default:
            break;
        }

        gst_photography_set_scene_mode(m_session->photography(), sceneMode);
        break;
    }
    default:
        return false;
    }

    if (!qFuzzyCompare(m_requestedValues.value(parameter).toReal(), value.toReal())) {
        m_requestedValues[parameter] = value;
        emit requestedValueChanged(parameter);
    }

    QVariant newValue = actualValue(parameter);
    if (!qFuzzyCompare(oldValue.toReal(), newValue.toReal()))
        emit actualValueChanged(parameter);

    return true;
}

QT_END_NAMESPACE
