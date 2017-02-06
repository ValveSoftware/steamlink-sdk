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



#include "qtexttospeech.h"
#include "qtexttospeech_p.h"

#include <qdebug.h>

#include <QtCore/private/qfactoryloader_p.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_LIBRARY
Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, loader,
        ("org.qt-project.qt.speech.tts.plugin/5.0",
         QLatin1String("/texttospeech")))
#endif

QMutex QTextToSpeechPrivate::m_mutex;

QTextToSpeechPrivate::QTextToSpeechPrivate(QTextToSpeech *speech, const QString &engine)
    : m_engine(0),
      m_speech(speech),
      m_providerName(engine),
      m_plugin(0)
{
    qRegisterMetaType<QTextToSpeech::State>();
    if (m_providerName.isEmpty()) {
        m_providerName = QTextToSpeech::availableEngines().value(0);
        if (m_providerName.isEmpty()) {
            qCritical() << "No text-to-speech plug-ins were found.";
            return;
        }
    }
    if (!loadMeta()) {
        qCritical() << "Text-to-speech plug-in" << m_providerName << "is not supported.";
        return;
    }
    loadPlugin();
    if (m_plugin) {
        QString errorString;
        m_engine = m_plugin->createTextToSpeechEngine(QVariantMap(), 0, &errorString);
        if (!m_engine) {
            qCritical() << "Error creating text-to-speech engine" << m_providerName
                        << (errorString.isEmpty() ? QStringLiteral("") : (QStringLiteral(": ") + errorString));
        }
    } else {
        qCritical() << "Error loading text-to-speech plug-in" << m_providerName;
    }
}

QTextToSpeechPrivate::~QTextToSpeechPrivate()
{
    m_speech->stop();
    delete m_engine;
}

bool QTextToSpeechPrivate::loadMeta()
{
    m_plugin = 0;
    m_metaData = QJsonObject();
    m_metaData.insert(QLatin1String("index"), -1);

    QList<QJsonObject> candidates = QTextToSpeechPrivate::plugins().values(m_providerName);

    int versionFound = -1;
    int idx = -1;

    // figure out which version of the plugin we want
    for (int i = 0; i < candidates.size(); ++i) {
        QJsonObject meta = candidates[i];
        if (meta.contains(QLatin1String("Version"))
                && meta.value(QLatin1String("Version")).isDouble()) {
            int ver = int(meta.value(QLatin1String("Version")).toDouble());
            if (ver > versionFound) {
                versionFound = ver;
                idx = i;
            }
        }
    }

    if (idx != -1) {
        m_metaData = candidates[idx];
        return true;
    }
    return false;
}

void QTextToSpeechPrivate::loadPlugin()
{
    if (int(m_metaData.value(QLatin1String("index")).toDouble()) < 0) {
        m_plugin = 0;
        return;
    }
    int idx = int(m_metaData.value(QLatin1String("index")).toDouble());
    m_plugin = qobject_cast<QTextToSpeechPlugin *>(loader()->instance(idx));
}

QHash<QString, QJsonObject> QTextToSpeechPrivate::plugins(bool reload)
{
    static QHash<QString, QJsonObject> plugins;
    static bool alreadyDiscovered = false;
    QMutexLocker lock(&m_mutex);

    if (reload == true)
        alreadyDiscovered = false;

    if (!alreadyDiscovered) {
        loadPluginMetadata(plugins);
        alreadyDiscovered = true;
    }
    return plugins;
}

void QTextToSpeechPrivate::loadPluginMetadata(QHash<QString, QJsonObject> &list)
{
    QFactoryLoader *l = loader();
    QList<QJsonObject> meta = l->metaData();
    for (int i = 0; i < meta.size(); ++i) {
        QJsonObject obj = meta.at(i).value(QLatin1String("MetaData")).toObject();
        obj.insert(QLatin1String("index"), i);
        list.insertMulti(obj.value(QLatin1String("Provider")).toString(), obj);
    }
}

/*!
  \class QTextToSpeech
  \brief The QTextToSpeech class provides a convenient access to text-to-speech engines
  \inmodule QtSpeech

  Use \l say() to start synthesizing text.
  It is possible to specify the language with \l setLocale().
  To select between the available voices use \l setVoice().
  The languages and voices depend on the available synthesizers on each platform.
  On Linux by default speech-dispatcher is used.
*/

/*!
  \enum QTextToSpeech::State
  \value Ready          The synthesizer is ready to start a new text. This is
                        also the state after a text was finished.
  \value Speaking       The current text is being spoken.
  \value Paused         The sythetization was paused and can be resumed with \l resume().
  \value BackendError   The backend was unable to synthesize the current string.
*/

/*!
  \property QTextToSpeech::state
  This property holds the current state of the speech synthesizer.
  Use \l say() to start synthesizing text with the current voice and locale.

*/

/*!
    Loads a text-to-speech engine from a plug-in that uses the default
    engine plug-in and constructs a QTextToSpeech object as the child
    of \a parent.

    The default engine may be platform-specific.

    If loading the plug-in fails, QTextToSpeech::state() will return
    QTextToSpeech::BackendError.

    \sa availableEngines()
*/
QTextToSpeech::QTextToSpeech(QObject *parent)
    : QObject(*new QTextToSpeechPrivate(this, QString()), parent)
{
    Q_D(QTextToSpeech);
    // Connect state change signal directly from the engine to the public API signal
    if (d->m_engine)
        connect(d->m_engine, &QTextToSpeechEngine::stateChanged, this, &QTextToSpeech::stateChanged);
}

/*!
  Loads a text-to-speech engine from a plug-in that matches parameter \a engine and
  constructs a QTextToSpeech object as the child of \a parent.

  If \a engine is empty, the default engine plug-in is used. The default
  engine may be platform-specific.

  If loading the plug-in fails, QTextToSpeech::state() will return QTextToSpeech::BackendError.

  \sa availableEngines()
*/
QTextToSpeech::QTextToSpeech(const QString &engine, QObject *parent)
    : QObject(*new QTextToSpeechPrivate(this, engine), parent)
{
    Q_D(QTextToSpeech);
    // Connect state change signal directly from the engine to the public API signal
    if (d->m_engine)
        connect(d->m_engine, &QTextToSpeechEngine::stateChanged, this, &QTextToSpeech::stateChanged);
}

/*!
 Gets the list of supported text-to-speech engine plug-ins.
*/
QStringList QTextToSpeech::availableEngines()
{
    return QTextToSpeechPrivate::plugins().keys();
}

QTextToSpeech::State QTextToSpeech::state() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->state();
    return QTextToSpeech::BackendError;
}

/*!
  Start synthesizing the \a text.
  This function will start the asynchronous speaking of the text.
  The current state is available using the \l state property. Once the
  synthetization is done, a \l stateChanged() signal with the \l Ready state
  will be emitted.
*/
void QTextToSpeech::say(const QString &text)
{
    Q_D(QTextToSpeech);
    if (d->m_engine)
        d->m_engine->say(text);
}

/*!
  Stop the currently speaking text.
*/
void QTextToSpeech::stop()
{
    Q_D(QTextToSpeech);
    if (d->m_engine)
        d->m_engine->stop();
}

/*!
  Pause the current speech.
  \note this function depends on the platform and backend and may not work at all,
  take several seconds until it takes effect or may pause instantly.
  Some synthesizers will look for a break that they can later resume from, such as
  a sentence end.
  \note Due to Android platform limitations, pause() stops the current utterance,
  while resume() starts the previously queued utterance from the beginning.
  \sa resume()
*/
void QTextToSpeech::pause()
{
    Q_D(QTextToSpeech);
    if (d->m_engine)
        d->m_engine->pause();
}

/*!
  Resume speaking after \l pause() has been called.
  \sa pause()
*/
void QTextToSpeech::resume()
{
    Q_D(QTextToSpeech);
    if (d->m_engine)
        d->m_engine->resume();
}

//QVector<QString> QTextToSpeech::availableVoiceTypes() const
//{
//    Q_D(const QTextToSpeech);
//    return d->availableVoiceTypes();
//}

//void QTextToSpeech::setVoiceType(const QString& type)
//{
//    Q_D(QTextToSpeech);
//    d->setVoiceType(type);
//}
//QString QTextToSpeech::currentVoiceType() const
//{
//    Q_D(const QTextToSpeech);
//    return d->currentVoiceType();
//}


/*!
 \property QTextToSpeech::pitch
 This property holds the voice pitch in the range -1.0 to 1.0.
 The default of 0.0 is normal speech pitch.
*/

void QTextToSpeech::setPitch(double pitch)
{
    Q_D(QTextToSpeech);
    if (d->m_engine && d->m_engine->setPitch(pitch))
        emit pitchChanged(pitch);
}

double QTextToSpeech::pitch() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->pitch();
    return 0.0;
}

/*!
 \property QTextToSpeech::rate
 This property holds the current voice rate in the range -1.0 to 1.0.
 The default value of 0.0 is normal speech flow.
*/
void QTextToSpeech::setRate(double rate)
{
    Q_D(QTextToSpeech);
    if (d->m_engine && d->m_engine->setRate(rate))
        emit rateChanged(rate);
}

double QTextToSpeech::rate() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->rate();
    return 0.0;
}

/*!
 \property QTextToSpeech::volume
 This property holds the current volume in the range 0.0 to 1.0.
 The default value is the platform's default volume.
*/
void QTextToSpeech::setVolume(double volume)
{
    Q_D(QTextToSpeech);
    volume = qMin(qMax(volume, 0.0), 1.0);
    if (d->m_engine && d->m_engine->setVolume(volume))
        emit volumeChanged(volume);
}

double QTextToSpeech::volume() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->volume();
    return 0.0;
}

/*!
 Sets the \a locale to a given locale if possible.
 The default is the system locale.
*/
void QTextToSpeech::setLocale(const QLocale &locale)
{
    Q_D(QTextToSpeech);
    if (d->m_engine && d->m_engine->setLocale(locale)) {
        emit localeChanged(locale);
        emit voiceChanged(d->m_engine->voice());
    }
}

/*!
 \property QTextToSpeech::locale
 This property holds the current locale in use. By default, the system locale
 is used.
*/
QLocale QTextToSpeech::locale() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->locale();
    return QLocale();
}

/*!
 Gets a vector of locales that are currently supported. Note on some platforms
 these can change when the backend changes synthesizers for example.
*/
QVector<QLocale> QTextToSpeech::availableLocales() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->availableLocales();
    return QVector<QLocale>();
}

/*!
 Sets the \a voice to use.

 \note On some platforms setting the voice changes other voice attributes
 such as locale, pitch, etc. in which case signals are emitted for these
 changes.
*/
void QTextToSpeech::setVoice(const QVoice &voice)
{
    Q_D(QTextToSpeech);
    if (d->m_engine && d->m_engine->setVoice(voice))
        emit voiceChanged(voice);
}

/*!
 \property QTextToSpeech::voice
 This property holds the current voice used for the speech.
*/
QVoice QTextToSpeech::voice() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->voice();
    return QVoice();
}

/*!
 Gets a vector of voices available for the current locale.
 \note if no locale has been set, the system locale is used.
*/
QVector<QVoice> QTextToSpeech::availableVoices() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->availableVoices();
    return QVector<QVoice>();
}

QT_END_NAMESPACE
