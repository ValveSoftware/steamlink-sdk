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


#include "qmediaplaylistcontrol_p.h"
#include "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QMediaPlaylistControl
    \internal

    \inmodule QtMultimedia


    \ingroup multimedia_control


    \brief The QMediaPlaylistControl class provides access to the playlist
    functionality of a QMediaService.

    If a QMediaService contains an internal playlist it will implement
    QMediaPlaylistControl.  This control provides access to the contents of the
    \l {playlistProvider()}{playlist}, as well as the \l
    {currentIndex()}{position} of the current media, and a means of navigating
    to the \l {next()}{next} and \l {previous()}{previous} media.

    The functionality provided by the control is exposed to application code
    through the QMediaPlaylist class.

    The interface name of QMediaPlaylistControl is \c org.qt-project.qt.mediaplaylistcontrol/5.0 as
    defined in QMediaPlaylistControl_iid.

    \sa QMediaService::requestControl(), QMediaPlayer
*/

/*!
    \macro QMediaPlaylistControl_iid

    \c org.qt-project.qt.mediaplaylistcontrol/5.0

    Defines the interface name of the QMediaPlaylistControl class.

    \relates QMediaPlaylistControl
*/

/*!
  Create a new playlist control object with the given \a parent.
*/
QMediaPlaylistControl::QMediaPlaylistControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
  Destroys the playlist control.
*/
QMediaPlaylistControl::~QMediaPlaylistControl()
{
}


/*!
  \fn QMediaPlaylistControl::playlistProvider() const

  Returns the playlist used by this media player.
*/

/*!
  \fn QMediaPlaylistControl::setPlaylistProvider(QMediaPlaylistProvider *playlist)

  Set the playlist of this media player to \a playlist.

  In many cases it is possible just to use the playlist
  constructed by player, but sometimes replacing the whole
  playlist allows to avoid copyting of all the items bettween playlists.

  Returns true if player can use this passed playlist; otherwise returns false.

*/

/*!
  \fn QMediaPlaylistControl::currentIndex() const

  Returns position of the current media source in the playlist.
*/

/*!
  \fn QMediaPlaylistControl::setCurrentIndex(int position)

  Jump to the item at the given \a position.
*/

/*!
  \fn QMediaPlaylistControl::nextIndex(int step) const

  Returns the index of item, which were current after calling next()
  \a step times.

  Returned value depends on the size of playlist, current position
  and playback mode.

  \sa QMediaPlaylist::playbackMode
*/

/*!
  \fn QMediaPlaylistControl::previousIndex(int step) const

  Returns the index of item, which were current after calling previous()
  \a step times.

  \sa QMediaPlaylist::playbackMode
*/

/*!
  \fn QMediaPlaylistControl::next()

  Moves to the next item in playlist.
*/

/*!
  \fn QMediaPlaylistControl::previous()

  Returns to the previous item in playlist.
*/

/*!
  \fn QMediaPlaylistControl::playbackMode() const

  Returns the playlist navigation mode.

  \sa QMediaPlaylist::PlaybackMode
*/

/*!
  \fn QMediaPlaylistControl::setPlaybackMode(QMediaPlaylist::PlaybackMode mode)

  Sets the playback \a mode.

  \sa QMediaPlaylist::PlaybackMode
*/

/*!
  \fn QMediaPlaylistControl::playlistProviderChanged()

  Signal emitted when the playlist provider has changed.
*/

/*!
  \fn QMediaPlaylistControl::currentIndexChanged(int position)

  Signal emitted when the playlist \a position is changed.
*/

/*!
  \fn QMediaPlaylistControl::playbackModeChanged(QMediaPlaylist::PlaybackMode mode)

  Signal emitted when the playback \a mode is changed.
*/

/*!
  \fn QMediaPlaylistControl::currentMediaChanged(const QMediaContent& content)

  Signal emitted when current media changes to \a content.
*/

#include "moc_qmediaplaylistcontrol_p.cpp"
QT_END_NAMESPACE

