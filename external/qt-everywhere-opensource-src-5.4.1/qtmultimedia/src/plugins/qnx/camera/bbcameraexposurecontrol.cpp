/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
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
#include "bbcameraexposurecontrol.h"

#include "bbcamerasession.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

BbCameraExposureControl::BbCameraExposureControl(BbCameraSession *session, QObject *parent)
    : QCameraExposureControl(parent)
    , m_session(session)
    , m_requestedExposureMode(QCameraExposure::ExposureAuto)
{
    connect(m_session, SIGNAL(statusChanged(QCamera::Status)), this, SLOT(statusChanged(QCamera::Status)));
}

bool BbCameraExposureControl::isParameterSupported(ExposureParameter parameter) const
{
    switch (parameter) {
    case QCameraExposureControl::ISO:
        return false;
    case QCameraExposureControl::Aperture:
        return false;
    case QCameraExposureControl::ShutterSpeed:
        return false;
    case QCameraExposureControl::ExposureCompensation:
        return false;
    case QCameraExposureControl::FlashPower:
        return false;
    case QCameraExposureControl::FlashCompensation:
        return false;
    case QCameraExposureControl::TorchPower:
        return false;
    case QCameraExposureControl::SpotMeteringPoint:
        return false;
    case QCameraExposureControl::ExposureMode:
        return true;
    case QCameraExposureControl::MeteringMode:
        return false;
    default:
        return false;
    }
}

QVariantList BbCameraExposureControl::supportedParameterRange(ExposureParameter parameter, bool *continuous) const
{
    if (parameter != QCameraExposureControl::ExposureMode) // no other parameter supported by BB10 API at the moment
        return QVariantList();

    if (m_session->status() != QCamera::ActiveStatus) // we can query supported exposure modes only with active viewfinder
        return QVariantList();

    if (continuous)
        *continuous = false;

    int supported = 0;
    camera_scenemode_t modes[20];
    const camera_error_t result = camera_get_scene_modes(m_session->handle(), 20, &supported, modes);
    if (result != CAMERA_EOK) {
        qWarning() << "Unable to retrieve supported scene modes:" << result;
        return QVariantList();
    }

    QVariantList exposureModes;
    for (int i = 0; i < supported; ++i) {
        switch (modes[i]) {
        case CAMERA_SCENE_AUTO:
            exposureModes << QVariant::fromValue(QCameraExposure::ExposureAuto);
            break;
        case CAMERA_SCENE_SPORTS:
            exposureModes << QVariant::fromValue(QCameraExposure::ExposureSports);
            break;
        case CAMERA_SCENE_CLOSEUP:
            exposureModes << QVariant::fromValue(QCameraExposure::ExposurePortrait);
            break;
        case CAMERA_SCENE_ACTION:
            exposureModes << QVariant::fromValue(QCameraExposure::ExposureSports);
            break;
        case CAMERA_SCENE_BEACHANDSNOW:
            exposureModes << QVariant::fromValue(QCameraExposure::ExposureBeach) << QVariant::fromValue(QCameraExposure::ExposureSnow);
            break;
        case CAMERA_SCENE_NIGHT:
            exposureModes << QVariant::fromValue(QCameraExposure::ExposureNight);
            break;
        default: break;
        }
    }

    return exposureModes;
}

QVariant BbCameraExposureControl::requestedValue(ExposureParameter parameter) const
{
    if (parameter != QCameraExposureControl::ExposureMode) // no other parameter supported by BB10 API at the moment
        return QVariant();

    return QVariant::fromValue(m_requestedExposureMode);
}

QVariant BbCameraExposureControl::actualValue(ExposureParameter parameter) const
{
#ifndef Q_OS_BLACKBERRY_TABLET
    if (parameter != QCameraExposureControl::ExposureMode) // no other parameter supported by BB10 API at the moment
        return QVariantList();

    if (m_session->status() != QCamera::ActiveStatus) // we can query actual scene modes only with active viewfinder
        return QVariantList();

    camera_scenemode_t sceneMode = CAMERA_SCENE_DEFAULT;
    const camera_error_t result = camera_get_scene_mode(m_session->handle(), &sceneMode);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to retrieve scene mode:" << result;
        return QVariant();
    }

    switch (sceneMode) {
    case CAMERA_SCENE_AUTO:
        return QVariant::fromValue(QCameraExposure::ExposureAuto);
    case CAMERA_SCENE_SPORTS:
        return QVariant::fromValue(QCameraExposure::ExposureSports);
    case CAMERA_SCENE_CLOSEUP:
        return QVariant::fromValue(QCameraExposure::ExposurePortrait);
    case CAMERA_SCENE_ACTION:
        return QVariant::fromValue(QCameraExposure::ExposureSports);
    case CAMERA_SCENE_BEACHANDSNOW:
        return (m_requestedExposureMode == QCameraExposure::ExposureBeach ? QVariant::fromValue(QCameraExposure::ExposureBeach)
                                                                          : QVariant::fromValue(QCameraExposure::ExposureSnow));
    case CAMERA_SCENE_NIGHT:
        return QVariant::fromValue(QCameraExposure::ExposureNight);
    default:
        return QVariant();
    }
#else
    return QVariant();
#endif
}

bool BbCameraExposureControl::setValue(ExposureParameter parameter, const QVariant& value)
{
    if (parameter != QCameraExposureControl::ExposureMode) // no other parameter supported by BB10 API at the moment
        return false;

    if (m_session->status() != QCamera::ActiveStatus) // we can set actual scene modes only with active viewfinder
        return false;

    camera_scenemode_t sceneMode = CAMERA_SCENE_DEFAULT;

    if (value.isValid()) {
        m_requestedExposureMode = value.value<QCameraExposure::ExposureMode>();
        emit requestedValueChanged(QCameraExposureControl::ExposureMode);

        switch (m_requestedExposureMode) {
        case QCameraExposure::ExposureAuto:
            sceneMode = CAMERA_SCENE_AUTO;
            break;
        case QCameraExposure::ExposureSports:
            sceneMode = CAMERA_SCENE_SPORTS;
            break;
        case QCameraExposure::ExposurePortrait:
            sceneMode = CAMERA_SCENE_CLOSEUP;
            break;
        case QCameraExposure::ExposureBeach:
            sceneMode = CAMERA_SCENE_BEACHANDSNOW;
            break;
        case QCameraExposure::ExposureSnow:
            sceneMode = CAMERA_SCENE_BEACHANDSNOW;
            break;
        case QCameraExposure::ExposureNight:
            sceneMode = CAMERA_SCENE_NIGHT;
            break;
        default:
            sceneMode = CAMERA_SCENE_DEFAULT;
            break;
        }
    }

    const camera_error_t result = camera_set_scene_mode(m_session->handle(), sceneMode);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to set scene mode:" << result;
        return false;
    }

    emit actualValueChanged(QCameraExposureControl::ExposureMode);

    return true;
}

void BbCameraExposureControl::statusChanged(QCamera::Status status)
{
    if (status == QCamera::ActiveStatus || status == QCamera::LoadedStatus)
        emit parameterRangeChanged(QCameraExposureControl::ExposureMode);
}

QT_END_NAMESPACE
