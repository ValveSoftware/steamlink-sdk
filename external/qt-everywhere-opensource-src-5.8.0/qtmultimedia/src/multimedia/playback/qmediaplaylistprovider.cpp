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

#include "qmediaplaylistprovider_p.h"

#include <QtCore/qurl.h>

QT_BEGIN_NAMESPACE

/*!
    \class QMediaPlaylistProvider
    \internal

    \brief The QMediaPlaylistProvider class provides an abstract list of media.
    \inmodule QtMultimedia

    \ingroup multimedia
    \ingroup multimedia_playback

    \sa QMediaPlaylist
*/

/*!
    Constructs a playlist provider with the given \a parent.
*/
QMediaPlaylistProvider::QMediaPlaylistProvider(QObject *parent)
    :QObject(parent), d_ptr(new QMediaPlaylistProviderPrivate)
{
}

/*!
    \internal
*/
QMediaPlaylistProvider::QMediaPlaylistProvider(QMediaPlaylistProviderPrivate &dd, QObject *parent)
    :QObject(parent), d_ptr(&dd)
{
}

/*!
    Destroys a playlist provider.
*/
QMediaPlaylistProvider::~QMediaPlaylistProvider()
{
    delete d_ptr;
}

/*!
    \fn QMediaPlaylistProvider::mediaCount() const;

    Returns the size of playlist.
*/

/*!
    \fn QMediaPlaylistProvider::media(int index) const;

    Returns the media at \a index in the playlist.

    If the index is invalid this will return a null media content.
*/


/*!
    Loads a playlist using network \a request. If no playlist \a format is specified the loader
    will inspect the URL or probe the headers to guess the format.

    New items are appended to playlist.

    Returns true if the provider supports the format and loading from the locations URL protocol,
    otherwise this will return false.
*/
bool QMediaPlaylistProvider::load(const QNetworkRequest &request, const char *format)
{
    Q_UNUSED(request);
    Q_UNUSED(format);
    return false;
}

/*!
    Loads a playlist from from an I/O \a device. If no playlist \a format is specified the loader
    will probe the headers to guess the format.

    New items are appended to playlist.

    Returns true if the provider supports the format and loading from an I/O device, otherwise this
    will return false.
*/
bool QMediaPlaylistProvider::load(QIODevice * device, const char *format)
{
    Q_UNUSED(device);
    Q_UNUSED(format);
    return false;
}

/*!
    Saves the contents of a playlist to a URL \a location.  If no playlist \a format is specified
    the writer will inspect the URL to guess the format.

    Returns true if the playlist was saved successfully; and false otherwise.
  */
bool QMediaPlaylistProvider::save(const QUrl &location, const char *format)
{
    Q_UNUSED(location);
    Q_UNUSED(format);
    return false;
}

/*!
    Saves the contents of a playlist to an I/O \a device in the specified \a format.

    Returns true if the playlist was saved successfully; and false otherwise.
*/
bool QMediaPlaylistProvider::save(QIODevice * device, const char *format)
{
    Q_UNUSED(device);
    Q_UNUSED(format);
    return false;
}

/*!
    Returns true if a playlist is read-only; otherwise returns false.
*/
bool QMediaPlaylistProvider::isReadOnly() const
{
    return true;
}

/*!
    Append \a media to a playlist.

    Returns true if the media was appended; and false otherwise.
*/
bool QMediaPlaylistProvider::addMedia(const QMediaContent &media)
{
    Q_UNUSED(media);
    return false;
}

/*!
    Append multiple media \a items to a playlist.

    Returns true if the media items were appended; and false otherwise.
*/
bool QMediaPlaylistProvider::addMedia(const QList<QMediaContent> &items)
{
    for (const QMediaContent &item : items) {
        if (!addMedia(item))
            return false;
    }

    return true;
}

/*!
    Inserts \a media into a playlist at \a position.

    Returns true if the media was inserted; and false otherwise.
*/
bool QMediaPlaylistProvider::insertMedia(int position, const QMediaContent &media)
{
    Q_UNUSED(position);
    Q_UNUSED(media);
    return false;
}

/*!
    Inserts multiple media \a items into a playlist at \a position.

    Returns true if the media \a items were inserted; and false otherwise.
*/
bool QMediaPlaylistProvider::insertMedia(int position, const QList<QMediaContent> &items)
{
    for (int i=0; i<items.count(); i++) {
        if (!insertMedia(position+i,items.at(i)))
            return false;
    }

    return true;
}

/*!
  Move the media from position \a from to position \a to.

  Returns true if the operation is successful, otherwise false.

  \since 5.7
*/
bool QMediaPlaylistProvider::moveMedia(int from, int to)
{
    Q_UNUSED(from);
    Q_UNUSED(to);

    return false;
}

/*!
    Removes the media at \a position from a playlist.

    Returns true if the media was removed; and false otherwise.
*/
bool QMediaPlaylistProvider::removeMedia(int position)
{
    Q_UNUSED(position);
    return false;
}

/*!
    Removes the media between the given \a start and \a end positions from a playlist.

    Returns true if the media was removed; and false otherwise.
  */
bool QMediaPlaylistProvider::removeMedia(int start, int end)
{
    for (int pos=start; pos<=end; pos++) {
        if (!removeMedia(pos))
            return false;
    }

    return true;
}

/*!
    Removes all media from a playlist.

    Returns true if the media was removed; and false otherwise.
*/
bool QMediaPlaylistProvider::clear()
{
    return removeMedia(0, mediaCount()-1);
}

/*!
    Shuffles the contents of a playlist.
*/
void QMediaPlaylistProvider::shuffle()
{
}

/*!
    \fn void QMediaPlaylistProvider::mediaAboutToBeInserted(int start, int end);

    Signals that new media is about to be inserted into a playlist between the \a start and \a end
    positions.
*/

/*!
    \fn void QMediaPlaylistProvider::mediaInserted(int start, int end);

    Signals that new media has been inserted into a playlist between the \a start and \a end
    positions.
*/

/*!
    \fn void QMediaPlaylistProvider::mediaAboutToBeRemoved(int start, int end);

    Signals that media is about to be removed from a playlist between the \a start and \a end
    positions.
*/

/*!
    \fn void QMediaPlaylistProvider::mediaRemoved(int start, int end);

    Signals that media has been removed from a playlist between the \a start and \a end positions.
*/

/*!
    \fn void QMediaPlaylistProvider::mediaChanged(int start, int end);

    Signals that media in playlist between the \a start and \a end positions inclusive has changed.
*/

/*!
    \fn void QMediaPlaylistProvider::loaded()

    Signals that a load() finished successfully.
*/

/*!
    \fn void QMediaPlaylistProvider::loadFailed(QMediaPlaylist::Error error, const QString& errorMessage)

    Signals that a load failed() due to an \a error.  The \a errorMessage provides more information.
*/

#include "moc_qmediaplaylistprovider_p.cpp"
QT_END_NAMESPACE

