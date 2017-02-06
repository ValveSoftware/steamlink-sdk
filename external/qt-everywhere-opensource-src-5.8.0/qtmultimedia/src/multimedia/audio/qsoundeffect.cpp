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

#include "qsoundeffect.h"

#if defined(QT_MULTIMEDIA_PULSEAUDIO)
#include "qsoundeffect_pulse_p.h"
#elif(QT_MULTIMEDIA_QAUDIO)
#include "qsoundeffect_qaudio_p.h"
#endif

QT_BEGIN_NAMESPACE

/*!
    \class QSoundEffect
    \brief The QSoundEffect class provides a way to play low latency sound effects.

    \ingroup multimedia
    \ingroup multimedia_audio
    \inmodule QtMultimedia

    This class allows you to play uncompressed audio files (typically WAV files) in
    a generally lower latency way, and is suitable for "feedback" type sounds in
    response to user actions (e.g. virtual keyboard sounds, positive or negative
    feedback for popup dialogs, or game sounds).  If low latency is not important,
    consider using the QMediaPlayer class instead, since it supports a wider
    variety of media formats and is less resource intensive.

    This example shows how a looping, somewhat quiet sound effect
    can be played:

    \snippet multimedia-snippets/qsound.cpp 2

    Typically the sound effect should be reused, which allows all the
    parsing and preparation to be done ahead of time, and only triggered
    when necessary.  This assists with lower latency audio playback.

    \snippet multimedia-snippets/qsound.cpp 3

    Since QSoundEffect requires slightly more resources to achieve lower
    latency playback, the platform may limit the number of simultaneously playing
    sound effects.
*/


/*!
    \qmltype SoundEffect
    \instantiates QSoundEffect
    \brief The SoundEffect type provides a way to play sound effects in QML.

    \inmodule QtMultimedia
    \ingroup multimedia_qml
    \ingroup multimedia_audio_qml
    \inqmlmodule QtMultimedia

    This type allows you to play uncompressed audio files (typically WAV files) in
    a generally lower latency way, and is suitable for "feedback" type sounds in
    response to user actions (e.g. virtual keyboard sounds, positive or negative
    feedback for popup dialogs, or game sounds).  If low latency is not important,
    consider using the MediaPlayer or Audio types instead, since they support a wider
    variety of media formats and are less resource intensive.

    Typically the sound effect should be reused, which allows all the
    parsing and preparation to be done ahead of time, and only triggered
    when necessary.  This is easy to achieve with QML, since you can declare your
    SoundEffect instance and refer to it elsewhere.

    The following example plays a WAV file on mouse click.

    \snippet multimedia-snippets/soundeffect.qml complete snippet

    Since SoundEffect requires slightly more resources to achieve lower
    latency playback, the platform may limit the number of simultaneously playing
    sound effects.
*/

/*!
    Creates a QSoundEffect with the given \a parent.
*/
QSoundEffect::QSoundEffect(QObject *parent) :
    QObject(parent)
{
    d = new QSoundEffectPrivate(this);
    connect(d, SIGNAL(loopsRemainingChanged()), SIGNAL(loopsRemainingChanged()));
    connect(d, SIGNAL(volumeChanged()), SIGNAL(volumeChanged()));
    connect(d, SIGNAL(mutedChanged()), SIGNAL(mutedChanged()));
    connect(d, SIGNAL(loadedChanged()), SIGNAL(loadedChanged()));
    connect(d, SIGNAL(playingChanged()), SIGNAL(playingChanged()));
    connect(d, SIGNAL(statusChanged()), SIGNAL(statusChanged()));
    connect(d, SIGNAL(categoryChanged()), SIGNAL(categoryChanged()));
}

/*!
    Destroys this sound effect.
 */
QSoundEffect::~QSoundEffect()
{
    d->release();
}

/*!
    \fn QSoundEffect::supportedMimeTypes()

    Returns a list of the supported mime types for this platform.
*/
QStringList QSoundEffect::supportedMimeTypes()
{
    return QSoundEffectPrivate::supportedMimeTypes();
}

/*!
    \qmlproperty url QtMultimedia::SoundEffect::source

    This property holds the url for the sound to play. For the SoundEffect
    to attempt to load the source, the URL must exist and the application must have read permission
    in the specified directory. If the desired source is a local file the URL may be specified
    using either absolute or relative (to the file that declared the SoundEffect) pathing.
*/
/*!
    \property QSoundEffect::source

    This property holds the url for the sound to play. For the SoundEffect
    to attempt to load the source, the URL must exist and the application must have read permission
    in the specified directory.
*/

/*! Returns the URL of the current source to play */
QUrl QSoundEffect::source() const
{
    return d->source();
}

/*! Set the current URL to play to \a url. */
void QSoundEffect::setSource(const QUrl &url)
{
    if (d->source() == url)
        return;

    d->setSource(url);

    emit sourceChanged();
}

/*!
    \qmlproperty int QtMultimedia::SoundEffect::loops

    This property holds the number of times the sound is played. A value of 0 or 1 means
    the sound will be played only once; set to SoundEffect.Infinite to enable infinite looping.

    The value can be changed while the sound effect is playing, in which case it will update
    the remaining loops to the new value.
*/

/*!
    \property QSoundEffect::loops
    This property holds the number of times the sound is played. A value of 0 or 1 means
    the sound will be played only once; set to SoundEffect.Infinite to enable infinite looping.

    The value can be changed while the sound effect is playing, in which case it will update
    the remaining loops to the new value.
*/

/*!
    Returns the total number of times that this sound effect will be played before stopping.

    See the \l loopsRemaining() method for the number of loops currently remaining.
 */
int QSoundEffect::loopCount() const
{
    return d->loopCount();
}

/*!
    \enum QSoundEffect::Loop

    \value Infinite  Used as a parameter to \l setLoopCount() for infinite looping
*/

/*!
    Set the total number of times to play this sound effect to \a loopCount.

    Setting the loop count to 0 or 1 means the sound effect will be played only once;
    pass \c QSoundEffect::Infinite to repeat indefinitely. The loop count can be changed while
    the sound effect is playing, in which case it will update the remaining loops to
    the new \a loopCount.

    \sa loopsRemaining()
*/
void QSoundEffect::setLoopCount(int loopCount)
{
    if (loopCount < 0 && loopCount != Infinite) {
        qWarning("SoundEffect: loops should be SoundEffect.Infinite, 0 or positive integer");
        return;
    }
    if (loopCount == 0)
        loopCount = 1;
    if (d->loopCount() == loopCount)
        return;

    d->setLoopCount(loopCount);
    emit loopCountChanged();
}

/*!
    \qmlproperty int QtMultimedia::SoundEffect::loopsRemaining

    This property contains the number of loops remaining before the sound effect
    stops by itself, or SoundEffect.Infinite if that's what has been set in \l loops.
*/
/*!
    \property QSoundEffect::loopsRemaining

    This property contains the number of loops remaining before the sound effect
    stops by itself, or QSoundEffect::Infinite if that's what has been set in \l loops.
*/
int QSoundEffect::loopsRemaining() const
{
    return d->loopsRemaining();
}


/*!
    \qmlproperty qreal QtMultimedia::SoundEffect::volume

    This property holds the volume of the sound effect playback.

    The volume is scaled linearly from \c 0.0 (silence) to \c 1.0 (full volume). Values outside this
    range will be clamped.

    The default volume is \c 1.0.

    UI volume controls should usually be scaled nonlinearly. For example, using a logarithmic scale
    will produce linear changes in perceived loudness, which is what a user would normally expect
    from a volume control. See \l {QtMultimedia::QtMultimedia::convertVolume()}{QtMultimedia.convertVolume()}
    for more details.
*/
/*!
    \property QSoundEffect::volume

    This property holds the volume of the sound effect playback, from 0.0 (silence) to 1.0 (full volume).
*/

/*!
    Returns the current volume of this sound effect, from 0.0 (silent) to 1.0 (maximum volume).
 */
qreal QSoundEffect::volume() const
{
    return d->volume();
}

/*!
    Sets the sound effect volume to \a volume.

    The volume is scaled linearly from \c 0.0 (silence) to \c 1.0 (full volume). Values outside this
    range will be clamped.

    The default volume is \c 1.0.

    UI volume controls should usually be scaled nonlinearly. For example, using a logarithmic scale
    will produce linear changes in perceived loudness, which is what a user would normally expect
    from a volume control. See QAudio::convertVolume() for more details.
 */
void QSoundEffect::setVolume(qreal volume)
{
    volume = qBound(qreal(0.0), volume, qreal(1.0));
    if (qFuzzyCompare(d->volume(), volume))
        return;

    d->setVolume(volume);
}

/*!
    \qmlproperty bool QtMultimedia::SoundEffect::muted

    This property provides a way to control muting. A value of \c true will mute this effect.
    Otherwise, playback will occur with the currently specified \l volume.
*/
/*!
    \property QSoundEffect::muted

    This property provides a way to control muting. A value of \c true will mute this effect.
*/

/*! Returns whether this sound effect is muted */
bool QSoundEffect::isMuted() const
{
    return d->isMuted();
}

/*!
    Sets whether to mute this sound effect's playback.

    If \a muted is true, playback will be muted (silenced),
    and otherwise playback will occur with the currently
    specified volume().
*/
void QSoundEffect::setMuted(bool muted)
{
    if (d->isMuted() == muted)
        return;

    d->setMuted(muted);
}

/*!
    \fn QSoundEffect::isLoaded() const

    Returns whether the sound effect has finished loading the \l source().
*/
/*!
    \qmlmethod bool QtMultimedia::SoundEffect::isLoaded()

    Returns whether the sound effect has finished loading the \l source.
*/
bool QSoundEffect::isLoaded() const
{
    return d->isLoaded();
}

/*!
    \qmlmethod QtMultimedia::SoundEffect::play()

    Start playback of the sound effect, looping the effect for the number of
    times as specified in the loops property.

    This is the default method for SoundEffect.

    \snippet multimedia-snippets/soundeffect.qml play sound on click
*/
/*!
    \fn QSoundEffect::play()

    Start playback of the sound effect, looping the effect for the number of
    times as specified in the loops property.
*/
void QSoundEffect::play()
{
    d->play();
}

/*!
    \qmlproperty bool QtMultimedia::SoundEffect::playing

    This property indicates whether the sound effect is playing or not.
*/
/*!
    \property QSoundEffect::playing

    This property indicates whether the sound effect is playing or not.
*/

/*! Returns true if the sound effect is currently playing, or false otherwise */
bool QSoundEffect::isPlaying() const
{
    return d->isPlaying();
}

/*!
    \enum QSoundEffect::Status

    \value Null  No source has been set or the source is null.
    \value Loading  The SoundEffect is trying to load the source.
    \value Ready  The source is loaded and ready for play.
    \value Error  An error occurred during operation, such as failure of loading the source.

*/

/*!
    \qmlproperty enumeration QtMultimedia::SoundEffect::status

    This property indicates the current status of the SoundEffect
    as enumerated within SoundEffect.
    Possible statuses are listed below.

    \table
    \header \li Value \li Description
    \row \li SoundEffect.Null    \li No source has been set or the source is null.
    \row \li SoundEffect.Loading \li The SoundEffect is trying to load the source.
    \row \li SoundEffect.Ready   \li The source is loaded and ready for play.
    \row \li SoundEffect.Error   \li An error occurred during operation, such as failure of loading the source.
    \endtable
*/
/*!
    \property QSoundEffect::status

    This property indicates the current status of the sound effect
    from the \l QSoundEffect::Status enumeration.
*/

/*!
    Returns the current status of this sound effect.
 */
QSoundEffect::Status QSoundEffect::status() const
{
    return d->status();
}

/*!
    \qmlproperty string QtMultimedia::SoundEffect::category

    This property contains the \e category of this sound effect.

    Some platforms can perform different audio routing
    for different categories, or may allow the user to
    set different volume levels for different categories.

    This setting will be ignored on platforms that do not
    support audio categories.
*/
/*!
    \property QSoundEffect::category

    This property contains the \e category of this sound effect.

    Some platforms can perform different audio routing
    for different categories, or may allow the user to
    set different volume levels for different categories.

    This setting will be ignored on platforms that do not
    support audio categories.
*/
/*!
    Returns the current \e category for this sound effect.

    Some platforms can perform different audio routing
    for different categories, or may allow the user to
    set different volume levels for different categories.

    This setting will be ignored on platforms that do not
    support audio categories.

    \sa setCategory()
*/
QString QSoundEffect::category() const
{
    return d->category();
}

/*!
    Sets the \e category of this sound effect to \a category.

    Some platforms can perform different audio routing
    for different categories, or may allow the user to
    set different volume levels for different categories.

    This setting will be ignored on platforms that do not
    support audio categories.

    If this setting is changed while a sound effect is playing
    it will only take effect when the sound effect has stopped
    playing.

    \sa category()
 */
void QSoundEffect::setCategory(const QString &category)
{
    d->setCategory(category);
}


/*!
  \qmlmethod QtMultimedia::SoundEffect::stop()

  Stop current playback.

 */
/*!
  \fn QSoundEffect::stop()

  Stop current playback.

 */
void QSoundEffect::stop()
{
    d->stop();
}

/* Signals */

/*!
    \fn void QSoundEffect::sourceChanged()

    The \c sourceChanged signal is emitted when the source has been changed.
*/
/*!
    \qmlsignal QtMultimedia::SoundEffect::sourceChanged()

    The \c sourceChanged signal is emitted when the source has been changed.

    The corresponding handler is \c onSourceChanged.
*/
/*!
    \fn void QSoundEffect::loadedChanged()

    The \c loadedChanged signal is emitted when the loading state has changed.
*/
/*!
    \qmlsignal QtMultimedia::SoundEffect::loadedChanged()

    The \c loadedChanged signal is emitted when the loading state has changed.

    The corresponding handler is \c onLoadedChanged.
*/

/*!
    \fn void QSoundEffect::loopCountChanged()

    The \c loopCountChanged signal is emitted when the initial number of loops has changed.
*/
/*!
    \qmlsignal QtMultimedia::SoundEffect::loopCountChanged()

    The \c loopCountChanged signal is emitted when the initial number of loops has changed.

    The corresponding handler is \c onLoopCountChanged.
*/

/*!
    \fn void QSoundEffect::loopsRemainingChanged()

    The \c loopsRemainingChanged signal is emitted when the remaining number of loops has changed.
*/
/*!
    \qmlsignal QtMultimedia::SoundEffect::loopsRemainingChanged()

    The \c loopsRemainingChanged signal is emitted when the remaining number of loops has changed.

    The corresponding handler is \c onLoopsRemainingChanged.
*/

/*!
    \fn void QSoundEffect::volumeChanged()

    The \c volumeChanged signal is emitted when the volume has changed.
*/
/*!
    \qmlsignal QtMultimedia::SoundEffect::volumeChanged()

    The \c volumeChanged signal is emitted when the volume has changed.

    The corresponding handler is \c onVolumeChanged.
*/

/*!
    \fn void QSoundEffect::mutedChanged()

    The \c mutedChanged signal is emitted when the mute state has changed.
*/
/*!
    \qmlsignal QtMultimedia::SoundEffect::mutedChanged()

    The \c mutedChanged signal is emitted when the mute state has changed.

    The corresponding handler is \c onMutedChanged.
*/

/*!
    \fn void QSoundEffect::playingChanged()

    The \c playingChanged signal is emitted when the playing property has changed.
*/
/*!
    \qmlsignal QtMultimedia::SoundEffect::playingChanged()

    The \c playingChanged signal is emitted when the playing property has changed.

    The corresponding handler is \c onPlayingChanged.
*/

/*!
    \fn void QSoundEffect::statusChanged()

    The \c statusChanged signal is emitted when the status property has changed.
*/
/*!
    \qmlsignal QtMultimedia::SoundEffect::statusChanged()

    The \c statusChanged signal is emitted when the status property has changed.

    The corresponding handler is \c onStatusChanged.
*/

/*!
    \fn void QSoundEffect::categoryChanged()

    The \c categoryChanged signal is emitted when the category property has changed.
*/
/*!
    \qmlsignal QtMultimedia::SoundEffect::categoryChanged()

    The \c categoryChanged signal is emitted when the category property has changed.

    The corresponding handler is \c onCategoryChanged.
*/


QT_END_NAMESPACE

#include "moc_qsoundeffect.cpp"
