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

#include "camerabinexposure.h"
#include "camerabinsession.h"
#include <gst/interfaces/photography.h>

#include <QDebug>

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
        GstSceneMode sceneMode;
        gst_photography_get_scene_mode(m_session->photography(), &sceneMode);

        switch (sceneMode) {
        case GST_PHOTOGRAPHY_SCENE_MODE_PORTRAIT:
            return QCameraExposure::ExposurePortrait;
        case GST_PHOTOGRAPHY_SCENE_MODE_SPORT:
            return QCameraExposure::ExposureSports;
        case GST_PHOTOGRAPHY_SCENE_MODE_NIGHT:
            return QCameraExposure::ExposureNight;
        case GST_PHOTOGRAPHY_SCENE_MODE_MANUAL:
            return QCameraExposure::ExposureManual;
        case GST_PHOTOGRAPHY_SCENE_MODE_CLOSEUP:
            //no direct mapping available so mapping to auto mode
        case GST_PHOTOGRAPHY_SCENE_MODE_LANDSCAPE:
            //no direct mapping available so mapping to auto mode
        case GST_PHOTOGRAPHY_SCENE_MODE_AUTO:
        default:
            return QCameraExposure::ExposureAuto;
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
        QCameraExposure::ExposureMode mode = QCameraExposure::ExposureMode(value.toInt());
        GstSceneMode sceneMode;
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
