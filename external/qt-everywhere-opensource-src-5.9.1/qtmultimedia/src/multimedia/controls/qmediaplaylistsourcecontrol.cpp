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


#include "qmediaplaylistsourcecontrol_p.h"
#include "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QMediaPlaylistSourceControl
    \internal

    \inmodule QtMultimedia


    \ingroup multimedia_control


    \brief The QMediaPlaylistSourceControl class provides access to the playlist playback
    functionality of a QMediaService.

    This control allows QMediaPlaylist to be passed directly to the service
    instead of playing media sources one by one.  This control should be
    implemented if backend benefits from knowing the next media source to be
    played, for example for preloading, cross fading or gap-less playback.

    If QMediaPlaylistSourceControl is provided, the backend must listen for
    current playlist item changes to load corresponding media source and
    advance the playlist  with QMediaPlaylist::next() when playback of the
    current media is finished.

    The interface name of QMediaPlaylistSourceControl is \c org.qt-project.qt.mediaplaylistsourcecontrol/5.0 as
    defined in QMediaPlaylistSourceControl_iid.

    \sa QMediaService::requestControl(), QMediaPlayer
*/

/*!
    \macro QMediaPlaylistSourceControl_iid

    \c org.qt-project.qt.mediaplaylistsourcecontrol/5.0

    Defines the interface name of the QMediaPlaylistSourceControl class.

    \relates QMediaPlaylistSourceControl
*/

/*!
  Create a new playlist source control object with the given \a parent.
*/
QMediaPlaylistSourceControl::QMediaPlaylistSourceControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
  Destroys the playlist control.
*/
QMediaPlaylistSourceControl::~QMediaPlaylistSourceControl()
{
}


/*!
  \fn QMediaPlaylistSourceControl::playlist() const

  Returns the current playlist.
  Should return a null pointer if no playlist is assigned.
*/

/*!
  \fn QMediaPlaylistSourceControl::setPlaylist(QMediaPlaylist *playlist)

  Set the playlist of this media player to \a playlist.
  If a null pointer is passed, the playlist source should be disabled.

  The current media should be replaced with the current item of the media playlist.
*/


/*!
  \fn QMediaPlaylistSourceControl::playlistChanged(QMediaPlaylist* playlist)

  Signal emitted when the playlist has changed to \a playlist.
*/

#include "moc_qmediaplaylistsourcecontrol_p.cpp"
QT_END_NAMESPACE

