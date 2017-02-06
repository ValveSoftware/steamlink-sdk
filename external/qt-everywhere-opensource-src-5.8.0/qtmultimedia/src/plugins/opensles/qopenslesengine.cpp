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

#include "qopenslesengine.h"

#include "qopenslesaudioinput.h"
#include <qdebug.h>

#ifdef ANDROID
#include <SLES/OpenSLES_Android.h>
#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/private/qjni_p.h>
#endif

#define MINIMUM_PERIOD_TIME_MS 5
#define DEFAULT_PERIOD_TIME_MS 50

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

QByteArray QOpenSLESEngine::defaultDevice(QAudio::Mode mode) const
{
    const auto &devices = availableDevices(mode);
    return !devices.isEmpty() ? devices.first() : QByteArray();
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

int QOpenSLESEngine::getOutputValue(QOpenSLESEngine::OutputValue type, int defaultValue)
{
#if defined(Q_OS_ANDROID)
    static int sampleRate = 0;
    static int framesPerBuffer = 0;
    static const int sdkVersion = QtAndroidPrivate::androidSdkVersion();

    if (sdkVersion < 17) // getProperty() was added in API level 17...
        return defaultValue;

    if (type == FramesPerBuffer && framesPerBuffer != 0)
        return framesPerBuffer;

    if (type == SampleRate && sampleRate != 0)
        return sampleRate;

    QJNIObjectPrivate ctx(QtAndroidPrivate::activity());
    if (!ctx.isValid())
        return defaultValue;


    QJNIObjectPrivate audioServiceString = ctx.getStaticObjectField("android/content/Context",
                                                                    "AUDIO_SERVICE",
                                                                    "Ljava/lang/String;");
    QJNIObjectPrivate am = ctx.callObjectMethod("getSystemService",
                                                "(Ljava/lang/String;)Ljava/lang/Object;",
                                                audioServiceString.object());
    if (!am.isValid())
        return defaultValue;

    QJNIObjectPrivate sampleRateField = QJNIObjectPrivate::getStaticObjectField("android/media/AudioManager",
                                                                                "PROPERTY_OUTPUT_SAMPLE_RATE",
                                                                                "Ljava/lang/String;");
    QJNIObjectPrivate framesPerBufferField = QJNIObjectPrivate::getStaticObjectField("android/media/AudioManager",
                                                                                     "PROPERTY_OUTPUT_FRAMES_PER_BUFFER",
                                                                                     "Ljava/lang/String;");

    QJNIObjectPrivate sampleRateString = am.callObjectMethod("getProperty",
                                                             "(Ljava/lang/String;)Ljava/lang/String;",
                                                             sampleRateField.object());
    QJNIObjectPrivate framesPerBufferString = am.callObjectMethod("getProperty",
                                                                  "(Ljava/lang/String;)Ljava/lang/String;",
                                                                  framesPerBufferField.object());

    if (!sampleRateString.isValid() || !framesPerBufferString.isValid())
        return defaultValue;

    framesPerBuffer = framesPerBufferString.toString().toInt();
    sampleRate = sampleRateString.toString().toInt();

    if (type == FramesPerBuffer)
        return framesPerBuffer;

    if (type == SampleRate)
        return sampleRate;

#endif // Q_OS_ANDROID

    return defaultValue;
}

int QOpenSLESEngine::getDefaultBufferSize(const QAudioFormat &format)
{
#if defined(Q_OS_ANDROID)
    if (!format.isValid())
        return 0;

    const int channelConfig = [&format]() -> int
    {
        if (format.channelCount() == 1)
            return 4; /* MONO */
        else if (format.channelCount() == 2)
            return 12; /* STEREO */
        else if (format.channelCount() > 2)
            return 1052; /* SURROUND */
        else
            return 1; /* DEFAULT */
    }();

    const int audioFormat = [&format]() -> int
    {
        if (format.sampleType() == QAudioFormat::Float && QtAndroidPrivate::androidSdkVersion() >= 21)
            return 4; /* PCM_FLOAT */
        else if (format.sampleSize() == 8)
            return 3; /* PCM_8BIT */
        else if (format.sampleSize() == 16)
            return 2; /* PCM_16BIT*/
        else
            return 1; /* DEFAULT */
    }();

    const int sampleRate = format.sampleRate();
    return QJNIObjectPrivate::callStaticMethod<jint>("android/media/AudioTrack",
                                                     "getMinBufferSize",
                                                     "(III)I",
                                                     sampleRate,
                                                     channelConfig,
                                                     audioFormat);
#else
    return format.bytesForDuration(DEFAULT_PERIOD_TIME_MS);
#endif // Q_OS_ANDROID
}

int QOpenSLESEngine::getLowLatencyBufferSize(const QAudioFormat &format)
{
    return format.bytesForFrames(QOpenSLESEngine::getOutputValue(QOpenSLESEngine::FramesPerBuffer,
                                                                 format.framesForDuration(MINIMUM_PERIOD_TIME_MS)));
}

bool QOpenSLESEngine::supportsLowLatency()
{
#if defined(Q_OS_ANDROID)
    static int isSupported = -1;

    if (isSupported != -1)
        return (isSupported == 1);

    QJNIObjectPrivate ctx(QtAndroidPrivate::activity());
    if (!ctx.isValid())
        return false;

    QJNIObjectPrivate pm = ctx.callObjectMethod("getPackageManager", "()Landroid/content/pm/PackageManager;");
    if (!pm.isValid())
        return false;

    QJNIObjectPrivate audioFeatureField = QJNIObjectPrivate::getStaticObjectField("android/content/pm/PackageManager",
                                                                                  "FEATURE_AUDIO_LOW_LATENCY",
                                                                                  "Ljava/lang/String;");
    if (!audioFeatureField.isValid())
        return false;

    isSupported = pm.callMethod<jboolean>("hasSystemFeature",
                                          "(Ljava/lang/String;)Z",
                                          audioFeatureField.object());
    return (isSupported == 1);
#else
    return true;
#endif // Q_OS_ANDROID
}

bool QOpenSLESEngine::printDebugInfo()
{
    return qEnvironmentVariableIsSet("QT_OPENSL_INFO");
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
