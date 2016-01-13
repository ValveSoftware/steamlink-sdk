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

#include <QtCore/qstring.h>
#include <QtCore/qdebug.h>
#include <QtCore/QFile>

#include "dsserviceplugin.h"
#include "dsvideodevicecontrol.h"

#ifdef QMEDIA_DIRECTSHOW_CAMERA
#include <dshow.h>
#include "dscameraservice.h"
#endif

#ifdef QMEDIA_DIRECTSHOW_PLAYER
#include "directshowplayerservice.h"
#endif

#include <qmediaserviceproviderplugin.h>


#ifdef QMEDIA_DIRECTSHOW_CAMERA

extern const CLSID CLSID_VideoInputDeviceCategory;


#ifndef _STRSAFE_H_INCLUDED_
#include <tchar.h>
#endif
#include <dshow.h>
#include <objbase.h>
#include <initguid.h>
#ifdef Q_CC_MSVC
#  pragma comment(lib, "strmiids.lib")
#  pragma comment(lib, "ole32.lib")
#endif // Q_CC_MSVC
#include <windows.h>
#include <ocidl.h>
#endif

QT_USE_NAMESPACE

static int g_refCount = 0;
void addRefCount()
{
    if (++g_refCount == 1)
        CoInitialize(NULL);
}

void releaseRefCount()
{
    if (--g_refCount == 0)
        CoUninitialize();
}

QMediaService* DSServicePlugin::create(QString const& key)
{
#ifdef QMEDIA_DIRECTSHOW_CAMERA
    if (key == QLatin1String(Q_MEDIASERVICE_CAMERA)) {
        addRefCount();
        return new DSCameraService;
    }
#endif
#ifdef QMEDIA_DIRECTSHOW_PLAYER
    if (key == QLatin1String(Q_MEDIASERVICE_MEDIAPLAYER)) {
        addRefCount();
        return new DirectShowPlayerService;
    }
#endif

    return 0;
}

void DSServicePlugin::release(QMediaService *service)
{
    delete service;
    releaseRefCount();
}

QMediaServiceProviderHint::Features DSServicePlugin::supportedFeatures(
        const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_MEDIAPLAYER)
        return QMediaServiceProviderHint::StreamPlayback | QMediaServiceProviderHint::VideoSurface;
    else
        return QMediaServiceProviderHint::Features();
}

QByteArray DSServicePlugin::defaultDevice(const QByteArray &service) const
{
#ifdef QMEDIA_DIRECTSHOW_CAMERA
    if (service == Q_MEDIASERVICE_CAMERA) {
        if (m_cameraDevices.isEmpty())
            updateDevices();

        return m_defaultCameraDevice;
    }
#endif

    return QByteArray();
}

QList<QByteArray> DSServicePlugin::devices(const QByteArray &service) const
{
#ifdef QMEDIA_DIRECTSHOW_CAMERA
    if (service == Q_MEDIASERVICE_CAMERA) {
        if (m_cameraDevices.isEmpty())
            updateDevices();

        return m_cameraDevices;
    }
#endif

    return QList<QByteArray>();
}

QString DSServicePlugin::deviceDescription(const QByteArray &service, const QByteArray &device)
{
#ifdef QMEDIA_DIRECTSHOW_CAMERA
    if (service == Q_MEDIASERVICE_CAMERA) {
        if (m_cameraDevices.isEmpty())
            updateDevices();

        for (int i=0; i<m_cameraDevices.count(); i++)
            if (m_cameraDevices[i] == device)
                return m_cameraDescriptions[i];
    }
#endif
    return QString();
}

#ifdef QMEDIA_DIRECTSHOW_CAMERA

void DSServicePlugin::updateDevices() const
{
    addRefCount();

    m_defaultCameraDevice.clear();
    DSVideoDeviceControl::enumerateDevices(&m_cameraDevices, &m_cameraDescriptions);

    if (m_cameraDevices.isEmpty()) {
        qWarning() << "No camera devices found";
    } else {
        m_defaultCameraDevice = m_cameraDevices.first();
    }

    releaseRefCount();
}
#endif

