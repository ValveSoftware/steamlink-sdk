/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include <AVFoundation/AVFoundation.h>

#include "qtexttospeech_ios.h"

@interface QIOSSpeechSynthesizerDelegate : NSObject <AVSpeechSynthesizerDelegate> {
    QTextToSpeechEngineIos *_engine;
}
@end

@implementation QIOSSpeechSynthesizerDelegate

- (id)initWithQIOSTextToSpeechEngineIos:(QTextToSpeechEngineIos *)engine
{
    if (self = [super init]) {
        _engine = engine;
    }
    return self;
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didCancelSpeechUtterance:(AVSpeechUtterance *)utterance
{
    Q_UNUSED(synthesizer);
    Q_UNUSED(utterance);
    _engine->setState(QTextToSpeech::Ready);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didContinueSpeechUtterance:(AVSpeechUtterance *)utterance
{
    Q_UNUSED(synthesizer);
    Q_UNUSED(utterance);
    _engine->setState(QTextToSpeech::Speaking);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didFinishSpeechUtterance:(AVSpeechUtterance *)utterance
{
    Q_UNUSED(synthesizer);
    Q_UNUSED(utterance);
    _engine->setState(QTextToSpeech::Ready);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didPauseSpeechUtterance:(AVSpeechUtterance *)utterance
{
    Q_UNUSED(synthesizer);
    Q_UNUSED(utterance);
    _engine->setState(QTextToSpeech::Paused);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didStartSpeechUtterance:(AVSpeechUtterance *)utterance
{
    Q_UNUSED(synthesizer);
    Q_UNUSED(utterance);
    _engine->setState(QTextToSpeech::Speaking);
}

@end

// -------------------------------------------------------------------------

QT_BEGIN_NAMESPACE

QTextToSpeechEngineIos::QTextToSpeechEngineIos(const QVariantMap &/*parameters*/, QObject *parent)
    : QTextToSpeechEngine(parent)
    , m_speechSynthesizer([AVSpeechSynthesizer new])
    , m_locale(QLocale())
    , m_voice(QVoice())
    , m_state(QTextToSpeech::Ready)
    , m_pitch(0)
    , m_rate(0)
    , m_volume(1)
{
    m_speechSynthesizer.delegate = [[QIOSSpeechSynthesizerDelegate alloc] initWithQIOSTextToSpeechEngineIos:this];
}

QTextToSpeechEngineIos::~QTextToSpeechEngineIos()
{
    [m_speechSynthesizer.delegate autorelease];
    [m_speechSynthesizer release];
}

void QTextToSpeechEngineIos::say(const QString &text)
{
    stop();

    AVSpeechUtterance *utterance = [AVSpeechUtterance speechUtteranceWithString:text.toNSString()];
    utterance.volume = m_volume;
    utterance.voice = fromQVoice(m_voice);
    // Pitch: Qt range: [-1.0, 1.0], iOS range: [0.5, 2.0]
    utterance.pitchMultiplier = 0.5 + ((m_pitch + 1.0) / 2.0 * 1.5);

    if (m_rate >= 0) {
        // The QtTextToSpeech documentation states that a rate of 0.0 represents normal speech flow.
        // To map that to AVSpeechUtteranceDefaultSpeechRate while at the same time preserve the Qt
        // range [-1.0, 1.0], we choose to operate with two differente rate convertions; one for
        // values in the range [-1, 0), and for [0, 1].
        float range = AVSpeechUtteranceMaximumSpeechRate - AVSpeechUtteranceDefaultSpeechRate;
        utterance.rate = AVSpeechUtteranceDefaultSpeechRate + (m_rate * range);
    } else {
        float range = AVSpeechUtteranceDefaultSpeechRate - AVSpeechUtteranceMinimumSpeechRate;
        utterance.rate = AVSpeechUtteranceMinimumSpeechRate + (m_rate * range);
    }

    [m_speechSynthesizer speakUtterance:utterance];
}

void QTextToSpeechEngineIos::stop()
{
    [m_speechSynthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
}

void QTextToSpeechEngineIos::pause()
{
    [m_speechSynthesizer pauseSpeakingAtBoundary:AVSpeechBoundaryWord];
}

void QTextToSpeechEngineIos::resume()
{
    [m_speechSynthesizer continueSpeaking];
}

bool QTextToSpeechEngineIos::setRate(double rate)
{
    m_rate = rate;
    return true;
}

double QTextToSpeechEngineIos::rate() const
{
    return m_rate;
}

bool QTextToSpeechEngineIos::setPitch(double pitch)
{
    m_pitch = pitch;
    return true;
}

double QTextToSpeechEngineIos::pitch() const
{
    return m_pitch;
}

bool QTextToSpeechEngineIos::setVolume(double volume)
{
    m_volume = volume;
    return true;
}

double QTextToSpeechEngineIos::volume() const
{
    return m_volume;
}

QVector<QLocale> QTextToSpeechEngineIos::availableLocales() const
{
    QVector<QLocale> localeVector;
    QString prevVoiceLanguage;
    for (AVSpeechSynthesisVoice *voice in [AVSpeechSynthesisVoice speechVoices]) {
        QString language = QString::fromNSString(voice.language);
        // Filter out languages already added. A language will occur several times
        // in sequence if more than one voice name exists for them.
        if (language == prevVoiceLanguage)
            continue;
        localeVector << QLocale(language);
        prevVoiceLanguage = language;
    }

    return localeVector;
}

bool QTextToSpeechEngineIos::setLocale(const QLocale &locale)
{
    NSString *bcp47 = locale.bcp47Name().toNSString();
    AVSpeechSynthesisVoice *defaultAvVoice = [AVSpeechSynthesisVoice voiceWithLanguage:bcp47];
    if (!defaultAvVoice)
        return false;

    m_locale = locale;
    m_voice = toQVoice(defaultAvVoice);
    return true;
}

QLocale QTextToSpeechEngineIos::locale() const
{
    return m_locale;
}

QVector<QVoice> QTextToSpeechEngineIos::availableVoices() const
{
    QVector<QVoice> voiceVector;
    QString countryCode = m_locale.name().mid(3);

    for (AVSpeechSynthesisVoice *avVoice in [AVSpeechSynthesisVoice speechVoices]) {
        if (QString::fromNSString(avVoice.language).endsWith(countryCode))
            voiceVector << toQVoice(avVoice);
    }

    return voiceVector;
}

bool QTextToSpeechEngineIos::setVoice(const QVoice &voice)
{
    AVSpeechSynthesisVoice *avVoice = fromQVoice(voice);
    if (!avVoice)
        return false;

    m_voice = voice;
    return true;
}

QVoice QTextToSpeechEngineIos::voice() const
{
    return m_voice;
}

AVSpeechSynthesisVoice *QTextToSpeechEngineIos::fromQVoice(const QVoice &voice) const
{
    for (AVSpeechSynthesisVoice *avVoice in [AVSpeechSynthesisVoice speechVoices]) {
        if (voice.name() == QString::fromNSString(avVoice.name))
            return avVoice;
    }

    return Q_NULLPTR;
}

QVoice QTextToSpeechEngineIos::toQVoice(AVSpeechSynthesisVoice *avVoice) const
{
    return createVoice(QString::fromNSString(avVoice.name), QVoice::Unknown, QVoice::Other, QVariant());
}

void QTextToSpeechEngineIos::setState(QTextToSpeech::State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(m_state);
}

QTextToSpeech::State QTextToSpeechEngineIos::state() const
{
    return m_state;
}



QT_END_NAMESPACE
