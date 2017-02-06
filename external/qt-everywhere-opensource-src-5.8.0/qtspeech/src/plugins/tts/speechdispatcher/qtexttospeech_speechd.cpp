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


#include "qtexttospeech_speechd.h"

#include <qdebug.h>
#include <speech-dispatcher/libspeechd.h>

#if LIBSPEECHD_MAJOR_VERSION > 0 || LIBSPEECHD_MINOR_VERSION >= 9
  #define HAVE_SPD_090
#endif

QT_BEGIN_NAMESPACE

QString dummyModule = QStringLiteral("dummy");

typedef QList<QTextToSpeechEngineSpeechd*> QTextToSpeechSpeechDispatcherBackendList;
Q_GLOBAL_STATIC(QTextToSpeechSpeechDispatcherBackendList, backends)

void speech_finished_callback(size_t msg_id, size_t client_id, SPDNotificationType state);

QLocale QTextToSpeechEngineSpeechd::localeForVoice(SPDVoice *voice) const
{
    QString lang_var = QString::fromLatin1(voice->language);
    if (qstrcmp(voice->variant, "none") != 0) {
        QString var = QString::fromLatin1(voice->variant);
        lang_var += QLatin1Char('_') + var;
    }
    return QLocale(lang_var);
}

QTextToSpeechEngineSpeechd::QTextToSpeechEngineSpeechd(const QVariantMap &, QObject *)
    : speechDispatcher(0)
{
    backends->append(this);
    connectToSpeechDispatcher();
}

QTextToSpeechEngineSpeechd::~QTextToSpeechEngineSpeechd()
{
    if (speechDispatcher) {
        if ((m_state != QTextToSpeech::BackendError) && (m_state != QTextToSpeech::Ready))
            spd_cancel_all(speechDispatcher);
        spd_close(speechDispatcher);
    }
    backends->removeAll(this);
}

bool QTextToSpeechEngineSpeechd::connectToSpeechDispatcher()
{
    if (speechDispatcher)
        return true;

    speechDispatcher = spd_open("QTextToSpeech", "main", 0, SPD_MODE_THREADED);
    if (speechDispatcher) {
        speechDispatcher->callback_begin = speech_finished_callback;
        spd_set_notification_on(speechDispatcher, SPD_BEGIN);
        speechDispatcher->callback_end = speech_finished_callback;
        spd_set_notification_on(speechDispatcher, SPD_END);
        speechDispatcher->callback_cancel = speech_finished_callback;
        spd_set_notification_on(speechDispatcher, SPD_CANCEL);
        speechDispatcher->callback_resume = speech_finished_callback;
        spd_set_notification_on(speechDispatcher, SPD_RESUME);
        speechDispatcher->callback_pause = speech_finished_callback;
        spd_set_notification_on(speechDispatcher, SPD_PAUSE);

        QStringList availableModules;
        char **modules = spd_list_modules(speechDispatcher);
        int i = 0;
        while (modules && modules[i]) {
            availableModules.append(QString::fromUtf8(modules[i]));
            ++i;
        }

        if (availableModules.length() == 0) {
            qWarning() << "Found no modules in speech-dispatcher. No text to speech possible.";
        } else if (availableModules.length() == 1 && availableModules.at(0) == dummyModule) {
            qWarning() << "Found only the dummy module in speech-dispatcher. No text to speech possible. Install a tts module (e.g. espeak).";
        } else {
            m_state = QTextToSpeech::Ready;
        }

        // Default to system locale, since there's no api to get this from spd yet.
        m_currentLocale = QLocale::system();
        updateVoices();
        return true;
    } else {
        qWarning() << "Connection to speech-dispatcher failed";
        m_state = QTextToSpeech::BackendError;
        return false;
    }
    return false;
}

// hack to get state notifications
void QTextToSpeechEngineSpeechd::spdStateChanged(SPDNotificationType state)
{
    QTextToSpeech::State s = QTextToSpeech::BackendError;
    if (state == SPD_EVENT_PAUSE)
        s = QTextToSpeech::Paused;
    else if ((state == SPD_EVENT_BEGIN) || (state == SPD_EVENT_RESUME))
        s = QTextToSpeech::Speaking;
    else if ((state == SPD_EVENT_CANCEL) || (state == SPD_EVENT_END))
        s = QTextToSpeech::Ready;

    if (m_state != s) {
        m_state = s;
        emit stateChanged(m_state);
    }
}

void QTextToSpeechEngineSpeechd::say(const QString &text)
{
    if (text.isEmpty() || !connectToSpeechDispatcher())
        return;

    if (m_state != QTextToSpeech::Ready)
        stop();
    spd_say(speechDispatcher, SPD_MESSAGE, text.toUtf8().constData());
}

void QTextToSpeechEngineSpeechd::stop()
{
    if (!connectToSpeechDispatcher())
        return;

    if (m_state == QTextToSpeech::Paused)
        spd_resume_all(speechDispatcher);
    spd_cancel_all(speechDispatcher);
}

void QTextToSpeechEngineSpeechd::pause()
{
    if (!connectToSpeechDispatcher())
        return;

    if (m_state == QTextToSpeech::Speaking) {
        spd_pause_all(speechDispatcher);
    }
}

void QTextToSpeechEngineSpeechd::resume()
{
    if (!connectToSpeechDispatcher())
        return;

    if (m_state == QTextToSpeech::Paused) {
        spd_resume_all(speechDispatcher);
    }
}

bool QTextToSpeechEngineSpeechd::setPitch(double pitch)
{
    if (!connectToSpeechDispatcher())
        return false;

    int result = spd_set_voice_pitch(speechDispatcher, static_cast<int>(pitch * 100));
    if (result == 0)
        return true;
    return false;
}

double QTextToSpeechEngineSpeechd::pitch() const
{
    double pitch = 0.0;
#ifdef HAVE_SPD_090
    if (speechDispatcher != 0) {
        int result = spd_get_voice_pitch(speechDispatcher);
        pitch = result / 100.0;
    }
#endif
    return pitch;
}

bool QTextToSpeechEngineSpeechd::setRate(double rate)
{
    if (!connectToSpeechDispatcher())
        return false;

    int result = spd_set_voice_rate(speechDispatcher, static_cast<int>(rate * 100));
    if (result == 0)
        return true;
    return false;
}

double QTextToSpeechEngineSpeechd::rate() const
{
    double rate = 0.0;
#ifdef HAVE_SPD_090
    if (speechDispatcher != 0) {
        int result = spd_get_voice_rate(speechDispatcher);
        rate = result / 100.0;
    }
#endif
    return rate;
}

bool QTextToSpeechEngineSpeechd::setVolume(double volume)
{
    if (!connectToSpeechDispatcher())
        return false;

    // convert from 0.0..1.0 to -100..100
    int result = spd_set_volume(speechDispatcher, (volume - 0.5) * 200);
    if (result == 0)
        return true;
    return false;
}

double QTextToSpeechEngineSpeechd::volume() const
{
    double volume = 0.0;
#ifdef HAVE_SPD_090
    if (speechDispatcher != 0) {
        int result = spd_get_volume(speechDispatcher);
        // -100..100 to 0.0..1.0
        volume = (result + 100) / 200.0;
    }
#endif
    return volume;
}

bool QTextToSpeechEngineSpeechd::setLocale(const QLocale &locale)
{
    if (!connectToSpeechDispatcher())
        return false;

    int result = spd_set_language(speechDispatcher, locale.uiLanguages().at(0).toUtf8().data());
    if (result == 0) {
        QLocale previousLocale = m_currentLocale;
        QVoice previousVoice = m_currentVoice;
        m_currentLocale = locale;

        QVector<QVoice> voices = availableVoices();
        if (voices.size() > 0 && setVoice(voices.at(0)))
            return true;

        // try to go back to the previous locale/voice
        m_currentLocale = previousLocale;
        setVoice(previousVoice);
    }
    return false;
}

QLocale QTextToSpeechEngineSpeechd::locale() const
{
    return m_currentLocale;
}

bool QTextToSpeechEngineSpeechd::setVoice(const QVoice &voice)
{
    if (!connectToSpeechDispatcher())
        return false;

    const int result = spd_set_output_module(speechDispatcher, voiceData(voice).toString().toUtf8().data());
    if (result != 0)
        return false;
    const int result2 = spd_set_synthesis_voice(speechDispatcher, voice.name().toUtf8().data());
    if (result2 == 0) {
        m_currentVoice = voice;
        return true;
    }
    return false;
}

QVoice QTextToSpeechEngineSpeechd::voice() const
{
    return m_currentVoice;
}

QTextToSpeech::State QTextToSpeechEngineSpeechd::state() const
{
    return m_state;
}

void QTextToSpeechEngineSpeechd::updateVoices()
{
    char **modules = spd_list_modules(speechDispatcher);
#ifdef HAVE_SPD_090
    char *original_module = spd_get_output_module(speechDispatcher);
#else
    char *original_module = modules[0];
#endif
    QVoice originalVoice;
    char **module = modules;
    while (module != NULL && module[0] != NULL) {
        spd_set_output_module(speechDispatcher, module[0]);

        SPDVoice **voices = spd_list_synthesis_voices(speechDispatcher);
        int i = 0;
        while (voices != NULL && voices[i] != NULL) {
            QLocale locale = localeForVoice(voices[i]);
            if (!m_locales.contains(locale))
                m_locales.append(locale);
            const QString name = QString::fromUtf8(voices[i]->name);
            // iterate over genders and ages, creating a voice for each one
            QVoice voice = createVoice(name, QVoice::Unknown, QVoice::Other, QLatin1String(module[0]));
            m_voices.insert(locale.name(), voice);
            if (module[0] == original_module && i == 0) {
                // in lack of better options, remember the first voice as default
                originalVoice = voice;
            }
            ++i;
        }
        // free voices.
#ifdef HAVE_SPD_090
        free_spd_voices(voices);
#endif
        ++module;
    }

#ifdef HAVE_SPD_090
    // Also free modules.
    free_spd_modules(modules);
#endif
    // Set the output module back to what it was.
    spd_set_output_module(speechDispatcher, original_module);
    setVoice(originalVoice);
#ifdef HAVE_SPD_090
    free(original_module);
#endif
}

QVector<QLocale> QTextToSpeechEngineSpeechd::availableLocales() const
{
    return m_locales;
}

QVector<QVoice> QTextToSpeechEngineSpeechd::availableVoices() const
{
    return m_voices.values(m_currentLocale.name()).toVector();
}

// We have no way of knowing our own client_id since speech-dispatcher seems to be incomplete
// (history functions are just stubs)
void speech_finished_callback(size_t /*msg_id*/, size_t /*client_id*/, SPDNotificationType state)
{
    Q_FOREACH (QTextToSpeechEngineSpeechd *backend, *backends)
        backend->spdStateChanged(state);
}

QT_END_NAMESPACE
