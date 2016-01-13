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
#include "bbcamerazoomcontrol.h"

#include "bbcamerasession.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

BbCameraZoomControl::BbCameraZoomControl(BbCameraSession *session, QObject *parent)
    : QCameraZoomControl(parent)
    , m_session(session)
    , m_minimumZoomFactor(1.0)
    , m_maximumZoomFactor(1.0)
    , m_supportsSmoothZoom(false)
    , m_requestedZoomFactor(1.0)
{
    connect(m_session, SIGNAL(statusChanged(QCamera::Status)), this, SLOT(statusChanged(QCamera::Status)));
}

qreal BbCameraZoomControl::maximumOpticalZoom() const
{
    //TODO: optical zoom support not available in BB10 API yet
    return 1.0;
}

qreal BbCameraZoomControl::maximumDigitalZoom() const
{
    return m_maximumZoomFactor;
}

qreal BbCameraZoomControl::requestedOpticalZoom() const
{
    //TODO: optical zoom support not available in BB10 API yet
    return 1.0;
}

qreal BbCameraZoomControl::requestedDigitalZoom() const
{
    return currentDigitalZoom();
}

qreal BbCameraZoomControl::currentOpticalZoom() const
{
    //TODO: optical zoom support not available in BB10 API yet
    return 1.0;
}

qreal BbCameraZoomControl::currentDigitalZoom() const
{
    if (m_session->status() != QCamera::ActiveStatus)
        return 1.0;

    unsigned int zoomFactor = 0;
    camera_error_t result = CAMERA_EOK;

    if (m_session->captureMode() & QCamera::CaptureStillImage)
        result = camera_get_photovf_property(m_session->handle(), CAMERA_IMGPROP_ZOOMFACTOR, &zoomFactor);
    else if (m_session->captureMode() & QCamera::CaptureVideo)
        result = camera_get_videovf_property(m_session->handle(), CAMERA_IMGPROP_ZOOMFACTOR, &zoomFactor);

    if (result != CAMERA_EOK)
        return 1.0;

    return zoomFactor;
}

void BbCameraZoomControl::zoomTo(qreal optical, qreal digital)
{
    Q_UNUSED(optical)

    if (m_session->status() != QCamera::ActiveStatus)
        return;

    const qreal actualZoom = qBound(m_minimumZoomFactor, digital, m_maximumZoomFactor);

    const camera_error_t result = camera_set_zoom(m_session->handle(), actualZoom, false);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to change zoom factor:" << result;
        return;
    }

    if (m_requestedZoomFactor != digital) {
        m_requestedZoomFactor = digital;
        emit requestedDigitalZoomChanged(m_requestedZoomFactor);
    }

    emit currentDigitalZoomChanged(actualZoom);
}

void BbCameraZoomControl::statusChanged(QCamera::Status status)
{
    if (status == QCamera::ActiveStatus) {
        // retrieve information about zoom limits
        unsigned int maximumZoomLimit = 0;
        unsigned int minimumZoomLimit = 0;
        bool smoothZoom = false;

        const camera_error_t result = camera_get_zoom_limits(m_session->handle(), &maximumZoomLimit, &minimumZoomLimit, &smoothZoom);
        if (result == CAMERA_EOK) {
            const qreal oldMaximumZoomFactor = m_maximumZoomFactor;
            m_maximumZoomFactor = maximumZoomLimit;

            if (oldMaximumZoomFactor != m_maximumZoomFactor)
                emit maximumDigitalZoomChanged(m_maximumZoomFactor);

            m_minimumZoomFactor = minimumZoomLimit;
            m_supportsSmoothZoom = smoothZoom;
        } else {
            m_maximumZoomFactor = 1.0;
            m_minimumZoomFactor = 1.0;
            m_supportsSmoothZoom = false;
        }
    }
}

QT_END_NAMESPACE
