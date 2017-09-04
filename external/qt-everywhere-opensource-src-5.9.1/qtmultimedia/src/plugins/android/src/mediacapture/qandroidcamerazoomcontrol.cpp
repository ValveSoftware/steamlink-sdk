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

#include "qandroidcamerazoomcontrol.h"

#include "qandroidcamerasession.h"
#include "androidcamera.h"
#include "qandroidmultimediautils.h"
#include <qmath.h>

QT_BEGIN_NAMESPACE

QAndroidCameraZoomControl::QAndroidCameraZoomControl(QAndroidCameraSession *session)
    : QCameraZoomControl()
    , m_cameraSession(session)
    , m_maximumZoom(1.0)
    , m_requestedZoom(1.0)
    , m_currentZoom(1.0)
{
    connect(m_cameraSession, SIGNAL(opened()),
            this, SLOT(onCameraOpened()));
}

qreal QAndroidCameraZoomControl::maximumOpticalZoom() const
{
    // Optical zoom not supported
    return 1.0;
}

qreal QAndroidCameraZoomControl::maximumDigitalZoom() const
{
    return m_maximumZoom;
}

qreal QAndroidCameraZoomControl::requestedOpticalZoom() const
{
    // Optical zoom not supported
    return 1.0;
}

qreal QAndroidCameraZoomControl::requestedDigitalZoom() const
{
    return m_requestedZoom;
}

qreal QAndroidCameraZoomControl::currentOpticalZoom() const
{
    // Optical zoom not supported
    return 1.0;
}

qreal QAndroidCameraZoomControl::currentDigitalZoom() const
{
    return m_currentZoom;
}

void QAndroidCameraZoomControl::zoomTo(qreal optical, qreal digital)
{
    Q_UNUSED(optical);

    if (!qFuzzyCompare(m_requestedZoom, digital)) {
        m_requestedZoom = digital;
        emit requestedDigitalZoomChanged(m_requestedZoom);
    }

    if (m_cameraSession->camera()) {
        digital = qBound(qreal(1), digital, m_maximumZoom);
        int validZoomIndex = qt_findClosestValue(m_zoomRatios, qRound(digital * 100));
        qreal newZoom = m_zoomRatios.at(validZoomIndex) / qreal(100);
        if (!qFuzzyCompare(m_currentZoom, newZoom)) {
            m_cameraSession->camera()->setZoom(validZoomIndex);
            m_currentZoom = newZoom;
            emit currentDigitalZoomChanged(m_currentZoom);
        }
    }
}

void QAndroidCameraZoomControl::onCameraOpened()
{
    if (m_cameraSession->camera()->isZoomSupported()) {
        m_zoomRatios = m_cameraSession->camera()->getZoomRatios();
        qreal maxZoom = m_zoomRatios.last() / qreal(100);
        if (m_maximumZoom != maxZoom) {
            m_maximumZoom = maxZoom;
            emit maximumDigitalZoomChanged(m_maximumZoom);
        }
        zoomTo(1, m_requestedZoom);
    } else {
        m_zoomRatios.clear();
        if (!qFuzzyCompare(m_maximumZoom, qreal(1))) {
            m_maximumZoom = 1.0;
            emit maximumDigitalZoomChanged(m_maximumZoom);
        }
    }
}

QT_END_NAMESPACE
