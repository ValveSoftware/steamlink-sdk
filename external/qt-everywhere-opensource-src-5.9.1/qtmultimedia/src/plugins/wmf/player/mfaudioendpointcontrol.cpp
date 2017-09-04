/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
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

#include "QtCore/qdebug.h"
#include "mfaudioendpointcontrol.h"

#include <mmdeviceapi.h>

MFAudioEndpointControl::MFAudioEndpointControl(QObject *parent)
    : QAudioOutputSelectorControl(parent)
    , m_currentActivate(0)
{
}

MFAudioEndpointControl::~MFAudioEndpointControl()
{
    clear();
}

void MFAudioEndpointControl::clear()
{
    m_activeEndpoint.clear();

    for (auto it = m_devices.cbegin(), end = m_devices.cend(); it != end; ++it)
         CoTaskMemFree(it.value());

    m_devices.clear();

    if (m_currentActivate)
        m_currentActivate->Release();
    m_currentActivate = NULL;
}

QList<QString> MFAudioEndpointControl::availableOutputs() const
{
    return m_devices.keys();
}

QString MFAudioEndpointControl::outputDescription(const QString &name) const
{
    return name.section(QLatin1Char('\\'), -1);
}

QString MFAudioEndpointControl::defaultOutput() const
{
    return m_defaultEndpoint;
}

QString MFAudioEndpointControl::activeOutput() const
{
    return m_activeEndpoint;
}

void MFAudioEndpointControl::setActiveOutput(const QString &name)
{
    if (m_activeEndpoint == name)
        return;
    QMap<QString, LPWSTR>::iterator it = m_devices.find(name);
    if (it == m_devices.end())
        return;

    LPWSTR wstrID = *it;
    IMFActivate *activate = NULL;
    HRESULT hr = MFCreateAudioRendererActivate(&activate);
    if (FAILED(hr)) {
        qWarning() << "Failed to create audio renderer activate";
        return;
    }

    if (wstrID) {
        hr = activate->SetString(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ID, wstrID);
    } else {
        //This is the default one that has been inserted in updateEndpoints(),
        //so give the activate a hint that we want to use the device for multimedia playback
        //then the media foundation will choose an appropriate one.

        //from MSDN:
        //The ERole enumeration defines constants that indicate the role that the system has assigned to an audio endpoint device.
        //eMultimedia: Music, movies, narration, and live music recording.
        hr = activate->SetUINT32(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ROLE, eMultimedia);
    }

    if (FAILED(hr)) {
        qWarning() << "Failed to set attribute for audio device" << name;
        return;
    }

    if (m_currentActivate)
        m_currentActivate->Release();
    m_currentActivate = activate;
    m_activeEndpoint = name;
}

IMFActivate*  MFAudioEndpointControl::createActivate()
{
    clear();

    updateEndpoints();

    // Check if an endpoint is available ("Default" is always inserted)
    if (m_devices.count() <= 1)
        return NULL;

    setActiveOutput(m_defaultEndpoint);

    return m_currentActivate;
}

void MFAudioEndpointControl::updateEndpoints()
{
    m_defaultEndpoint = QString::fromLatin1("Default");
    m_devices.insert(m_defaultEndpoint, NULL);

    IMMDeviceEnumerator *pEnum = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                          NULL, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                         (void**)&pEnum);
    if (SUCCEEDED(hr)) {
        IMMDeviceCollection *pDevices = NULL;
        hr = pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices);
        if (SUCCEEDED(hr)) {
            UINT count;
            hr = pDevices->GetCount(&count);
            if (SUCCEEDED(hr)) {
                for (UINT i = 0; i < count; ++i) {
                    IMMDevice *pDevice = NULL;
                    hr = pDevices->Item(i, &pDevice);
                    if (SUCCEEDED(hr)) {
                        LPWSTR wstrID = NULL;
                        hr = pDevice->GetId(&wstrID);
                        if (SUCCEEDED(hr)) {
                            QString deviceId = QString::fromWCharArray(wstrID);
                            m_devices.insert(deviceId, wstrID);
                        }
                        pDevice->Release();
                    }
                }
            }
            pDevices->Release();
        }
        pEnum->Release();
    }
}
