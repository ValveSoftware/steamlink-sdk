/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "qwasapiutils.h"

#include <QtCore/QCoreApplication>
#include <QtCore/private/qeventdispatcher_winrt_p.h>

// For Desktop Win32 support
#ifdef CLASSIC_APP_BUILD
#define Q_OS_WINRT
#endif
#include <QtCore/qfunctions_winrt.h>

#include <QtMultimedia/QAudioDeviceInfo>
#include <Audioclient.h>
#include <windows.devices.enumeration.h>
#include <windows.foundation.collections.h>
#include <windows.media.devices.h>

#include <functional>

using namespace ABI::Windows::Devices::Enumeration;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Media::Devices;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define RETURN_EMPTY_LIST_IF_FAILED(msg) RETURN_IF_FAILED(msg, return QList<QByteArray>())

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcMmAudioInterface, "qt.multimedia.audiointerface")
Q_LOGGING_CATEGORY(lcMmUtils, "qt.multimedia.utils")

#ifdef CLASSIC_APP_BUILD
// Opening bracket has to be in the same line as MSVC2013 and 2015 complain on
// different lines otherwise
#pragma warning (suppress: 4273)
HRESULT QEventDispatcherWinRT::runOnXamlThread(const std::function<HRESULT()> &delegate, bool waitForRun) {
    Q_UNUSED(waitForRun)
    return delegate();
}
#endif

namespace QWasapiUtils {
struct DeviceMapping {
    QList<QByteArray> outputDeviceNames;
    QList<QString> outputDeviceIds;
    QList<QByteArray> inputDeviceNames;
    QList<QString> inputDeviceIds;
};
Q_GLOBAL_STATIC(DeviceMapping, gMapping)
}

AudioInterface::AudioInterface()
{
    qCDebug(lcMmAudioInterface) << __FUNCTION__;
    m_currentState = Initialized;
}

AudioInterface::~AudioInterface()
{
    qCDebug(lcMmAudioInterface) << __FUNCTION__;
}

HRESULT AudioInterface::ActivateCompleted(IActivateAudioInterfaceAsyncOperation *op)
{
    qCDebug(lcMmAudioInterface) << __FUNCTION__;

    IUnknown *aInterface;
    HRESULT hr;
    HRESULT hrActivate;
    hr = op->GetActivateResult(&hrActivate, &aInterface);
    if (FAILED(hr) || FAILED(hrActivate)) {
        qCDebug(lcMmAudioInterface) << __FUNCTION__ << "Could not query activate results.";
        m_currentState = Error;
        return hr;
    }

    hr = aInterface->QueryInterface(IID_PPV_ARGS(&m_client));
    if (FAILED(hr)) {
        qCDebug(lcMmAudioInterface) << __FUNCTION__ << "Could not access AudioClient interface.";
        m_currentState = Error;
        return hr;
    }

    WAVEFORMATEX *format;
    hr = m_client->GetMixFormat(&format);
    if (FAILED(hr)) {
        qCDebug(lcMmAudioInterface) << __FUNCTION__ << "Could not get mix format.";
        m_currentState = Error;
        return hr;
    }

    QWasapiUtils::convertFromNativeFormat(format, &m_mixFormat);

    m_currentState = Activated;
    return S_OK;
}

bool QWasapiUtils::convertToNativeFormat(const QAudioFormat &qt, WAVEFORMATEX *native)
{
    if (!native
            || !qt.isValid()
            || qt.codec() != QStringLiteral("audio/pcm")
            || qt.sampleRate() <= 0
            || qt.channelCount() <= 0
            || qt.sampleSize() <= 0
            || qt.byteOrder() != QAudioFormat::LittleEndian) {
        return false;
    }

    native->nSamplesPerSec = qt.sampleRate();
    native->wBitsPerSample = qt.sampleSize();
    native->nChannels = qt.channelCount();
    native->nBlockAlign = (native->wBitsPerSample * native->nChannels) / 8;
    native->nAvgBytesPerSec = native->nBlockAlign * native->nSamplesPerSec;
    native->cbSize = 0;

    if (qt.sampleType() == QAudioFormat::Float)
        native->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    else
        native->wFormatTag = WAVE_FORMAT_PCM;

    return true;
}

bool QWasapiUtils::convertFromNativeFormat(const WAVEFORMATEX *native, QAudioFormat *qt)
{
    if (!native || !qt)
        return false;

    qt->setByteOrder(QAudioFormat::LittleEndian);
    qt->setChannelCount(native->nChannels);
    qt->setCodec(QStringLiteral("audio/pcm"));
    qt->setSampleRate(native->nSamplesPerSec);
    qt->setSampleSize(native->wBitsPerSample);
    qt->setSampleType(native->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ? QAudioFormat::Float : QAudioFormat::SignedInt);

    return true;
}

QByteArray QWasapiUtils::defaultDevice(QAudio::Mode mode)
{
    qCDebug(lcMmUtils) << __FUNCTION__ << mode;

    QList<QByteArray> &deviceNames = mode == QAudio::AudioInput ? gMapping->inputDeviceNames : gMapping->outputDeviceNames;
    QList<QString> &deviceIds = mode == QAudio::AudioInput ? gMapping->inputDeviceIds : gMapping->outputDeviceIds;
    if (deviceNames.isEmpty() || deviceIds.isEmpty()) // Initialize
        availableDevices(mode);
    if (deviceNames.isEmpty() || deviceIds.isEmpty()) // No audio devices at all
        return QByteArray();

    ComPtr<IMediaDeviceStatics> mediaDeviceStatics;
    HRESULT hr;

    hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Media_Devices_MediaDevice).Get(), &mediaDeviceStatics);
    Q_ASSERT_SUCCEEDED(hr);

    HString defaultAudioDevice;
    quint32 dADSize = 0;

    if (mode == QAudio::AudioOutput)
        hr = mediaDeviceStatics->GetDefaultAudioRenderId(AudioDeviceRole_Default, defaultAudioDevice.GetAddressOf());
    else
        hr = mediaDeviceStatics->GetDefaultAudioCaptureId(AudioDeviceRole_Default, defaultAudioDevice.GetAddressOf());

    const wchar_t *dadWStr = defaultAudioDevice.GetRawBuffer(&dADSize);
    const QString defaultAudioDeviceId = QString::fromWCharArray(dadWStr, dADSize);
    Q_ASSERT(deviceIds.indexOf(defaultAudioDeviceId) != -1);

    return deviceNames.at(deviceIds.indexOf(defaultAudioDeviceId));
}

QList<QByteArray> QWasapiUtils::availableDevices(QAudio::Mode mode)
{
    qCDebug(lcMmUtils) << __FUNCTION__ << mode;

    ComPtr<IDeviceInformationStatics> statics;
    HRESULT hr;

    hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Devices_Enumeration_DeviceInformation).Get(),
                              &statics);
    Q_ASSERT_SUCCEEDED(hr);

    DeviceClass dc = mode == QAudio::AudioInput ? DeviceClass_AudioCapture : DeviceClass_AudioRender;

    QList<QByteArray> &deviceNames = mode == QAudio::AudioInput ? gMapping->inputDeviceNames : gMapping->outputDeviceNames;
    QList<QString> &deviceIds = mode == QAudio::AudioInput ? gMapping->inputDeviceIds : gMapping->outputDeviceIds;

    // We need to refresh due to plugable devices (ie USB)
    deviceNames.clear();
    deviceIds.clear();

    ComPtr<IAsyncOperation<ABI::Windows::Devices::Enumeration::DeviceInformationCollection *>> op;
    hr = statics->FindAllAsyncDeviceClass(dc, &op );
    RETURN_EMPTY_LIST_IF_FAILED("Could not query audio devices.");

    ComPtr<IVectorView<DeviceInformation *>> resultVector;
    hr = QWinRTFunctions::await(op, resultVector.GetAddressOf());
    RETURN_EMPTY_LIST_IF_FAILED("Could not receive audio device list.");

    quint32 deviceCount;
    hr = resultVector->get_Size(&deviceCount);
    RETURN_EMPTY_LIST_IF_FAILED("Could not access audio device count.");
    qCDebug(lcMmUtils) << "Found " << deviceCount << " audio devices for" << mode;

    for (quint32 i = 0; i < deviceCount; ++i) {
        ComPtr<IDeviceInformation> item;
        hr = resultVector->GetAt(i, &item);
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Could not access audio device item.");
            continue;
        }

        HString hString;
        quint32 size;

        hr = item->get_Name(hString.GetAddressOf());
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Could not access audio device name.");
            continue;
        }
        const wchar_t *nameWStr = hString.GetRawBuffer(&size);
        const QString deviceName = QString::fromWCharArray(nameWStr, size);

        hr = item->get_Id(hString.GetAddressOf());
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Could not access audio device id.");
            continue;
        }
        const wchar_t *idWStr = hString.GetRawBuffer(&size);
        const QString deviceId = QString::fromWCharArray(idWStr, size);

        boolean enabled;
        hr = item->get_IsEnabled(&enabled);
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Could not access audio device enabled.");
            continue;
        }

        qCDebug(lcMmUtils) << "Audio Device:" << deviceName << " ID:" << deviceId
                            << " Enabled:" << enabled;

        deviceNames.append(deviceName.toLocal8Bit());
        deviceIds.append(deviceId);
    }
    return deviceNames;
}

Microsoft::WRL::ComPtr<AudioInterface> QWasapiUtils::createOrGetInterface(const QByteArray &dev, QAudio::Mode mode)
{
    qCDebug(lcMmUtils) << __FUNCTION__ << dev << mode;
    Q_ASSERT((mode == QAudio::AudioInput ? gMapping->inputDeviceNames.indexOf(dev) : gMapping->outputDeviceNames.indexOf(dev)) != -1);

    Microsoft::WRL::ComPtr<AudioInterface> result;
    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([dev, mode, &result]() {
        HRESULT hr;
        QString id = mode == QAudio::AudioInput ? gMapping->inputDeviceIds.at(gMapping->inputDeviceNames.indexOf(dev)) :
                                                  gMapping->outputDeviceIds.at(gMapping->outputDeviceNames.indexOf(dev));

        result = Make<AudioInterface>();

        ComPtr<IActivateAudioInterfaceAsyncOperation> op;

        // We cannot use QWinRTFunctions::await here as that will return
        // E_NO_INTERFACE. Instead we leave the lambda and wait for the
        // status to get out of Activating
        result->setState(AudioInterface::Activating);
        hr = ActivateAudioInterfaceAsync(reinterpret_cast<LPCWSTR>(id.utf16()), __uuidof(IAudioClient), NULL, result.Get(), op.GetAddressOf());
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Could not invoke audio interface activation.");
            result->setState(AudioInterface::Error);
        }
        return hr;
    });
    qCDebug(lcMmUtils) << "Activation stated:" << hr;
    while (result->state() == AudioInterface::Activating) {
        QThread::yieldCurrentThread();
    }
    qCDebug(lcMmUtils) << "Activation done:" << hr;
    return result;
}

QT_END_NAMESPACE
