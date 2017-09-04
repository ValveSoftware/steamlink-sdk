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

#include "qtexttospeech_flite.h"

QT_BEGIN_NAMESPACE

QTextToSpeechEngineFlite::QTextToSpeechEngineFlite(
    const QVariantMap &parameters, QObject *parent) :
        QTextToSpeechEngine(parent),
        m_state(QTextToSpeech::Ready),
        m_processor(QTextToSpeechProcessorFlite::instance())
{
    Q_UNUSED(parameters);
}

QTextToSpeechEngineFlite::~QTextToSpeechEngineFlite()
{
}

QVector<QLocale> QTextToSpeechEngineFlite::availableLocales() const
{
    return m_locales;
}

QVector<QVoice> QTextToSpeechEngineFlite::availableVoices() const
{
    return m_voices.values(m_currentLocale.name()).toVector();
}

void QTextToSpeechEngineFlite::say(const QString &text)
{
    int id = QTextToSpeechEngine::voiceData(m_currentVoice).toInt();
    m_state = QTextToSpeech::Speaking;
    emit stateChanged(m_state);
    m_processor->say(text, id);
}

void QTextToSpeechEngineFlite::stop()
{
    m_processor->stop();
    m_state = QTextToSpeech::Ready;
    emit stateChanged(m_state);
}

void QTextToSpeechEngineFlite::pause()
{
    if (m_state == QTextToSpeech::Speaking) {
        m_processor->pause();
        m_state = QTextToSpeech::Paused;
        emit stateChanged(m_state);
    }
}

void QTextToSpeechEngineFlite::resume()
{
    if (m_state == QTextToSpeech::Paused) {
        m_processor->resume();
        m_state = QTextToSpeech::Speaking;
        emit stateChanged(m_state);
    }
}

double QTextToSpeechEngineFlite::rate() const
{
    return m_processor->rate();
}

bool QTextToSpeechEngineFlite::setRate(double rate)
{
    return m_processor->setRate(rate);
}

double QTextToSpeechEngineFlite::pitch() const
{
    return m_processor->pitch();
}

bool QTextToSpeechEngineFlite::setPitch(double pitch)
{
    return m_processor->setPitch(pitch);
}

QLocale QTextToSpeechEngineFlite::locale() const
{
    return m_currentLocale;
}

bool QTextToSpeechEngineFlite::setLocale(const QLocale &locale)
{
    bool localeFound = false;
    foreach (const QLocale &l, m_locales) {
        if (l.name() == locale.name()) {
            localeFound = true;
            break;
        }
    }
    if (!localeFound)
        return false;
    if (m_currentLocale.name() != locale.name()) {
        m_currentLocale = locale;
        m_currentVoice = availableVoices().at(0);
    }
    return true;
}

double QTextToSpeechEngineFlite::volume() const
{
    return m_processor->volume();
}

bool QTextToSpeechEngineFlite::setVolume(double volume)
{
    return m_processor->setVolume(volume);
}

QVoice QTextToSpeechEngineFlite::voice() const
{
    return m_currentVoice;
}

bool QTextToSpeechEngineFlite::setVoice(const QVoice &voice)
{
    foreach (const QVoice &availableVoice, availableVoices()) {
        if (QTextToSpeechEngine::voiceData(availableVoice) == QTextToSpeechEngine::voiceData(voice)) {
            m_currentVoice = voice;
            return true;
        }
    }
    return false;
}

QTextToSpeech::State QTextToSpeechEngineFlite::state() const
{
    return m_state;
}

bool QTextToSpeechEngineFlite::init(QString *errorString)
{
    int i = 0;
    const QVector<QTextToSpeechProcessor::VoiceInfo> &voices = m_processor->voices();
    foreach (const QTextToSpeechProcessor::VoiceInfo &voiceInfo, voices) {
        QString name = voiceInfo.name;
        QLocale locale(voiceInfo.locale);
        QVoice voice = QTextToSpeechEngine::createVoice(name, voiceInfo.gender, voiceInfo.age,
                                                        QVariant(voiceInfo.id));
        m_voices.insert(voiceInfo.locale, voice);
        if (!m_locales.contains(locale))
            m_locales.append(locale);
        // Use the first available locale/voice as a fallback
        if (i == 0) {
            m_currentVoice = voice;
            m_currentLocale = locale;
        }
        i++;
    }
    // Attempt to switch to the system locale
    setLocale(QLocale::system());
    connect(m_processor.data(), &QTextToSpeechProcessor::notSpeaking,
            this, &QTextToSpeechEngineFlite::onNotSpeaking);
    if (errorString)
        *errorString = QString();
    return true;
}

void QTextToSpeechEngineFlite::onNotSpeaking(int statusCode)
{
    Q_UNUSED(statusCode);
    if (m_state != QTextToSpeech::Ready && m_processor->isIdle()) {
        m_state = QTextToSpeech::Ready;
        emit stateChanged(m_state);
    }
}

QT_END_NAMESPACE
