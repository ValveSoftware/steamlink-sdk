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

#include <Cocoa/Cocoa.h>
#include "qtexttospeech_osx.h"
#include <qdebug.h>

@interface QT_MANGLE_NAMESPACE(StateDelegate) : NSObject <NSSpeechSynthesizerDelegate>
{
    QT_PREPEND_NAMESPACE(QTextToSpeechEngineOsx) *speechPrivate;
}
@end

@implementation QT_MANGLE_NAMESPACE(StateDelegate)
- (id)initWithSpeechPrivate:(QTextToSpeechEngineOsx *) priv {
    self = [super init];
    speechPrivate = priv;
    return self;
}
- (void)speechSynthesizer:(NSSpeechSynthesizer *)sender didFinishSpeaking:(BOOL)success {
    Q_UNUSED(sender);
    speechPrivate->speechStopped(success);
}
@end

QT_BEGIN_NAMESPACE

QTextToSpeechEngineOsx::QTextToSpeechEngineOsx(const QVariantMap &/*parameters*/, QObject *parent)
    : QTextToSpeechEngine(parent), m_state(QTextToSpeech::Ready)
{
    stateDelegate = [[QT_MANGLE_NAMESPACE(StateDelegate) alloc] initWithSpeechPrivate:this];

    speechSynthesizer = [[NSSpeechSynthesizer alloc] init];
    [speechSynthesizer setDelegate: stateDelegate];
    updateVoices();
}

QTextToSpeechEngineOsx::~QTextToSpeechEngineOsx()
{
    [speechSynthesizer release];
    [stateDelegate release];
}


QTextToSpeech::State QTextToSpeechEngineOsx::state() const
{
    return m_state;
}

bool QTextToSpeechEngineOsx::isSpeaking() const
{
    return [speechSynthesizer isSpeaking];
}

void QTextToSpeechEngineOsx::speechStopped(bool success)
{
    Q_UNUSED(success);
    if (m_state != QTextToSpeech::Ready) {
        m_state = QTextToSpeech::Ready;
        emit stateChanged(m_state);
    }
}

void QTextToSpeechEngineOsx::say(const QString &text)
{
    if (text.isEmpty())
        return;

    if (m_state != QTextToSpeech::Ready)
        stop();

    if([speechSynthesizer isSpeaking]) {
        [speechSynthesizer stopSpeaking];
    }

    NSString *ntext = text.toNSString();
    [speechSynthesizer startSpeakingString:ntext];

    if (m_state != QTextToSpeech::Speaking) {
        m_state = QTextToSpeech::Speaking;
        emit stateChanged(m_state);
    }
}

void QTextToSpeechEngineOsx::stop()
{
    if([speechSynthesizer isSpeaking])
        [speechSynthesizer stopSpeaking];
}

void QTextToSpeechEngineOsx::pause()
{
    if ([speechSynthesizer isSpeaking]) {
        [speechSynthesizer pauseSpeakingAtBoundary: NSSpeechWordBoundary];
        m_state = QTextToSpeech::Paused;
        emit stateChanged(m_state);
    }
}

bool QTextToSpeechEngineOsx::isPaused() const
{
    return m_state == QTextToSpeech::Paused;
}

void QTextToSpeechEngineOsx::resume()
{
    m_state = QTextToSpeech::Speaking;
    emit stateChanged(m_state);
    [speechSynthesizer continueSpeaking];
}

double QTextToSpeechEngineOsx::rate() const
{
    return ([speechSynthesizer rate] - 200) / 200.0;
}

bool QTextToSpeechEngineOsx::setPitch(double pitch)
{
    // 30 to 65
    double p = 30.0 + ((pitch + 1.0) / 2.0) * 35.0;
    [speechSynthesizer setObject:[NSNumber numberWithFloat:p] forProperty:NSSpeechPitchBaseProperty error:nil];
    return true;
}

double QTextToSpeechEngineOsx::pitch() const
{
    double pitch = [[speechSynthesizer objectForProperty:NSSpeechPitchBaseProperty error:nil] floatValue];
    return (pitch - 30.0) / 35.0 * 2.0 - 1.0;
}

double QTextToSpeechEngineOsx::volume() const
{
    return [speechSynthesizer volume];
}

bool QTextToSpeechEngineOsx::setRate(double rate)
{
    // NSSpeechSynthesizer supports words per minute,
    // human speech is 180 to 220 - use 0 to 400 as range here
    [speechSynthesizer setRate: 200 + (rate * 200)];
    return true;
}

bool QTextToSpeechEngineOsx::setVolume(double volume)
{
    [speechSynthesizer setVolume: volume];
    return true;
}

QLocale localeForVoice(NSString *voice)
{
    NSDictionary *attrs = [NSSpeechSynthesizer attributesForVoice:voice];
    return QString::fromNSString(attrs[NSVoiceLocaleIdentifier]);
}

QVoice QTextToSpeechEngineOsx::voiceForNSVoice(NSString *voiceString) const
{
    NSDictionary *attrs = [NSSpeechSynthesizer attributesForVoice:voiceString];
    QString voiceName = QString::fromNSString(attrs[NSVoiceName]);

    NSString *genderString = attrs[NSVoiceGender];
    QVoice::Gender gender = [genderString isEqualToString:NSVoiceGenderMale] ? QVoice::Male :
                            [genderString isEqualToString:NSVoiceGenderFemale] ? QVoice::Female :
                            QVoice::Unknown;

    NSNumber *ageNSNumber = attrs[NSVoiceAge];
    int ageInt = ageNSNumber.intValue;
    QVoice::Age age = (ageInt < 13 ? QVoice::Child :
                       ageInt < 20 ? QVoice::Teenager :
                       ageInt < 45 ? QVoice::Adult :
                       ageInt < 90 ? QVoice::Senior : QVoice::Other);
    QVariant data = QString::fromNSString(attrs[NSVoiceIdentifier]);
    QVoice voice = createVoice(voiceName, gender, age, data);
    return voice;
}

QVector<QLocale> QTextToSpeechEngineOsx::availableLocales() const
{
    return m_locales;
}

bool QTextToSpeechEngineOsx::setLocale(const QLocale &locale)
{
    NSArray *voices = [NSSpeechSynthesizer availableVoices];
    NSString *voice = [NSSpeechSynthesizer defaultVoice];
    // always prefer default
    if (locale == localeForVoice(voice)) {
        [speechSynthesizer setVoice:voice];
        return true;
    }

    for (voice in voices) {
        QLocale voiceLocale = localeForVoice(voice);
        if (locale == voiceLocale) {
            [speechSynthesizer setVoice:voice];
            return true;
        }
    }
    return false;
}

QLocale QTextToSpeechEngineOsx::locale() const
{
    NSString *voice = [speechSynthesizer voice];
    return localeForVoice(voice);
}

void QTextToSpeechEngineOsx::updateVoices()
{
    NSArray *voices = [NSSpeechSynthesizer availableVoices];
    for (NSString *voice in voices) {
        QLocale locale = localeForVoice(voice);
        QVoice data = voiceForNSVoice(voice);
        if (!m_locales.contains(locale))
            m_locales.append(locale);
        m_voices.insert(locale.name(), data);
    }
}

QVector<QVoice> QTextToSpeechEngineOsx::availableVoices() const
{
    return m_voices.values(locale().name()).toVector();
}

bool QTextToSpeechEngineOsx::setVoice(const QVoice &voice)
{
    NSString *identifier = voiceData(voice).toString().toNSString();
    [speechSynthesizer setVoice:identifier];
    return true;
}

QVoice QTextToSpeechEngineOsx::voice() const
{
    NSString *voice = [speechSynthesizer voice];
    return voiceForNSVoice(voice);
}

QT_END_NAMESPACE
