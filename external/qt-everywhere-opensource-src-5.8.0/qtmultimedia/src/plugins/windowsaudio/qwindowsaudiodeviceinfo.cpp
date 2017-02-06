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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// INTERNAL USE ONLY: Do NOT use for any other purpose.
//


#include <QtCore/qt_windows.h>
#include <QtCore/QDataStream>
#include <mmsystem.h>
#include "qwindowsaudiodeviceinfo.h"
#include "qwindowsaudioutils.h"

#if defined(Q_CC_MINGW) && !defined(__MINGW64_VERSION_MAJOR)
struct IBaseFilter; // Needed for strmif.h from stock MinGW.
struct _DDPIXELFORMAT;
typedef struct _DDPIXELFORMAT* LPDDPIXELFORMAT;
#endif

#include <strmif.h>
#if !defined(Q_CC_MINGW) || defined(__MINGW64_VERSION_MAJOR)
#  include <uuids.h>
#else

extern GUID CLSID_AudioInputDeviceCategory;
extern GUID CLSID_AudioRendererCategory;
extern GUID IID_ICreateDevEnum;
extern GUID CLSID_SystemDeviceEnum;

#ifndef __ICreateDevEnum_INTERFACE_DEFINED__
#define __ICreateDevEnum_INTERFACE_DEFINED__

DECLARE_INTERFACE_(ICreateDevEnum, IUnknown)
{
    STDMETHOD(CreateClassEnumerator)(REFCLSID clsidDeviceClass,
                                     IEnumMoniker **ppEnumMoniker,
                                     DWORD dwFlags) PURE;
};

#endif //  __ICreateDevEnum_INTERFACE_DEFINED__

#ifndef __IErrorLog_INTERFACE_DEFINED__
#define __IErrorLog_INTERFACE_DEFINED__

DECLARE_INTERFACE_(IErrorLog, IUnknown)
{
    STDMETHOD(AddError)(THIS_ LPCOLESTR, EXCEPINFO *) PURE;
};

#endif /* __IErrorLog_INTERFACE_DEFINED__ */

#ifndef __IPropertyBag_INTERFACE_DEFINED__
#define __IPropertyBag_INTERFACE_DEFINED__

const GUID IID_IPropertyBag = {0x55272A00, 0x42CB, 0x11CE, {0x81, 0x35, 0x00, 0xAA, 0x00, 0x4B, 0xB8, 0x51}};

DECLARE_INTERFACE_(IPropertyBag, IUnknown)
{
    STDMETHOD(Read)(THIS_ LPCOLESTR, VARIANT *, IErrorLog *) PURE;
    STDMETHOD(Write)(THIS_ LPCOLESTR, VARIANT *) PURE;
};

#endif /* __IPropertyBag_INTERFACE_DEFINED__ */

#endif // defined(Q_CC_MINGW) && !defined(__MINGW64_VERSION_MAJOR)

QT_BEGIN_NAMESPACE

// For mingw toolchain mmsystem.h only defines half the defines, so add if needed.
#ifndef WAVE_FORMAT_44M08
#define WAVE_FORMAT_44M08 0x00000100
#define WAVE_FORMAT_44S08 0x00000200
#define WAVE_FORMAT_44M16 0x00000400
#define WAVE_FORMAT_44S16 0x00000800
#define WAVE_FORMAT_48M08 0x00001000
#define WAVE_FORMAT_48S08 0x00002000
#define WAVE_FORMAT_48M16 0x00004000
#define WAVE_FORMAT_48S16 0x00008000
#define WAVE_FORMAT_96M08 0x00010000
#define WAVE_FORMAT_96S08 0x00020000
#define WAVE_FORMAT_96M16 0x00040000
#define WAVE_FORMAT_96S16 0x00080000
#endif


QWindowsAudioDeviceInfo::QWindowsAudioDeviceInfo(QByteArray dev, QAudio::Mode mode)
{
    QDataStream ds(&dev, QIODevice::ReadOnly);
    ds >> devId >> device;
    this->mode = mode;

    updateLists();
}

QWindowsAudioDeviceInfo::~QWindowsAudioDeviceInfo()
{
    close();
}

bool QWindowsAudioDeviceInfo::isFormatSupported(const QAudioFormat& format) const
{
    return testSettings(format);
}

QAudioFormat QWindowsAudioDeviceInfo::preferredFormat() const
{
    QAudioFormat nearest;
    if (mode == QAudio::AudioOutput) {
        nearest.setSampleRate(44100);
        nearest.setChannelCount(2);
        nearest.setByteOrder(QAudioFormat::LittleEndian);
        nearest.setSampleType(QAudioFormat::SignedInt);
        nearest.setSampleSize(16);
        nearest.setCodec(QLatin1String("audio/pcm"));
    } else {
        nearest.setSampleRate(11025);
        nearest.setChannelCount(1);
        nearest.setByteOrder(QAudioFormat::LittleEndian);
        nearest.setSampleType(QAudioFormat::SignedInt);
        nearest.setSampleSize(8);
        nearest.setCodec(QLatin1String("audio/pcm"));
    }
    return nearest;
}

QString QWindowsAudioDeviceInfo::deviceName() const
{
    return device;
}

QStringList QWindowsAudioDeviceInfo::supportedCodecs()
{
    return QStringList() << QStringLiteral("audio/pcm");
}

QList<int> QWindowsAudioDeviceInfo::supportedSampleRates()
{
    updateLists();
    return sampleRatez;
}

QList<int> QWindowsAudioDeviceInfo::supportedChannelCounts()
{
    updateLists();
    return channelz;
}

QList<int> QWindowsAudioDeviceInfo::supportedSampleSizes()
{
    updateLists();
    return sizez;
}

QList<QAudioFormat::Endian> QWindowsAudioDeviceInfo::supportedByteOrders()
{
    return QList<QAudioFormat::Endian>() << QAudioFormat::LittleEndian;
}

QList<QAudioFormat::SampleType> QWindowsAudioDeviceInfo::supportedSampleTypes()
{
    updateLists();
    return typez;
}


bool QWindowsAudioDeviceInfo::open()
{
    return true;
}

void QWindowsAudioDeviceInfo::close()
{
}

bool QWindowsAudioDeviceInfo::testSettings(const QAudioFormat& format) const
{
    WAVEFORMATEXTENSIBLE wfx;
    if (qt_convertFormat(format, &wfx)) {
        // query only, do not open device
        if (mode == QAudio::AudioOutput) {
            return (waveOutOpen(NULL, UINT_PTR(devId), &wfx.Format, 0, 0,
                                WAVE_FORMAT_QUERY) == MMSYSERR_NOERROR);
        } else { // AudioInput
            return (waveInOpen(NULL, UINT_PTR(devId), &wfx.Format, 0, 0,
                                WAVE_FORMAT_QUERY) == MMSYSERR_NOERROR);
        }
    }

    return false;
}

void QWindowsAudioDeviceInfo::updateLists()
{
    if (!sizez.isEmpty())
        return;

    bool hasCaps = false;
    DWORD fmt = 0;

    if(mode == QAudio::AudioOutput) {
        WAVEOUTCAPS woc;
        if (waveOutGetDevCaps(devId, &woc, sizeof(WAVEOUTCAPS)) == MMSYSERR_NOERROR) {
            hasCaps = true;
            fmt = woc.dwFormats;
        }
    } else {
        WAVEINCAPS woc;
        if (waveInGetDevCaps(devId, &woc, sizeof(WAVEINCAPS)) == MMSYSERR_NOERROR) {
            hasCaps = true;
            fmt = woc.dwFormats;
        }
    }

    sizez.clear();
    sampleRatez.clear();
    channelz.clear();
    typez.clear();

    if (hasCaps) {
        // Check sample size
        if ((fmt & WAVE_FORMAT_1M08)
            || (fmt & WAVE_FORMAT_1S08)
            || (fmt & WAVE_FORMAT_2M08)
            || (fmt & WAVE_FORMAT_2S08)
            || (fmt & WAVE_FORMAT_4M08)
            || (fmt & WAVE_FORMAT_4S08)
            || (fmt & WAVE_FORMAT_48M08)
            || (fmt & WAVE_FORMAT_48S08)
            || (fmt & WAVE_FORMAT_96M08)
            || (fmt & WAVE_FORMAT_96S08)) {
            sizez.append(8);
        }
        if ((fmt & WAVE_FORMAT_1M16)
            || (fmt & WAVE_FORMAT_1S16)
            || (fmt & WAVE_FORMAT_2M16)
            || (fmt & WAVE_FORMAT_2S16)
            || (fmt & WAVE_FORMAT_4M16)
            || (fmt & WAVE_FORMAT_4S16)
            || (fmt & WAVE_FORMAT_48M16)
            || (fmt & WAVE_FORMAT_48S16)
            || (fmt & WAVE_FORMAT_96M16)
            || (fmt & WAVE_FORMAT_96S16)) {
            sizez.append(16);
        }

        // Check sample rate
        if ((fmt & WAVE_FORMAT_1M08)
           || (fmt & WAVE_FORMAT_1S08)
           || (fmt & WAVE_FORMAT_1M16)
           || (fmt & WAVE_FORMAT_1S16)) {
            sampleRatez.append(11025);
        }
        if ((fmt & WAVE_FORMAT_2M08)
           || (fmt & WAVE_FORMAT_2S08)
           || (fmt & WAVE_FORMAT_2M16)
           || (fmt & WAVE_FORMAT_2S16)) {
            sampleRatez.append(22050);
        }
        if ((fmt & WAVE_FORMAT_4M08)
           || (fmt & WAVE_FORMAT_4S08)
           || (fmt & WAVE_FORMAT_4M16)
           || (fmt & WAVE_FORMAT_4S16)) {
            sampleRatez.append(44100);
        }
        if ((fmt & WAVE_FORMAT_48M08)
            || (fmt & WAVE_FORMAT_48S08)
            || (fmt & WAVE_FORMAT_48M16)
            || (fmt & WAVE_FORMAT_48S16)) {
            sampleRatez.append(48000);
        }
        if ((fmt & WAVE_FORMAT_96M08)
           || (fmt & WAVE_FORMAT_96S08)
           || (fmt & WAVE_FORMAT_96M16)
           || (fmt & WAVE_FORMAT_96S16)) {
            sampleRatez.append(96000);
        }

        // Check channel count
        if (fmt & WAVE_FORMAT_1M08
                || fmt & WAVE_FORMAT_1M16
                || fmt & WAVE_FORMAT_2M08
                || fmt & WAVE_FORMAT_2M16
                || fmt & WAVE_FORMAT_4M08
                || fmt & WAVE_FORMAT_4M16
                || fmt & WAVE_FORMAT_48M08
                || fmt & WAVE_FORMAT_48M16
                || fmt & WAVE_FORMAT_96M08
                || fmt & WAVE_FORMAT_96M16) {
            channelz.append(1);
        }
        if (fmt & WAVE_FORMAT_1S08
                || fmt & WAVE_FORMAT_1S16
                || fmt & WAVE_FORMAT_2S08
                || fmt & WAVE_FORMAT_2S16
                || fmt & WAVE_FORMAT_4S08
                || fmt & WAVE_FORMAT_4S16
                || fmt & WAVE_FORMAT_48S08
                || fmt & WAVE_FORMAT_48S16
                || fmt & WAVE_FORMAT_96S08
                || fmt & WAVE_FORMAT_96S16) {
            channelz.append(2);
        }

        typez.append(QAudioFormat::SignedInt);
        typez.append(QAudioFormat::UnSignedInt);

        // WAVEOUTCAPS and WAVEINCAPS contains information only for the previously tested parameters.
        // WaveOut and WaveInt might actually support more formats, the only way to know is to try
        // opening the device with it.
        QAudioFormat testFormat;
        testFormat.setCodec(QStringLiteral("audio/pcm"));
        testFormat.setByteOrder(QAudioFormat::LittleEndian);
        testFormat.setSampleType(QAudioFormat::SignedInt);
        testFormat.setChannelCount(channelz.first());
        testFormat.setSampleRate(sampleRatez.at(sampleRatez.size() / 2));
        testFormat.setSampleSize(sizez.last());
        const QAudioFormat defaultTestFormat(testFormat);

        // Check if float samples are supported
        testFormat.setSampleType(QAudioFormat::Float);
        testFormat.setSampleSize(32);
        if (testSettings(testFormat))
            typez.append(QAudioFormat::Float);

        // Check channel counts > 2
        testFormat = defaultTestFormat;
        for (int i = 3; i < 19; ++i) { // <mmreg.h> defines 18 different channels
            testFormat.setChannelCount(i);
            if (testSettings(testFormat))
                channelz.append(i);
        }

        // Check more sample sizes
        testFormat = defaultTestFormat;
        const QList<int> testSampleSizes = QList<int>() << 24 << 32 << 48 << 64;
        for (int s : testSampleSizes) {
            testFormat.setSampleSize(s);
            if (testSettings(testFormat))
                sizez.append(s);
        }

        // Check more sample rates
        testFormat = defaultTestFormat;
        const QList<int> testSampleRates = QList<int>() << 8000 << 16000 << 32000 << 88200 << 192000;
        for (int r : testSampleRates) {
            testFormat.setSampleRate(r);
            if (testSettings(testFormat))
                sampleRatez.append(r);
        }
        std::sort(sampleRatez.begin(), sampleRatez.end());
    }
}

QList<QByteArray> QWindowsAudioDeviceInfo::availableDevices(QAudio::Mode mode)
{
    Q_UNUSED(mode)

    QList<QByteArray> devices;
    //enumerate device fullnames through directshow api
    CoInitialize(NULL);
    ICreateDevEnum *pDevEnum = NULL;
    IEnumMoniker *pEnum = NULL;
    // Create the System device enumerator
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
                 CLSCTX_INPROC_SERVER, IID_ICreateDevEnum,
                 reinterpret_cast<void **>(&pDevEnum));

    unsigned long iNumDevs = mode == QAudio::AudioOutput ? waveOutGetNumDevs() : waveInGetNumDevs();
    if (SUCCEEDED(hr)) {
        // Create the enumerator for the audio input/output category
        if (pDevEnum->CreateClassEnumerator(
             mode == QAudio::AudioOutput ? CLSID_AudioRendererCategory : CLSID_AudioInputDeviceCategory,
             &pEnum, 0) == S_OK) {
            pEnum->Reset();
            // go through and find all audio devices
            IMoniker *pMoniker = NULL;
            while (pEnum->Next(1, &pMoniker, NULL) == S_OK) {
                IPropertyBag *pPropBag;
                hr = pMoniker->BindToStorage(0,0,IID_IPropertyBag,
                     reinterpret_cast<void **>(&pPropBag));
                if (FAILED(hr)) {
                    pMoniker->Release();
                    continue; // skip this one
                }
                // Find if it is a wave device
                VARIANT var;
                VariantInit(&var);
                hr = pPropBag->Read(mode == QAudio::AudioOutput ? L"WaveOutID" : L"WaveInID", &var, 0);
                if (SUCCEEDED(hr)) {
                    LONG waveID = var.lVal;
                    if (waveID >= 0 && waveID < LONG(iNumDevs)) {
                        VariantClear(&var);
                        // Find the description
                        hr = pPropBag->Read(L"FriendlyName", &var, 0);
                        if (SUCCEEDED(hr)) {
                            QByteArray  device;
                            QDataStream ds(&device, QIODevice::WriteOnly);
                            ds << quint32(waveID) << QString::fromWCharArray(var.bstrVal);
                            devices.append(device);
                        }
                    }
                }

                pPropBag->Release();
                pMoniker->Release();
            }
            pEnum->Release();
        }
        pDevEnum->Release();
    }
    CoUninitialize();

    return devices;
}

QByteArray QWindowsAudioDeviceInfo::defaultDevice(QAudio::Mode mode)
{
    const QString &name = (mode == QAudio::AudioOutput) ? QStringLiteral("Default Output Device")
                                                        : QStringLiteral("Default Input Device");
    QByteArray defaultDevice;
    QDataStream ds(&defaultDevice, QIODevice::WriteOnly);
    ds << quint32(WAVE_MAPPER) // device ID for default device
       << name;

    return defaultDevice;
}

QT_END_NAMESPACE
