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

#include "camerabinzoom.h"
#include "camerabinsession.h"

#define ZOOM_PROPERTY "zoom"
#define MAX_ZOOM_PROPERTY "max-zoom"

QT_BEGIN_NAMESPACE

CameraBinZoom::CameraBinZoom(CameraBinSession *session)
    : QCameraZoomControl(session)
    , m_session(session)
    , m_requestedOpticalZoom(1.0)
    , m_requestedDigitalZoom(1.0)
{

}

CameraBinZoom::~CameraBinZoom()
{
}

qreal CameraBinZoom::maximumOpticalZoom() const
{
    return 1.0;
}

qreal CameraBinZoom::maximumDigitalZoom() const
{
    gfloat zoomFactor = 1.0;
    g_object_get(GST_BIN(m_session->cameraBin()), MAX_ZOOM_PROPERTY, &zoomFactor, NULL);
    return zoomFactor;
}

qreal CameraBinZoom::requestedDigitalZoom() const
{
    return m_requestedDigitalZoom;
}

qreal CameraBinZoom::requestedOpticalZoom() const
{
    return m_requestedOpticalZoom;
}

qreal CameraBinZoom::currentOpticalZoom() const
{
    return 1.0;
}

qreal CameraBinZoom::currentDigitalZoom() const
{
    gfloat zoomFactor = 1.0;
    g_object_get(GST_BIN(m_session->cameraBin()), ZOOM_PROPERTY, &zoomFactor, NULL);
    return zoomFactor;
}

void CameraBinZoom::zoomTo(qreal optical, qreal digital)
{
    qreal oldDigitalZoom = currentDigitalZoom();

    if (m_requestedDigitalZoom != digital) {
        m_requestedDigitalZoom = digital;
        emit requestedDigitalZoomChanged(digital);
    }

    if (m_requestedOpticalZoom != optical) {
        m_requestedOpticalZoom = optical;
        emit requestedOpticalZoomChanged(optical);
    }

    digital = qBound(qreal(1.0), digital, maximumDigitalZoom());
    g_object_set(GST_BIN(m_session->cameraBin()), ZOOM_PROPERTY, digital, NULL);

    qreal newDigitalZoom = currentDigitalZoom();
    if (!qFuzzyCompare(oldDigitalZoom, newDigitalZoom))
        emit currentDigitalZoomChanged(digital);
}

QT_END_NAMESPACE
