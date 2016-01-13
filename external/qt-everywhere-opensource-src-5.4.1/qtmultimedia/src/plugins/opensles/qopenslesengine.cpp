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

#include "qopenslesengine.h"

#include "qopenslesaudioinput.h"
#include <qdebug.h>

#ifdef ANDROID
#include <SLES/OpenSLES_Android.h>
#endif

#define CheckError(message) if (result != SL_RESULT_SUCCESS) { qWarning(message); return; }

Q_GLOBAL_STATIC(QOpenSLESEngine, openslesEngine);

QOpenSLESEngine::QOpenSLESEngine()
    : m_engineObject(0)
    , m_engine(0)
    , m_checkedInputFormats(false)
{
    SLresult result;

    result = slCreateEngine(&m_engineObject, 0, 0, 0, 0, 0);
    CheckError("Failed to create engine");

    result = (*m_engineObject)->Realize(m_engineObject, SL_BOOLEAN_FALSE);
    CheckError("Failed to realize engine");

    result = (*m_engineObject)->GetInterface(m_engineObject, SL_IID_ENGINE, &m_engine);
    CheckError("Failed to get engine interface");
}

QOpenSLESEngine::~QOpenSLESEngine()
{
    if (m_engineObject)
        (*m_engineObject)->Destroy(m_engineObject);
}

QOpenSLESEngine *QOpenSLESEngine::instance()
{
    return openslesEngine();
}

SLDataFormat_PCM QOpenSLESEngine::audioFormatToSLFormatPCM(const QAudioFormat &format)
{
    SLDataFormat_PCM format_pcm;
    format_pcm.formatType = SL_DATAFORMAT_PCM;
    format_pcm.numChannels = format.channelCount();
    format_pcm.samplesPerSec = format.sampleRate() * 1000;
    format_pcm.bitsPerSample = format.sampleSize();
    format_pcm.containerSize = format.sampleSize();
    format_pcm.channelMask = (format.channelCount() == 1 ?
                                  SL_SPEAKER_FRONT_CENTER :
                                  SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT);
    format_pcm.endianness = (format.byteOrder() == QAudioFormat::LittleEndian ?
                                 SL_BYTEORDER_LITTLEENDIAN :
                                 SL_BYTEORDER_BIGENDIAN);
    return format_pcm;

}

QList<QByteArray> QOpenSLESEngine::availableDevices(QAudio::Mode mode) const
{
    QList<QByteArray> devices;
    if (mode == QAudio::AudioInput) {
#ifdef ANDROID
        devices << QT_ANDROID_PRESET_MIC
                << QT_ANDROID_PRESET_CAMCORDER
                << QT_ANDROID_PRESET_VOICE_RECOGNITION;
#else
        devices << "default";
#endif
    } else {
        devices << "default";
    }
    return devices;
}

QList<int> QOpenSLESEngine::supportedChannelCounts(QAudio::Mode mode) const
{
    if (mode == QAudio::AudioInput) {
        if (!m_checkedInputFormats)
            const_cast<QOpenSLESEngine *>(this)->checkSupportedInputFormats();
        return m_supportedInputChannelCounts;
    } else {
        return QList<int>() << 1 << 2;
    }
}

QList<int> QOpenSLESEngine::supportedSampleRates(QAudio::Mode mode) const
{
    if (mode == QAudio::AudioInput) {
        if (!m_checkedInputFormats)
            const_cast<QOpenSLESEngine *>(this)->checkSupportedInputFormats();
        return m_supportedInputSampleRates;
    } else {
        return QList<int>() << 8000 << 11025 << 12000 << 16000 << 22050
                            << 24000 << 32000 << 44100 << 48000;
    }
}

void QOpenSLESEngine::checkSupportedInputFormats()
{
    m_supportedInputChannelCounts = QList<int>() << 1;
    m_supportedInputSampleRates.clear();

    SLDataFormat_PCM defaultFormat;
    defaultFormat.formatType = SL_DATAFORMAT_PCM;
    defaultFormat.numChannels = 1;
    defaultFormat.samplesPerSec = SL_SAMPLINGRATE_44_1;
    defaultFormat.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    defaultFormat.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
    defaultFormat.channelMask = SL_SPEAKER_FRONT_CENTER;
    defaultFormat.endianness = SL_BYTEORDER_LITTLEENDIAN;

    const SLuint32 rates[9] = { SL_SAMPLINGRATE_8,
                                SL_SAMPLINGRATE_11_025,
                                SL_SAMPLINGRATE_12,
                                SL_SAMPLINGRATE_16,
                                SL_SAMPLINGRATE_22_05,
                                SL_SAMPLINGRATE_24,
                                SL_SAMPLINGRATE_32,
                                SL_SAMPLINGRATE_44_1,
                                SL_SAMPLINGRATE_48 };


    // Test sampling rates
    for (int i = 0 ; i < 9; ++i) {
        SLDataFormat_PCM format = defaultFormat;
        format.samplesPerSec = rates[i];

        if (inputFormatIsSupported(format))
            m_supportedInputSampleRates.append(rates[i] / 1000);

    }

    // Test if stereo is supported
    {
        SLDataFormat_PCM format = defaultFormat;
        format.numChannels = 2;
        format.channelMask = 0;
        if (inputFormatIsSupported(format))
            m_supportedInputChannelCounts.append(2);
    }

    m_checkedInputFormats = true;
}

bool QOpenSLESEngine::inputFormatIsSupported(SLDataFormat_PCM format)
{
    SLresult result;
    SLObjectItf recorder = 0;
    SLDataLocator_IODevice loc_dev = { SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                       SL_DEFAULTDEVICEID_AUDIOINPUT, NULL };
    SLDataSource audioSrc = { &loc_dev, NULL };

#ifdef ANDROID
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1 };
#else
    SLDataLocator_BufferQueue loc_bq = { SL_DATALOCATOR_BUFFERQUEUE, 1 };
#endif
    SLDataSink audioSnk = { &loc_bq, &format };

    result = (*m_engine)->CreateAudioRecorder(m_engine, &recorder, &audioSrc, &audioSnk, 0, 0, 0);
    if (result == SL_RESULT_SUCCESS)
        result = (*recorder)->Realize(recorder, false);

    if (result == SL_RESULT_SUCCESS) {
        (*recorder)->Destroy(recorder);
        return true;
    }

    return false;
}
