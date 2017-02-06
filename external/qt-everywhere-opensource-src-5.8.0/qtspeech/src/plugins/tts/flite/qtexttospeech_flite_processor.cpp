/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Speech module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtexttospeech_flite_processor.h"
#include "qtexttospeech_flite_plugin.h"

#include <QtCore/QString>
#include <QtCore/QLocale>
#include <QtCore/QMap>

#include <flite/flite.h>

// en_US voice:
extern "C" cst_voice *register_cmu_us_kal16();
extern "C" void unregister_cmu_us_kal16(cst_voice *vox);

QT_BEGIN_NAMESPACE

QWeakPointer<QTextToSpeechProcessorFlite> QTextToSpeechProcessorFlite::m_instance;
QMutex QTextToSpeechProcessorFlite::m_instanceLock;

QSharedPointer<QTextToSpeechProcessorFlite> QTextToSpeechProcessorFlite::instance()
{
    QSharedPointer<QTextToSpeechProcessorFlite> inst = m_instance.toStrongRef();
    if (inst.isNull()) {
        QMutexLocker lock(&m_instanceLock);
        inst = m_instance.toStrongRef();
        if (inst.isNull()) {
            inst = QSharedPointer<QTextToSpeechProcessorFlite>(new QTextToSpeechProcessorFlite());
            m_instance = inst;
        }
    }
    Q_ASSERT(inst);
    Q_ASSERT(inst == m_instance);
    return inst;
}

QTextToSpeechProcessorFlite::QTextToSpeechProcessorFlite():
    m_initialized(false),
    m_currentVoice(-1)
{
    if (init()) {
        m_initialized = true;
        start();
    } else {
        deinit();
    }
}

QTextToSpeechProcessorFlite::~QTextToSpeechProcessorFlite()
{
    if (m_initialized) {
        exit();
        deinit();
    }
}

const QVector<QTextToSpeechProcessor::VoiceInfo> &QTextToSpeechProcessorFlite::voices() const
{
    return m_voices;
}

int QTextToSpeechProcessorFlite::fliteOutputCb(const cst_wave *w, int start, int size,
    int last, cst_audio_streaming_info *asi)
{
    QTextToSpeechProcessorFlite *processor = static_cast<QTextToSpeechProcessorFlite *>(asi->userdata);
    if (processor)
        return processor->fliteOutput(w, start, size, last, asi);
    return CST_AUDIO_STREAM_STOP;
}

int QTextToSpeechProcessorFlite::fliteOutput(const cst_wave *w, int start, int size,
                int last, cst_audio_streaming_info *asi)
{
    Q_UNUSED(asi);
    QString errorString;
    if (start == 0) {
        if (!audioStart(w->sample_rate, w->num_channels, &errorString)) {
            if (!errorString.isEmpty())
                qCCritical(lcSpeechTtsFlite) << errorString;
            return CST_AUDIO_STREAM_STOP;
        }
    }
    int bytesToWrite = size * sizeof(short);
    if (!audioOutput((const char *)(&w->samples[start]), bytesToWrite, &errorString)) {
        if (!errorString.isEmpty())
            qCCritical(lcSpeechTtsFlite) << errorString;
        audioStop(true); // Abort audio output
        return CST_AUDIO_STREAM_STOP;
    }
    if (last == 1)
        audioStop();
    return CST_AUDIO_STREAM_CONT;
}

int QTextToSpeechProcessorFlite::processText(const QString &text, int voiceId)
{
    qCDebug(lcSpeechTtsFlite) << "processText() begin";
    if (voiceId >= 0 && voiceId < m_fliteVoices.size()) {
        const FliteVoice &voiceInfo = m_fliteVoices.at(voiceId);
        cst_voice *voice = voiceInfo.vox;
        cst_audio_streaming_info *asi = new_audio_streaming_info();
        asi->asc = QTextToSpeechProcessorFlite::fliteOutputCb;
        asi->userdata = (void *)this;
        feat_set(voice->features, "streaming_info", audio_streaming_info_val(asi));
        setRateForVoice(voice, rate());
        setPitchForVoice(voice, pitch());
        flite_text_to_speech(text.toUtf8().constData(), voice, "none");
    }
    qCDebug(lcSpeechTtsFlite) << "processText() end";
    return 0;
}

void QTextToSpeechProcessorFlite::setRateForVoice(cst_voice *voice, float rate)
{
    float stretch = 1.0;
    Q_ASSERT(rate >= -1.0 && rate <= 1.0);
    // Stretch multipliers taken from Speech Dispatcher
    if (rate < 0)
        stretch -= rate * 2;
    if (rate > 0)
        stretch -= rate * (100.0 / 175.0);
    feat_set_float(voice->features, "duration_stretch", stretch);
}

void QTextToSpeechProcessorFlite::setPitchForVoice(cst_voice *voice, float pitch)
{
    float f0;
    Q_ASSERT(pitch >= -1.0 && pitch <= 1.0);
    // Conversion taken from Speech Dispatcher
    f0 = (pitch * 80) + 100;
    feat_set_float(voice->features, "int_f0_target_mean", f0);
}

bool QTextToSpeechProcessorFlite::init()
{
    flite_init();
    FliteVoice voice_enus = {
        register_cmu_us_kal16(),
        unregister_cmu_us_kal16,
        "kal16",
        QLocale(QLocale::English, QLocale::UnitedStates).name(),
        QVoice::Male,
        QVoice::Adult
    };
    m_fliteVoices.append(voice_enus);

    int totalVoiceCount = 0;
    foreach (const FliteVoice &voice, m_fliteVoices) {
        QTextToSpeechProcessor::VoiceInfo voiceInfo;
        voiceInfo.name = voice.name;
        voiceInfo.locale = voice.locale;
        voiceInfo.age = voice.age;
        voiceInfo.gender = voice.gender;
        voiceInfo.id = totalVoiceCount;
        m_voices.append(voiceInfo);
        totalVoiceCount++;
    }
    return true;
}

void QTextToSpeechProcessorFlite::deinit()
{
    foreach (const FliteVoice &voice, m_fliteVoices)
        voice.unregister_func(voice.vox);
    m_fliteVoices.clear();
    m_voices.clear();
}

QT_END_NAMESPACE
