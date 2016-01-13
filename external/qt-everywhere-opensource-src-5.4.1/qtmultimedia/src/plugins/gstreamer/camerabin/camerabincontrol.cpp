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
    m_status(QCamera::UnloadedStatus),
    m_reloadPending(false)
{
    connect(m_session, SIGNAL(stateChanged(QCamera::State)),
            this, SLOT(updateStatus()));

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
#if (GST_VERSION_MAJOR == 0) && ((GST_VERSION_MINOR < 10) || (GST_VERSION_MICRO < 23))
            //due to bug in v4l2src, it's necessary to reload camera on video caps changes
            //https://bugzilla.gnome.org/show_bug.cgi?id=649832
            reloadLater();
#endif
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
                m_session->state() == QCamera::ActiveState &&
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

void CameraBinControl::updateStatus()
{
    QCamera::State sessionState = m_session->state();
    QCamera::Status oldStatus = m_status;

    switch (m_state) {
    case QCamera::UnloadedState:
        m_status = QCamera::UnloadedStatus;
        break;
    case QCamera::LoadedState:
        switch (sessionState) {
        case QCamera::UnloadedState:
            m_status = m_resourcePolicy->isResourcesGranted()
                    ? QCamera::LoadingStatus
                    : QCamera::UnavailableStatus;
            break;
        case QCamera::LoadedState:
            m_status = QCamera::LoadedStatus;
            break;
        case QCamera::ActiveState:
            m_status = QCamera::ActiveStatus;
            break;
        }
        break;
    case QCamera::ActiveState:
        switch (sessionState) {
        case QCamera::UnloadedState:
            m_status = m_resourcePolicy->isResourcesGranted()
                    ? QCamera::LoadingStatus
                    : QCamera::UnavailableStatus;
            break;
        case QCamera::LoadedState:
            m_status = QCamera::StartingStatus;
            break;
        case QCamera::ActiveState:
            m_status = QCamera::ActiveStatus;
            break;
        }
    }

    if (m_status != oldStatus) {
#ifdef CAMEABIN_DEBUG
        qDebug() << "Camera status changed" << ENUM_NAME(QCamera, "Status", m_status);
#endif
        emit statusChanged(m_status);
    }
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
    if (!busy && m_session->state() == QCamera::ActiveState) {
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
    case QCameraControl::CaptureMode:
    case QCameraControl::ImageEncodingSettings:
    case QCameraControl::VideoEncodingSettings:
    case QCameraControl::Viewfinder:
        return true;
    default:
        return false;
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
