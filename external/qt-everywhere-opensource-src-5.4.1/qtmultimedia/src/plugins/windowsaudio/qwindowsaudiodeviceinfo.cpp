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
#include <mmsystem.h>
#include "qwindowsaudiodeviceinfo.h"

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
    updateLists();
    return codecz;
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
    updateLists();
    return byteOrderz;
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
    // Set nearest to closest settings that do work.
    // See if what is in settings will work (return value).

    bool failed = false;
    bool match = false;

    // check codec
    for( int i = 0; i < codecz.count(); i++) {
        if (format.codec() == codecz.at(i))
            match = true;
    }
    if (!match) failed = true;

    // check channel
    match = false;
    if (!failed) {
        for (int i = 0; i < channelz.count(); i++) {
            if (format.channelCount() == channelz.at(i)) {
                match = true;
                break;
            }
        }
        if (!match)
            failed = true;
    }

    // check sampleRate
    match = false;
    if (!failed) {
        for (int i = 0; i < sampleRatez.count(); i++) {
            if (format.sampleRate() == sampleRatez.at(i)) {
                match = true;
                break;
            }
        }
        if (!match)
            failed = true;
    }

    // check sample size
    match = false;
    if (!failed) {
        for( int i = 0; i < sizez.count(); i++) {
            if (format.sampleSize() == sizez.at(i)) {
                match = true;
                break;
            }
        }
        if (!match)
            failed = true;
    }

    // check byte order
    match = false;
    if (!failed) {
        for( int i = 0; i < byteOrderz.count(); i++) {
            if (format.byteOrder() == byteOrderz.at(i)) {
                match = true;
                break;
            }
        }
        if (!match)
            failed = true;
    }

    // check sample type
    match = false;
    if (!failed) {
        for( int i = 0; i < typez.count(); i++) {
            if (format.sampleType() == typez.at(i)) {
                match = true;
                break;
            }
        }
        if (!match)
            failed = true;
    }

    if(!failed) {
        // settings work
        return true;
    }
    return false;
}

void QWindowsAudioDeviceInfo::updateLists()
{
    // redo all lists based on current settings
    bool match = false;
    DWORD fmt = 0;

    if(mode == QAudio::AudioOutput) {
        WAVEOUTCAPS woc;
        if (waveOutGetDevCaps(devId, &woc, sizeof(WAVEOUTCAPS)) == MMSYSERR_NOERROR) {
            match = true;
            fmt = woc.dwFormats;
        }
    } else {
        WAVEINCAPS woc;
        if (waveInGetDevCaps(devId, &woc, sizeof(WAVEINCAPS)) == MMSYSERR_NOERROR) {
            match = true;
            fmt = woc.dwFormats;
        }
    }
    sizez.clear();
    sampleRatez.clear();
    channelz.clear();
    byteOrderz.clear();
    typez.clear();
    codecz.clear();

    if(match) {
        if ((fmt & WAVE_FORMAT_1M08)
            || (fmt & WAVE_FORMAT_1S08)
            || (fmt & WAVE_FORMAT_2M08)
            || (fmt & WAVE_FORMAT_2S08)
            || (fmt & WAVE_FORMAT_4M08)
            || (fmt & WAVE_FORMAT_4S08)
            || (fmt & WAVE_FORMAT_48M08)
            || (fmt & WAVE_FORMAT_48S08)
            || (fmt & WAVE_FORMAT_96M08)
            || (fmt & WAVE_FORMAT_96S08)
       ) {
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
            || (fmt & WAVE_FORMAT_96S16)
       ) {
            sizez.append(16);
        }
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
        channelz.append(1);
        channelz.append(2);
        if (mode == QAudio::AudioOutput) {
            channelz.append(4);
            channelz.append(6);
            channelz.append(8);
        }

        byteOrderz.append(QAudioFormat::LittleEndian);

        typez.append(QAudioFormat::SignedInt);
        typez.append(QAudioFormat::UnSignedInt);

        codecz.append(QLatin1String("audio/pcm"));
    }
    if (sampleRatez.count() > 0)
        sampleRatez.prepend(8000);
}

QList<QByteArray> QWindowsAudioDeviceInfo::availableDevices(QAudio::Mode mode)
{
    Q_UNUSED(mode)

    QList<QByteArray> devices;
#ifndef Q_OS_WINCE
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
        }
    }
    CoUninitialize();
#else // Q_OS_WINCE
    if (mode == QAudio::AudioOutput) {
        WAVEOUTCAPS woc;
        unsigned long iNumDevs,i;
        iNumDevs = waveOutGetNumDevs();
        for (i=0;i<iNumDevs;i++) {
            if (waveOutGetDevCaps(i, &woc, sizeof(WAVEOUTCAPS))
                == MMSYSERR_NOERROR) {
                QByteArray  device;
                QDataStream ds(&device, QIODevice::WriteOnly);
                ds << quint32(i) << QString::fromWCharArray(woc.szPname);
                devices.append(device);
            }
        }
    } else {
        WAVEINCAPS woc;
        unsigned long iNumDevs,i;
        iNumDevs = waveInGetNumDevs();
        for (i=0;i<iNumDevs;i++) {
            if (waveInGetDevCaps(i, &woc, sizeof(WAVEINCAPS))
                == MMSYSERR_NOERROR) {
                QByteArray  device;
                QDataStream ds(&device, QIODevice::WriteOnly);
                ds << quint32(i) << QString::fromWCharArray(woc.szPname);
                devices.append(device);
            }
        }
    }
#endif // !Q_OS_WINCE

    return devices;
}

QByteArray QWindowsAudioDeviceInfo::defaultOutputDevice()
{
    QByteArray defaultDevice;
    QDataStream ds(&defaultDevice, QIODevice::WriteOnly);
    ds << quint32(WAVE_MAPPER) // device ID for default device
       << QStringLiteral("Default Output Device");

    return defaultDevice;
}

QByteArray QWindowsAudioDeviceInfo::defaultInputDevice()
{
    QByteArray defaultDevice;
    QDataStream ds(&defaultDevice, QIODevice::WriteOnly);
    ds << quint32(WAVE_MAPPER) // device ID for default device
       << QStringLiteral("Default Input Device");

    return defaultDevice;
}

QT_END_NAMESPACE
