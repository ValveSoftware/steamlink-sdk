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

#include "qtexttospeechengine.h"

#include <QLoggingCategory>

QT_BEGIN_NAMESPACE


/*!
  \class QTextToSpeechPluginEngine
  \inmodule QtSpeech
  \brief The QTextToSpeechPluginEngine class is the base for text-to-speech engine integrations.

  An engine implementation should derive from QTextToSpeechPluginEngine and implement all
  the pure virtual methods.
*/

/*! \fn QVector<QLocale> QTextToSpeechPluginEngine::availableLocales() const

  Implementation of \l QTextToSpeech::availableLocales()
*/

/*! \fn QVector<QVoice> QTextToSpeechPluginEngine::availableVoices() const

  Implementation of \l QTextToSpeech::availableVoices()
*/

/*! \fn void QTextToSpeechPluginEngine::say(const QString &text)

  Implementation of \l QTextToSpeech::say()
*/

/*! \fn void QTextToSpeechPluginEngine::stop()

  Implementation of \l QTextToSpeech::stop()
*/

/*! \fn void QTextToSpeechPluginEngine::pause()

  Implementation of \l QTextToSpeech::pause()
*/

/*! \fn void QTextToSpeechPluginEngine::resume()

  Implementation of \l QTextToSpeech::resume()
*/

/*! \fn void QTextToSpeechPluginEngine::rate() const

  Implementation of \l QTextToSpeech::rate()
*/

/*! \fn bool QTextToSpeechPluginEngine::setRate(double rate)

  Implementation of \l QTextToSpeech::setRate().

  Return \c true if the operation was successful.
*/

/*! \fn void QTextToSpeechPluginEngine::pitch() const

  Implementation of \l QTextToSpeech::pitch()
*/

/*! \fn bool QTextToSpeechPluginEngine::setPitch(double pitch)

  Implementation of \l QTextToSpeech::setPitch()

  Return \c true if the operation was successful.
*/

/*! \fn QLocale QTextToSpeechPluginEngine::locale() const

  Implementation of \l QTextToSpeech::locale()
*/

/*! \fn bool QTextToSpeechPluginEngine::setLocale(const QLocale &locale)

  Implementation of \l QTextToSpeech::setLocale()

  Return \c true if the operation was successful. In this case, the
  current voice (returned by \l voice()) should also have been updated
  to a valid new value.
*/

/*! \fn void QTextToSpeechPluginEngine::volume() const

  Implementation of \l QTextToSpeech::volume()
*/

/*! \fn bool QTextToSpeechPluginEngine::setVolume(int volume)

  Implementation of \l QTextToSpeech::setVolume()

  Return \c true if the operation was successful.
*/

/*! \fn QVoice QTextToSpeechPluginEngine::voice() const

  Implementation of \l QTextToSpeech::voice()
*/

/*! \fn bool QTextToSpeechPluginEngine::setVoice(const QVoice &voice)

  Implementation of \l QTextToSpeech::setVoice()

  Return \c true if the operation was successful.
*/

/*! \fn QTextToSpeech::State QTextToSpeechPluginEngine::state() const

  Implementation of \l QTextToSpeech::state()
*/

/*! \fn void QTextToSpeechPluginEngine::stateChanged(QTextToSpeech::State state)

  Emitted when the text-to-speech engine state has changed.
  This signal is connected to signal QTextToSpeech::stateChanged().
*/

/*!
  Constructs the text-to-speech engine base class.
*/
QTextToSpeechEngine::QTextToSpeechEngine(QObject *parent):
    QObject(parent)
{
}

QTextToSpeechEngine::~QTextToSpeechEngine()
{
}

/*!
  Creates a voice for a text-to-speech engine.

  Parameters \a name, \a gender, \a age and \a data are directly stored in the QVoice instance.
*/
QVoice QTextToSpeechEngine::createVoice(const QString &name, QVoice::Gender gender, QVoice::Age age, const QVariant &data)
{
    return QVoice(name, gender, age, data);
}

/*!
  Returns the engine-specific private data for the given \a voice.

*/
QVariant QTextToSpeechEngine::voiceData(const QVoice &voice)
{
    return voice.data();
}

QT_END_NAMESPACE
