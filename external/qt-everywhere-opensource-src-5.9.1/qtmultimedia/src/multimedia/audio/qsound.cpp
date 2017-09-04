/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qsound.h"
#include "qsoundeffect.h"
#include "qcoreapplication.h"


/*!
    \class QSound
    \brief The QSound class provides a method to play .wav sound files.
    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_audio

    Qt provides the most commonly required audio operation in GUI
    applications: asynchronously playing a sound file. This is most
    easily accomplished using the static play() function:

    \snippet multimedia-snippets/qsound.cpp 0

    Alternatively, create a QSound object from the sound file first
    and then call the play() slot:

    \snippet multimedia-snippets/qsound.cpp 1

    Once created a QSound object can be queried for its fileName() and
    total number of loops() (i.e. the number of times the sound will
    play). The number of repetitions can be altered using the
    setLoops() function. While playing the sound, the loopsRemaining()
    function returns the remaining number of repetitions. Use the
    isFinished() function to determine whether the sound has finished
    playing.

    Sounds played using a QSound object may use more memory than the
    static play() function, but it may also play more immediately
    (depending on the underlying platform audio facilities).

    If you require finer control over playing sounds, consider the
    \l QSoundEffect or \l QAudioOutput classes.

    \sa QSoundEffect
*/

/*!
    \enum QSound::Loop

    \value Infinite  Can be used as a parameter to \l setLoops() to loop infinitely.
*/


/*!
    Plays the sound stored in the file specified by the given \a filename.

    \sa stop(), loopsRemaining(), isFinished()
*/
void QSound::play(const QString& filename)
{
    // Object destruction is generaly handled via deleteOnComplete
    // Unexpected cases will be handled via parenting of QSound objects to qApp
    QSound *sound = new QSound(filename, qApp);
    sound->connect(sound->m_soundEffect, SIGNAL(playingChanged()), SLOT(deleteOnComplete()));
    sound->play();
}

/*!
    Constructs a QSound object from the file specified by the given \a
    filename and with the given \a parent.

    \sa play()
*/
QSound::QSound(const QString& filename, QObject* parent)
    : QObject(parent)
{
    m_soundEffect = new QSoundEffect(this);
    m_soundEffect->setSource(QUrl::fromLocalFile(filename));
}

/*!
    Destroys this sound object. If the sound is not finished playing,
    the stop() function is called before the sound object is
    destroyed.

    \sa stop(), isFinished()
*/
QSound::~QSound()
{
    if (!isFinished())
        stop();
}

/*!
    Returns true if the sound has finished playing; otherwise returns false.
*/
bool QSound::isFinished() const
{
    return !m_soundEffect->isPlaying();
}

/*!
    \overload

    Starts playing the sound specified by this QSound object.

    The function returns immediately.  Depending on the platform audio
    facilities, other sounds may stop or be mixed with the new
    sound. The sound can be played again at any time, possibly mixing
    or replacing previous plays of the sound.

    \sa fileName()
*/
void QSound::play()
{
    m_soundEffect->play();
}

/*!
    Returns the number of times the sound will play.
    Return value of \c QSound::Infinite indicates infinite number of loops

    \sa loopsRemaining(), setLoops()
*/
int QSound::loops() const
{
    // retain old API value for infite loops
    int loopCount = m_soundEffect->loopCount();
    if (loopCount == QSoundEffect::Infinite)
        loopCount = Infinite;

    return loopCount;
}

/*!
    Returns the remaining number of times the sound will loop (for all
    positive values this value decreases each time the sound is played).
    Return value of \c QSound::Infinite indicates infinite number of loops

    \sa loops(), isFinished()
*/
int QSound::loopsRemaining() const
{
    // retain old API value for infite loops
    int loopsRemaining = m_soundEffect->loopsRemaining();
    if (loopsRemaining == QSoundEffect::Infinite)
        loopsRemaining = Infinite;

    return loopsRemaining;
}

/*!
    \fn void QSound::setLoops(int number)

    Sets the sound to repeat the given \a number of times when it is
    played.

    Note that passing the value \c QSound::Infinite will cause the sound to loop
    indefinitely.

    \sa loops()
*/
void QSound::setLoops(int n)
{
    if (n == Infinite)
        n = QSoundEffect::Infinite;

    m_soundEffect->setLoopCount(n);
}

/*!
    Returns the filename associated with this QSound object.

    \sa QSound()
*/
QString QSound::fileName() const
{
    return m_soundEffect->source().toLocalFile();
}

/*!
    Stops the sound playing.

    \sa play()
*/
void QSound::stop()
{
    m_soundEffect->stop();
}

/*!
    \internal
*/
void QSound::deleteOnComplete()
{
    if (!m_soundEffect->isPlaying())
        deleteLater();
}

#include "moc_qsound.cpp"
