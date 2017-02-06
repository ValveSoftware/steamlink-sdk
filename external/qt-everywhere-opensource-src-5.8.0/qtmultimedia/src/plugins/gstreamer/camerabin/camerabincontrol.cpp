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

#include "camerabincontrol.h"
#include "camerabincontainer.h"
#include "camerabinaudioencoder.h"
#include "camerabinvideoencoder.h"
#include "camerabinimageencoder.h"
#include "camerabinresourcepolicy.h"

#include <QtCore/qdebug.h>
#include <QtCore/qfile.h>
#include <QtCore/qmetaobject.h>

QT_BEGIN_NAMESPACE

//#define CAMEABIN_DEBUG 1
#define ENUM_NAME(c,e,v) (c::staticMetaObject.enumerator(c::staticMetaObject.indexOfEnumerator(e)).valueToKey((v)))

CameraBinControl::CameraBinControl(CameraBinSession *session)
    :QCameraControl(session),
    m_session(session),
    m_state(QCamera::UnloadedState),
    m_reloadPending(false)
{
    connect(m_session, SIGNAL(statusChanged(QCamera::Status)),
            this, SIGNAL(statusChanged(QCamera::Status)));

    connect(m_session, SIGNAL(viewfinderChanged()),
            SLOT(reloadLater()));
    connect(m_session, SIGNAL(readyChanged(bool)),
            SLOT(reloadLater()));
    connect(m_session, SIGNAL(error(int,QString)),
            SLOT(handleCameraError(int,QString)));

    m_resourcePolicy = new CamerabinResourcePolicy(this);
    connect(m_resourcePolicy, SIGNAL(resourcesGranted()),
            SLOT(handleResourcesGranted()));
    connect(m_resourcePolicy, SIGNAL(resourcesDenied()),
            SLOT(handleResourcesLost()));
    connect(m_resourcePolicy, SIGNAL(resourcesLost()),
            SLOT(handleResourcesLost()));

    connect(m_session, SIGNAL(busyChanged(bool)),
            SLOT(handleBusyChanged(bool)));
}

CameraBinControl::~CameraBinControl()
{
}

QCamera::CaptureModes CameraBinControl::captureMode() const
{
    return m_session->captureMode();
}

void CameraBinControl::setCaptureMode(QCamera::CaptureModes mode)
{
    if (m_session->captureMode() != mode) {
        m_session->setCaptureMode(mode);

        if (m_state == QCamera::ActiveState) {
            m_resourcePolicy->setResourceSet(
                        captureMode() == QCamera::CaptureStillImage ?
                            CamerabinResourcePolicy::ImageCaptureResources :
                            CamerabinResourcePolicy::VideoCaptureResources);
        }
        emit captureModeChanged(mode);
    }
}

bool CameraBinControl::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    return mode == QCamera::CaptureStillImage || mode == QCamera::CaptureVideo;
}

void CameraBinControl::setState(QCamera::State state)
{
#ifdef CAMEABIN_DEBUG
    qDebug() << Q_FUNC_INFO << ENUM_NAME(QCamera, "State", state);
#endif
    if (m_state != state) {
        m_state = state;

        //special case for stopping the camera while it's busy,
        //it should be delayed until the camera is idle
        if (state == QCamera::LoadedState &&
                m_session->status() == QCamera::ActiveStatus &&
                m_session->isBusy()) {
#ifdef CAMEABIN_DEBUG
            qDebug() << Q_FUNC_INFO << "Camera is busy, QCamera::stop() is delayed";
#endif
            emit stateChanged(m_state);
            return;
        }

        CamerabinResourcePolicy::ResourceSet resourceSet = CamerabinResourcePolicy::NoResources;
        switch (state) {
        case QCamera::UnloadedState:
            resourceSet = CamerabinResourcePolicy::NoResources;
            break;
        case QCamera::LoadedState:
            resourceSet = CamerabinResourcePolicy::LoadedResources;
            break;
        case QCamera::ActiveState:
            resourceSet = captureMode() == QCamera::CaptureStillImage ?
                            CamerabinResourcePolicy::ImageCaptureResources :
                            CamerabinResourcePolicy::VideoCaptureResources;
            break;
        }

        m_resourcePolicy->setResourceSet(resourceSet);

        if (m_resourcePolicy->isResourcesGranted()) {
            //postpone changing to Active if the session is nor ready yet
            if (state == QCamera::ActiveState) {
                if (m_session->isReady()) {
                    m_session->setState(state);
                } else {
#ifdef CAMEABIN_DEBUG
                    qDebug() << "Camera session is not ready yet, postpone activating";
#endif
                }
            } else
                m_session->setState(state);
        }

        emit stateChanged(m_state);
    }
}

QCamera::State CameraBinControl::state() const
{
    return m_state;
}

QCamera::Status CameraBinControl::status() const
{
    return m_session->status();
}

void CameraBinControl::reloadLater()
{
#ifdef CAMEABIN_DEBUG
    qDebug() << "CameraBinControl: reload pipeline requested" << ENUM_NAME(QCamera, "State", m_state);
#endif
    if (!m_reloadPending && m_state == QCamera::ActiveState) {
        m_reloadPending = true;

        if (!m_session->isBusy()) {
            m_session->setState(QCamera::LoadedState);
            QMetaObject::invokeMethod(this, "delayedReload", Qt::QueuedConnection);
        }
    }
}

void CameraBinControl::handleResourcesLost()
{
#ifdef CAMEABIN_DEBUG
    qDebug() << Q_FUNC_INFO << ENUM_NAME(QCamera, "State", m_state);
#endif
    m_session->setState(QCamera::UnloadedState);
}

void CameraBinControl::handleResourcesGranted()
{
#ifdef CAMEABIN_DEBUG
    qDebug() << Q_FUNC_INFO << ENUM_NAME(QCamera, "State", m_state);
#endif

    //camera will be started soon by delayedReload()
    if (m_reloadPending && m_state == QCamera::ActiveState)
        return;

    if (m_state == QCamera::ActiveState && m_session->isReady())
        m_session->setState(QCamera::ActiveState);
    else if (m_state == QCamera::LoadedState)
        m_session->setState(QCamera::LoadedState);
}

void CameraBinControl::handleBusyChanged(bool busy)
{
    if (!busy && m_session->status() == QCamera::ActiveStatus) {
        if (m_state == QCamera::LoadedState) {
            //handle delayed stop() because of busy camera
            m_resourcePolicy->setResourceSet(CamerabinResourcePolicy::LoadedResources);
            m_session->setState(QCamera::LoadedState);
        } else if (m_state == QCamera::ActiveState && m_reloadPending) {
            //handle delayed reload because of busy camera
            m_session->setState(QCamera::LoadedState);
            QMetaObject::invokeMethod(this, "delayedReload", Qt::QueuedConnection);
        }
    }
}

void CameraBinControl::handleCameraError(int errorCode, const QString &errorString)
{
    emit error(errorCode, errorString);
    setState(QCamera::UnloadedState);
}

void CameraBinControl::delayedReload()
{
#ifdef CAMEABIN_DEBUG
    qDebug() << "CameraBinControl: reload pipeline";
#endif
    if (m_reloadPending) {
        m_reloadPending = false;
        if (m_state == QCamera::ActiveState &&
            m_session->isReady() &&
            m_resourcePolicy->isResourcesGranted()) {
                m_session->setState(QCamera::ActiveState);
        }
    }
}

bool CameraBinControl::canChangeProperty(PropertyChangeType changeType, QCamera::Status status) const
{
    Q_UNUSED(status);

    switch (changeType) {
    case QCameraControl::Viewfinder:
        return true;
    case QCameraControl::CaptureMode:
    case QCameraControl::ImageEncodingSettings:
    case QCameraControl::VideoEncodingSettings:
    case QCameraControl::ViewfinderSettings:
    default:
        return status != QCamera::ActiveStatus;
    }
}

#define VIEWFINDER_COLORSPACE_CONVERSION 0x00000004

bool CameraBinControl::viewfinderColorSpaceConversion() const
{
    gint flags = 0;
    g_object_get(G_OBJECT(m_session->cameraBin()), "flags", &flags, NULL);

    return flags & VIEWFINDER_COLORSPACE_CONVERSION;
}

void CameraBinControl::setViewfinderColorSpaceConversion(bool enabled)
{
    gint flags = 0;
    g_object_get(G_OBJECT(m_session->cameraBin()), "flags", &flags, NULL);

    if (enabled)
        flags |= VIEWFINDER_COLORSPACE_CONVERSION;
    else
        flags &= ~VIEWFINDER_COLORSPACE_CONVERSION;

    g_object_set(G_OBJECT(m_session->cameraBin()), "flags", flags, NULL);
}

QT_END_NAMESPACE
