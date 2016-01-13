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

#include "directshowaudioendpointcontrol.h"

#include "directshowglobal.h"
#include "directshowplayerservice.h"

DirectShowAudioEndpointControl::DirectShowAudioEndpointControl(
        DirectShowPlayerService *service, QObject *parent)
    : QAudioOutputSelectorControl(parent)
    , m_service(service)
    , m_bindContext(0)
    , m_deviceEnumerator(0)
{
    if (CreateBindCtx(0, &m_bindContext) == S_OK) {
        m_deviceEnumerator = com_new<ICreateDevEnum>(CLSID_SystemDeviceEnum, IID_ICreateDevEnum);

        updateEndpoints();

        setActiveOutput(m_defaultEndpoint);
    }
}

DirectShowAudioEndpointControl::~DirectShowAudioEndpointControl()
{
    foreach (IMoniker *moniker, m_devices)
        moniker->Release();

    if (m_bindContext)
        m_bindContext->Release();

    if (m_deviceEnumerator)
        m_deviceEnumerator->Release();
}

QList<QString> DirectShowAudioEndpointControl::availableOutputs() const
{
    return m_devices.keys();
}

QString DirectShowAudioEndpointControl::outputDescription(const QString &name) const
{
#ifdef __IPropertyBag_INTERFACE_DEFINED__
    QString description;

    if (IMoniker *moniker = m_devices.value(name, 0)) {
        IPropertyBag *propertyBag = 0;
        if (SUCCEEDED(moniker->BindToStorage(
                0, 0, IID_IPropertyBag, reinterpret_cast<void **>(&propertyBag)))) {
            VARIANT name;
            VariantInit(&name);
            if (SUCCEEDED(propertyBag->Read(L"FriendlyName", &name, 0)))
                description = QString::fromWCharArray(name.bstrVal);
            VariantClear(&name);
            propertyBag->Release();
        }
    }

    return description;
#else
    return name.section(QLatin1Char('\\'), -1);
#endif
}

QString DirectShowAudioEndpointControl::defaultOutput() const
{
    return m_defaultEndpoint;
}

QString DirectShowAudioEndpointControl::activeOutput() const
{
    return m_activeEndpoint;
}

void DirectShowAudioEndpointControl::setActiveOutput(const QString &name)
{
    if (m_activeEndpoint == name)
        return;

    if (IMoniker *moniker = m_devices.value(name, 0)) {
        IBaseFilter *filter = 0;

        if (moniker->BindToObject(
                m_bindContext,
                0,
                IID_IBaseFilter,
                reinterpret_cast<void **>(&filter)) == S_OK) {
            m_service->setAudioOutput(filter);

            filter->Release();
        }
    }
}

void DirectShowAudioEndpointControl::updateEndpoints()
{
    IMalloc *oleMalloc = 0;
    if (m_deviceEnumerator && CoGetMalloc(1, &oleMalloc) == S_OK) {
        IEnumMoniker *monikers = 0;

        if (m_deviceEnumerator->CreateClassEnumerator(
                CLSID_AudioRendererCategory, &monikers, 0) == S_OK) {
            for (IMoniker *moniker = 0; monikers->Next(1, &moniker, 0) == S_OK; moniker->Release()) {
                OLECHAR *string = 0;
                if (moniker->GetDisplayName(m_bindContext, 0, &string) == S_OK) {
                    QString deviceId = QString::fromWCharArray(string);
                    oleMalloc->Free(string);

                    moniker->AddRef();
                    m_devices.insert(deviceId, moniker);

                    if (m_defaultEndpoint.isEmpty()
                            || deviceId.endsWith(QLatin1String("Default DirectSound Device"))) {
                        m_defaultEndpoint = deviceId;
                    }
                }
            }
            monikers->Release();
        }
        oleMalloc->Release();
    }
}
